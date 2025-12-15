#!/bin/bash

# 2310675: COW Challenge - 独立测试脚本
# Copy-on-Write 机制测试

echo "========================================="
echo " COW Challenge 测试 - 学号 2310675"
echo "========================================="
echo ""

cd /home/wsy/lab5

# 编译
echo "[1/2] 编译项目..."
make -j4 > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "编译失败!"
    exit 1
fi
echo "✓ 编译成功"
echo ""

# 运行测试
echo "[2/2] 运行 COW 测试..."
echo "----------------------------------------"

timeout 10 bash -c "make qemu 2>&1" | tee /tmp/cow_run.log | grep --color=auto "COW Test"

echo "----------------------------------------"
echo ""

# 检查结果
if grep -q "COW Test: \*\*\* PASSED \*\*\*" .cowtest.log 2>/dev/null; then
    echo "========================================="
    echo "  ✓✓✓ COW 测试通过! ✓✓✓"
    echo "========================================="
    echo ""
    echo "验证通过的功能:"
    echo "  ✓ fork时父子进程共享物理页面"
    echo "  ✓ 读操作不触发COW (共享访问)"
    echo "  ✓ 写操作触发page fault"
    echo "  ✓ do_cow_page_fault成功复制页面"
    echo "  ✓ 父子进程数据独立互不影响"
    echo ""
    echo "查看完整日志: cat .cowtest.log"
    exit 0
else
    echo "========================================="
    echo "  ✗ COW 测试失败"
    echo "========================================="
    echo "查看日志: cat .cowtest.log"
    exit 1
fi
