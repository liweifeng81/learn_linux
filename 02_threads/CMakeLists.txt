# 02_threads/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(thread)

find_package(Threads REQUIRED)

add_executable(thread_demo thread_demo.c)
target_link_libraries(thread_demo PRIVATE Threads::Threads)
