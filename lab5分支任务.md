# Lab 5 GDB 双重调试实验报告

## 1. 实验概述

**实验目标**：
使用“双重 GDB”方案（同时调试 uCore 内核与 QEMU 模拟器），完整观测 RISC-V 系统调用的全流程。重点观察从用户态（U Mode）触发 `ecall` 进入内核态（S Mode），以及从内核态执行 `sret` 返回用户态的硬件处理细节。

**实验环境**：
- 操作系统：WSL (Ubuntu)
- 模拟器：QEMU (RISC-V 64位)
- 调试器：GDB (Multi-architecture)

---

## 2. 完整调试方案（终极版）

经过多次尝试与优化，我们总结出了一套“全速触发 + 人工筛选”的调试方案，能够有效规避时钟中断干扰，精准捕获目标指令。

### 第一阶段：环境启动
1.  **终端 1 (QEMU Host)**：
    执行 `make debug`，启动 QEMU 并等待 GDB 连接（监听 localhost:1234）。

### 第二阶段：用户态锚点部署 (终端 3 - uCore GDB)
1.  **连接内核**：
    执行 `make gdb`，执行`file bin/kernel`加载 bin/kernel 符号表并连接 QEMU。
2.  **跨过加载阶段**：
    ```gdb
    break load_icode
    continue
    finish
    ```
    *目的：等待内核将用户程序加载到内存中，否则后续无法加载用户符号。*
3.  **加载用户符号**：
    ```gdb
    add-symbol-file obj/__user_exit.out
    y
    ```
    *目的：让 GDB 识别用户程序的函数和变量。*
4.  **定位系统调用**：
    ```gdb
    break *0x800020  # 用户程序入口 _start
    continue
    break syscall    # 用户库函数 syscall
    continue
    ```
5.  **锁定 ecall 指令**：
    使用 `disassemble` 查看 `syscall` 函数汇编代码，找到 `ecall` 指令的地址（本实验中为 `0x800102`）。
    ```gdb
    break *0x800102
    continue
    display/i $pc
    ```
    *确认状态：GDB 停在 `ecall` 指令处。*

### 第三阶段：上帝视角拦截 (终端 2 - QEMU GDB)
1.  **附加进程**：
    使用 `pgrep -f qemu` 获取 PID，执行 `sudo gdb` 并 `attach <PID>`。
2.  **设置拦截网**：
    ```gdb
    handle SIGPIPE nostop noprint
    break riscv_cpu_do_interrupt
    continue
    ```
    *状态：终端 2 显示 `Continuing.` 并挂起，等待 QEMU 触发中断。*

### 第四阶段：观测 ecall (核心演示)
1.  **触发 (终端 3)**：
    输入 `continue`（注意：使用 continue 而非 si，确保程序不因时钟中断而暂停，直奔 ecall）。
    *现象：终端 3 卡死，等待硬件响应。*
2.  **筛选与观测 (终端 2)**：
    终端 2 断点触发。检查中断原因：
    ```gdb
    print/x ((RISCVCPU *)cs)->env.scause
    ```
    - 若为 `0x...5` (时钟中断)，输入 `continue` 放行然后再运行上面指令。
    - 若为 `0x8` (User ecall)，**拦截成功**。
3.  **验证数据**：
    ```gdb
    print/x ((RISCVCPU *)cs)->env.sepc  # 应为 0x800102 (ecall 地址)
    print ((RISCVCPU *)cs)->env.priv    # 应为 1 (已切换至 S Mode)
    ```

### 第五阶段：观测 sret (核心演示)
1.  **放行 QEMU (终端 2)**：`delete breakpoints` -> `continue`。
2.  **定位返回指令 (终端 3)**：
    ```gdb
    break __trapret
    continue
    disassemble
    ```
    找到 `sret` 指令地址（如 `0xffffffffc0200f02`），并打断点 `break *0xffffffffc0200f02` -> `continue`。
3.  **拦截 helper 函数 (终端 2)**：
    按 `Ctrl+C` 暂停，设置断点 `break helper_sret` -> `continue`。
4.  **触发返回 (终端 3)**：输入 `si`。
5.  **验证数据 (终端 2)**：
    ```gdb
    print/x env->sepc  # 应为 0x800106 (ecall 下一条指令)
    print env->priv    # 应为 1 (函数执行完后即将变回 0)
    ```

---

## 3. QEMU 处理流程与源码分析

### 3.1 ecall 指令的处理 (`riscv_cpu_do_interrupt`)
当用户程序执行 `ecall` 时，QEMU 的 TCG 引擎会捕获该异常，并调用 `riscv_cpu_do_interrupt` 函数。
- **关键流程**：
    1.  **异常识别**：函数读取 `env->scause`，发现其值为 `8` (Environment call from U-mode)。
    2.  **保存现场**：将当前的程序计数器 (PC) 保存到 `sepc` 寄存器 (`env->sepc = env->pc`)。在我们的实验中，`sepc` 被设置为 `0x800102`。
    3.  **特权级切换**：将 `priv` 从 User Mode (0) 提升到 Supervisor Mode (1)。这解释了为什么我们在断点处看到 `priv=1`。
    4.  **跳转**：将 PC 设置为内核中断向量表的地址 (`stvec`)。

### 3.2 sret 指令的处理 (`helper_sret`)
`sret` 是一条特权指令，QEMU 通过辅助函数 `helper_sret` 来模拟其行为。
- **关键流程**：
    1.  **权限检查**：检查当前是否处于 S Mode 或更高权限。
    2.  **恢复 PC**：读取 `sepc` 寄存器的值，并将其赋值给 `pc` (`env->pc = env->sepc`)。在实验中，我们看到 `sepc` 为 `0x800106`，即 `ecall` 的下一条指令。
    3.  **恢复特权级**：读取 `sstatus` 中的 `SPP` (Previous Privilege) 位，恢复之前的特权级（通常是 U Mode）。
    4.  **状态更新**：更新 `sstatus` 的 `SIE` (Interrupt Enable) 等位。

### 3.3 TCG Translation 与地址翻译实验的关联

QEMU 的核心机制是 TCG (Tiny Code Generator)，它将目标指令（如 RISC-V）翻译成宿主机指令（如 x86）执行。

1.  **本实验 (Lab 5) 中的 TCG**：
    *   对于简单的算术指令（如 `add`），TCG 会直接生成对应的 x86 `add` 指令。
    *   对于复杂的特权指令（如 `ecall`, `sret`），TCG 无法直接生成对应的简单指令，而是生成调用 C 语言编写的辅助函数（Helper Functions）的代码。这就是为什么我们能在 GDB 中拦截到 `helper_sret` 和 `riscv_cpu_do_interrupt` 的原因。

2.  **与 Lab 2 (地址翻译) 的关联**：
    *   在 Lab 2 的双重 GDB 实验中，我们观测的是**虚拟地址到物理地址的转换**。
    *   这个过程同样依赖于 TCG 的机制，特别是 **SoftMMU**。
    *   当 TCG 翻译 Load/Store 指令（如 `ld`, `sd`）时，它不会直接生成访问宿主机内存的指令，而是插入查找 TLB 的逻辑。
    *   如果 TLB 未命中（TLB Miss），TCG 就会调用内存管理的 Helper 函数（如 `tlb_fill`）。这个 Helper 函数会遍历 RISC-V 的页表，找到物理地址，并填充到 TLB 中。
    *   **结论**：Lab 5 和 Lab 2 的双重 GDB 调试本质上都在观测 **QEMU 如何通过调用 Helper 函数来模拟硬件的复杂行为**。Lab 5 模拟的是**控制流和特权级**的硬件逻辑（中断/异常），而 Lab 2 模拟的是**内存管理单元 (MMU)** 的硬件逻辑（地址转换）。

---

## 4. 大模型辅助调试记录

在本次实验中，我与 AI 助手（大模型）紧密合作，解决了多个棘手问题。以下是关键的交互记录：

### 4.1 问题一：无法加载用户程序符号
- **情景**：在终端 3 中使用 `break user/libs/syscall.c:15` 时，GDB 报错找不到源文件。
- **大模型思路**：指出 uCore 采用 "Link-in-Kernel" 方式，用户程序是作为二进制 blob 链接进内核的，`make debug` 默认只加载了内核符号。
- **解决方案**：提示使用 `add-symbol-file obj/__user_exit.out` 手动加载用户程序符号表，并解释了需要在 `load_icode` 之后加载的原理。

### 4.2 问题二：时钟中断干扰与 ecall 捕获失败
- **情景**：在终端 2 设置 `break riscv_cpu_do_interrupt` 后，总是频繁断在 `scause=0x8000...05` (时钟中断)，导致很难抓到 `ecall`。
- **大模型思路**：
    1.  初次尝试：建议使用条件断点 `break ... if ...scause == 8`。
    2.  遇到困难：由于时序问题（终端 3 使用 `si` 单步执行），可能在 `si` 的间隙正好处理了一个时钟中断，导致终端 3 认为指令执行完毕，从而错过了 `ecall` 的触发时机。
    3.  **终极方案**：提出“全速触发”策略。让终端 3 使用 `continue` 全速奔跑，强行撞击终端 2 的断点网，配合人工筛选或条件断点，最终成功捕获。

### 4.3 有趣的细节
- **特权级的“超前”显示**：在拦截到 `riscv_cpu_do_interrupt` 时，我们发现 `priv` 已经是 1 了。大模型解释说，这是因为 QEMU 在调用该函数前，硬件逻辑层面已经完成了陷入动作，这让我们对模拟器的“原子性”有了更深的理解。
- **helper_sret 的状态**：在 `helper_sret` 内部观测时，`priv` 仍然是 1。直到函数执行完毕（`finish` 后），特权级才真正变回 0。这展示了软件模拟中状态更新的时序细节。

---

## 5. 总结
通过本次双重 GDB 调试，我们不仅验证了 uCore 系统调用的实现逻辑，更深入到了 QEMU 模拟器的源码层面，亲眼见证了虚拟化技术如何通过软件代码模拟硬件行为。从 `ecall` 的异常分发到 `sret` 的上下文恢复，每一个寄存器的变化都验证了 RISC-V 架构规范的定义。
