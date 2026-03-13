#!/bin/bash
# 13_debugging/debug_commands.sh
# ===============================
# Reference script for common embedded Linux debugging tools.
# This is a REFERENCE — not all commands run meaningfully on the host.
# Run on a Linux target or VM with debug_demo binary built.

set -e
BINARY="./debug_demo"

echo "=============================================="
echo " Embedded Linux Debugging Cheatsheet"
echo "=============================================="

# ── Build with debug symbols ───────────────────────────────
echo ""
echo "── Build Options ──────────────────────────────"
echo "Debug build (default CMake -DCMAKE_BUILD_TYPE=Debug):"
echo "  cmake -DCMAKE_BUILD_TYPE=Debug .."
echo "  cmake --build ."

echo ""
echo "AddressSanitizer build:"
echo "  cmake -DCMAKE_C_FLAGS=\"-fsanitize=address,leak,undefined -g\" .."

# ── GDB ───────────────────────────────────────────────────
echo ""
echo "── GDB ────────────────────────────────────────"
cat << 'EOF'
# Local debugging
gdb ./debug_demo
  (gdb) run 1            # bug 1: NULL deref → SIGSEGV
  (gdb) bt               # backtrace
  (gdb) bt full          # with local vars
  (gdb) info locals      # print locals in current frame
  (gdb) p variable_name  # print variable
  (gdb) x/16xb buf       # examine 16 bytes at 'buf'
  (gdb) watch g_counter  # watchpoint: break on write
  (gdb) break main       # breakpoint at main
  (gdb) next / step / continue / finish

# Remote GDB (cross-debugging embedded target)
# On target:
gdbserver :1234 ./debug_demo 1
# On host:
arm-linux-gnueabihf-gdb ./debug_demo
  (gdb) target remote 192.168.1.100:1234
  (gdb) continue

# Coredump analysis
ulimit -c unlimited
echo "/tmp/core.%e.%p" > /proc/sys/kernel/core_pattern
./debug_demo 1           # generates /tmp/core.debug_demo.<pid>
gdb ./debug_demo /tmp/core.debug_demo.*
  (gdb) bt
EOF

# ── Valgrind ───────────────────────────────────────────────
echo ""
echo "── Valgrind ───────────────────────────────────"
cat << 'EOF'
# Memory leak check
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes --verbose \
         ./debug_demo 3

# Heap buffer overflow / UAF
valgrind --tool=memcheck --undef-value-errors=yes ./debug_demo 2

# Helgrind: data race detector (threads)
valgrind --tool=helgrind ./thread_demo

# Massif: heap profiler
valgrind --tool=massif ./debug_demo 3
ms_print massif.out.<pid>
EOF

# ── strace / ltrace ───────────────────────────────────────
echo ""
echo "── strace / ltrace ────────────────────────────"
cat << 'EOF'
# Trace all syscalls
strace ./debug_demo 3

# Trace specific syscalls only
strace -e trace=open,read,write,mmap ./debug_demo 3

# Attach to running process
strace -p <pid>

# Count syscalls (summary)
strace -c ./debug_demo 3

# ltrace: library calls
ltrace ./debug_demo 3
EOF

# ── addr2line / nm / objdump ───────────────────────────────
echo ""
echo "── Binary Analysis Tools ──────────────────────"
cat << 'EOF'
# Convert crash address to source line
addr2line -e ./debug_demo -f 0x<crash_address>

# List symbols
nm -n ./debug_demo | grep ' T '    # text (code) symbols, sorted by addr
nm --demangle ./debug_demo          # C++ demangled names

# Disassemble
objdump -d -M intel ./debug_demo | less
objdump -d -S ./debug_demo | less   # interleave source (needs -g)

# ELF headers / sections
readelf -h ./debug_demo
readelf -S ./debug_demo     # section headers
readelf -l ./debug_demo     # program headers (segments)
EOF

# ── /proc filesystem inspection ───────────────────────────
echo ""
echo "── /proc Process Inspection ───────────────────"
cat << 'EOF'
PID=<target_pid>

# Virtual memory map
cat /proc/$PID/maps
cat /proc/$PID/smaps        # detailed memory stats per VMA

# Open file descriptors
ls -la /proc/$PID/fd

# Process status and statistics
cat /proc/$PID/status
cat /proc/$PID/stat
cat /proc/$PID/statm        # memory pages

# Stack trace (requires CONFIG_STACKTRACE)
cat /proc/$PID/wchan        # what kernel function it's waiting in

# CPU / memory system-wide
cat /proc/cpuinfo
cat /proc/meminfo
cat /proc/interrupts
cat /proc/iomem
EOF

# ── perf ──────────────────────────────────────────────────
echo ""
echo "── perf (Performance Profiling) ───────────────"
cat << 'EOF'
# CPU profile for 10 seconds
perf record -g ./debug_demo 3
perf report

# System-wide performance counters
perf stat ./debug_demo 3

# Cache miss analysis
perf stat -e cache-misses,cache-references ./debug_demo 3

# Live top-like CPU usage
perf top
EOF

echo ""
echo "Done printing debugging reference."
