project(networking C)
# 07_networking/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

add_executable(tcp_server tcp_server.c)
add_executable(tcp_client tcp_client.c)
add_executable(udp_demo   udp_demo.c)
