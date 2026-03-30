cmake_minimum_required(VERSION 3.16)

project(gpio C)

find_package(Threads REQUIRED)

add_executable(gpio_demo gpio_demo.c)

#target_link_libraries(gpio_demo PRIVATE gpiod)
# a proper CMakeLists.txt would use find_package() to locate the library and then link against it, 
#  rather than hardcoding the library name.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED libgpiod)

target_include_directories(gpio_demo PRIVATE ${GPIOD_INCLUDE_DIRS})
target_link_libraries(gpio_demo PRIVATE ${GPIOD_LIBRARIES})