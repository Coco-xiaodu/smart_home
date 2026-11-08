#!/bin/bash
# 韦东山 IMX6ULL mqttClient 一键编译脚本
# 用法：在 mqtt 目录下执行  ./build.sh
set -e

cd "$(dirname "$0")"

echo "==> 1/4 清理旧的 build 缓存"
rm -rf build
mkdir build
cd build

echo "==> 2/4 使用交叉工具链配置 CMake"
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-wedongshan.cmake ..

echo "==> 3/4 编译"
make -j$(nproc)

echo "==> 4/4 检查产物架构"
echo ""
file ../mqttClient
echo ""
echo "编译完成！可执行文件: $(pwd)/../mqttClient"
echo ""
echo "部署到板子(把程序和 ARM 版 paho 动态库一起拷过去):"
echo "  scp ../mqttClient root@<板子IP>:/root/"
echo "  scp /home/lixiaodu/tools/paho.mqtt.c/install/lib/libpaho-mqtt3c.so.1.3.16 root@<板子IP>:/usr/lib/"
