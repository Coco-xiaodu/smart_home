cmake_minimum_required(VERSION 3.15)

# 只启用 C 语言，避免 CMake 默认去检测主机 g++，导致工具链混乱
project(mqttClient C)

set(home /home/lixiaodu/桌面/smart_home/mqtt)
set(EXECUTABLE_OUTPUT_PATH ${home})

# 交叉编译保护：
# 如果没有通过 toolchain 文件指定交叉编译，直接报错，
# 避免静默链接到主机 x86 的 paho 库，编出板子跑不了的 x86 程序
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "\n[错误] 当前不是交叉编译环境！\n请使用韦东山工具链编译，例如：\n  cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-wedongshan.cmake ..\n或者直接运行: ./build.sh")
endif()

aux_source_directory(${PROJECT_SOURCE_DIR} SRC_LIST)
add_executable(mqttClient ${SRC_LIST})

target_include_directories(mqttClient PRIVATE /home/lixiaodu/tools/paho.mqtt.c/install/include)
target_link_directories(mqttClient PRIVATE /home/lixiaodu/tools/paho.mqtt.c/install/lib)
target_link_libraries(mqttClient PRIVATE paho-mqtt3c)
