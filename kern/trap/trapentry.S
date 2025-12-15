#include <riscv.h>

    .altmacro
    .align 2
    # 笔记: SAVE_ALL宏 - 保存所有寄存器到内核栈，实现双栈机制的关键
    .macro SAVE_ALL
    LOCAL _restore_kernel_sp
    LOCAL _save_context

    # If coming from userspace, preserve the user stack pointer and load
    # the kernel stack pointer. If we came from the kernel, sscratch
    # will contain 0, and we should continue on the current stack.
    # 笔记: 双栈切换的核心 - 交换sp和sscratch
    # 笔记: 若来自用户态，sscratch保存内核栈指针，sp保存用户栈指针
    # 笔记: 若来自内核态，sscratch为0，sp保存内核栈指针
    csrrw sp, sscratch, sp
    # 笔记: sp不为0说明来自用户态(现在sp是内核栈，sscratch是用户栈)，跳到_save_context
    bnez sp, _save_context

_restore_kernel_sp:
    # 笔记: sp为0说明来自内核态，需要从sscratch恢复内核栈指针
    csrr sp, sscratch
_save_context:
    # 笔记: 在内核栈上分配trapframe空间(36个寄存器大小)
    addi sp, sp, -36 * REGBYTES
    # save x registers
    STORE x0, 0*REGBYTES(sp)
    STORE x1, 1*REGBYTES(sp)
    STORE x3, 3*REGBYTES(sp)
    STORE x4, 4*REGBYTES(sp)
    STORE x5, 5*REGBYTES(sp)
    STORE x6, 6*REGBYTES(sp)
    STORE x7, 7*REGBYTES(sp)
    STORE x8, 8*REGBYTES(sp)
    STORE x9, 9*REGBYTES(sp)
    STORE x10, 10*REGBYTES(sp)
    STORE x11, 11*REGBYTES(sp)
    STORE x12, 12*REGBYTES(sp)
    STORE x13, 13*REGBYTES(sp)
    STORE x14, 14*REGBYTES(sp)
    STORE x15, 15*REGBYTES(sp)
    STORE x16, 16*REGBYTES(sp)
    STORE x17, 17*REGBYTES(sp)
    STORE x18, 18*REGBYTES(sp)
    STORE x19, 19*REGBYTES(sp)
    STORE x20, 20*REGBYTES(sp)
    STORE x21, 21*REGBYTES(sp)
    STORE x22, 22*REGBYTES(sp)
    STORE x23, 23*REGBYTES(sp)
    STORE x24, 24*REGBYTES(sp)
    STORE x25, 25*REGBYTES(sp)
    STORE x26, 26*REGBYTES(sp)
    STORE x27, 27*REGBYTES(sp)
    STORE x28, 28*REGBYTES(sp)
    STORE x29, 29*REGBYTES(sp)
    STORE x30, 30*REGBYTES(sp)
    STORE x31, 31*REGBYTES(sp)

    # get sr, epc, tval, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    # 笔记: 若来自用户态，s0保存用户栈指针，sscratch清零(标记进入内核)
    csrrw s0, sscratch, x0
    # 笔记: 保存CSR寄存器到trapframe
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, 0x143  # stval
    csrr s4, scause

    # 笔记: 将用户栈指针(若来自用户态)和CSR保存到trapframe
    STORE s0, 2*REGBYTES(sp)   # 笔记: 用户栈指针或0
    STORE s1, 32*REGBYTES(sp)  # sstatus
    STORE s2, 33*REGBYTES(sp)  # sepc
    STORE s3, 34*REGBYTES(sp)  # stval
    STORE s4, 35*REGBYTES(sp)  # scause
    .endm

    # 笔记: RESTORE_ALL宏 - 从内核栈恢复所有寄存器，返回用户态或内核态
    .macro RESTORE_ALL
    LOCAL _save_kernel_sp
    LOCAL _restore_context

    # 笔记: 加载sstatus检查返回哪个特权级
    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)

    # 笔记: 检查SSTATUS_SPP位判断返回用户态还是内核态
    andi s0, s1, SSTATUS_SPP
    # 笔记: SPP!=0说明返回内核态，跳过sscratch设置
    bnez s0, _restore_context

_save_kernel_sp:
    # Save unwound kernel stack pointer in sscratch
    # 笔记: SPP=0说明返回用户态，将内核栈顶保存到sscratch，供下次中断使用
    addi s0, sp, 36 * REGBYTES
    csrw sscratch, s0
_restore_context:
    # 笔记: 恢复CSR寄存器
    csrw sstatus, s1
    csrw sepc, s2

    # restore x registers
    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    LOAD x4, 4*REGBYTES(sp)
    LOAD x5, 5*REGBYTES(sp)
    LOAD x6, 6*REGBYTES(sp)
    LOAD x7, 7*REGBYTES(sp)
    LOAD x8, 8*REGBYTES(sp)
    LOAD x9, 9*REGBYTES(sp)
    LOAD x10, 10*REGBYTES(sp)
    LOAD x11, 11*REGBYTES(sp)
    LOAD x12, 12*REGBYTES(sp)
    LOAD x13, 13*REGBYTES(sp)
    LOAD x14, 14*REGBYTES(sp)
    LOAD x15, 15*REGBYTES(sp)
    LOAD x16, 16*REGBYTES(sp)
    LOAD x17, 17*REGBYTES(sp)
    LOAD x18, 18*REGBYTES(sp)
    LOAD x19, 19*REGBYTES(sp)
    LOAD x20, 20*REGBYTES(sp)
    LOAD x21, 21*REGBYTES(sp)
    LOAD x22, 22*REGBYTES(sp)
    LOAD x23, 23*REGBYTES(sp)
    LOAD x24, 24*REGBYTES(sp)
    LOAD x25, 25*REGBYTES(sp)
    LOAD x26, 26*REGBYTES(sp)
    LOAD x27, 27*REGBYTES(sp)
    LOAD x28, 28*REGBYTES(sp)
    LOAD x29, 29*REGBYTES(sp)
    LOAD x30, 30*REGBYTES(sp)
    LOAD x31, 31*REGBYTES(sp)
    # restore sp last
    # 笔记: 最后恢复sp，若返回用户态则sp恢复为用户栈指针
    LOAD x2, 2*REGBYTES(sp)
    .endm

    .globl __alltraps
__alltraps:
    # 笔记: 保存所有寄存器，建立trapframe
    SAVE_ALL

    # 笔记: 将trapframe指针作为参数传递给trap函数
    move  a0, sp
    jal trap
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    # 笔记: 恢复所有寄存器，根据SPP决定返回用户态还是内核态
    RESTORE_ALL
    # return from supervisor call
    # 笔记: sret指令：PC←sepc, 特权级←SPP, 中断使能←SPIE
    sret

    # 笔记: forkrets - 新进程第一次被调度时的入口，从trapframe开始执行
    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    # 笔记: 设置sp指向新进程的trapframe，然后通过__trapret返回用户态
    move sp, a0
    j __trapret

    .global kernel_execve_ret
kernel_execve_ret:
    // adjust sp to beneath kstacktop of current process
    addi a1, a1, -36*REGBYTES

    // copy from previous trapframe to new trapframe
    LOAD s1, 35*REGBYTES(a0)
    STORE s1, 35*REGBYTES(a1)
    LOAD s1, 34*REGBYTES(a0)
    STORE s1, 34*REGBYTES(a1)
    LOAD s1, 33*REGBYTES(a0)
    STORE s1, 33*REGBYTES(a1)
    LOAD s1, 32*REGBYTES(a0)
    STORE s1, 32*REGBYTES(a1)
    LOAD s1, 31*REGBYTES(a0)
    STORE s1, 31*REGBYTES(a1)
    LOAD s1, 30*REGBYTES(a0)
    STORE s1, 30*REGBYTES(a1)
    LOAD s1, 29*REGBYTES(a0)
    STORE s1, 29*REGBYTES(a1)
    LOAD s1, 28*REGBYTES(a0)
    STORE s1, 28*REGBYTES(a1)
    LOAD s1, 27*REGBYTES(a0)
    STORE s1, 27*REGBYTES(a1)
    LOAD s1, 26*REGBYTES(a0)
    STORE s1, 26*REGBYTES(a1)
    LOAD s1, 25*REGBYTES(a0)
    STORE s1, 25*REGBYTES(a1)
    LOAD s1, 24*REGBYTES(a0)
    STORE s1, 24*REGBYTES(a1)
    LOAD s1, 23*REGBYTES(a0)
    STORE s1, 23*REGBYTES(a1)
    LOAD s1, 22*REGBYTES(a0)
    STORE s1, 22*REGBYTES(a1)
    LOAD s1, 21*REGBYTES(a0)
    STORE s1, 21*REGBYTES(a1)
    LOAD s1, 20*REGBYTES(a0)
    STORE s1, 20*REGBYTES(a1)
    LOAD s1, 19*REGBYTES(a0)
    STORE s1, 19*REGBYTES(a1)
    LOAD s1, 18*REGBYTES(a0)
    STORE s1, 18*REGBYTES(a1)
    LOAD s1, 17*REGBYTES(a0)
    STORE s1, 17*REGBYTES(a1)
    LOAD s1, 16*REGBYTES(a0)
    STORE s1, 16*REGBYTES(a1)
    LOAD s1, 15*REGBYTES(a0)
    STORE s1, 15*REGBYTES(a1)
    LOAD s1, 14*REGBYTES(a0)
    STORE s1, 14*REGBYTES(a1)
    LOAD s1, 13*REGBYTES(a0)
    STORE s1, 13*REGBYTES(a1)
    LOAD s1, 12*REGBYTES(a0)
    STORE s1, 12*REGBYTES(a1)
    LOAD s1, 11*REGBYTES(a0)
    STORE s1, 11*REGBYTES(a1)
    LOAD s1, 10*REGBYTES(a0)
    STORE s1, 10*REGBYTES(a1)
    LOAD s1, 9*REGBYTES(a0)
    STORE s1, 9*REGBYTES(a1)
    LOAD s1, 8*REGBYTES(a0)
    STORE s1, 8*REGBYTES(a1)
    LOAD s1, 7*REGBYTES(a0)
    STORE s1, 7*REGBYTES(a1)
    LOAD s1, 6*REGBYTES(a0)
    STORE s1, 6*REGBYTES(a1)
    LOAD s1, 5*REGBYTES(a0)
    STORE s1, 5*REGBYTES(a1)
    LOAD s1, 4*REGBYTES(a0)
    STORE s1, 4*REGBYTES(a1)
    LOAD s1, 3*REGBYTES(a0)
    STORE s1, 3*REGBYTES(a1)
    LOAD s1, 2*REGBYTES(a0)
    STORE s1, 2*REGBYTES(a1)
    LOAD s1, 1*REGBYTES(a0)
    STORE s1, 1*REGBYTES(a1)
    LOAD s1, 0*REGBYTES(a0)
    STORE s1, 0*REGBYTES(a1)

    // acutually adjust sp
    move sp, a1
    j __trapret