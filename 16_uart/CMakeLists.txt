cmake_minimum_required(VERSION 3.16)

project(uart C)

find_package(Threads REQUIRED)

add_executable(uart_demo uart_demo.c)
