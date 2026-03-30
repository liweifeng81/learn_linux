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

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

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

    fflush(stdout);
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
        //execlp("ls", "ls", "-lh", ".", (char *)NULL);
        execl("/bin/ls", "ls", "-lh", ".", (char *)NULL);
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
    //ps aux | awk '$8 ~ /Z/ {printf "user: %s, pid: %s, state:%s, process:%s\n", $1, $2,$8, $11}'
    printf("Tip: run  ps aux | grep Z in another terminal to see the zombie\n\n");

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        printf("[CHILD ] PID=%d exiting immediately — becoming zombie…\n",
               getpid());
        exit(0);
    } else {
        printf("[PARENT] PID=%d NOT calling wait() for 10 s — child is zombie!\n",
               getpid());
        //the following code is commented out to keep the child process as a zombie
        //for demonstration purposes. You can uncomment it to see how the parent reaps the zombie.
        //and if the parent does not call wait() at all, the zombie will remain until the parent exits,
        //at which point init/systemd will reap it automatically.

        //sleep(30);   /* child is zombie during this window */
        //printf("[PARENT] now reaping zombie with wait()\n");
        //wait(NULL);
        //printf("[PARENT] zombie reaped.\n");
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
        sleep(180);
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
        printf("[CHILD ] PID=%d exiting after 2s\n", getpid());
        exit(7);
    } else {
        int status;
        /* Poll without blocking */
        for (int i = 0; i < 6; i++) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == 0) {
                printf("[PARENT] child still running (poll %d),status=%d\n", i + 1, WIFEXITED(status));
                usleep(500000); /* 0.5 s */
            } else if (r > 0) {
                printf("[PARENT] child pid=%d, exited, code=%d\n", r, WEXITSTATUS(status));
                break;
            } else {
                perror("waitpid");
                break;
            }
        }
    }
}
/* ── Demo 6: vfork() — Shared Memory with Parent Until exec/_exit */
static void demo_vfork() {
    separator("Demo 6: vfork() — Shared Memory with Parent Until exec/_exit");
    printf("parent PID=%d\n", getpid());
    int fd = shm_open("/test", O_RDWR | O_CREAT, 0644);
    int result = ftruncate(fd, 4096); /* Ensure it has some size */
    if (result != 0) {
        perror("ftruncate");
        close(fd);
        return;
    }

    pid_t pid = vfork();
    if (pid < 0) { perror("vfork"); close(fd);return; }
    if(pid == 0) {
        char log[128];
        sprintf(log, "Child PID=%d, PPID=%d — writing to shared memory before exec()…\n",
                getpid(), getppid());
        if(write(STDOUT_FILENO, log, strlen(log)) == -1) {
            perror("write");
        }
        if(dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2");
        }
        //close(fd);//should not close fd in child, it will close parent's fd as well since they share memory until exec/_exit
        /* Must call exec() or _exit() immediately after vfork() */
        execlp("ls", "ls", "-lh", "/", (char *)NULL);
        perror("execlp ls");
        _exit(EXIT_FAILURE);
    }
    wait(NULL);
    char *child_output = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    if (child_output == MAP_FAILED) {
        perror("mmap");
        close(fd);        return;
    }
    close(fd); /* Can close fd after mmap */
    printf("[PARENT] read from shared memory (child's output):\n%s", child_output);
    munmap(child_output, 4096);
    shm_unlink("/test");
    printf("[PARENT] vfork child finished\n");
}
/* ── main ─────────────────────────────────────────────── */
int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);  /* line-buffer even when redirected */
    printf("=== Embedded Linux Demo: Processes ===\n");
    printf("Parent PID: %d\n", getpid());

    demo_fork_wait();
    demo_exec();
    demo_vfork();

    /* demo_zombie and demo_orphan call exit() internally — run them last */
    demo_waitpid_nohang();

    /* Zombie demo needs its own process so parent can wait properly */
    demo_zombie();

    /* Orphan — parent will exit, so we fork a wrapper */
    pid_t o = fork();
    if (o == 0) { demo_orphan(); exit(0); }
    waitpid(o, NULL, 0);


    printf("\n[DONE] Process demo complete.\n");
    return 0;
}
