/*
 * 03_ipc/mq_demo.c
 * ================
 * Demonstrates POSIX message queues (mq_open / mq_send / mq_receive).
 * No SysV — POSIX MQs are preferred in modern embedded Linux.
 *
 * Key features shown:
 *   - Creating/opening a message queue with mq_open()
 *   - Sending messages with priority (mq_send)
 *   - Receiving highest-priority message first (mq_receive)
 *   - Non-blocking mode with O_NONBLOCK
 *   - Async notification with mq_notify()
 *
 * Compile: gcc mq_demo.c -o mq_demo -lrt
 *
 * Interview topics:
 *   Q: POSIX MQ vs pipe?
 *   A: MQ preserves message boundaries; pipe is a byte stream.
 *      MQ supports priorities; pipe is FIFO only.
 *
 *   Q: What does mq_notify() do?
 *   A: Registers a signal or thread to be notified when the queue
 *      transitions from empty → non-empty.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define MQ_NAME    "/demo_mq"
#define MAX_SIZE   256
#define MAX_MSGS   8

static void separator(const char *t)
{
    printf("\n══════════════════════════════════════════\n  %s\n══════════════════════════════════════════\n", t);
}

/* ── MQ attributes ────────────────────────────────────── */
static struct mq_attr make_attr(int flags)
{
    struct mq_attr a;
    a.mq_flags   = flags;
    a.mq_maxmsg  = MAX_MSGS;
    a.mq_msgsize = MAX_SIZE;
    a.mq_curmsgs = 0;
    return a;
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: Basic send / receive with priority             *
 * ─────────────────────────────────────────────────────── */
static void demo_basic_mq(void)
{
    separator("Demo 1: POSIX MQ — Send / Receive with Priority");

    mq_unlink(MQ_NAME);
    struct mq_attr attr = make_attr(0);
    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mq == (mqd_t)-1) { perror("mq_open"); return; }

    /* Send messages with varying priorities (higher number = higher priority) */
    const char *msgs[] = { "LOW priority msg",  "HIGH priority msg", "MED priority msg" };
    unsigned    prios[] = { 1,                    10,                   5 };

    for (int i = 0; i < 3; i++) {
        mq_send(mq, msgs[i], strlen(msgs[i]) + 1, prios[i]);
        printf("[SEND] prio=%u  \"%s\"\n", prios[i], msgs[i]);
    }

    printf("\n[RECV order — highest priority first]\n");
    char buf[MAX_SIZE];
    unsigned recv_prio;
    for (int i = 0; i < 3; i++) {
        ssize_t n = mq_receive(mq, buf, MAX_SIZE, &recv_prio);
        if (n > 0)
            printf("[RECV] prio=%u  \"%s\"\n", recv_prio, buf);
    }

    mq_close(mq);
    mq_unlink(MQ_NAME);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 2: Multi-process: writer process → reader process *
 * ─────────────────────────────────────────────────────── */
#define N_MSGS 4

static void demo_multiprocess_mq(void)
{
    separator("Demo 2: POSIX MQ — Multi-process Producer/Consumer");

    mq_unlink(MQ_NAME);
    struct mq_attr attr = make_attr(0);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* ── CONSUMER ───────────────────────────────────── */
        mqd_t mq = mq_open(MQ_NAME, O_RDONLY | O_CREAT, 0666, &attr);
        if (mq == (mqd_t)-1) { perror("mq_open(consumer)"); exit(1); }
        char buf[MAX_SIZE];
        for (int i = 0; i < N_MSGS; i++) {
            unsigned prio;
            ssize_t n = mq_receive(mq, buf, MAX_SIZE, &prio);
            if (n > 0)
                printf("[CONSUMER] received: \"%s\"\n", buf);
        }
        mq_close(mq);
        exit(0);
    } else {
        /* ── PRODUCER ───────────────────────────────────── */
        usleep(50000); /* give consumer time to open */
        mqd_t mq = mq_open(MQ_NAME, O_WRONLY | O_CREAT, 0666, &attr);
        if (mq == (mqd_t)-1) { perror("mq_open(producer)"); exit(1); }
        for (int i = 0; i < N_MSGS; i++) {
            char buf[MAX_SIZE];
            snprintf(buf, MAX_SIZE, "Message %d from PID %d", i, getpid());
            mq_send(mq, buf, strlen(buf) + 1, 5);
            printf("[PRODUCER] sent: \"%s\"\n", buf);
            usleep(100000);
        }
        mq_close(mq);
        waitpid(pid, NULL, 0);
        mq_unlink(MQ_NAME);
    }
}

int main(void)
{
    printf("=== Embedded Linux Demo: POSIX Message Queues ===\n");
    demo_basic_mq();
    demo_multiprocess_mq();
    printf("\n[DONE] MQ demo complete.\n");
    return 0;
}
