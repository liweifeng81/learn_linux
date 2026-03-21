project(device_driver_test C)

cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

add_executable(device_driver_test device_driver_test.c)
