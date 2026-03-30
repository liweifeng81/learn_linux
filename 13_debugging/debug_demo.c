/*
 * 13_debugging/debug_demo.c
 * =========================
 * Intentional bugs for practicing debugging tools:
 *   - NULL pointer dereference → SIGSEGV  (run under gdb)
 *   - Heap buffer overflow (undefined behavior)
 *   - Memory leak (detect with valgrind)
 *   - Use-after-free (detect with AddressSanitizer or valgrind)
 *   - Stack buffer overflow (overflow a local array)
 *   - Double free
 *   - Uninitialized variable (run with valgrind --track-origins)
 *
 * Usage examples:
 *   Normal run:          ./debug_demo <bug_id 1..7>
 *   gdb:                 gdb ./debug_demo  → run <id> → bt
 *   valgrind leak:       valgrind --leak-check=full ./debug_demo 3
 *   AddressSanitizer:    Recompile with -fsanitize=address then run
 *   strace:              strace ./debug_demo 1  (see syscalls before crash)
 *
 * Interview tips:
 *   Q: How do you debug a crash in a deployed embedded system?
 *   A: Enable core dumps (ulimit -c unlimited / /proc/sys/kernel/core_pattern).
 *      Load coredump in GDB: gdb binary core → bt full.
 *
 *   Q: What is ASAN / Valgrind tradeoff?
 *   A: ASAN is 2x slower, compile-time instrumented, catches bugs faster.
 *      Valgrind is ~20x slower, no recompile needed, better leak tracking.
 *
 *   Q: What does addr2line do?
 *   A: Converts a crash address (from backtrace / dmesg oops) to a
 *      source file + line number: addr2line -e binary 0x<addr>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(const char *prog)
{
    printf("Usage: %s <bug_id>\n", prog);
    printf("  1 — NULL pointer dereference\n");
    printf("  2 — Heap buffer overflow\n");
    printf("  3 — Memory leak\n");
    printf("  4 — Use-after-free\n");
    printf("  5 — Stack buffer overflow\n");
    printf("  6 — Double free\n");
    printf("  7 — Uninitialized variable read\n");
}

/* Bug 1: NULL dereference */
static void bug_null_deref(void)
{
    printf("[Bug 1] NULL pointer dereference\n");
    int *p = NULL;
    printf("Value: %d\n", *p); /* SIGSEGV */
}

/* Bug 2: Heap buffer overflow */
static void bug_heap_overflow(void)
{
    printf("[Bug 2] Heap buffer overflow\n");
    char *buf = malloc(16);
    /* Writing past the end of allocated buffer */
    for (int i = 0; i < 40; i++)
        buf[i] = (char)i; /* overflow by 24 bytes */
    printf("Wrote past buffer end (UB)\n");
    free(buf);
}

/* Bug 3: Memory leak */
static void bug_leak(void)
{
    printf("[Bug 3] Memory leak — never freed\n");
    char *leak1 = malloc(1024);
    char *leak2 = malloc(4096);
    strncpy(leak1, "leaked allocation", 1024);
    strncpy(leak2, "also leaked", 4096);
    (void)leak1;
    free(leak2);
    /* No free() — valgrind will report lost bytes */
    printf("Allocated 5120 bytes without freeing\n");
}

/* Bug 4: Use-after-free */
static void bug_use_after_free(void)
{
    printf("[Bug 4] Use-after-free\n");
    char *buf = malloc(64);
    strcpy(buf, "original data");
    free(buf);
    /* Accessing freed memory — undefined behavior */
    printf("After free: \"%s\" (UB!)\n", buf);
}

/* Bug 5: Stack buffer overflow */
static void bug_stack_overflow(void)
{
    printf("[Bug 5] Stack buffer overflow\n");
    char local[16];
    /* Writing 64 bytes into a 16-byte stack buffer */
    memset(local, 'A', 64); /* stack smashing → SIGABRT or SIGSEGV */
    local[63] = '\0';
    printf("Stack smashed with: %s\n", local);
}

/* Bug 6: Double free */
static void bug_double_free(void)
{
    printf("[Bug 6] Double free\n");
    char *buf = malloc(32);
    strcpy(buf, "data");
    free(buf);
    free(buf); /* undefined behavior — typically abort() */
}

/* Bug 7: Uninitialized variable */
static void bug_uninit(void)
{
    printf("[Bug 7] Uninitialized variable read\n");
    int x;  /* not initialized */
    /* Valgrind: "Conditional jump or move depends on uninitialised value" */
    if (x > 0)
        printf("x is positive (%d)\n", x);
    else
        printf("x is not positive (%d)\n", x);
}

int main(int argc, char *argv[])
{
    if (argc < 2) { usage(argv[0]); return 1; }
    int id = atoi(argv[1]);

    switch (id) {
    case 1: bug_null_deref();     break;
    case 2: bug_heap_overflow();  break;
    case 3: bug_leak();           break;
    case 4: bug_use_after_free(); break;
    case 5: bug_stack_overflow(); break;
    case 6: bug_double_free();    break;
    case 7: bug_uninit();         break;
    default: usage(argv[0]);      return 1;
    }
    return 0;
}
