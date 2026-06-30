# 1.指定CMake最低版本
cmake_minimum_required(VERSION 3.16)

# 2.项目名称
project(MemoryPoolProject)

# 3. 设置c++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 4. 头文件
include_directories(${PROJECT_SOURCE_DIR}/../include)

# 5. 源文件 测试文件地址
file(GLOB SRC_FILES "${PROJECT_SOURCE_DIR}/../src/*.cpp")
file(GLOB TEST_FILES "${PROJECT_SOURCE_DIR}/*.cpp")

# 6. 编译生成可执行程序
add_executable(${PROJECT_NAME} ${SRC_FILES} ${TEST_FILES})

# 开启调试信息-g，开启所有警告-Wall，优化关闭（Debug调试用）
target_compile_options(${PROJECT_NAME} PRIVATE -g -Wall)

# 跨平台标准方式链接多线程库，替代老旧pthread写法
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# 预留扩展：后续链接第三方静态库示例
# target_link_libraries(${PROJECT_NAME} 自定义库名 Threads::Threads)

