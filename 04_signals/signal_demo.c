/*
 * 04_signals/signal_demo.c
 * ========================
 * Demonstrates Linux signal handling:
 *   - signal() vs sigaction() (always prefer sigaction in production)
 *   - Blocking signals with sigprocmask / pthread_sigmask
 *   - Sending signals: kill(), raise(), sigqueue()
 *   - Real-time signals (SIGRTMIN … SIGRTMAX)
 *   - Self-pipe trick (signal-safe async notification)
 *   - signalfd (Linux-specific: read signals like a file)
 *   - Volatile sig_atomic_t for shared flags
 *
 * Interview topics:
 *   Q: Why prefer sigaction() over signal()?
 *   A: sigaction() has well-defined, portable behavior. signal() resets the
 *      handler to SIG_DFL and may not block the signal during handler execution,
 *      leading to re-entrant issues.
 *
 *   Q: What is a signal-safe function?
 *   A: A function that can be safely called from a signal handler.
 *      Only async-signal-safe functions (POSIX list) are allowed.
 *      printf() is NOT async-signal-safe; write() IS.
 *
 *   Q: What is the self-pipe trick?
 *   A: Write a byte to a pipe in the signal handler, and poll/select/epoll
 *      the read end — converts a signal into a file-descriptor event.
 *
 *   Q: Difference between regular and real-time signals?
 *   A: Real-time signals are queued (multiple instances delivered),
 *      can carry data (sigval), and have guaranteed delivery order.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

/* ── helpers ──────────────────────────────────────────── */
static void separator(const char *title)
{
    printf("\n══════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("══════════════════════════════════════════\n");
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: sigaction() — reliable signal handling          *
 * ─────────────────────────────────────────────────────── */
static volatile sig_atomic_t g_sigint_count = 0;

static void sigint_handler(int signo, siginfo_t *info, void *ctx)
{
    (void)ctx;
    /* Only async-signal-safe calls here! */
    const char msg[] = "[SIGNAL] Caught SIGINT\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    g_sigint_count++;

    (void)signo; (void)info;
}

static void demo_sigaction(void)
{
    separator("Demo 1: sigaction() — Reliable Handler");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigint_handler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART; /* SA_RESTART: auto-restart syscalls */
    sigemptyset(&sa.sa_mask);
    /* Block SIGTERM while handling SIGINT */
    sigaddset(&sa.sa_mask, SIGTERM);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return;
    }

    printf("Sending SIGINT to self with raise()…\n");
    raise(SIGINT);
    raise(SIGINT);
    printf("g_sigint_count = %d (expected 2)\n", g_sigint_count);

    /* Restore default */
    sa.sa_handler = SIG_DFL;
    sa.sa_flags   = 0;
    sigaction(SIGINT, &sa, NULL);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 2: Blocking signals with sigprocmask              *
 * ─────────────────────────────────────────────────────── */
static void demo_sigprocmask(void)
{
    separator("Demo 2: sigprocmask() — Block / Unblock");

    sigset_t block_set, old_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGUSR1);

    /* Block SIGUSR1 */
    sigprocmask(SIG_BLOCK, &block_set, &old_set);
    printf("SIGUSR1 blocked. Sending it now — it won't be delivered yet.\n");
    kill(getpid(), SIGUSR1);

    /* Check pending signals */
    sigset_t pending;
    sigpending(&pending);
    if (sigismember(&pending, SIGUSR1))
        printf("SIGUSR1 is PENDING (queued by kernel)\n");

    /* Unblock — pending signal delivered immediately */
    struct sigaction sa = { .sa_handler = SIG_IGN };
    sigaction(SIGUSR1, &sa, NULL); /* ignore so we don't crash */
    sigprocmask(SIG_SETMASK, &old_set, NULL);
    printf("SIGUSR1 unblocked (and ignored).\n");
}

/* ─────────────────────────────────────────────────────── *
 * Demo 3: Real-time signals + sigqueue (carries data)    *
 * ─────────────────────────────────────────────────────── */
static volatile sig_atomic_t g_rt_recv = 0;
static volatile int          g_rt_val  = 0;

static void rt_handler(int signo, siginfo_t *info, void *ctx)
{
    (void)ctx; (void)signo;
    g_rt_recv = 1;
    g_rt_val  = info->si_value.sival_int; /* the data carried by sigqueue */
    const char msg[] = "[RT SIGNAL] received\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

static void demo_realtime_signal(void)
{
    separator("Demo 3: Real-time Signal + sigqueue()");

    int rtsig = SIGRTMIN + 1;
    printf("Using signal SIGRTMIN+1 = %d\n", rtsig);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = rt_handler;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(rtsig, &sa, NULL);

    union sigval sv;
    sv.sival_int = 12345;
    printf("Sending RT signal with data %d via sigqueue()…\n", sv.sival_int);
    sigqueue(getpid(), rtsig, sv);

    /* Ensure handler runs */
    usleep(10000);
    printf("Received=%d, value=%d\n", g_rt_recv, g_rt_val);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 4: Self-pipe trick                                *
 * ─────────────────────────────────────────────────────── */
static int g_pipe_fds[2] = {-1, -1};

static void pipe_sig_handler(int signo)
{
    (void)signo;
    char byte = 'S';
    write(g_pipe_fds[1], &byte, 1); /* write-end: async-signal-safe */
}

static void demo_self_pipe(void)
{
    separator("Demo 4: Self-Pipe Trick");

    if (pipe(g_pipe_fds) == -1) { perror("pipe"); return; }

    /* Make write end non-blocking to avoid deadlock if pipe is full */
    fcntl(g_pipe_fds[1], F_SETFL, O_NONBLOCK);

    struct sigaction sa = { .sa_handler = pipe_sig_handler };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR2, &sa, NULL);

    printf("Sending SIGUSR2 — handler writes to pipe…\n");
    kill(getpid(), SIGUSR2);

    /* Read from pipe (in main loop — not in signal handler) */
    char buf[8];
    ssize_t n = read(g_pipe_fds[0], buf, sizeof(buf));
    if (n > 0)
        printf("Read %zd byte(s) from self-pipe — signal safely converted!\n", n);

    close(g_pipe_fds[0]);
    close(g_pipe_fds[1]);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 5: signalfd (Linux-specific)                      *
 * ─────────────────────────────────────────────────────── */
static void demo_signalfd(void)
{
    separator("Demo 5: signalfd() — Read Signals Like a File");

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    /* Block SIGUSR1 so it goes through signalfd instead */
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (sfd == -1) { perror("signalfd"); return; }

    printf("Sending SIGUSR1 — will be read via signalfd…\n");
    kill(getpid(), SIGUSR1);

    struct signalfd_siginfo fdsi;
    ssize_t s = read(sfd, &fdsi, sizeof(fdsi));
    if (s == sizeof(fdsi)) {
        printf("signalfd read: signo=%u, PID=%u\n",
               fdsi.ssi_signo, fdsi.ssi_pid);
    }
    close(sfd);
}

/* ─────────────────────────────────────────────────────── *
 * main                                                   *
 * ─────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== Embedded Linux Demo: Signals ===\n");

    demo_sigaction();
    demo_sigprocmask();
    demo_realtime_signal();
    demo_self_pipe();
    demo_signalfd();

    printf("\n[DONE] Signal demo complete.\n");
    return 0;
}
