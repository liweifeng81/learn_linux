project(ipc C)
# 03_ipc/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Threads REQUIRED)

add_executable(pipe_demo   pipe_demo.c)
add_executable(shm_demo    shm_demo.c)
add_executable(mq_demo     mq_demo.c)

# Shared memory and semaphores need -lrt on older glibc
target_link_libraries(shm_demo  PRIVATE rt)
target_link_libraries(mq_demo   PRIVATE rt)
target_link_libraries(pipe_demo   PRIVATE rt)