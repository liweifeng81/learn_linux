# 08_timers/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

add_executable(timer_demo timer_demo.c)
target_link_libraries(timer_demo PRIVATE rt)  # -lrt for timer_create / clock_gettime
