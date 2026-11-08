cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER /home/lixiaodu/100ask_imx6ull-sdk/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER /home/lixiaodu/100ask_imx6ull-sdk/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-g++)
set(CMAKE_FIND_ROOT_PATH /home/lixiaodu/100ask_imx6ull-sdk/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

project(yolo_ncnn_test CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# ncnn 头文件和库
set(ncnn_DIR /home/lixiaodu/tools/ncnn/build-arm/install/lib/cmake/ncnn)
find_package(ncnn REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    /home/lixiaodu/tools/ncnn/build-arm/install/include
)

add_executable(yolo_ncnn_test yolo_ncnn_test.cpp)
target_link_libraries(yolo_ncnn_test ncnn)
