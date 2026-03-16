project(memory C)
# 05_memory/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

add_executable(memory_demo memory_demo.c)
