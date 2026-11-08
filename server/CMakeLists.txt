cmake_minimum_required(VERSION 3.15)

# 指定交叉编译器（必须放在 project() 之前）
set(CMAKE_C_COMPILER arm-buildroot-linux-gnueabihf-gcc)

project(server)

set(home /home/lixiaodu/桌面/smart_home/server)
set(EXECUTABLE_OUTPUT_PATH ${home})

# 添加头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include /home/lixiaodu/tools/paho.mqtt.c/install/include)  

#set(SRC_LIST dht11.c http.c led.c pir.c server.c main.c)   #手动添加源文件
#aux_source_directory(${home} SRC_LIST)   #绝对路径寻找源文件
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)  # 收集cmake命令后面跟随的路径下的所有源文件，存入SRC_LIST变量中
#file(GLOB SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)  #收集CMakeLists.txt所在路径下的所有源文件，存入SRC_LIST变量中
add_executable(main ${SRC_LIST})

# 链接 Paho MQTT 库（交叉编译好的 ARM 版）+ pthread
target_link_libraries(main /home/lixiaodu/tools/paho.mqtt.c/install/lib/libpaho-mqtt3c.so pthread)


#set(LIBRARY_OUTPUT_PATH ${home})  #设置库文件输出路径
#add_library(server SHARED ${SRC_LIST})  #生成动态库
#add_library(server STATIC ${SRC_LIST})  #生成静态库
