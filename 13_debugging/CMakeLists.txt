# 13_debugging/CMakeLists.txt
add_executable(debug_demo debug_demo.c)

# Debug-friendly compile flags
target_compile_options(debug_demo PRIVATE
    -g3           # full debug info
    -O0           # no optimization
    -fno-omit-frame-pointer  # keep frame pointer for stack traces
    # Uncomment to build with AddressSanitizer:
    # -fsanitize=address,leak,undefined
)
# target_link_options(debug_demo PRIVATE -fsanitize=address,leak,undefined)
