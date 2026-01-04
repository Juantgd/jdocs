# Copyright (c) 2025 Juantgd. All Rights Reserved.

cmake_minimum_required(VERSION 3.15)

project(jdocs VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 设置静态库、动态库、可执行文件输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 设置编译器严格模式
add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)

# 获取依赖包
include(FetchContent)
# 使用spdlog用于日志记录
message(STATUS "Fetching spdlog...")
FetchContent_Declare(
  spdlog
  # GIT_REPOSITORY https://github.com/gabime/spdlog.git
  # GIT_TAG 6fa36017cfd5731d617e1a934f0e5ea9c4445b13 # release-1.15.3
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog
)
FetchContent_MakeAvailable(spdlog)

# 使用googletest用于单元测试
message(STATUS "Fetching googletest...")
FetchContent_Declare(
  googletest
  # GIT_REPOSITORY https://github.com/google/googletest.git
  # GIT_TAG 6910c9d9165801d8827d628cb72eb7ea9dd538c5 #release-1.16.0
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 使用第三方json库用于解析json格式数据
message(STATUS "Fetching json...")
FetchContent_Declare(
  nlohmann_json
  # GIT_REPOSITORY https://github.com/nlohmann/json.git
  # GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03 # release-3.11.3
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(PkgConfig REQUIRED)

pkg_check_modules(MYSQLCLIENT REQUIRED mysqlclient)

add_library(corelib
  STATIC
    src/core/event_loop.cc
    src/core/timer.cc
    src/core/buffer.cc
    src/core/server.cc
    src/core/worker.cc
    src/net/tcp_connection.cc
    src/utils/bitmap.cc
    src/utils/helpers.cc
    src/database/mysql_connection.cc
    src/protocol/http/http_parser.cc
    src/protocol/websocket/websocket_parser.cc
)

# 添加mysqlclient库的搜索路径
target_link_directories(corelib
  PUBLIC
    /usr/lib64/mysql
)

# corelib链接第三方库
target_link_libraries(corelib
  PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    pthread         # 提供多线程支持
    uring           # 提供io异步接口
    mysqlclient     # 提供mysql数据库开发接口
    ssl
    crypto
)

# 添加include目录
target_include_directories(corelib
  PUBLIC
    src
    include/jdocs
)

# 添加可执行文件
add_executable(${PROJECT_NAME} src/main.cc)

# 链接核心库
target_link_libraries(${PROJECT_NAME} PRIVATE corelib)

# 启用测试
enable_testing()

# 添加测试文件
add_executable(bitmap_test tests/bitmap_test.cc)
target_link_libraries(bitmap_test
  PRIVATE
    corelib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(bitmap_test)
