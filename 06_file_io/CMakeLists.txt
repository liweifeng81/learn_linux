project(file_io C)
# 06_file_io/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

add_executable(fileio_demo fileio_demo.c)
