##################################
# 韦东山 100ASK-IMX6ULL 交叉编译工具链【纯C专用】
#################################
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 韦东山 SDK 自带的交叉编译器（gcc 7.5.0，glibc 老版本，板子才能运行）
# 注意：不要用 Ubuntu 自带的 arm-linux-gnueabihf-gcc（glibc 2.43 太新，板子跑不了）
set(CMAKE_C_COMPILER /home/lixiaodu/100ask_imx6ull-sdk/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc)

# SDK 自带的 sysroot（避免误用 Ubuntu 的新版头文件/库）
set(CMAKE_SYSROOT /home/lixiaodu/100ask_imx6ull-sdk/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf/sysroot)
set(CMAKE_FIND_ROOT_PATH /home/lixiaodu/100ask_imx6ull-sdk/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf/sysroot)

# imx6ull Cortex-A7 编译参数，只给C用
set(CMAKE_C_FLAGS "-mcpu=cortex-a7 -mfpu=neon -mfloat-abi=hard" CACHE STRING "C flags")

# 查找规则
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
#################################
# end
##################################
