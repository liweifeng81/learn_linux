/*
 * 01_processes/process_demo.c
 * ===========================
 * Demonstrates core Linux process concepts:
 *   - fork() / exec() / wait() / waitpid()
 *   - Zombie process: child exits before parent calls wait()
 *   - Orphan process: parent exits before child finishes
 *   - getpid() / getppid()
 *
 * Interview topics:
 *   Q: What is a zombie process?
 *   A: A process that has exited but whose exit status hasn't been collected
 *      by its parent (via wait/waitpid). It occupies a PID slot in the kernel.
 *
 *   Q: What is an orphan process?
 *   A: A process whose parent has exited. Adopted by init (PID 1).
 *
 *   Q: Difference between fork() and vfork()?
 *   A: vfork() does NOT copy parent's page table (CoW not applied),
 *      child shares parent's memory and must call exec() or _exit() immediately.
 *
 *   Q: What does exec() do to the process?
 *   A: Replaces the current process image with a new program.
 *      PID stays the same, memory/code/data are replaced.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

/* ── helpers ─────────────────────────────────────────── */
static void separator(const char *title)
{
    printf("\n══════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("══════════════════════════════════════════\n");
}

/* ── Demo 1: basic fork / wait ────────────────────────── */
static void demo_fork_wait(void)
{
    separator("Demo 1: fork() + wait()");

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        /* ── CHILD ── */
        printf("[CHILD ] PID=%d, PPID=%d — doing work…\n",
               getpid(), getppid());
        sleep(1);
        printf("[CHILD ] exiting with code 42\n");
        exit(42);
    } else {
        /* ── PARENT ── */
        printf("[PARENT] PID=%d, child PID=%d — waiting…\n",
               getpid(), pid);
        int status;
        pid_t reaped = waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("[PARENT] child %d exited normally, code=%d\n",
                   reaped, WEXITSTATUS(status));
        }
    }
}

/* ── Demo 2: exec() ───────────────────────────────────── */
static void demo_exec(void)
{
    separator("Demo 2: fork() + execl()");

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* Replace child image with /bin/ls */
        printf("[CHILD ] about to execl /bin/ls …\n");
        execl("/bin/ls", "ls", "-lh", "/tmp", (char *)NULL);
        /* execl only returns on error */
        perror("execl");
        exit(EXIT_FAILURE);
    } else {
        wait(NULL);
        printf("[PARENT] exec child finished\n");
    }
}

/* ── Demo 3: zombie process ───────────────────────────── */
static void demo_zombie(void)
{
    separator("Demo 3: Zombie Process");
    printf("Tip: run 'ps aux | grep Z' in another terminal to see the zombie\n\n");

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        printf("[CHILD ] PID=%d exiting immediately — becoming zombie…\n",
               getpid());
        exit(0);
    } else {
        printf("[PARENT] PID=%d NOT calling wait() for 5 s — child is zombie!\n",
               getpid());
        sleep(5);   /* child is zombie during this window */
        printf("[PARENT] now reaping zombie with wait()\n");
        wait(NULL);
        printf("[PARENT] zombie reaped.\n");
    }
}

/* ── Demo 4: orphan process ───────────────────────────── */
static void demo_orphan(void)
{
    separator("Demo 4: Orphan Process");

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* Child runs for a while after parent exits */
        sleep(2);
        printf("[ORPHAN] PID=%d, PPID=%d (adopted by init/systemd=1)\n",
               getpid(), getppid());
        exit(0);
    } else {
        printf("[PARENT] PID=%d exiting — child %d becomes orphan\n",
               getpid(), pid);
        /* Parent exits without waiting → child adopted by PID 1 */
        exit(0);
    }
}

/* ── Demo 5: waitpid() with WNOHANG (non-blocking) ────── */
static void demo_waitpid_nohang(void)
{
    separator("Demo 5: waitpid() with WNOHANG");

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        sleep(2);
        exit(7);
    } else {
        int status;
        /* Poll without blocking */
        for (int i = 0; i < 6; i++) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == 0) {
                printf("[PARENT] child still running (poll %d)…\n", i + 1);
                sleep(500 * 1000 / 1000000 + 1); /* ~0.5 s */
                usleep(500000);
            } else if (r > 0) {
                printf("[PARENT] child exited, code=%d\n", WEXITSTATUS(status));
                break;
            } else {
                perror("waitpid");
                break;
            }
        }
    }
}

/* ── main ─────────────────────────────────────────────── */
int main(void)
{
    printf("=== Embedded Linux Demo: Processes ===\n");
    printf("Parent PID: %d\n", getpid());

    demo_fork_wait();
    demo_exec();
    /* demo_zombie and demo_orphan call exit() internally — run them last */
    demo_waitpid_nohang();

    /* Zombie demo needs its own process so parent can wait properly */
    pid_t z = fork();
    if (z == 0) { demo_zombie(); exit(0); }
    waitpid(z, NULL, 0);

    /* Orphan — parent will exit, so we fork a wrapper */
    pid_t o = fork();
    if (o == 0) { demo_orphan(); exit(0); }
    waitpid(o, NULL, 0);

    printf("\n[DONE] Process demo complete.\n");
    return 0;
}
