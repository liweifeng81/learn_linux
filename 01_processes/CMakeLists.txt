# 01_processes/CMakeLists.txt

find_package(Threads REQUIRED)

add_executable(process_demo process_demo.c)
target_link_libraries(process_demo PRIVATE Threads::Threads)
