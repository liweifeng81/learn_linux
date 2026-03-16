# 04_signals/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

add_executable(signal_demo signal_demo.c)
