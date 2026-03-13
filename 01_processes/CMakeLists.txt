# 01_processes/CMakeLists.txt
add_executable(process_demo process_demo.c)
target_link_libraries(process_demo PRIVATE Threads::Threads)
