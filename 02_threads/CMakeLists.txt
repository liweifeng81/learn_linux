# 02_threads/CMakeLists.txt
add_executable(thread_demo thread_demo.c)
target_link_libraries(thread_demo PRIVATE Threads::Threads)
# Enable pthreads compile and link flags
target_compile_options(thread_demo PRIVATE -pthread)
target_link_options(thread_demo PRIVATE -pthread)
