#!/bin/bash

# 2310675: COW Challenge - 独立测试脚本
# 用于测试 Copy-on-Write 机制的实现

echo "========================================="
echo "  COW (Copy-on-Write) Challenge 测试"
echo "  学号: 2310675"
echo "========================================="
echo ""

# 进入 lab5 目录
cd "$(dirname "$0")/.." || exit 1

# 清理旧的构建
echo "[1/4] 清理旧的构建文件..."
make clean > /dev/null 2>&1

# 编译
echo "[2/4] 编译 uCore with COW..."
if make -j4 > /dev/null 2>&1; then
    echo "      ✓ 编译成功"
else
    echo "      ✗ 编译失败"
    exit 1
fi

# 运行 cowtest
echo "[3/4] 运行 COW 测试程序..."
echo ""

timeout 15 qemu-system-riscv64 \
    -machine virt \
    -nographic \
    -bios default \
    -device loader,file=bin/ucore.img,addr=0x80200000 \
    -s -S 2>/dev/null &

QEMU_PID=$!
sleep 1

# 使用 gdb 运行
echo "c" | riscv64-unknown-elf-gdb -quiet -ex "target remote :1234" \
    -ex "file bin/kernel" \
    -ex "break kern_init" \
    obj/kernel.asm 2>/dev/null | grep -A 100 "COW Test" > /tmp/cow_output.txt

# 杀掉 qemu
kill -9 $QEMU_PID 2>/dev/null

# 直接使用 make qemu 更简单
echo "正在运行测试..."
timeout 10 bash -c "echo | make qemu 2>&1" > /tmp/cow_test_output.log

# 分析结果
echo "[4/4] 分析测试结果..."
echo ""
echo "========================================="
echo "  测试输出："
echo "========================================="

# 提取关键输出
if [ -f .cowtest.log ]; then
    grep "COW Test" .cowtest.log
    echo ""

    # 检查测试结果
    if grep -q "COW Test: \*\*\* PASSED \*\*\*" .cowtest.log; then
        echo "========================================="
        echo "  测试结果: ✓ PASSED"
        echo "========================================="
        echo ""
        echo "验证项目："
        echo "  ✓ fork 后父子进程共享页面"
        echo "  ✓ 读操作不触发 COW"
        echo "  ✓ 写操作触发 page fault"
        echo "  ✓ 页面成功复制"
        echo "  ✓ 父子进程数据独立"
        echo ""
        exit 0
    else
        echo "========================================="
        echo "  测试结果: ✗ FAILED"
        echo "========================================="
        exit 1
    fi
else
    echo "错误: 找不到测试日志文件"
    echo "尝试手动运行: make qemu"
    exit 1
fi
