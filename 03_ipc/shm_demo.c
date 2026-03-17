/*
 * 03_ipc/shm_demo.c
 * =================
 * Demonstrates POSIX shared memory + semaphores (POSIX, no SysV):
 *   - shm_open() / ftruncate() / mmap()
 *   - sem_open() (named semaphore) for mutual exclusion
 *   - Producer writes, consumer reads via shared region
 *
 * Interview topics:
 *   Q: Fastest IPC mechanism?
 *   A: Shared memory — no kernel copy; processes directly access the same
 *      physical pages. Requires external synchronization (mutex/semaphore).
 *
 *   Q: POSIX shm vs mmap(MAP_SHARED)?
 *   A: shm_open creates a /dev/shm entry (tmpfs); mmap MAP_SHARED maps a
 *      regular file or anonymous region. Both use the same underlying mechanism.
 *
 *   Q: Named vs unnamed semaphore?
 *   A: Named (sem_open) lives in the filesystem, usable by unrelated processes.
 *      Unnamed (sem_init with pshared=1) is embedded in shared memory.
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <errno.h>

#define SHM_NAME  "/demo_shm"
#define SEM_WRITE "/demo_sem_write" /* writer can write */
#define SEM_READ  "/demo_sem_read"  /* reader can read  */
#define BUF_SIZE  256
#define N_MESSAGES 5

typedef struct {
    char     data[BUF_SIZE];
    int      msg_number;
} SharedBuf;

int main(void)
{
    printf("=== Embedded Linux Demo: POSIX Shared Memory + Semaphores ===\n\n");

    /* ── Create shared memory ─────────────────────────── */
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open"); exit(1); }
    ftruncate(shm_fd, sizeof(SharedBuf));

    SharedBuf *shm = mmap(NULL, sizeof(SharedBuf),
                          PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); exit(1); }
    close(shm_fd);

    /* ── Create semaphores ────────────────────────────── */
    //it's better to check whether sem_w and sem_r is already existing by errno == EEXIST
    sem_unlink(SEM_WRITE); sem_unlink(SEM_READ);
    sem_t *sem_w = sem_open(SEM_WRITE, O_CREAT | O_EXCL, 0666, 1); /* writer ready */
    sem_t *sem_r = sem_open(SEM_READ,  O_CREAT | O_EXCL, 0666, 0); /* reader waits */
    if (sem_w == SEM_FAILED || sem_r == SEM_FAILED) { perror("sem_open"); exit(1); }

    /* ── Fork ─────────────────────────────────────────── */
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }

    if (pid == 0) {
        /* ── CONSUMER (child) ──────────────────────────── */
        for (int i = 0; i < N_MESSAGES; i++) {
            sem_wait(sem_r);   /* wait for data */
            printf("[CONSUMER] msg #%d: \"%s\"\n", shm->msg_number, shm->data);
            sem_post(sem_w);   /* tell producer it can write again */
        }
        sem_close(sem_w); sem_close(sem_r);
        munmap(shm, sizeof(SharedBuf));
        exit(0);
    } else {
        /* ── PRODUCER (parent) ─────────────────────────── */
        for (int i = 0; i < N_MESSAGES; i++) {
            sem_wait(sem_w);   /* wait for write slot */
            shm->msg_number = i;
            snprintf(shm->data, BUF_SIZE, "Hello #%d from producer (PID %d)", i, getpid());
            printf("[PRODUCER] wrote msg #%d\n", i);
            sem_post(sem_r);   /* signal consumer */
        }
        waitpid(pid, NULL, 0);

        /* ── Cleanup ───────────────────────────────────── */
        sem_close(sem_w); sem_close(sem_r);
        sem_unlink(SEM_WRITE); sem_unlink(SEM_READ);
        munmap(shm, sizeof(SharedBuf));
        shm_unlink(SHM_NAME);
        printf("\n[DONE] Shared memory demo complete.\n");
    }
    return 0;
}
