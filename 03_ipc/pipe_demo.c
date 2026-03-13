/*
 * 03_ipc/pipe_demo.c
 * ==================
 * Demonstrates inter-process communication via pipes:
 *   - Anonymous pipes: pipe() — parent ↔ child one-way
 *   - Named pipes (FIFO): mkfifo() — unrelated processes
 *   - Bidirectional communication pattern
 *
 * Interview topics:
 *   Q: What is a pipe?
 *   A: A unidirectional, kernel-buffered byte stream between processes
 *      connected by a common ancestor.  Capacity ≈ 64 KB (PIPE_BUF atomic).
 *
 *   Q: What happens when you write to a pipe with no readers?
 *   A: SIGPIPE is sent to the writer (or write() returns EPIPE if ignored).
 *
 *   Q: Difference between pipe() and FIFO?
 *   A: pipe() requires a common ancestor (fork); FIFO lives in the filesystem
 *      and allows unrelated processes to communicate.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#define FIFO_PATH "/tmp/demo_fifo"

static void separator(const char *t)
{
    printf("\n══════════════════════════════════════════\n  %s\n══════════════════════════════════════════\n", t);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: Anonymous pipe — parent writes, child reads    *
 * ─────────────────────────────────────────────────────── */
static void demo_anon_pipe(void)
{
    separator("Demo 1: Anonymous Pipe (pipe())");

    int fd[2]; /* fd[0]=read, fd[1]=write */
    if (pipe(fd) == -1) { perror("pipe"); return; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* CHILD: close write end, read from pipe */
        close(fd[1]);
        char buf[128];
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("[CHILD ] received: \"%s\"\n", buf);
        close(fd[0]);
        exit(0);
    } else {
        /* PARENT: close read end, write to pipe */
        close(fd[0]);
        const char *msg = "Hello from parent via anonymous pipe!";
        write(fd[1], msg, strlen(msg));
        printf("[PARENT] sent: \"%s\"\n", msg);
        close(fd[1]); /* EOF — child's read() will return */
        waitpid(pid, NULL, 0);
    }
}

/* ─────────────────────────────────────────────────────── *
 * Demo 2: Named pipe (FIFO)                              *
 * ─────────────────────────────────────────────────────── */
static void demo_named_pipe(void)
{
    separator("Demo 2: Named Pipe / FIFO (mkfifo())");

    /* Create FIFO (ignore EEXIST) */
    if (mkfifo(FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo"); return;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* READER: opens FIFO — blocks until writer opens */
        int rfd = open(FIFO_PATH, O_RDONLY);
        char buf[128];
        ssize_t n = read(rfd, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("[READER] FIFO received: \"%s\"\n", buf);
        close(rfd);
        exit(0);
    } else {
        /* WRITER: opens FIFO — blocks until reader opens */
        int wfd = open(FIFO_PATH, O_WRONLY);
        const char *msg = "FIFO message from writer!";
        write(wfd, msg, strlen(msg));
        printf("[WRITER] FIFO sent: \"%s\"\n", msg);
        close(wfd);
        waitpid(pid, NULL, 0);
        unlink(FIFO_PATH);
    }
}

int main(void)
{
    printf("=== Embedded Linux Demo: Pipes & FIFOs ===\n");
    demo_anon_pipe();
    demo_named_pipe();
    printf("\n[DONE] Pipe demo complete.\n");
    return 0;
}
