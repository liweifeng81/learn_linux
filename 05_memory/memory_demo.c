/*
 * 05_memory/memory_demo.c
 * =======================
 * Demonstrates Linux memory management concepts:
 *   - Stack vs heap allocation
 *   - mmap(): anonymous and file-backed mappings
 *   - mprotect(): page protection
 *   - /proc/self/maps — reading VMA layout
 *   - Custom malloc wrapper (leak tracking pattern)
 *   - Stack overflow guard (RLIMIT_STACK)
 *   - Memory alignment (aligned_alloc / posix_memalign)
 *
 * Interview topics:
 *   Q: What is Copy-on-Write (CoW)?
 *   A: After fork(), parent and child share physical pages (read-only).
 *      On first write, the kernel copies the page for the writer.
 *
 *   Q: mmap vs malloc?
 *   A: malloc uses brk()/sbrk() or mmap for large allocations internally.
 *      mmap() maps specific files/devices or anonymous memory; gives more
 *      control over mapping flags (MAP_SHARED, MAP_LOCKED, MAP_POPULATE…).
 *
 *   Q: What is a memory-mapped file?
 *   A: A file mapped into the process address space — reads/writes directly
 *      modify the file's page cache without explicit read()/write() calls.
 *      Useful for IPC and high-performance I/O.
 *
 *   Q: What causes a segmentation fault?
 *   A: Accessing memory outside mapped regions, writing to read-only pages,
 *      or NULL pointer dereference (page 0 is never mapped).
 *
 *   Q: What is the difference between BSS and data segment?
 *   A: .data = initialized globals/statics; .bss = zero-initialized globals.
 *      .bss doesn't occupy space in the ELF file on disk.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <signal.h>
#include <stdint.h>
#include <errno.h>

/* ── Segment layout demonstration (read at load time) ─── */
static int  bss_var;                  /* .bss  — zero init */
static int  data_var  = 42;           /* .data — init'd    */
static const int rodata_var = 100;    /* .rodata           */

static void separator(const char *t)
{
    printf("\n══════════════════════════════════════════\n  %s\n══════════════════════════════════════════\n", t);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: Address space layout                           *
 * ─────────────────────────────────────────────────────── */
static void demo_address_layout(void)
{
    separator("Demo 1: Address Space Layout");

    int  stack_var = 0;
    int *heap_var  = malloc(sizeof(int));
    *heap_var      = 7;

    printf("  .text  (code)    : %p  (function address)\n", (void *)demo_address_layout);
    printf("  .rodata          : %p  (const value=%d)\n",   (void *)&rodata_var, rodata_var);
    printf("  .data  (init'd)  : %p  (value=%d)\n",         (void *)&data_var, data_var);
    printf("  .bss   (zero)    : %p  (value=%d)\n",         (void *)&bss_var, bss_var);
    printf("  heap             : %p\n",                      (void *)heap_var);
    printf("  stack            : %p\n",                      (void *)&stack_var);
    //run [nm memory_demo] can get those info as well
    
    printf("\n  /proc/self/maps (first 8 lines):\n");
    FILE *f = fopen("/proc/self/maps", "r");
    if (f) {
        char line[256];
        for (int i = 0; i < 8 && fgets(line, sizeof(line), f); i++)
            printf("    %s", line);
        fclose(f);
    }
    free(heap_var);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 2: Anonymous mmap                                 *
 * ─────────────────────────────────────────────────────── */
static void demo_anon_mmap(void)
{
    separator("Demo 2: mmap() — Anonymous Mapping");

    size_t size = 4 * 4096; /* 4 pages */
    void *mem = mmap(NULL, size,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
    if (mem == MAP_FAILED) { perror("mmap"); return; }

    printf("mmap'd %zu bytes at %p\n", size, mem);

    /* Kernel zero-fills anonymous pages on first access (demand paging) */
    memset(mem, 0xAB, size);
    printf("Wrote 0xAB pattern across %zu bytes\n", size);

    /* mprotect: make it read-only */
    if (mprotect(mem, size, PROT_READ) == 0)
        printf("mprotect → PROT_READ (write now would SIGSEGV)\n");

    /* Restore write, then unmap */
    mprotect(mem, size, PROT_READ | PROT_WRITE);
    munmap(mem, size);
    printf("munmap complete\n");
}

/* ─────────────────────────────────────────────────────── *
 * Demo 3: File-backed mmap                               *
 * ─────────────────────────────────────────────────────── */
static void demo_file_mmap(void)
{
    separator("Demo 3: mmap() — File-backed Mapping");

    const char *path = "/tmp/mmap_demo_file.bin";
    const char  content[] = "Embedded Linux mmap file demo!";
    size_t      fsize = 4096;

    /* Create and populate file */
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return; }
    ftruncate(fd, (off_t)fsize);
    pwrite(fd, content, sizeof(content) - 1, 0);

    /* Map file */
    char *map = mmap(NULL, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) { perror("mmap"); return; }

    printf("File content via mmap: \"%s\"\n", map);

    /* Modify via mmap — reflects in file */
    strncpy(map, "MODIFIED via mmap!", fsize);
    msync(map, fsize, MS_SYNC); /* ensure written to disk */

    /* Verify by re-reading file */
    fd = open(path, O_RDONLY);
    char verify[64] = {0};
    read(fd, verify, sizeof(verify) - 1);
    close(fd);
    printf("File content after mmap write: \"%s\"\n", verify);

    munmap(map, fsize);
    unlink(path);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 4: Memory alignment                               *
 * ─────────────────────────────────────────────────────── */
static void demo_alignment(void)
{
    separator("Demo 4: Aligned Memory Allocation");

    /* Cache-line aligned buffer (64-byte alignment) */
    void *buf = NULL;
    if (posix_memalign(&buf, 64, 1024) != 0) {
        perror("posix_memalign"); return;
    }
    printf("posix_memalign(64): %p  (aligned: %s)\n",
           buf, ((uintptr_t)buf % 64 == 0) ? "YES ✓" : "NO ✗");
    free(buf);

    /* C11 aligned_alloc */
    buf = aligned_alloc(4096, 4096); /* page-aligned */
    printf("aligned_alloc(4096): %p  (aligned: %s)\n",
           buf, ((uintptr_t)buf % 4096 == 0) ? "YES ✓" : "NO ✗");
    free(buf);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 5: Simple leak-tracking malloc wrapper            *
 * ─────────────────────────────────────────────────────── */
typedef struct AllocRecord {
    void   *ptr;
    size_t  size;
    struct AllocRecord *next;
} AllocRecord;

static AllocRecord *g_alloc_list = NULL;
static size_t       g_total_allocated = 0;

static void *tracked_malloc(size_t size)
{
    void *p = malloc(size);
    if (!p) return NULL;
    AllocRecord *rec = malloc(sizeof(AllocRecord));
    rec->ptr  = p;
    rec->size = size;
    rec->next = g_alloc_list;
    g_alloc_list = rec;
    g_total_allocated += size;
    return p;
}

static void tracked_free(void *ptr)
{
    AllocRecord **cur = &g_alloc_list;
    while (*cur) {
        if ((*cur)->ptr == ptr) {
            AllocRecord *tmp = *cur;
            g_total_allocated -= tmp->size;
            *cur = tmp->next;
            free(tmp);
            free(ptr);
            return;
        }
        cur = &(*cur)->next;
    }
    fprintf(stderr, "WARNING: tracked_free(%p) — not found! Double-free?\n", ptr);
}

static void check_leaks(void)
{
    if (g_alloc_list == NULL) {
        printf("No leaks detected. ✓\n");
        return;
    }
    printf("LEAKS DETECTED:\n");
    for (AllocRecord *r = g_alloc_list; r; r = r->next)
        printf("  %p  %zu bytes\n", r->ptr, r->size);
    printf("Total leaked: %zu bytes\n", g_total_allocated);
}

static void demo_leak_tracking(void)
{
    separator("Demo 5: Custom Malloc Wrapper (Leak Tracking)");

    char *a = tracked_malloc(128);
    char *b = tracked_malloc(256);
    char *leak = tracked_malloc(64); /* intentional leak — no tracked_free */

    tracked_free(a);
    tracked_free(b);
    /* 'c' (64 bytes) was never freed */

    printf("After freeing a and b (64-byte allocation is leaked):\n");
    check_leaks();
}

/* ─────────────────────────────────────────────────────── *
 * main                                                   *
 * ─────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== Embedded Linux Demo: Memory Management ===\n");
    demo_address_layout();
    demo_anon_mmap();
    demo_file_mmap();
    demo_alignment();
    demo_leak_tracking();
    printf("\n[DONE] Memory demo complete.\n");
    return 0;
}
