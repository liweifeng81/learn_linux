# Linux Interview Cheat Sheet
*Based on the Embedded Linux Demo Project*

---

## Processes vs Threads

| Aspect | Process | Thread |
|--------|---------|--------|
| Address Space | Separate (fork creates copy) | Shared |
| Overhead | High (fork/exec, separate MMU context) | Low (same address space) |
| Communication | IPC required (pipes, shared mem) | Direct (shared globals + sync) |
| Crash Isolation | Yes (one crash doesn't affect others) | No (SIGSEGV kills whole process) |
| Creation | `fork()` + `exec()` | `pthread_create()` |
| Synchronization | Semaphores, message queues | Mutex, cond vars, RW locks |

**Key APIs:**
- Process: `fork()`, `execve()`, `waitpid()`, `exit()`
- Thread: `pthread_create()`, `pthread_join()`, `pthread_detach()`

---

## IPC Mechanisms

| Type | Boundary | Direction | Persistence | Use Case |
|------|----------|-----------|-------------|----------|
| **Pipe** (`pipe()`) | Related processes only | Unidirectional | Kernel buffer only | Parent ↔ child data flow |
| **FIFO** (`mkfifo()`) | Any processes | Bidirectional | Filesystem | Named pipe for unrelated procs |
| **Shared Memory** (`shm_open()`) | Any processes | Any | `/dev/shm` | High-speed data sharing |
| **Message Queue** (`mq_open()`) | Any processes | Any | `/dev/mqueue` | Structured message passing |
| **Unix Domain Socket** | Same host | Bidirectional | None | Local IPC with socket API |

**SysV vs POSIX IPC:**
- **SysV**: Older, uses keys (`ftok`), global namespace
- **POSIX**: Newer, uses names, better integration with filesystems

---

## Signals

**Signal Handling:**
- `sigaction()`: Modern, with `SA_SIGINFO` for extra data
- `signal()`: Legacy, not thread-safe
- `sigprocmask()`: Block/unblock signals
- `sigpending()`: Check blocked signals

**Real-time Signals:** `SIGRTMIN` to `SIGRTMAX` (32-64), queued, with values.

**Self-pipe Trick:** Use pipe to wake `select()`/`epoll()` from signal handler.

**Signal Safety:** Only call async-signal-safe functions in handlers:
- ✅ `read()`, `write()`, `_exit()`, `kill()`
- ❌ `printf()`, `malloc()`, `pthread_mutex_lock()`

**Common Signals:**
- `SIGINT` (Ctrl+C), `SIGTERM`, `SIGKILL` (uncatchable), `SIGSEGV`, `SIGPIPE`

---

## Memory Management

**Virtual Memory Areas (VMA):**
- Text: Executable code (read-only)
- Data: Initialized globals
- BSS: Uninitialized globals (zeroed)
- Heap: `malloc()`/`free()` (grows via `brk()`/`sbrk()`)
- Stack: Local variables, grows down
- mmap: File mappings, shared memory

**Key APIs:**
- `mmap()`: Map files or anonymous memory
- `mprotect()`: Change page permissions
- `posix_memalign()`: Aligned allocations
- `mlock()`: Lock pages in RAM (prevent swap)

**Memory Issues:**
- Leak: `valgrind --leak-check=full`
- Corruption: AddressSanitizer (`-fsanitize=address`)
- Use-after-free: Valgrind or ASAN

---

## Networking

**TCP vs UDP:**
| | TCP | UDP |
|--|----|-----|
| Reliable | Yes | No |
| Ordered | Yes | No |
| Connection | Yes (3-way handshake) | No |
| Flow Control | Yes | No |
| Use Case | HTTP, SSH | DNS, VoIP |

**Socket Options:**
- `SO_REUSEADDR`: Bind to port in TIME_WAIT
- `SO_KEEPALIVE`: Detect dead connections
- `TCP_NODELAY`: Disable Nagle's algorithm
- `SO_RCVBUF`/`SO_SNDBUF`: Buffer sizes

**Key APIs:**
- `socket()`, `bind()`, `listen()`, `accept()`, `connect()`
- `send()`, `recv()`, `sendto()`, `recvfrom()`
- `getaddrinfo()`: Resolve host/port
- `epoll()`: Scalable I/O multiplexing

**TCP States:** LISTEN → SYN → SYN-ACK → ESTABLISHED → FIN → TIME_WAIT

---

## File I/O

**Multiplexing Comparison:**
| | select | poll | epoll |
|--|--------|------|-------|
| FD Limit | 1024 | Unlimited | Unlimited |
| Performance | O(n) | O(n) | O(1) per event |
| Edge-triggered | No | No | Yes (`EPOLLET`) |
| Kernel Copy | No | No | Yes (ready list) |

**Key APIs:**
- `open()`, `read()`, `write()`, `close()`
- `ioctl()`: Device-specific commands
- `sendfile()`: Zero-copy file transfer
- `readv()`/`writev()`: Scatter-gather I/O
- `lseek()`: File positioning

**File Descriptors:** 0=stdin, 1=stdout, 2=stderr

---

## Timers

**Timer Types:**
- `alarm()`: Simple SIGALRM after seconds
- `setitimer()`: Interval timers (real, virtual, prof)
- `timer_create()`: POSIX timers with signals
- `timerfd_create()`: Timer as file descriptor (epoll-able)

**Clocks:**
- `CLOCK_REALTIME`: Wall time, settable
- `CLOCK_MONOTONIC`: Elapsed time, not settable
- `CLOCK_PROCESS_CPUTIME_ID`: Process CPU time
- `CLOCK_THREAD_CPUTIME_ID`: Thread CPU time

**High Resolution:** `clock_gettime()`, nanosecond precision

---

## Kernel Space vs User Space

| | User Space | Kernel Space |
|--|------------|--------------|
| Privilege | Ring 3 | Ring 0 |
| Access | Syscalls only | Direct HW |
| Memory | Per-process | Shared |
| Crash Impact | Process dies | Kernel panic/oops |
| Libraries | libc | None |
| Allocation | `malloc()` | `kmalloc()`/`vmalloc()` |

**Syscalls:** `read()`, `write()`, `open()`, `fork()`, etc. (via `int 0x80` or `syscall`)

**Kernel Modules:**
- `module_init()`/`module_exit()`
- `printk()`: Kernel logging (dmesg)
- Parameters: `module_param()`

---

## Device Drivers

**Character vs Block:**
- **Char**: Byte stream, sequential (tty, sensors)
- **Block**: Block-based, random access (disks)

**Key Structures:**
- `file_operations`: open, read, write, ioctl
- `cdev`: Character device
- `class_create()`/`device_create()`: Auto /dev nodes via udev

**Memory Transfer:**
- `copy_from_user()`/`copy_to_user()`: Safe user↔kernel copy
- Never use `memcpy()` directly with user pointers

**IOCTL:** Custom commands via `ioctl()` system call

---

## Boot Sequence (ARM)

```
Power-on → ROM Code → SPL/MLO → U-Boot → Kernel → init
```

**U-Boot Commands:**
- `printenv`, `setenv bootargs "console=ttyS0 root=/dev/mmcblk0p2"`
- `bootm`/`bootz`/`booti`: Boot kernel with DTB

**Device Tree:** Hardware description passed to kernel

---

## Build Systems

**Buildroot vs Yocto:**
| | Buildroot | Yocto |
|--|-----------|-------|
| Complexity | Simple | Complex |
| Config | Kconfig | BitBake |
| Output | Single image | Flexible layers |
| Use Case | Small devices | Enterprise |

**Key Files:**
- Buildroot: `Config.in`, `*.mk`
- Yocto: `*.bb` recipes, layers

---

## Debugging Tools

**GDB:**
- `gdb binary`, `run`, `bt` (backtrace), `print var`, `break func`

**Valgrind:**
- `--leak-check=full`: Memory leaks
- `--track-origins=yes`: Uninitialized vars

**AddressSanitizer:**
- Compile with `-fsanitize=address`
- Catches overflows, use-after-free instantly

**Other:**
- `strace`: Trace syscalls
- `perf`: Performance profiling
- `addr2line`: Address to source line
- `/proc/<pid>/maps`: Memory layout

---

## Common Commands

**Process Management:**
- `ps aux`, `top`, `htop`
- `kill -9 <pid>`, `pkill <name>`

**System Info:**
- `uname -a`, `cat /proc/cpuinfo`
- `free -h`, `df -h`
- `ip addr`, `ifconfig`

**Kernel:**
- `dmesg`, `lsmod`, `modprobe`
- `sysctl -a`, `/proc/sys/`

**Build:**
- `make`, `cmake`, `gcc -o bin src.c`
- Cross: `aarch64-linux-gnu-gcc`

---

## Interview Tips

- **Explain Trade-offs:** Always discuss pros/cons (e.g., threads vs processes)
- **Know Limits:** FD limits, pipe buffer size (64KB), etc.
- **Real-world Experience:** Mention embedded constraints (memory, RT)
- **Ask Questions:** "What kernel version?", "Real-time requirements?"
- **Practice Code:** Write simple demos for fork, threads, sockets

---

*Generated from embedded_linux_demo project. Study the code for deeper understanding.*</content>
<filePath>interview_cheat_sheet.md