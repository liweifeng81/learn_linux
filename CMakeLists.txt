cmake_minimum_required(VERSION 3.16)
project(EmbeddedLinuxDemo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common compiler flags for embedded Linux development
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -g          # Debug info
    -O0         # No optimization (for clear debugging)
)

# Required libraries
find_package(Threads REQUIRED)

# ---------- Subdirectories ----------
add_subdirectory(01_processes)
add_subdirectory(02_threads)
add_subdirectory(03_ipc)
add_subdirectory(04_signals)
add_subdirectory(05_memory)
add_subdirectory(06_file_io)
add_subdirectory(07_networking)
add_subdirectory(08_timers)
# 09 & 10 are kernel modules — built separately with kbuild
add_subdirectory(13_debugging)

message(STATUS "")
message(STATUS "=== Embedded Linux Interview Demo Project ===")
message(STATUS "Build dir : ${CMAKE_BINARY_DIR}")
message(STATUS "Compiler  : ${CMAKE_C_COMPILER}")
message(STATUS "Modules   : 01-08, 13 (userspace)")
message(STATUS "Kernel    : 09_kernel_module / 10_device_driver (build separately)")
message(STATUS "")
