/* 2310675: COW Challenge - Copy-on-Write测试程序
 *
 * 这个程序测试COW机制的基本功能：
 * 1. fork后父子进程共享页面
 * 2. 写操作触发页面复制
 * 3. 复制后父子进程数据独立
 */

#include <stdio.h>
#include <ulib.h>

#define ARRAY_SIZE 1024

int shared_data[ARRAY_SIZE];  // 全局数组，测试COW

int main(void) {
    cprintf("COW Test: Starting Copy-on-Write test...\n");

    // 初始化共享数据
    int i;
    for (i = 0; i < ARRAY_SIZE; i++) {
        shared_data[i] = i;
    }
    cprintf("COW Test: Initialized %d elements\n", ARRAY_SIZE);

    // fork创建子进程
    int pid = fork();

    if (pid == 0) {
        // 子进程
        cprintf("COW Test: Child process (pid=%d) started\n", getpid());

        // 先读取数据（不应触发COW）
        cprintf("COW Test: Child reading data...\n");
        int sum = 0;
        for (i = 0; i < ARRAY_SIZE; i++) {
            sum += shared_data[i];
        }
        cprintf("COW Test: Child read sum = %d\n", sum);

        // 写入数据（应触发COW）
        cprintf("COW Test: Child writing data (should trigger COW)...\n");
        for (i = 0; i < ARRAY_SIZE; i++) {
            shared_data[i] = i * 2;  // 修改数据
        }

        // 验证子进程的修改
        int child_val = shared_data[100];
        cprintf("COW Test: Child modified shared_data[100] = %d (expected 200)\n", child_val);

        if (child_val != 200) {
            cprintf("COW Test: FAILED - Child data incorrect!\n");
            exit(-1);
        }

        cprintf("COW Test: Child process completed successfully\n");
        exit(0);

    } else if (pid > 0) {
        // 父进程
        cprintf("COW Test: Parent process forked child pid=%d\n", pid);

        // 等待子进程完成
        cprintf("COW Test: Parent waiting for child...\n");
        int exit_code;
        waitpid(pid, &exit_code);

        // 验证父进程的数据未被修改
        int parent_val = shared_data[100];
        cprintf("COW Test: Parent's shared_data[100] = %d (expected 100)\n", parent_val);

        if (parent_val != 100) {
            cprintf("COW Test: FAILED - Parent data was modified by child!\n");
            exit(-1);
        }

        cprintf("COW Test: Parent process completed successfully\n");
        cprintf("COW Test: *** PASSED *** Copy-on-Write works correctly!\n");

    } else {
        cprintf("COW Test: Fork failed!\n");
        exit(-1);
    }

    return 0;
}
