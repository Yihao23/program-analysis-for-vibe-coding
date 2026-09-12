cmake_minimum_required(VERSION 3.16)
project(program_analysis_for_vibe_coding CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # clang-tidy 需要

# 07 讲：误报变便宜 → 开你对人不敢开的 flags。-Werror 让编译器本身成为一道上界。
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Werror)
include_directories(${CMAKE_SOURCE_DIR})   # 所有 #include 从仓库根写起，方便 gate 解析

add_library(core STATIC app/core/billing.cpp app/core/users.cpp app/core/users_write.cpp)  # trusted
# OBJECT 库：静态库会丢掉没被引用的 .o 连同其静态初始化器；OBJECT 保证 features 一定被链进 golden_full
add_library(features OBJECT app/features/promo.cpp)                                        # untrusted

add_executable(specs specs/specs.cpp)
target_link_libraries(specs PRIVATE core)
enable_testing()
add_test(NAME lower COMMAND specs lower)
add_test(NAME upper COMMAND specs upper)

# 09 讲 非干扰：同一份 golden.cpp 构建两个"世界"，gate 比对输出
add_executable(golden_core specs/golden.cpp)
target_link_libraries(golden_core PRIVATE core)
add_executable(golden_full specs/golden.cpp)
target_link_libraries(golden_full PRIVATE core features)

add_executable(gate gate/gate.cpp)
target_compile_definitions(gate PRIVATE GATE_ROOT="${CMAKE_SOURCE_DIR}" GATE_BUILD="${CMAKE_BINARY_DIR}")
