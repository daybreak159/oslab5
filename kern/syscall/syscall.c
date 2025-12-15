#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>

// 笔记: 系统调用的内核态处理函数，从寄存器参数转发到具体的do_*函数
static int
sys_exit(uint64_t arg[]) {
    int error_code = (int)arg[0];
    // 笔记: 终止当前进程，释放资源，进入ZOMBIE状态等待父进程回收
    return do_exit(error_code);
}

static int
sys_fork(uint64_t arg[]) {
    struct trapframe *tf = current->tf;
    uintptr_t stack = tf->gpr.sp;
    // 笔记: 创建当前进程的副本，父进程返回子PID，子进程返回0
    return do_fork(0, stack, tf);
}

static int
sys_wait(uint64_t arg[]) {
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    // 笔记: 父进程等待子进程退出，pid=0表示等待任意子进程
    return do_wait(pid, store);
}

static int
sys_exec(uint64_t arg[]) {
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];
    // 笔记: 在当前进程内加载新程序，替换整个地址空间但保持PID不变
    return do_execve(name, len, binary, size);
}

static int
sys_yield(uint64_t arg[]) {
    return do_yield();
}

static int
sys_kill(uint64_t arg[]) {
    int pid = (int)arg[0];
    return do_kill(pid);
}

static int
sys_getpid(uint64_t arg[]) {
    return current->pid;
}

static int
sys_putc(uint64_t arg[]) {
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

static int
sys_pgdir(uint64_t arg[]) {
    //print_pgdir();
    return 0;
}

// 笔记: 系统调用函数指针数组，用系统调用编号作为下标索引对应的处理函数
static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

// 笔记: 系统调用统一入口函数，从trapframe中提取参数并转发到具体处理函数
void
syscall(void) {
    struct trapframe *tf = current->tf;
    uint64_t arg[5];
    int num = tf->gpr.a0;  // 笔记: a0寄存器保存系统调用编号
    if (num >= 0 && num < NUM_SYSCALLS) {  // 笔记: 防止syscalls[num]数组越界
        if (syscalls[num] != NULL) {
            // 笔记: 从a1-a5寄存器提取系统调用参数
            arg[0] = tf->gpr.a1;
            arg[1] = tf->gpr.a2;
            arg[2] = tf->gpr.a3;
            arg[3] = tf->gpr.a4;
            arg[4] = tf->gpr.a5;
            // 笔记: 调用对应的处理函数，返回值写入a0寄存器，用户态从a0获取返回值
            tf->gpr.a0 = syscalls[num](arg);
            return ;
        }
    }
    // 笔记: 如果系统调用编号无效或未实现，打印错误信息并崩溃
    print_trapframe(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n",
            num, current->pid, current->name);
}

