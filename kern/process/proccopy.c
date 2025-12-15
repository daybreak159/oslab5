#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

/* ------------- 进程/线程机制的设计与实现 -------------
(一个简化的 Linux 进程/线程机制)
简介:
  ucore 实现了一个简单的进程/线程机制。进程包含独立的内存空间，至少一个用于
  执行的线程，内核数据（用于管理），处理器状态（用于上下文切换），文件（在 lab6 中）等。ucore 需要
  高效地管理所有这些细节。在 ucore 中，线程只是一种特殊的进程（共享进程的内存）。
------------------------------
进程状态            :     含义               -- 原因
    PROC_UNINIT     :   未初始化             -- alloc_proc
    PROC_SLEEPING   :   睡眠中               -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   可运行(可能正在运行) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   几近死亡             -- do_exit

-----------------------------
进程状态变化:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
进程关系
parent:           proc->parent  (proc 是子进程)
children:         proc->cptr    (proc 是父进程)
older sibling:    proc->optr    (proc 是弟弟进程 [指向哥哥])
younger sibling:  proc->yptr    (proc 是哥哥进程 [指向弟弟])
-----------------------------
相关的进程系统调用:
SYS_exit        : 进程退出,                               -->do_exit
SYS_fork        : 创建子进程, 复制 mm (内存管理结构)       -->do_fork-->wakeup_proc
SYS_wait        : 等待进程                                -->do_wait
SYS_exec        : fork 之后, 进程执行一个程序              -->加载一个程序并刷新 mm
SYS_clone       : 创建子线程                              -->do_fork-->wakeup_proc
SYS_yield       : 进程标记自己需要重新调度, -- proc->need_sched=1, 然后调度器将重新调度此进程
SYS_sleep       : 进程睡眠                                -->do_sleep
SYS_kill        : 杀死进程                                -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : 获取进程的 pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;
static int nr_process = 0; // 记录系统中当前的进程总数

// 声明外部汇编或其它C文件定义的函数
void kernel_thread_entry(void); // 内核线程的汇编入口，通常用于新创建的内核线程首次执行
void forkrets(struct trapframe *tf); // 进程切换后的汇编入口，用于恢复中断帧并跳转到用户态或内核线程函数
void switch_to(struct context *from, struct context *to); // 上下文切换的核心汇编函数

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
// 功能：分配一个新的进程控制块（PCB），并初始化其所有成员变量
// 返回值：成功返回指向新 proc_struct 的指针，失败返回 NULL
static struct proc_struct *
alloc_proc(void)
{
    // 1. 在内核堆中分配 proc_struct 结构体的内存
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));

    if (proc != NULL)
    {
        // 2. 初始化进程状态
        // PROC_UNINIT: 进程处于“未初始化”状态，表明它刚被分配，还没准备好运行
        proc->state = PROC_UNINIT;

        // 3. 初始化进程 ID (PID)
        // -1 表示无效 PID，后续在 do_fork 中会通过 get_pid() 分配一个唯一的正整数 PID
        proc->pid = -1;

        // 4. 初始化运行时间统计
        // 记录该进程已运行的时间片或次数，初始为 0
        proc->runs = 0;

        // 5. 初始化内核栈指针
        // kstack 记录该进程内核栈的虚拟地址。此时还未分配栈空间，故设为 0
        proc->kstack = 0;

        // 6. 初始化调度标志
        // need_resched = 0 表示当前不需要强制调度（不需要让出 CPU）
        // 如果设为 1，时钟中断或系统调用返回时会触发 schedule()
        proc->need_resched = 0;

        // 7. 初始化父进程指针
        // 初始时没有父进程，后续在 do_fork 中指向 current (当前进程)
        proc->parent = NULL;

        // 8. 初始化内存管理结构指针
        // mm_struct 管理进程的虚拟内存空间 (VMA, 页表等)。初始为空。
        proc->mm = NULL;

        // 9. 初始化上下文 (Context)
        // context 用于进程切换 (switch_to)。清零表示初始状态为空。
        memset(&(proc->context), 0, sizeof(struct context));

        // 10. 初始化中断帧指针 (TrapFrame)
        // tf 指向内核栈顶的中断帧，用于保存用户态寄存器或内核线程的初始状态。初始为空。
        proc->tf = NULL;

        // 11. 初始化页目录表物理地址 (Cr3 / SATP)
        // 初始时指向内核的页目录表 (boot_pgdir_pa)。
        // 这样即使它是空进程，也有基本的内核映射，能运行内核代码。
        // 对于用户进程，后续会分配独立的页表并更新此字段。
        proc->pgdir = boot_pgdir_pa;

        // 12. 初始化标志位
        // 用于记录进程的各种属性（如 PF_EXITING），初始为 0。
        proc->flags = 0;

        // 13. 初始化进程名
        // 清空 name 数组。
        memset(proc->name, 0, sizeof(proc->name));

        // 14. 初始化退出码
        // 当进程退出时，会将退出原因/返回值保存在这里供父进程查询。
        proc->exit_code = 0;

        // 15. 初始化等待状态
        // 记录进程在等待什么事件 (如 WT_CHILD, WT_TIMER)。初始为 0 (无等待)。
        proc->wait_state = 0;

        // 16. 初始化进程家族关系指针
        // cptr: child pointer (最年轻的子进程)
        // yptr: younger sibling pointer (更年轻的兄弟进程)
        // optr: older sibling pointer (更年长的兄弟进程)
        // 这些指针构成了进程树，初始都为空。
        proc->cptr = NULL;
        proc->yptr = NULL;
        proc->optr = NULL;
    }
    return proc;
}

// set_proc_name - set the name of proc
// 功能：设置进程的名字
// 参数：
//   proc: 目标进程结构体指针
//   name: 要设置的名字字符串
// 返回值：指向进程名缓冲区的指针
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    // 先清空缓冲区，防止残留数据
    memset(proc->name, 0, sizeof(proc->name));
    // 拷贝名字，长度限制为 PROC_NAME_LEN
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
// 功能：获取进程的名字
// 参数：
//   proc: 目标进程结构体指针
// 返回值：指向名字的指针 (注意：返回的是静态局部变量，非线程安全，仅用于临时打印)
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1]; // 静态缓冲区，用于存储结果
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}
// set_links - set the relation links of process
// 功能：设置进程的家族关系链接，并将进程加入全局进程链表
// 参数：proc - 需要设置关系的进程
// 注意：该函数会修改 proc->parent 的子进程链表，因此调用前 proc->parent 必须已设置
static void
set_links(struct proc_struct *proc)
{
    // 1. 将进程加入全局进程链表 proc_list
    list_add(&proc_list, &(proc->list_link));

    // 2. 维护父子/兄弟关系链表
    // uCore 使用一种特定的方式维护家族树：
    // parent->cptr 指向最年轻的子进程 (Youngest Child)
    // 兄弟进程之间通过 yptr (Younger Sibling) 和 optr (Older Sibling) 连接

    // 新插入的进程通常作为最年轻的子进程
    proc->yptr = NULL; // 它是最年轻的，所以没有更年轻的弟弟

    // 如果父进程已经有子进程了 (parent->cptr != NULL)，
    // 那么那个原本最年轻的子进程现在变成了新进程的哥哥 (Older Sibling)
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc; // 更新哥哥的 yptr 指向新进程
    }

    // 父进程的 cptr 指针更新为指向这个最新的子进程
    proc->parent->cptr = proc;

    // 3. 增加系统进程计数
    nr_process++;
}

// remove_links - clean the relation links of process
// 功能：清除进程的家族关系链接，并从全局进程链表中移除
// 参数：proc - 需要移除的进程
static void
remove_links(struct proc_struct *proc)
{
    // 1. 从全局进程链表 proc_list 中删除
    list_del(&(proc->list_link));

    // 2. 维护兄弟关系链表 (双向链表的删除操作)
    // 如果有哥哥 (Older Sibling)
    if (proc->optr != NULL)
    {
        // 哥哥的弟弟指针，指向我的弟弟 (跳过我)
        proc->optr->yptr = proc->yptr;
    }
    
    // 如果有弟弟 (Younger Sibling)
    if (proc->yptr != NULL)
    {
        // 弟弟的哥哥指针，指向我的哥哥 (跳过我)
        proc->yptr->optr = proc->optr;
    }
    else
    {
        // 如果没有弟弟，说明我是父进程当前“最年轻”的子进程 (cptr 指向我)
        // 我走后，父进程的 cptr 应该指向我的哥哥
        proc->parent->cptr = proc->optr;
    }

    // 3. 减少系统进程计数
    nr_process--;
}

// get_pid - alloc a unique pid for process
// 功能：为新进程分配一个唯一的 PID
// 算法：使用简单的递增策略，配合遍历检查冲突，类似于 Linux 的 PID 分配机制
static int
get_pid(void)
{
    // 静态断言：确保 PID 的取值范围大于系统允许的最大进程数，否则 PID 肯定不够用
    static_assert(MAX_PID > MAX_PROCESS);

    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    
    // last_pid: 上一次分配的 PID
    // next_safe: 下一个“不安全”的 PID（即已知被占用的 PID），在 last_pid ~ next_safe 之间的 PID 都是安全的
    static int next_safe = MAX_PID, last_pid = MAX_PID;

    // 1. 尝试直接递增 last_pid
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1; // 如果超过最大值，回绕到 1
        goto inside;  // 回绕后，之前的 next_safe 失效，必须重新扫描
    }

    // 2. 如果递增后的 last_pid 还没碰到 next_safe，说明这个 PID 是安全的，直接返回
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID; // 先悲观地假设下一个占用的 PID 是最大值

    repeat:
        // 3. 遍历所有进程，检查 last_pid 是否冲突，并计算新的 next_safe
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);

            // 情况 A: 发现 last_pid 已经被某个进程占用了 (冲突)
            if (proc->pid == last_pid)
            {
                // 冲突了，尝试下一个 PID
                if (++last_pid >= next_safe)
                {
                    // 如果再次递增后又超过了 next_safe 或者回绕了，
                    // 需要重置状态，重新开始遍历寻找
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            // 情况 B: 进程 PID 大于 last_pid，但小于当前的 next_safe
            // 这意味着 proc->pid 是 last_pid 后面第一个遇到的“障碍”
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                // 更新 next_safe，标记这个障碍的位置
                // 这样在下次分配时，只要 last_pid < next_safe，就不用遍历链表了
                next_safe = proc->pid;
            }
        }
    }
    
    // 返回找到的唯一 PID
    return last_pid;
}

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load base addr of "proc"'s new PDT
// 功能：让指定的进程 "proc" 在 CPU 上运行（上下文切换的核心函数）
void proc_run(struct proc_struct *proc)
{
    // 只有当要运行的进程不是当前正在运行的进程时，才进行切换
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current; // 记录当前进程（即将被换下的进程）

        // 1. 关中断
        // 上下文切换必须是原子操作，中间不能被中断打断，否则会导致寄存器状态不一致
        local_intr_save(intr_flag);
        {
            // 2. 更新当前进程指针
            current = proc;

            // 3. 切换页表 (CR3 / SATP)
            // lsatp (Load Supervisor Address Translation and Protection) 指令用于加载新的页目录表物理地址。
            // 执行完这行后，CPU 看到的虚拟内存空间就变了（从 prev 的空间变成了 proc 的空间）。
            lsatp(proc->pgdir);

            // 4. 切换上下文 (Context Switch)
            // switch_to 是汇编实现的函数。
            // 它会保存当前 CPU 寄存器到 prev->context，并从 proc->context 恢复寄存器。
            // 当 switch_to 返回时，CPU 实际上已经是通过 proc->context 里的 ra (返回地址) 跳转到了 proc 的代码中执行。
            switch_to(&(prev->context), &(proc->context));
        }
        // 5. 恢复中断
        // 恢复到 proc 进程之前保存的中断状态
        local_intr_restore(intr_flag);
    }
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
// 功能：新进程/线程的“第一站”。
// 当新进程第一次被 schedule 选中并 switch_to 过来时，context.ra 指向这里。
static void
forkret(void)
{
    // forkrets 是汇编函数，它接受 current->tf (中断帧) 作为参数。
    // 它会把中断帧里的寄存器恢复到 CPU 中，然后执行 sret (从中断返回)。
    // 这样，新进程就好像是从一次“假装的”中断中返回一样，开始执行它的入口函数（内核线程入口或用户程序入口）。
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
// 功能：将进程加入到 PID 哈希表中，以便通过 PID 快速查找进程
static void
hash_proc(struct proc_struct *proc)
{
    // pid_hashfn(proc->pid) 计算哈希桶索引
    // list_add 将进程的哈希节点插入对应的链表桶中
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
// 功能：将进程从 PID 哈希表中移除
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
// 功能：根据 PID 快速查找进程结构体
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        // 1. 定位到哈希桶
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        
        // 2. 遍历该桶中的链表
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            // 3. 匹配 PID
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to
//       proc->tf in do_fork-->copy_thread function
// 功能：创建一个内核线程
// 参数：
//   fn: 线程要执行的函数指针
//   arg: 传递给函数的参数
//   clone_flags: 克隆标志（如 CLONE_VM）
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    // 1. 构造一个临时的中断帧 (TrapFrame)
    // 这个 tf 只是个模板，稍后会被 copy_thread 拷贝到新进程的内核栈顶。
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));

    // 2. 设置寄存器
    // 注意：这里使用的是 s0 和 s1 寄存器来暂存函数指针和参数。
    // 在 kernel_thread_entry (汇编入口) 中，会发生如下操作：
    //   move a0, s0  (把 fn 放入 a0)
    //   move a1, s1  (把 arg 放入 a1)
    //   jalr a0      (跳转执行 fn)
    tf.gpr.s0 = (uintptr_t)fn; 
    tf.gpr.s1 = (uintptr_t)arg;

    // 3. 设置状态寄存器 (sstatus)
    // SSTATUS_SPP: Supervisor Previous Privilege，设为 1 表示进入中断前是 S Mode（因为是内核线程）。
    // SSTATUS_SPIE: Supervisor Previous Interrupt Enable，设为 1 表示开启中断。
    // ~SSTATUS_SIE: 关闭当前的中断使能（防御性编程）。
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;

    // 4. 设置入口地址 (epc)
    // 当 forkrets 执行 sret 时，PC 指针会跳转到这里。
    // kernel_thread_entry 是所有内核线程通用的汇编引导代码。
    tf.epc = (uintptr_t)kernel_thread_entry;

    // 5. 调用 do_fork 创建进程
    // CLONE_VM: 内核线程共享内存空间
    // stack=0: 内核线程不需要用户栈
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}
// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
// 功能：为进程分配内核栈 (Kernel Stack)
// 每一个进程都需要一个独立的内核栈，用于在内核态执行代码（如系统调用、中断处理）时保存上下文。
static int
setup_kstack(struct proc_struct *proc)
{
    // 1. 分配物理页
    // KSTACKPAGE 通常定义为 2 (即 8KB)
    struct Page *page = alloc_pages(KSTACKPAGE);
    
    if (page != NULL)
    {
        // 2. 获取虚拟地址并赋值
        // page2kva 将物理页结构体转换为内核虚拟地址
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM; // 内存不足
}

// put_kstack - free the memory space of process kernel stack
// 功能：释放进程的内核栈
// 通常在进程彻底销毁 (do_wait 回收阶段) 时调用。
static void
put_kstack(struct proc_struct *proc)
{
    // kva2page: 虚拟地址 -> 物理页结构体
    // free_pages: 归还物理内存
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
// 功能：为进程分配页目录表 (Page Directory Table, PDT)
// 这是进程拥有独立地址空间的基础。
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    // 1. 分配一页物理内存作为页目录表
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    
    // 2. 获取页表的内核虚拟地址
    pde_t *pgdir = page2kva(page);

    // 3. [关键] 复制内核空间的映射
    // 我们希望所有进程在内核态都能看到相同的内核空间（代码和数据）。
    // boot_pgdir_va 是系统启动时建立的内核页表。
    // 这里将内核页表的内容完整拷贝给新进程，这样新进程也就拥有了内核空间的访问权限。
    // 在 RISC-V Sv39 中，内核通常映射在高地址，只需拷贝页目录表的部分项即可（这里为了简单直接拷贝了一整页）。
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
// 功能：释放页目录表占用的物理页
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

// copy_mm - process "proc" duplicate OR share process "current"'s mm according clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
// 功能：根据 clone_flags 决定是复制还是共享内存空间 (fork vs thread)
// 参数：
//   clone_flags: 包含 CLONE_VM 则共享内存（创建线程），否则复制内存（创建进程/fork）
//   proc: 新创建的子进程结构体
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *mm, *oldmm = current->mm;

    /* current is a kernel thread */
    // 如果当前进程是内核线程 (oldmm 为 NULL)，则新进程也是内核线程。
    // 内核线程直接使用内核的页表，不需要独立的 mm_struct。
    if (oldmm == NULL)
    {
        return 0;
    }

    // ---------------------------------------------------------
    // 情况 A: 创建线程 (CLONE_VM set)
    // ---------------------------------------------------------
    if (clone_flags & CLONE_VM)
    {
        // 共享内存：直接指向父进程的 mm_struct
        mm = oldmm;
        goto good_mm;
    }

    // ---------------------------------------------------------
    // 情况 B: 创建进程 / Fork (CLONE_VM unset)
    // ---------------------------------------------------------
    int ret = -E_NO_MEM;
    
    // 1. 创建一个新的内存描述符
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }
    
    // 2. 分配新的页目录表 (并拷贝内核映射)
    if (setup_pgdir(mm) != 0)
    {
        goto bad_pgdir_cleanup_mm;
    }

    // 3. 复制 VMA 和页表映射 (核心步骤)
    // 加锁防止父进程内存结构在复制过程中发生变化
    lock_mm(oldmm);
    {
        // 调用 dup_mmap (在 vmm.c 中定义)
        // 这会复制所有的 vma_struct，并设置 Copy-on-Write 机制
        ret = dup_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0)
    {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    // 成功处理：
    // 1. 增加 mm 的引用计数 (因为多了一个进程在使用它)
    mm_count_inc(mm);
    // 2. 让子进程指向这个 mm
    proc->mm = mm;
    // 3. 设置子进程的 CR3/SATP 寄存器值
    // 注意：硬件寄存器需要的是物理地址 (PADDR)
    proc->pgdir = PADDR(mm->pgdir);
    return 0;

// 错误处理路径：
bad_dup_cleanup_mmap:
    exit_mmap(mm); // 释放已复制的 VMA 映射
    put_pgdir(mm); // 释放页目录表
bad_pgdir_cleanup_mm:
    mm_destroy(mm); // 释放 mm_struct 本身
bad_mm:
    return ret;
}

// copy_thread - setup the trapframe on the process's kernel stack top and
//             - setup the kernel entry point and stack of process
// 功能：初始化新进程的中断帧 (TrapFrame) 和内核上下文 (Context)
// 这是让新进程能够被调度并正确“返回”到用户态（或内核线程入口）的关键步骤。
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    // 1. 在内核栈顶预留 TrapFrame 的空间
    // proc->kstack 是内核栈的低地址，KSTACKSIZE 是栈大小。
    // 栈是从高地址向低地址增长的，所以栈顶是 kstack + KSTACKSIZE。
    // 我们把 TrapFrame 放在栈的最顶端。
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    
    // 2. 拷贝 TrapFrame
    // 将父进程（或者 kernel_thread 传入的模板）的寄存器状态复制给子进程。
    *(proc->tf) = *tf;

    // 3. 设置返回值 (a0 寄存器)
    // [关键]：fork() 系统调用在子进程中返回 0 的原因就在这里！
    // 我们强制将子进程 TrapFrame 中的 a0 寄存器修改为 0。
    proc->tf->gpr.a0 = 0;

    // 4. 设置栈指针 (sp)
    // 如果 esp 为 0 (通常是内核线程)，则使用当前 TrapFrame 的地址作为栈顶（或由具体逻辑决定）。
    // 如果 esp 非 0 (通常是用户进程 fork)，则使用父进程传入的用户栈指针 (stack)。
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    // 5. 设置上下文 (Context) - 用于内核调度 (switch_to)
    // proc->context 是内核在进程切换时保存/恢复的寄存器集合。
    
    // ra (Return Address): 设置为 forkret 函数的入口地址。
    // 当调度器切换到这个进程时，switch_to 函数最后会执行 ret 指令，
    // CPU 就会跳转到 forkret -> forkrets -> sret，最终进入用户态或内核线程函数。
    proc->context.ra = (uintptr_t)forkret;
    
    // sp (Stack Pointer): 设置为内核栈顶 (指向 TrapFrame)。
    // 这样当进程在内核中运行时，它使用的是自己的内核栈。
    proc->context.sp = (uintptr_t)(proc->tf);
}

/* do_fork - parent process for a new child process
 * @clone_flags: used to guide how to clone the child process (共享内存? 信号?)
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
// 功能：创建一个新的子进程（实现 fork, clone, kernel_thread 的核心逻辑）
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;

    // 1. 检查当前进程总数是否超过上限
    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }

    ret = -E_NO_MEM;
    
    // 2. 分配并初始化进程控制块 (PCB)
    // 此时 proc 处于 UNINIT 状态
    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }

    // 3. 设置父子关系
    // 当前进程 (current) 成为新进程的父进程
    proc->parent = current;
    
    // 确保当前进程没有在等待状态（一种防御性断言）
    assert(current->wait_state == 0);

    // 4. 分配内核栈
    // 为子进程分配 2 页 (8KB) 的内核栈空间
    if ((ret = setup_kstack(proc)) != 0)
    {
        goto bad_fork_cleanup_proc;
    }

    // 5. 复制或共享内存空间 (Memory Management)
    // 根据 clone_flags 决定是复制页表 (fork) 还是共享 mm (线程)。
    // 如果是 fork，这里会建立 Copy-on-Write 映射。
    if ((ret = copy_mm(clone_flags, proc)) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }

    // 6. 设置中断帧和上下文
    // 准备子进程被调度运行时的初始状态 (寄存器、栈、入口地址)。
    copy_thread(proc, stack, tf);

    // 7. 临界区操作：分配 PID 并加入进程链表
    // 这里的操作涉及全局共享数据 (hash_list, proc_list)，必须关中断保护。
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        // 分配唯一的 PID
        proc->pid = get_pid();
        
        // 将进程加入 PID 哈希表 (用于通过 PID 快速查找)
        hash_proc(proc);
        
        // 将进程加入全局进程链表，并维护父子兄弟关系指针
        set_links(proc);
    }
    local_intr_restore(intr_flag);

    // 8. 唤醒子进程
    // 将子进程状态设置为 PROC_RUNNABLE，使其可以被调度器选中执行。
    wakeup_proc(proc);

    // 9. 返回子进程的 PID
    ret = proc->pid;

fork_out:
    return ret;

// 错误处理路径：按分配顺序的逆序释放资源
bad_fork_cleanup_kstack:
    put_kstack(proc); // 释放内核栈
bad_fork_cleanup_proc:
    kfree(proc);      // 释放 proc_struct 内存
    goto fork_out;
}
// do_exit - called by sys_exit
// 1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
// 2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
// 3. call scheduler to switch to other process
// 功能：进程退出的核心函数。负责释放资源、处理父子关系、并切换到其他进程。
int do_exit(int error_code)
{
    // 1. 检查特殊进程
    // idleproc (PID 0) 和 initproc (PID 1) 是系统基石，绝对不能退出。
    if (current == idleproc)
    {
        panic("idleproc exit.\n");
    }
    if (current == initproc)
    {
        panic("initproc exit.\n");
    }

    // 2. 释放内存空间 (Memory Management Cleanup)
    struct mm_struct *mm = current->mm;
    if (mm != NULL)
    {
        // [关键步骤] 切换到内核页表
        // 在销毁当前进程的页表之前，必须先切换回内核页表 (boot_pgdir_pa)。
        // 否则，一旦页表被释放，CPU 将无法进行地址转换，导致系统崩溃。
        lsatp(boot_pgdir_pa);

        // 减少 mm 的引用计数
        // 如果计数降为 0，说明没有其他线程共享这块内存，可以彻底释放。
        if (mm_count_dec(mm) == 0)
        {
            exit_mmap(mm);   // 释放用户态的物理页和页表项
            put_pgdir(mm);   // 释放页目录表本身
            mm_destroy(mm);  // 释放 mm_struct 结构体
        }
        // 切断当前进程与 mm 的联系
        current->mm = NULL;
    }

    // 3. 设置僵尸状态
    // 进程资源已释放，但在进程表中仍占有一个坑位 (proc_struct)，
    // 等待父进程通过 wait 来回收并获取 exit_code。
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;

    // 4. 临界区操作：处理父子进程关系
    // 涉及全局进程关系图的修改，必须关中断保护。
    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);
    {
        // 4.1 通知父进程
        proc = current->parent;
        // 如果父进程正在等待子进程退出 (WT_CHILD)，则唤醒父进程。
        if (proc->wait_state & WT_CHILD)
        {
            wakeup_proc(proc);
        }

        // 4.2 托孤 (Reparenting Orphans)
        // 如果当前进程有子进程，必须将它们过继给 initproc (PID 1)。
        // 否则这些子进程退出后将无人回收，变成永远的僵尸。
        while (current->cptr != NULL)
        {
            // 获取当前进程的一个子进程
            proc = current->cptr;
            // 将该子进程从当前进程的子进程链表中移除 (更新 current->cptr 为下一个兄弟)
            current->cptr = proc->optr;

            // 切断该子进程与旧兄弟的联系 (它现在是 initproc 的孩子了)
            proc->yptr = NULL;
            
            // 将该子进程插入 initproc 的子进程链表头部
            if ((proc->optr = initproc->cptr) != NULL)
            {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc; // 认贼作父...啊不，认祖归宗
            initproc->cptr = proc;

            // 4.3 检查刚过继的子进程状态
            // 如果这个子进程已经是僵尸状态 (PROC_ZOMBIE)，
            // 且 initproc 正在等待子进程，需要立即唤醒 initproc 来处理它。
            if (proc->state == PROC_ZOMBIE)
            {
                if (initproc->wait_state & WT_CHILD)
                {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);

    // 5. 进程调度
    // 当前进程已变成僵尸，无法继续运行。
    // 调用调度器，选择下一个就绪进程上 CPU。
    schedule();

    // 6. 防御性代码
    // schedule() 不应该返回，如果返回了说明调度器出错了。
    panic("do_exit will not return!! %d.\n", current->pid);
}

/* load_icode - load the content of binary program(ELF format) as the new content of current process
 * @binary:  the memory addr of the content of binary program
 * @size:  the size of the content of binary program
 */
// 功能：加载 ELF 格式的二进制程序到当前进程，构建新的内存空间和执行上下文。
static int
load_icode(unsigned char *binary, size_t size)
{
    // 0. 前置检查
    // 确保当前进程的 mm_struct 已经被释放（在 do_execve 中完成）。
    // 此时进程应该是一个只有内核栈和 proc_struct 的“空壳”。
    if (current->mm != NULL)
    {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;

    // (1) 创建新的内存管理器
    // 为进程创建一个新的 mm_struct，用于管理即将建立的 VMA 和页表。
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }

    // (2) 创建新的页目录表 (PDT)
    // 分配一页物理内存作为页目录表，并将内核空间的映射拷贝过来。
    // mm->pgdir 指向这个页表的内核虚拟地址。
    if (setup_pgdir(mm) != 0)
    {
        goto bad_pgdir_cleanup_mm;
    }

    // (3) 解析 ELF 文件并建立内存映射
    // 开始读取二进制数据，解析 ELF Header 和 Program Header。
    struct Page *page;
    
    // (3.1) 获取 ELF 文件头
    struct elfhdr *elf = (struct elfhdr *)binary;
    
    // (3.2) 获取 Program Header Table 的起始位置
    // Program Header 描述了段 (Segment) 的信息，如 .text, .data 等。
    struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);

    // (3.3) 校验 ELF 魔数
    // 确保这是一个合法的 ELF 文件。
    if (elf->e_magic != ELF_MAGIC)
    {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    uint32_t vm_flags, perm;
    struct proghdr *ph_end = ph + elf->e_phnum; // Program Header 结束位置

    // 遍历每一个 Program Header
    for (; ph < ph_end; ph++)
    {
        // (3.4) 过滤无效段
        // 我们只关心类型为 PT_LOAD 的段，只有它们需要被加载到内存。
        if (ph->p_type != ELF_PT_LOAD)
        {
            continue;
        }
        // 检查文件大小是否超过内存大小（基本合法性检查）
        if (ph->p_filesz > ph->p_memsz)
        {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0)
        {
            // 如果段大小为0，跳过（通常不会发生）
            // continue ;
        }

        // (3.5) 设置 VMA 权限标志
        // 根据 ELF 段的标志 (R/W/X)，设置 VMA 的 vm_flags 和页表的 perm。
        vm_flags = 0, perm = PTE_U | PTE_V; // 默认用户态可访问、有效
        
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;

        // 设置 RISC-V 页表权限位
        if (vm_flags & VM_READ) perm |= PTE_R;
        if (vm_flags & VM_WRITE) perm |= (PTE_W | PTE_R); // 写权限通常隐含读权限
        if (vm_flags & VM_EXEC) perm |= PTE_X;

        // 调用 mm_map 建立虚拟内存区域 (VMA)
        // 此时只是在链表里登记了“这一段地址是合法的”，还没有分配物理页。
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0)
        {
            goto bad_cleanup_mmap;
        }

        // 准备拷贝数据
        unsigned char *from = binary + ph->p_offset; // 源数据在 binary 中的偏移
        size_t off, size;
        // start: 段的虚拟起始地址
        // end: 段中包含文件数据的结束地址 (filesz)
        // la: 当前正在处理的页对齐地址
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

        // (3.6) 分配物理页并拷贝代码/数据
        // 处理该段中包含文件内容的部分 (.text, .data)
        end = ph->p_va + ph->p_filesz;
        
        // (3.6.1) 复制文件内容
        while (start < end)
        {
            // 为当前虚拟地址 la 分配一个物理页，并建立映射
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL)
            {
                goto bad_cleanup_mmap;
            }
            
            // 计算页内偏移和本页需要拷贝的大小
            off = start - la;
            size = PGSIZE - off;
            la += PGSIZE;
            
            // 如果这是最后一部分数据，可能不足一页
            if (end < la)
            {
                size -= la - end;
            }
            
            // 执行内存拷贝：将 binary 中的数据复制到新分配的物理页中
            // page2kva 将物理页转换为内核虚拟地址，以便内核写入
            memcpy(page2kva(page) + off, from, size);
            
            // 更新指针
            start += size, from += size;
        }

        // (3.6.2) 处理 BSS 段 (Block Started by Symbol)
        // BSS 段在文件中不占空间 (filesz < memsz)，但在内存中需要占空间并清零。
        end = ph->p_va + ph->p_memsz; // 段的实际内存结束地址
        
        // 如果 start < la，说明刚才处理文件数据的最后一页还有剩余空间，
        // 这部分空间属于 BSS，需要清零。
        if (start < la)
        {
            // 如果 filesz == memsz，说明没有 BSS，直接跳过
            if (start == end)
            {
                continue;
            }
            // 计算本页剩余需要清零的大小
            off = start + PGSIZE - la;
            size = PGSIZE - off;
            if (end < la)
            {
                size -= la - end;
            }
            // 清零
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        
        // 如果 BSS 段很大，跨越了多个新页，需要分配新页并全页清零
        while (start < end)
        {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL)
            {
                goto bad_cleanup_mmap;
            }
            off = start - la;
            size = PGSIZE - off;
            la += PGSIZE;
            if (end < la)
            {
                size -= la - end;
            }
            // 全页清零
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }

    // (4) 建立用户栈 (User Stack)
    // 映射用户栈区域，通常在用户空间最高地址下方。
    // 权限：可读、可写、是栈
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    // 预留 USTACKSIZE (通常是几页) 大小的虚拟空间
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0)
    {
        goto bad_cleanup_mmap;
    }
    
    // 立即分配物理页给用户栈 (这里硬编码分配了 4 页)
    // 注意：实际 OS 可能会采用按需分配 (Page Fault)，这里为了简单直接分配。
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE, PTE_USER) != NULL);

    // (5) 切换到新环境
    // 增加引用计数
    mm_count_inc(mm);
    // 让当前进程指向新的 mm
    current->mm = mm;
    // 记录页表物理地址
    current->pgdir = PADDR(mm->pgdir);
    // 切换页表寄存器 (SATP)，此时 CPU 正式进入新程序的内存空间
    lsatp(PADDR(mm->pgdir));

    // (6) 构建中断帧 (TrapFrame) - 这一步至关重要！
    // 它的目的是伪造一个“现场”，让 CPU 执行 sret 指令后，能“返回”到新程序的入口。
    struct trapframe *tf = current->tf;
    
    // 保留旧的 sstatus 寄存器值 (包含中断使能状态等)
    uintptr_t sstatus = tf->status;
    
    // 清空整个 TrapFrame，防止旧数据残留
    memset(tf, 0, sizeof(struct trapframe));
    
    /* LAB5:EXERCISE1 
     * 设置 TrapFrame 以便从内核态返回到用户态
     */
    
    // a. 设置用户栈指针 (sp)
    // 指向刚才建立的用户栈栈顶
    tf->gpr.sp = USTACKTOP;
    
    // b. 设置入口地址 (epc / sepc)
    // sret 后，PC 指针将跳转到这里。
    // elf->e_entry 是 ELF 文件头中记录的程序入口地址 (通常是 _start)。
    tf->epc = elf->e_entry;
    
    // c. 设置状态寄存器 (status / sstatus)
    // SSTATUS_SPP = 0: 将 SPP 位清零，表示 sret 返回后的特权级是 User Mode。
    // SSTATUS_SPIE = 1: 开启中断使能，允许用户程序运行时响应中断。
    tf->status = sstatus & ~SSTATUS_SPP;
    tf->status |= SSTATUS_SPIE;

    ret = 0;
out:
    return ret;

// 错误处理路径：逐层释放已分配的资源
bad_cleanup_mmap:
    exit_mmap(mm); // 释放 VMA 和物理页
bad_elf_cleanup_pgdir:
    put_pgdir(mm); // 释放页表
bad_pgdir_cleanup_mm:
    mm_destroy(mm); // 释放 mm_struct
bad_mm:
    goto out;
}

// do_execve - call exit_mmap(mm) & put_pgdir(mm) to reclaim memory space of current process
//           - call load_icode to setup new memory space accroding binary prog.
// 功能：用一个新的程序替换当前进程的执行内容（对应 exec 系统调用）。
// 这是一个“破旧立新”的过程：先销毁旧的内存空间，再建立新的。
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
{
    struct mm_struct *mm = current->mm;
    
    // 1. 检查传入的程序名称是否在合法的用户空间内存中
    if (!user_mem_check(mm, (uintptr_t)name, len, 0))
    {
        return -E_INVAL;
    }
    
    // 2. 限制进程名长度
    if (len > PROC_NAME_LEN)
    {
        len = PROC_NAME_LEN;
    }

    // 3. 在内核栈上备份进程名
    // 因为即将销毁用户空间内存，name 指针指向的字符串很快就会失效，必须先拷出来。
    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);

    // 4. 清理旧的内存空间 (Clean up old memory)
    if (mm != NULL)
    {
        cputs("mm != NULL"); // 调试信息
        
        // [关键] 切换回内核页表
        // 在销毁当前页表前，必须保证 CPU 能继续取指执行内核代码。
        lsatp(boot_pgdir_pa);
        
        // 减少引用计数，如果为 0 则释放
        if (mm_count_dec(mm) == 0)
        {
            exit_mmap(mm);   // 释放 VMA 和物理页
            put_pgdir(mm);   // 释放页目录表
            mm_destroy(mm);  // 释放 mm 结构体
        }
        // 置空当前进程的 mm 指针，表示它现在是一个“空壳”
        current->mm = NULL;
    }
    
    // 5. 加载新程序 (Load new program)
    // load_icode 会创建新的 mm，解析二进制，建立映射，设置 TrapFrame。
    int ret;
    if ((ret = load_icode(binary, size)) != 0)
    {
        goto execve_exit; // 加载失败
    }
    
    // 6. 更新进程名
    // 使用之前备份的 local_name
    set_proc_name(current, local_name);
    return 0;

execve_exit:
    // 如果加载新程序失败，由于旧内存已经释放，进程无法恢复，只能退出。
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

// do_yield - ask the scheduler to reschedule
// 功能：当前进程主动让出 CPU（对应 yield 系统调用）。
// 这通常用于实现用户态的协作式调度，或者在忙等待时出让时间片。
int do_yield(void)
{
    // 设置 need_resched 标志位
    // 当 trap 返回或者中断处理结束时，内核会检查此标志。
    // 如果为 1，则调用 schedule() 切换进程。
    current->need_resched = 1;
    return 0;
}

// do_wait - wait one OR any children with PROC_ZOMBIE state, and free memory space of kernel stack
//         - proc struct of this child.
// NOTE: only after do_wait function, all resources of the child proces are free.
// 功能：父进程等待子进程退出，并回收子进程剩余的资源（内核栈、PCB）。
// 参数：
//   pid: 0 表示等待任意子进程，>0 表示等待指定 PID 的子进程。
//   code_store: 用于存储子进程的退出码。
int do_wait(int pid, int *code_store)
{
    struct mm_struct *mm = current->mm;
    // 1. 检查 code_store 指针的合法性
    if (code_store != NULL)
    {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1))
        {
            return -E_INVAL;
        }
    }

    struct proc_struct *proc;
    bool intr_flag, haskid;

repeat:
    haskid = 0; // 标记是否有符合条件的子进程
    
    // 2. 查找目标子进程
    if (pid != 0) 
    {
        // 情况 A: 等待指定 PID 的子进程
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found; // 找到了一具僵尸！
            }
        }
    }
    else 
    {
        // 情况 B: 等待任意子进程 (pid == 0)
        // 遍历当前进程的所有子进程链表
        proc = current->cptr;
        for (; proc != NULL; proc = proc->optr)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found; // 找到了一具僵尸！
            }
        }
    }

    // 3. 处理查找结果
    if (haskid)
    {
        // 有子进程，但它们都还活着 (没变成 ZOMBIE)
        // 当前进程进入睡眠状态 (PROC_SLEEPING)，等待子进程唤醒
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD; // 等待原因：等待子进程
        
        schedule(); // 让出 CPU
        
        // 当被唤醒后（通常是子进程调用了 do_exit），检查是否被 kill 了
        if (current->flags & PF_EXITING)
        {
            do_exit(-E_KILLED);
        }
        
        // 重新开始查找（因为可能有多个子进程同时退出，或者被其他信号唤醒）
        goto repeat;
    }
    
    // 如果 haskid 为 0，说明没有符合条件的子进程（根本没有孩子，或者指定的 PID 不是我的孩子）
    return -E_BAD_PROC;

found:
    // 4. 回收僵尸子进程资源 (Reclaim Resources)
    // 找到了一个处于 ZOMBIE 状态的子进程 proc
    
    // 检查特殊进程（防御性编程）
    if (proc == idleproc || proc == initproc)
    {
        panic("wait idleproc or initproc.\n");
    }

    // 传回退出码
    if (code_store != NULL)
    {
        *code_store = proc->exit_code;
    }

    // 临界区操作：从全局链表中彻底删除该进程
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);   // 从哈希表中移除
        remove_links(proc);  // 从进程树和全链表中移除
    }
    local_intr_restore(intr_flag);

    // 释放内核栈
    put_kstack(proc);
    
    // 释放 PCB (proc_struct)
    kfree(proc);
    
    // 至此，该子进程在系统中彻底消失
    return 0;
}
// do_kill - kill process with pid by set this process's flags with PF_EXITING
// do_kill - 通过设置 PF_EXITING 标志来“杀死”指定 PID 的进程
// 注意：这里并没有立即销毁进程结构，而是打上标记。
// 真正的资源回收由进程自己（在中断返回或调度时检测到标记）调用 do_exit 完成。
int do_kill(int pid)
{
    struct proc_struct *proc;
    // 1. 根据 PID 查找进程控制块
    if ((proc = find_proc(pid)) != NULL)
    {
        // 2. 检查进程是否已经在退出过程中
        if (!(proc->flags & PF_EXITING))
        {
            // 3. 设置退出标志位
            // 进程下次被调度或从中断返回时，会检查此标志并主动调用 do_exit
            proc->flags |= PF_EXITING;
            
            // 4. 如果进程处于可中断的等待状态（如等待定时器或信号），将其唤醒
            // 这样它才能尽快获得 CPU 运行，看到退出标记并执行退出逻辑
            if (proc->wait_state & WT_INTERRUPTED)
            {
                wakeup_proc(proc);
            }
            return 0;
        }
        return -E_KILLED; // 错误：进程已经在退出了
    }
    return -E_INVAL; // 错误：PID 无效
}

// kernel_execve - do SYS_exec syscall to exec a user program called by user_main kernel_thread
// kernel_execve - 内核线程调用此函数来执行一个用户程序
// 原理：这是一个“黑科技”。user_main 是内核线程（运行在 S Mode），无法像用户进程那样直接通过 ecall 发起系统调用。
// 这里通过内联汇编手动设置参数，并使用 ebreak 触发断点异常。
// 中断处理程序会识别出 specific 的参数 (a7=10)，将其视为来自内核态的“伪造”系统调用，从而转发给 syscall 处理函数。
static int
kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    int64_t ret = 0, len = strlen(name);
    // ret = do_execve(name, len, binary, size); 
    // 直接调用 do_execve 是行不通的，因为需要完整的上下文切换流程（构建 TrapFrame、切换页表等），
    // 必须通过中断机制（trap return）来完成从内核态到用户态的飞跃。

    asm volatile(
        "li a0, %1\n"       // a0 = 系统调用号 (SYS_exec)
        "lw a1, %2\n"       // a1 = 程序名称字符串的指针
        "lw a2, %3\n"       // a2 = 程序名称长度
        "lw a3, %4\n"       // a3 = 二进制程序数据的起始地址
        "lw a4, %5\n"       // a4 = 二进制程序数据的大小
        "li a7, 10\n"       // a7 = 10 (魔数)，告诉 trap handler 这是内核发起的特殊请求
        "ebreak\n"          // 触发断点异常，陷入内核的中断处理流程 (trapentry.S -> trap())
        "sw a0, %0\n"       // 中断返回后，将返回值保存到 ret
        : "=m"(ret)
        : "i"(SYS_exec), "m"(name), "m"(len), "m"(binary), "m"(size)
        : "memory");
    
    cprintf("ret = %d\n", ret);
    return ret;
}

// 宏定义：辅助生成 kernel_execve 的调用代码
#define __KERNEL_EXECVE(name, binary, size) ({           \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, binary, (size_t)(size));         \
})

// KERNEL_EXECVE - 用于加载链接到内核镜像中的用户程序
// 在编译时，用户程序（如 exit.c, hello.c）被编译为二进制并链接进内核。
// 链接器会自动生成 _binary_obj___user_xxx_out_start/size 符号。
// 这个宏利用这些符号获取程序的地址和大小，并调用 kernel_execve。
#define KERNEL_EXECVE(x) ({                                    \
    extern unsigned char _binary_obj___user_##x##_out_start[], \
        _binary_obj___user_##x##_out_size[];                   \
    __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,    \
                    _binary_obj___user_##x##_out_size);        \
})

// KERNEL_EXECVE2 - 类似上面的宏，但允许显式指定 start 和 size 符号
#define __KERNEL_EXECVE2(x, xstart, xsize) ({   \
    extern unsigned char xstart[], xsize[];     \
    __KERNEL_EXECVE(#x, xstart, (size_t)xsize); \
})

#define KERNEL_EXECVE2(x, xstart, xsize) __KERNEL_EXECVE2(x, xstart, xsize)

// user_main - kernel thread used to exec a user program
// user_main - 这是一个内核线程函数，它是第一个用户进程的“前身”
// 它的唯一使命就是调用 kernel_execve 加载用户程序，从而“变身”为真正的用户进程。
static int
user_main(void *arg)
{
#ifdef TEST
    // 如果定义了测试宏，加载指定的测试程序
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    // 默认情况加载 "exit" 程序（或者其他默认程序）
    KERNEL_EXECVE(exit);
#endif
    // 如果 exec 成功，当前进程的代码段会被替换，控制权会跳转到用户程序入口，永远不会运行到这里。
    // 如果运行到这里，说明 exec 失败了。
    panic("user_main execve failed.\n");
}
// init_main - the second kernel thread used to create user_main kernel threads
// init_main - 这是 PID 1 (initproc) 的执行入口函数
// 它的生命周期贯穿系统运行的始终，负责启动第一个用户进程，并回收所有孤儿进程。
static int
init_main(void *arg)
{
    // 1. 记录当前的内存分配情况
    // 用于在所有进程退出后检查是否有内存泄漏
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();

    // 2. 创建 user_main 内核线程
    // 这是一个过渡线程，它稍后会调用 kernel_execve 加载用户程序，从而蜕变成真正的第一个用户进程
    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }

    // 3. 进入死循环，回收僵尸子进程 (The Reaper Loop)
    // initproc 的核心职责之一：作为所有孤儿进程的“继父”。
    // 当一个进程的父进程先于它退出时，该进程会被过继给 initproc。
    // initproc 必须不断调用 do_wait 来回收这些僵尸进程，否则进程表会被占满。
    while (do_wait(0, NULL) == 0)
    {
        // 如果当前没有僵尸子进程，或者子进程还在运行，do_wait 会让出 CPU。
        // 这里显式调用 schedule 再次让出 CPU，等待下一次被唤醒。
        schedule();
    }

    // 4. 系统关闭检查
    // 当 do_wait 返回非 0 值（通常是 -E_BAD_PROC），说明 initproc 已经没有任何子进程了。
    // 这意味着系统中所有的用户进程都已经退出。
    cprintf("all user-mode processes have quit.\n");
    
    // 5. 验证进程关系链表是否干净
    // 确保 initproc 没有任何残留的子进程或兄弟关系
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == 2); // 此时系统中应该只剩下 idleproc(0) 和 initproc(1)
    
    // 验证全局进程链表只包含 initproc (idleproc 通常不挂在 proc_list 上或者处理方式不同，具体视实现而定，这里假设链表中此时只剩 initproc 可见)
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));

    cprintf("init check memory pass.\n");
    return 0;
}

// proc_init - set up the first kernel thread idleproc "idle" by itself and
//           - create the second kernel thread init_main
// proc_init - 进程子系统的初始化函数
// 这一步在系统启动早期执行，负责创建系统中最基础的两个线程：idleproc (PID 0) 和 initproc (PID 1)。
void proc_init(void)
{
    int i;

    // 1. 初始化全局进程链表和 PID 哈希表
    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(hash_list + i);
    }

    // 2. 手工创建 idleproc (PID 0)
    // idleproc 是系统中唯一一个不是通过 fork 创建的进程。
    // 它的结构体是直接分配的，内核栈直接指向系统启动时的临时栈 (bootstack)。
    if ((idleproc = alloc_proc()) == NULL)
    {
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;                  // PID 恒为 0
    idleproc->state = PROC_RUNNABLE;    // 状态为就绪
    idleproc->kstack = (uintptr_t)bootstack; // 使用启动时的内核栈
    idleproc->need_resched = 1;         // 标记为需要调度，以便尽快让出 CPU 给 initproc
    set_proc_name(idleproc, "idle");
    nr_process++;

    // 将当前环境设置为 idleproc
    // 此时 CPU 执行流实际上就代表了 idleproc
    current = idleproc;

    // 3. 创建 initproc (PID 1)
    // 利用 kernel_thread 机制创建。
    // 这实际上是一次 do_fork 调用，它会复制 idleproc 的上下文（但会有自己的内核栈）。
    int pid = kernel_thread(init_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }

    // 根据 PID 找到刚刚创建的进程结构体，保存为全局变量 initproc
    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    // 4. 验证初始化结果
    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
// cpu_idle -这是 idleproc 的执行主体
// 当系统没有其他进程处于 RUNNABLE 状态时，调度器会切换到 idleproc 执行此函数。
// 它其实就是系统启动后的“主循环”。
void cpu_idle(void)
{
    while (1)
    {
        // 检查是否需要调度
        // need_resched 标志通常在时钟中断（时间片耗尽）或有新进程被唤醒时由硬件或调度器置位。
        if (current->need_resched)
        {
            schedule(); // 让出 CPU，选择下一个进程运行
        }
        // 在真实的操作系统中，这里通常会执行 "hlt" 或 "wfi" 指令，
        // 让 CPU 进入低功耗睡眠模式，等待中断唤醒。
    }
}