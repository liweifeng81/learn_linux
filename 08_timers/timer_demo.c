/*
 * 08_timers/timer_demo.c
 * ======================
 * Demonstrates Linux timer mechanisms:
 *   1. setitimer() / SIGALRM — traditional interval timer
 *   2. POSIX timer_create() with SIGEV_SIGNAL — per-process real-time timer
 *   3. timerfd_create() — timer as a file descriptor (epoll-friendly)
 *   4. clock_gettime() — monotonic and real-time clocks
 *   5. nanosleep() — precision sleep
 *
 * Interview topics:
 *   Q: Difference between CLOCK_REALTIME and CLOCK_MONOTONIC?
 *   A: REALTIME: wall-clock time, can jump (NTP, adjtime).
 *      MONOTONIC: never goes backward, measures uptime since some point.
 *      Always use MONOTONIC for timeouts and interval measurements.
 *
 *   Q: Why prefer timerfd over setitimer?
 *   A: timerfd integrates with epoll/select — no signal handler complexity,
 *      thread-safe, can be polled across threads.
 *
 *   Q: What is clock drift in embedded systems?
 *   A: Crystal oscillator inaccuracy causes the RTC to drift.
 *      Corrected by NTP, PPS GPS, or hardware RTC calibration.
 *
 *   Q: What is jitter in real-time systems?
 *   A: Variation in timer delivery time. Caused by scheduling latency,
 *      interrupts, cache effects. Minimized with RT kernel (PREEMPT_RT).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <stdint.h>

static void separator(const char *t)
{
    printf("\n══════════════════════════════════════════\n  %s\n══════════════════════════════════════════\n", t);
}

/* ─────────────────────────────────────────────────────── *
 * Helper: get current monotonic time in nanoseconds      *
 * ─────────────────────────────────────────────────────── */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: clock_gettime + nanosleep                      *
 * ─────────────────────────────────────────────────────── */
static void demo_clock_nanosleep(void)
{
    separator("Demo 1: clock_gettime() + nanosleep()");

    struct timespec now_real, now_mono;
    clock_gettime(CLOCK_REALTIME,  &now_real);
    clock_gettime(CLOCK_MONOTONIC, &now_mono);

    printf("CLOCK_REALTIME : %ld.%09ld s\n", now_real.tv_sec,  now_real.tv_nsec);
    printf("CLOCK_MONOTONIC: %ld.%09ld s\n", now_mono.tv_sec, now_mono.tv_nsec);

    uint64_t t0 = now_ns();
    struct timespec sleep_ts = { .tv_sec = 0, .tv_nsec = 100000000 }; /* 100 ms */
    nanosleep(&sleep_ts, NULL);
    uint64_t elapsed = now_ns() - t0;
    printf("nanosleep(100ms) actual: %lu ns (%.2f ms)\n",
           (unsigned long)elapsed, elapsed / 1e6);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 2: setitimer / SIGALRM                           *
 * ─────────────────────────────────────────────────────── */
static volatile int g_alarm_count = 0;

static void sigalrm_handler(int s) { (void)s; g_alarm_count++; }

static void demo_setitimer(void)
{
    separator("Demo 2: setitimer() / SIGALRM");

    struct sigaction sa = { .sa_handler = sigalrm_handler };
    struct sigaction old_sa;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, &old_sa);

    /* Fire every 100 ms, starting after 100 ms */
    struct itimerval itv = {
        .it_interval = { .tv_usec = 100000 }, /* repeat interval */
        .it_value    = { .tv_usec = 100000 }, /* first expiry    */
    };
    setitimer(ITIMER_REAL, &itv, NULL);

    printf("Waiting for 5 SIGALRM ticks…\n");
    while (g_alarm_count < 5)
        pause(); /* wait for signal */

    /* Disarm */
    memset(&itv, 0, sizeof(itv));
    setitimer(ITIMER_REAL, &itv, NULL);
    sigaction(SIGALRM, &old_sa, NULL);
    printf("Got %d SIGALRM signals ✓\n", g_alarm_count);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 3: POSIX timer_create with SIGEV_SIGNAL           *
 * ─────────────────────────────────────────────────────── */
static volatile int g_posix_count = 0;

static void posix_timer_handler(int s, siginfo_t *info, void *ctx)
{
    (void)s; (void)ctx;
    /* si_overrun: how many expirations we missed */
    g_posix_count += 1 + timer_getoverrun(*(timer_t *)info->si_value.sival_ptr);
}

static void demo_posix_timer(void)
{
    separator("Demo 3: timer_create() with SIGEV_SIGNAL");

    timer_t tid;
    struct sigevent sev = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo  = SIGRTMIN,
    };
    sev.sigev_value.sival_ptr = &tid;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = posix_timer_handler;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL); //todo: this is not reset!

    if (timer_create(CLOCK_MONOTONIC, &sev, &tid) == -1) {
        perror("timer_create");
        return;
    }

    struct itimerspec its = {
        .it_interval = { .tv_nsec = 100000000 }, /* 100 ms */
        .it_value    = { .tv_nsec = 100000000 },
    };
    timer_settime(tid, 0, &its, NULL);

    printf("Waiting for 5 POSIX timer expirations…\n");
    while (g_posix_count < 5)
        pause();

    its.it_value.tv_nsec = 0; /* disarm */
    timer_settime(tid, 0, &its, NULL);
    timer_delete(tid);
    printf("Got %d timer expirations ✓\n", g_posix_count);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 4: timerfd + epoll                                *
 * ─────────────────────────────────────────────────────── */
static void demo_timerfd(void)
{
    separator("Demo 4: timerfd_create() + epoll");

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (tfd < 0) { perror("timerfd_create"); return; }

    struct itimerspec its = {
        .it_interval = { .tv_nsec = 100000000 }, /* 100 ms */
        .it_value    = { .tv_nsec = 100000000 },
    };
    if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
        perror("timerfd_settime");
        close(tfd); return;
    }
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) { perror("epoll_create1"); close(tfd); return; }
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = tfd };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &ev) == -1) {
        perror("epoll_ctl");
        close(epfd);
        close(tfd);
        return;
    }

    printf("timerfd firing every 100ms — waiting for 5 expirations…\n");
    int fire_count = 0;
    while (fire_count < 5) {
        struct epoll_event events[1];
        int n = epoll_wait(epfd, events, 1, 1000);
        if (n > 0) {
            uint64_t exp;
            /* number of expirations since last read */
            if (read(tfd, &exp, sizeof(exp)) == sizeof(exp)) {
                fire_count += (int)exp;
                printf("  timerfd fired, exp=%llu, total=%d\n",
                       (unsigned long long)exp, fire_count);
            } else {
                perror("read timerfd");
            }
        }
    }

    close(epfd);
    close(tfd);
    printf("timerfd demo done ✓\n");
}

int main(void)
{
    printf("=== Embedded Linux Demo: Timers ===\n");
    demo_clock_nanosleep();
    demo_setitimer();
    demo_posix_timer();
    demo_timerfd();
    printf("\n[DONE] Timer demo complete.\n");
    return 0;
}
