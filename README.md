# Embedded Linux Interview Demo Project

A hands-on reference project covering the **most-tested topics** in embedded Linux engineering interviews. Each numbered module is self-contained with focused C demos, inline Q&A comments, and a CMakeLists.txt (or kbuild Makefile for kernel modules).

---

## Quick Start

> **Requires:** Linux host or VM with `gcc`, `cmake`, `build-essential`, `librt` (usually included).

```bash
git clone <this-repo>
cd embedded_linux_demo

# Configure + build all userspace modules
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)
```

---

## Module Map

| # | Directory | Topic | Key APIs / Concepts |
|---|-----------|-------|---------------------|
| 01 | `01_processes/` | Processes | `fork`, `exec`, `wait`, `waitpid`, zombie, orphan |
| 02 | `02_threads/` | Threads | `pthread_create`, mutex, condition variable, rwlock, TLS |
| 03 | `03_ipc/` | IPC (POSIX) | `pipe`, FIFO, `shm_open`, `mmap`, POSIX semaphore, MQ |
| 04 | `04_signals/` | Signals | `sigaction`, `sigprocmask`, real-time signals, self-pipe, `signalfd` |
| 05 | `05_memory/` | Memory | `mmap`, `mprotect`, `posix_memalign`, VMA layout, leak tracking |
| 06 | `06_file_io/` | File I/O | `epoll`, `select`, `sendfile`, `readv`/`writev`, `ioctl` |
| 07 | `07_networking/` | Networking | TCP server/client via `epoll`, UDP, `getaddrinfo`, `TCP_NODELAY` |
| 08 | `08_timers/` | Timers | `setitimer`, `timer_create`, `timerfd_create`, `clock_gettime` |
| 09 | `09_kernel_module/` | Kernel Module | `module_init`, proc entry, `printk`, `module_param`, `kmalloc` |
| 10 | `10_device_driver/` | Char Driver | `cdev`, `file_operations`, `copy_to/from_user`, `ioctl`, udev |
| 11 | `11_bootloader/` | Bootloader | U-Boot commands, boot sequence, device tree, `mkimage` |
| 12 | `12_build_systems/` | Build Systems | Buildroot & Yocto quick-start, custom packages, recipes |
| 13 | `13_debugging/` | Debugging | gdb, valgrind, strace, addr2line, /proc, perf, ASAN |
| 14 | `14_shell_scripts/` | Shell Scripts | sys_info, watchdog, GPIO toggle |

---

## Module Details

### 01 — Processes (`01_processes/process_demo.c`)
```bash
./build/01_processes/process_demo
```
Demonstrates: `fork()`, `execl()`, `waitpid()` with `WNOHANG`, zombie process (visible in `ps aux | grep Z`), and orphan process (re-parented to PID 1).

---

### 02 — Threads (`02_threads/thread_demo.c`)
```bash
./build/02_threads/thread_demo
```
Demonstrates: thread creation/join, mutex-protected counter (100k iterations per thread), producer/consumer with condition variables and an 8-slot queue, read-write lock with 3 readers + 1 writer, thread-local storage (`__thread`).

---

### 03 — IPC — POSIX Only (`03_ipc/`)
```bash
./build/03_ipc/pipe_demo     # anonymous pipe + FIFO
./build/03_ipc/shm_demo      # shared memory + named semaphore
./build/03_ipc/mq_demo       # POSIX message queue with priorities
```
> SysV IPC (shmget, msgget, semget) is intentionally **omitted** — POSIX APIs are preferred.

---

### 04 — Signals (`04_signals/signal_demo.c`)
```bash
./build/04_signals/signal_demo
```
Demonstrates: `sigaction()` with `SA_SIGINFO | SA_RESTART`, `sigprocmask()` blocking SIGUSR1 and checking pending, real-time signal `SIGRTMIN+1` with `sigqueue()`, self-pipe trick, and `signalfd()`.

---

### 05 — Memory (`05_memory/memory_demo.c`)
```bash
./build/05_memory/memory_demo
```
Demonstrates: address space layout (text/data/BSS/heap/stack via `/proc/self/maps`), anonymous `mmap`, file-backed `mmap` with `msync`, `posix_memalign` / `aligned_alloc`, custom malloc wrapper with leak detection.

---

### 06 — File I/O (`06_file_io/fileio_demo.c`)
```bash
./build/06_file_io/fileio_demo
```
Demonstrates: `open/read/write/lseek`, `ioctl(TIOCGWINSZ)`, `select()` with timeout, `epoll` edge-triggered with full drain, `sendfile()` zero-copy, `readv()`/`writev()` scatter-gather.

---

### 07 — Networking (`07_networking/`)
```bash
# Terminal 1 — start server
./build/07_networking/tcp_server 8080

# Terminal 2 — run client
./build/07_networking/tcp_client 127.0.0.1 8080

# UDP demo (forks internally)
./build/07_networking/udp_demo
```
`tcp_server`: multi-client epoll server, `SO_REUSEADDR`, `SO_KEEPALIVE`, `SIGPIPE` ignored.  
`tcp_client`: `getaddrinfo`, timed connect, `TCP_NODELAY`.

---

### 08 — Timers (`08_timers/timer_demo.c`)
```bash
./build/08_timers/timer_demo
```
Demonstrates: `nanosleep` with `CLOCK_MONOTONIC` measurement, `setitimer/SIGALRM` for 5 ticks, POSIX `timer_create` with `SIGRTMIN`, `timerfd_create` + `epoll`.

---

### 09 — Kernel Module (Linux only, requires kernel headers)
```bash
cd 09_kernel_module
make KDIR=/lib/modules/$(uname -r)/build
sudo insmod hello_module.ko name="EmbeddedLinux" count=5
cat /proc/hello_module
dmesg | tail -10
sudo rmmod hello_module
```

---

### 10 — Character Device Driver (Linux only)
```bash
cd 10_device_driver
make KDIR=/lib/modules/$(uname -r)/build
sudo insmod chardev.ko
echo "hello driver" > /dev/chardev0
cat /dev/chardev0
dmesg | tail -10
sudo rmmod chardev
```

IOCTL commands (from a C test program):
- `CHARDEV_RESET` — clear buffer
- `CHARDEV_GSIZE` — get bytes in buffer
- `CHARDEV_SECHO` — enable/disable kernel log echo

---

### 11 — Bootloader (`11_bootloader/uboot_notes.md`)
Reference document covering:
- ROM → SPL → U-Boot → Kernel boot sequence
- U-Boot command cheatsheet
- Environment variables (`bootcmd`, `bootargs`)
- `mkimage` usage
- Device tree basics

---

### 12 — Build Systems (`12_build_systems/build_systems_notes.md`)
Reference document covering:
- **Buildroot**: menuconfig, custom packages (Config.in + .mk), rootfs overlay
- **Yocto**: layers, BitBake, recipe writing, SDK generation
- Side-by-side comparison table

---

### 13 — Debugging (`13_debugging/`)
```bash
# Build with intentional bugs
./build/13_debugging/debug_demo <1..7>

# Memory leak detection
valgrind --leak-check=full ./build/13_debugging/debug_demo 3

# GDB
gdb ./build/13_debugging/debug_demo
(gdb) run 1

# See full tool reference
bash 13_debugging/debug_commands.sh
```

---

### 14 — Shell Scripts (`14_shell_scripts/`)
```bash
bash 14_shell_scripts/sys_info.sh
bash 14_shell_scripts/watchdog.sh my-daemon "/usr/bin/my-daemon"
bash 14_shell_scripts/gpio_toggle.sh 17
```

---

## Common Interview Topics Quick Reference

### Process vs Thread
| | Process | Thread |
|--|---------|--------|
| Address space | Separate | Shared |
| Overhead | High (fork/exec) | Low |
| Communication | IPC mechanisms | Shared globals + sync |
| Crash isolation | Yes | No (one crash kills all) |

### IPC Mechanism Comparison
| Mechanism | Boundary | Direction | Persistence |
|-----------|----------|-----------|-------------|
| Pipe | Byte stream | Unidirectional | None (in-kernel) |
| FIFO | Byte stream | Bidirectional | Filesystem |
| Shared Memory | Raw bytes | Any | `/dev/shm` |
| POSIX MQ | Messages | Any | `/dev/mqueue` |

### Epoll vs Select
| | select | poll | epoll |
|--|--------|------|-------|
| FD limit | 1024 | Unlimited | Unlimited |
| Complexity | O(n) | O(n) | O(1) per event |
| Edge-triggered | No | No | Yes (EPOLLET) |
| Kernel space list | No | No | Yes |

### Signal Safety Rule
Only **async-signal-safe** functions may be called from signal handlers.  
✅ `write()`, `read()`, `send()`, `_exit()`, `kill()`  
❌ `printf()`, `malloc()`, `free()`, `pthread_mutex_lock()`

---

## Build with AddressSanitizer

```bash
cmake -DCMAKE_C_FLAGS="-fsanitize=address,leak,undefined -g" -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)
./build/13_debugging/debug_demo 2   # heap overflow caught immediately
```

---

## Cross-Compilation Example

```bash
# Set toolchain in cmake
cmake -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
      -DCMAKE_SYSTEM_NAME=Linux \
      -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
      -DCMAKE_BUILD_TYPE=Debug \
      ..
cmake --build . -j$(nproc)
# Copy binaries to target via scp/tftp
```
