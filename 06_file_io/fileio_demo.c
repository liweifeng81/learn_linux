/*
 * 06_file_io/fileio_demo.c
 * ========================
 * Demonstrates Linux file I/O and I/O multiplexing:
 *   - open / read / write / lseek / close
 *   - O_NONBLOCK and handling EAGAIN
 *   - ioctl() — TIOCGWINSZ (terminal size)
 *   - select() with timeout
 *   - epoll() — edge-triggered (EPOLLET) and level-triggered
 *   - sendfile() — zero-copy file transfer
 *   - readv() / writev() — scatter-gather I/O
 *
 * Interview topics:
 *   Q: Difference between select(), poll(), and epoll()?
 *   A: select: FD_SET has limit (FD_SETSIZE=1024), rescans all fds each call.
 *      poll: no FD limit, still O(n) scan each call.
 *      epoll: O(1) per event, kernel maintains interest list, scales to millions of FDs.
 *
 *   Q: Edge-triggered vs Level-triggered epoll?
 *   A: Level-triggered (default): notified as long as data is available.
 *      Edge-triggered (EPOLLET): notified only when state changes (new data arrives).
 *      ET requires reading until EAGAIN to avoid missing events.
 *
 *   Q: What is sendfile()?
 *   A: Copies data between file descriptors in kernel space — avoids user-space
 *      buffer copy. Used in HTTP servers, file transfer. ~40% faster than read+write.
 *
 *   Q: What is scatter-gather I/O?
 *   A: readv()/writev() operate on multiple non-contiguous buffers in one syscall,
 *      reducing syscall overhead and simplifying protocol framing.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <sys/uio.h>
#include <termios.h>
#include <errno.h>
#include <time.h>

static void separator(const char *t)
{
    printf("\n══════════════════════════════════════════\n  %s\n══════════════════════════════════════════\n", t);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: Basic file I/O                                 *
 * ─────────────────────────────────────────────────────── */
static void demo_basic_fileio(void)
{
    separator("Demo 1: Basic File I/O (open/write/lseek/read)");

    const char *path = "/tmp/fileio_demo.txt";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return; }

    const char *lines[] = {
        "Line 1: Embedded Linux\n",
        "Line 2: Kernel Drivers\n",
        "Line 3: System Calls\n",
    };
    for (int i = 0; i < 3; i++)
        write(fd, lines[i], strlen(lines[i]));

    /* Seek back to start and read */
    lseek(fd, 0, SEEK_SET);
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    printf("Read back %zd bytes:\n%s", n, buf);

    /* File size via lseek(0, SEEK_END) */
    off_t size = lseek(fd, 0, SEEK_END);
    printf("File size: %lld bytes\n", (long long)size);

    close(fd);
    unlink(path);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 2: ioctl — TIOCGWINSZ (terminal dimensions)      *
 * ─────────────────────────────────────────────────────── */
static void demo_ioctl(void)
{
    separator("Demo 2: ioctl() — Terminal Window Size");

    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        printf("Terminal: %u cols x %u rows   (pixel: %u x %u)\n",
               ws.ws_col, ws.ws_row, ws.ws_xpixel, ws.ws_ypixel);
    } else {
        printf("ioctl TIOCGWINSZ not available (not a terminal): %s\n",
               strerror(errno));
    }
}

/* ─────────────────────────────────────────────────────── *
 * Demo 3: select() with timeout                         *
 * ─────────────────────────────────────────────────────── */
static void demo_select(void)
{
    separator("Demo 3: select() with Timeout");

    int pipefd[2];
    if (pipe(pipefd) < 0) { perror("pipe"); return; }

    /* Set 500 ms timeout */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pipefd[0], &rfds);

    printf("select() waiting 500ms for data on pipe read-end…\n");
    int ret = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
    if (ret == 0) {
        printf("select() timed out — no data (expected)\n");
    } else if (ret > 0) {
        printf("select() says data is ready\n");
    } else {
        perror("select");
    }

    /* Now write to pipe and select again (should immediately return) */
    write(pipefd[1], "X", 1);
    FD_ZERO(&rfds);
    FD_SET(pipefd[0], &rfds);
    //tv.tv_sec = 1; tv.tv_usec = 0;
    ret = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
    if (ret > 0 && FD_ISSET(pipefd[0], &rfds))
        printf("select() immediately detected data — fd is readable ✓\n");

    close(pipefd[0]); close(pipefd[1]);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 4: epoll — level-triggered + edge-triggered      *
 * ─────────────────────────────────────────────────────── */
static void demo_epoll(void)
{
    separator("Demo 4: epoll() — Level-Triggered & Edge-Triggered");

    int pipefd[2];
    if (pipe(pipefd) < 0) { perror("pipe"); return; }
    /* Make read-end non-blocking (required for EPOLLET) */
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);

    int pipefd2[2];
    if (pipe(pipefd2) < 0) { perror("pipe"); return; }
    /* Make read-end non-blocking (required for EPOLLET) */
    fcntl(pipefd2[0], F_SETFL, O_NONBLOCK);

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) { perror("epoll_create1"); return; }

    struct epoll_event ev;
    ev.events   = EPOLLIN | EPOLLET; /* Edge-Triggered */
    ev.data.fd  = pipefd[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev);
    ev.data.fd  = pipefd2[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd2[0], &ev);

    write(pipefd[1], "Hello", 5);
    write(pipefd2[1], "Hello2", 6);

    struct epoll_event events[4];
    int nfds = epoll_wait(epfd, events, 4, 500 /*ms*/);
    printf("1st epoll_wait returned %d event(s), write again without read\n", nfds);

    // write again without read
    write(pipefd[1], "World", 5);
    nfds = epoll_wait(epfd, events, 4, 500 /*ms*/);
    printf("2nd epoll_wait returned %d event(s)\n", nfds);

    /* With EPOLLET:(Edge Trigger) only ONE event trigger, the pipefd2 is not 
       W/O EPOLLET:(Level Trigger) still 2 event trigger. events[0] still get 10 bytes.
    */

    char buf[32];
    int total = 0;
    for (int i = 0; i < nfds; i++) {
        ssize_t n;
        while ((n = read(events[i].data.fd, buf+total, sizeof(buf))) > 0) {
            total += (int)n;
            printf("  fd=%d drained %ld bytes\n",
                events[i].data.fd, n);
        }
            
    }
    buf[total] = 0;
    printf(" total get %d bytes: %s )\n",total, buf);

    close(epfd);
    close(pipefd[0]); close(pipefd[1]);
    close(pipefd2[0]); close(pipefd2[1]);    
}

/* ─────────────────────────────────────────────────────── *
 * Demo 5: sendfile() — zero-copy transfer               *
 * ─────────────────────────────────────────────────────── */
static void demo_sendfile(void)
{
    separator("Demo 5: sendfile() — Zero-Copy Transfer");

    /* Create source file */
    const char *src_path  = "/tmp/sendfile_src.bin";
    const char *dst_path  = "/tmp/sendfile_dst.bin";

    int src = open(src_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    for (int i = 0; i < 100; i++) {
        char line[64];
        snprintf(line, sizeof(line), "Data line %d\n", i);
        write(src, line, strlen(line));
    }
    close(src);

    src = open(src_path, O_RDONLY);
    int dst = open(dst_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);

    struct stat st;
    fstat(src, &st);
    off_t offset = 0;
    ssize_t sent = sendfile(dst, src, &offset, st.st_size);
    printf("sendfile: copied %zd bytes without user-space buffer ✓\n", sent);

    close(src); close(dst);
    unlink(src_path); unlink(dst_path);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 6: readv / writev (scatter-gather)               *
 * ─────────────────────────────────────────────────────── */
static void demo_scatter_gather(void)
{
    separator("Demo 6: writev() / readv() — Scatter/Gather I/O");

    const char *path = "/tmp/iov_demo.bin";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);

    /* Write three non-contiguous buffers in one syscall */
    char h1[] = "[HEADER] ";
    char h2[] = "payload data ";
    char h3[] = "[TRAILER]\n";
    struct iovec wiov[3] = {
        { .iov_base = h1, .iov_len = strlen(h1) },
        { .iov_base = h2, .iov_len = strlen(h2) },
        { .iov_base = h3, .iov_len = strlen(h3) },
    };
    ssize_t written = writev(fd, wiov, 3);
    printf("writev: %zd bytes written\n", written);

    /* Read back with readv */
    lseek(fd, 0, SEEK_SET);
    char r1[32], r2[32], r3[32];
    struct iovec riov[3] = {
        { .iov_base = r1, .iov_len = strlen(h1) },
        { .iov_base = r2, .iov_len = strlen(h2) },
        { .iov_base = r3, .iov_len = strlen(h3) },
    };
    readv(fd, riov, 3);
    r1[strlen(h1)] = r2[strlen(h2)] = r3[strlen(h3)] = '\0';
    printf("readv:  [%s] [%s] [%s]\n", r1, r2, r3);

    close(fd); unlink(path);
}

/* ─────────────────────────────────────────────────────── *
 * main                                                   *
 * ─────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== Embedded Linux Demo: File I/O & Multiplexing ===\n");
    demo_basic_fileio();
    demo_ioctl();
    demo_select();
    demo_epoll();
    demo_sendfile();
    demo_scatter_gather();
    printf("\n[DONE] File I/O demo complete.\n");
    return 0;
}
