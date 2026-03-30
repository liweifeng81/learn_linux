/*
 * 02_threads/thread_demo.c
 * ========================
 * Demonstrates POSIX thread (pthreads) concepts:
 *   - Thread creation, joining, detaching
 *   - Mutex (exclusive lock)
 *   - Condition variable (wait/signal/broadcast)
 *   - Read-write lock (multiple readers OR one writer)
 *   - Producer-Consumer pattern
 *   - Thread-local storage (__thread)
 *   - Thread cancellation
 *
 * Compile: gcc -pthread thread_demo.c -o thread_demo
 *
 * Interview topics:
 *   Q: Difference between process and thread?
 *   A: Threads share address space (heap, globals, fd table);
 *      processes have separate address spaces.
 *
 *   Q: What is a race condition?
 *   A: Two threads access shared data concurrently and at least one writes,
 *      without synchronization → undefined behavior.
 *
 *   Q: Mutex vs Semaphore?
 *   A: Mutex is owned by the locking thread (must be unlocked by same thread).
 *      Semaphore is a signaling counter, can be posted from any thread/interrupt.
 *
 *   Q: What is priority inversion?
 *   A: High-priority thread waits for a mutex held by a low-priority thread,
 *      while a medium-priority thread preempts the low-priority one.
 *      Solution: priority inheritance (PTHREAD_PRIO_INHERIT).
 *
 *   Q: Spinlock vs Mutex?
 *   A: Spinlock busy-waits (good for very short critical sections on SMP);
 *      mutex puts thread to sleep (better when wait time is long).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

/* ── helpers ──────────────────────────────────────────── */
#define CHECK(fn, ...)  do { \
    int _e = (fn)(__VA_ARGS__); \
    if (_e) { fprintf(stderr, #fn " failed: %s\n", strerror(_e)); exit(1); } \
} while(0)

static void separator(const char *title)
{
    printf("\n══════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("══════════════════════════════════════════\n");
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: Basic thread create / join                      *
 * ─────────────────────────────────────────────────────── */
static void *basic_worker(void *arg)
{
    int id = (int)(long)arg;
    printf("[Thread %d] TID=%lu running\n", id, (unsigned long)pthread_self());
    sleep(1);
    printf("[Thread %d] done\n", id);
    return (void *)(long)(id * 10); /* return value */
}

static void demo_basic_threads(void)
{
    separator("Demo 1: Thread Create & Join");
#define N_THREADS 4
    pthread_t tids[N_THREADS];

    for (int i = 0; i < N_THREADS; i++) {
        //CHECK(pthread_create, &tids[i], NULL, basic_worker, &ids[i]);
        int err = pthread_create(&tids[i], NULL, basic_worker, (void *)(long)i);
        if(err != 0) {
            printf("Error creating thread %d: %s\n", i, strerror(err));
            exit(1);
        }
    }
    for (int i = 0; i < N_THREADS; i++) {
        void *retval;
        //CHECK(pthread_join, tids[i], &retval);
        int err = pthread_join(tids[i], &retval);
        if(err != 0) {
            printf("Error creating thread %d: %s\n", i, strerror(err));
            exit(1);
        }
        
        printf("[MAIN ] thread %d, tids %lu, returned %ld\n", i, tids[i], (long)retval);
    }
#undef N_THREADS
}

/* ─────────────────────────────────────────────────────── *
 * Demo 2: Mutex — protect a shared counter               *
 * ─────────────────────────────────────────────────────── */
static long        g_counter = 0;
//static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
#define ITERS 100000

static void *counter_increment(void *arg)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
    for (int i = 0; i < ITERS; i++) {
        pthread_mutex_lock(mutex);
        g_counter++;
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

static void demo_mutex(void)
{
    separator("Demo 2: Mutex — Shared Counter");
    //it's no need to use the attr
    pthread_mutex_t g_mutex ;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&g_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    
    //pthread_mutex_init(&g_mutex, NULL);
#define N 4
    pthread_t t[N];
    g_counter = 0;
    for (int i = 0; i < N; i++)
        CHECK(pthread_create, &t[i], NULL, counter_increment, &g_mutex);
    for (int i = 0; i < N; i++)
        CHECK(pthread_join, t[i], NULL);
    printf("Expected: %d  Got: %ld  %s\n",
           N * ITERS, g_counter,
           g_counter == N * ITERS ? "✓ CORRECT" : "✗ RACE DETECTED");
#undef N
}
#undef ITERS

/* ───────────────────────────────────────────────────────────────── *
 * Demo 3: Condition Variable — Producer / Consumer                  *
 * the mutex solution is good for mulitp producer and consumer case
 * ────────────────────────────────────────────────────────────────── */
#define QUEUE_SIZE 8
typedef struct {
    int       data[QUEUE_SIZE];
    int       head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} Queue;

static Queue g_queue = {
    .lock      = PTHREAD_MUTEX_INITIALIZER,
    .not_empty = PTHREAD_COND_INITIALIZER,
    .not_full  = PTHREAD_COND_INITIALIZER,
};

static void queue_push(Queue *q, int val)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == QUEUE_SIZE)
        pthread_cond_wait(&q->not_full, &q->lock);
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

static int queue_pop(Queue *q)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->lock);
    int val = q->data[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return val;
}

#define SENTINEL -1

static void *producer(void *arg)
{
    int n = *(int *)arg;
    for (int i = 0; i < n; i++) {
        printf("[PRODUCER] pushing %d\n", i);
        queue_push(&g_queue, i);
        usleep(1); /* 1 us */
    }
    queue_push(&g_queue, SENTINEL); /* signal consumer to stop */
    return NULL;
}

static void *consumer(void *arg)
{
    (void)arg;
    while (1) {
        int val = queue_pop(&g_queue);
        if (val == SENTINEL) break;
        printf("[CONSUMER] got %d\n", val);
    }
    printf("[CONSUMER] done\n");
    return NULL;
}

static void demo_cond_var(void)
{
    separator("Demo 3: Condition Variable — Producer/Consumer");
    pthread_t prod, cons;
    int n = 10;
    CHECK(pthread_create, &prod, NULL, producer, &n);
    CHECK(pthread_create, &cons, NULL, consumer, NULL);
    CHECK(pthread_join, prod, NULL);
    CHECK(pthread_join, cons, NULL);
}

/* ────────────────────────────────────────────────────────────────── *
 * Demo 3.1 use semaphore instead of cond var for Producer/Consumer   *
 * only for single producer and single consumer case
 * ────────────────────────────────────────────────────────────────── */
#include <semaphore.h>
typedef struct {
    int       data[QUEUE_SIZE];
    int       head, tail, count;
    sem_t     sem_not_full;
    sem_t     sem_not_empty;
} QueueSem_t;

static QueueSem_t _sem_queue;
static void raw_queue_push(QueueSem_t *q, int val) {
    sem_wait(&q->sem_not_full);

    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    sem_post(&q->sem_not_empty);
}
static int raw_queue_pop(QueueSem_t *q) {
    sem_wait(&q->sem_not_empty);

    int val = q->data[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    sem_post(&q->sem_not_full);

    return val;
}
static void * producer_sem(void *arg) {
    int count = *(int *)arg;
    for(int i = 0; i < count; i++){
        printf("[producer] pushing %d\n", i);
        raw_queue_push(&_sem_queue, i);
    }
    raw_queue_push(&_sem_queue, SENTINEL);
    return NULL;
}
static void *consumer_sem(void *arg) {
    (void)arg;
    while(1) {
        int val = raw_queue_pop(&_sem_queue);
        printf("[cunsumer] get: %d\n", val);
        if(val == SENTINEL)
            break;
    }
    return NULL;
}
static void demo_sem_prod_consumer(void)
{
    separator("Demo 3.1: semaphor — Producer/Consumer");
    pthread_t prod, cons;
    sem_init(&_sem_queue.sem_not_full, 0, 5);
    sem_init(&_sem_queue.sem_not_empty, 0, 0);

    int n = 10;
    CHECK(pthread_create, &prod, NULL, producer_sem, &n);
    CHECK(pthread_create, &cons, NULL, consumer_sem, NULL);
    CHECK(pthread_join, prod, NULL);
    CHECK(pthread_join, cons, NULL);
    sem_destroy(&_sem_queue.sem_not_full);
    sem_destroy(&_sem_queue.sem_not_empty);
}
/* ─────────────────────────────────────────────────────── *
 * Demo 4: Read-Write Lock                                 *
 * ─────────────────────────────────────────────────────── */
static int            g_shared_val = 0;
static pthread_rwlock_t g_rwlock   = PTHREAD_RWLOCK_INITIALIZER;

static void *reader(void *arg)
{
    int id = *(int *)arg;
    pthread_rwlock_rdlock(&g_rwlock);
    printf("[READER %d] shared_val = %d\n", id, g_shared_val);
    usleep(20000);
    pthread_rwlock_unlock(&g_rwlock);
    return NULL;
}

static void *writer(void *arg)
{
    int id = *(int *)arg;
    pthread_rwlock_wrlock(&g_rwlock);
    g_shared_val = id * 100;
    printf("[WRITER %d] set shared_val = %d\n", id, g_shared_val);
    usleep(10000);
    pthread_rwlock_unlock(&g_rwlock);
    return NULL;
}

static void demo_rwlock(void)
{
    separator("Demo 4: Read-Write Lock");
#define NR 3
    pthread_t rt[NR], wt[2];
    int ids[NR + 2];
    ids[NR] = 98;
    ids[NR+1] = 99;
    CHECK(pthread_create, &wt[0], NULL, writer, &ids[NR]);
    CHECK(pthread_create, &wt[1], NULL, writer, &ids[NR+1]);

    for (int i = 0; i < NR; i++) {
        ids[i] = i + 1;
        CHECK(pthread_create, &rt[i], NULL, reader, &ids[i]);
    }

    for (int i = 0; i < NR; i++)
        CHECK(pthread_join, rt[i], NULL);
    CHECK(pthread_join, wt[0], NULL);
    CHECK(pthread_join, wt[1], NULL);
#undef NR
}

/* ─────────────────────────────────────────────────────── *
 * Demo 5: Thread-Local Storage                           *
 * ─────────────────────────────────────────────────────── */
static __thread int tls_counter = 0; /* each thread has its own copy */

static void *tls_worker(void *arg)
{
    int id = *(int *)arg;
    for (int i = 0; i < 5; i++)
        tls_counter++;
    printf("[Thread %d] tls_counter = %d (independent of other threads)\n",
           id, tls_counter);
    return NULL;
}

static void demo_tls(void)
{
    separator("Demo 5: Thread-Local Storage (__thread)");
#define NT 3
    pthread_t t[NT];
    int ids[NT];
    for (int i = 0; i < NT; i++) {
        ids[i] = i + 1;
        CHECK(pthread_create, &t[i], NULL, tls_worker, &ids[i]);
    }
    for (int i = 0; i < NT; i++)
        CHECK(pthread_join, t[i], NULL);
#undef NT
}
struct MyConflictHandler {
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int count;
};
void *producer2(void *arg) {
    struct MyConflictHandler *n = (struct MyConflictHandler *)arg;
    pthread_mutex_lock(&n->mutex);
    for(size_t i = 0; i < 100; i++) {
        while (n->count > 10)
            pthread_cond_wait(&n->not_full, &n->mutex);
        n->count++;
        printf("[producer] push count: %d\n", n->count);
        pthread_cond_signal(&n->not_empty);
    }
    pthread_mutex_unlock(&n->mutex);

    return NULL;
}
void *consumer2(void *arg) {
    struct MyConflictHandler *n = (struct MyConflictHandler *)arg;
    pthread_mutex_lock(&n->mutex);
    for(size_t i = 0; i < 100; i++) {
        while (n->count == 0)
            pthread_cond_wait(&n->not_empty, &n->mutex);
        n->count--;
        printf("[consumer] pop count: %d\n", n->count);
        pthread_cond_signal(&n->not_full);
    }
    pthread_mutex_unlock(&n->mutex);

    return NULL;
}
void demo_cond_var2(void) {
    separator("Demo 3: Condition Variable — Producer/Consumer");
    pthread_t prod, cons;
    struct MyConflictHandler test_data = {
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .not_empty = PTHREAD_COND_INITIALIZER,
        .not_full = PTHREAD_COND_INITIALIZER,
        .count = 0,
    };

    CHECK(pthread_create, &prod, NULL, producer2, &test_data);
    CHECK(pthread_create, &cons, NULL, consumer2, &test_data);
    CHECK(pthread_join, prod, NULL);
    CHECK(pthread_join, cons, NULL);
}
/* ─────────────────────────────────────────────────────── *
 * main                                                   *
 * ─────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== Embedded Linux Demo: Threads (pthreads) ===\n");
    demo_basic_threads();
    demo_mutex();
    demo_cond_var2();
    demo_sem_prod_consumer();
    demo_rwlock();
    demo_tls();

    printf("\n[DONE] Thread demo complete.\n");
    return 0;
}
