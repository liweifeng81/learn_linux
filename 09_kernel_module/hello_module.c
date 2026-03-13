/*
 * 09_kernel_module/hello_module.c
 * ================================
 * A loadable kernel module demonstrating:
 *   - module_init / module_exit
 *   - printk() with log levels (KERN_INFO, KERN_DEBUG, etc.)
 *   - module_param() — pass parameters at insmod time
 *   - /proc filesystem entry via proc_create / seq_file API
 *   - /sys/module/<name>/parameters/<param>
 *   - Kernel version compatibility (LINUX_VERSION_CODE)
 *
 * Build:   make (uses kbuild Makefile in this directory)
 * Load:    sudo insmod hello_module.ko name="EmbeddedLinux" count=5
 * Test:    cat /proc/hello_module
 *          dmesg | tail -20
 * Unload:  sudo rmmod hello_module
 *
 * Interview topics:
 *   Q: What is a kernel module?
 *   A: Dynamically loadable object linked into the kernel at runtime.
 *      Runs in kernel space (ring 0), NOT user space. No libc.
 *
 *   Q: Difference between kernel space and user space?
 *   A: Kernel: privileged, direct HW access, shared address space with all processes.
 *      User: restricted, must use syscalls to access kernel resources.
 *      A bad pointer in kernel = kernel panic / oops.
 *
 *   Q: What is printk()?
 *   A: Kernel equivalent of printf(). Writes to kernel ring buffer (dmesg).
 *      Log levels: KERN_EMERG (0) … KERN_DEBUG (7).
 *
 *   Q: What is the difference between kmalloc and vmalloc?
 *   A: kmalloc: physically contiguous, suitable for DMA, limited size (~4MB).
 *      vmalloc: virtually contiguous only, can be large, slower (TLB flush).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/version.h>

/* ── Module metadata ──────────────────────────────────── */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Demo");
MODULE_DESCRIPTION("Interview demo: proc entry, module params, printk");
MODULE_VERSION("1.0");

/* ── Module parameters ────────────────────────────────── */
static char *name  = "World";
static int   count = 3;
module_param(name,  charp, 0444); /* read-only from sysfs */
module_param(count, int,   0644); /* read-write from sysfs */
MODULE_PARM_DESC(name,  "Name to greet (default: World)");
MODULE_PARM_DESC(count, "Number of greetings (default: 3)");

/* ── /proc entry ──────────────────────────────────────── */
#define PROC_NAME "hello_module"
static struct proc_dir_entry *proc_entry;

/* seq_show: called when /proc/hello_module is read */
static int hello_seq_show(struct seq_file *m, void *v)
{
    int i;
    seq_printf(m, "=== Hello Module ===\n");
    seq_printf(m, "Linux version: %d.%d.%d\n",
               LINUX_VERSION_MAJOR, LINUX_VERSION_PATCHLEVEL, LINUX_VERSION_SUBLEVEL);
    seq_printf(m, "Parameter name : %s\n", name);
    seq_printf(m, "Parameter count: %d\n", count);
    seq_printf(m, "\nGreetings:\n");
    for (i = 0; i < count; i++)
        seq_printf(m, "  [%d] Hello, %s!\n", i + 1, name);
    return 0;
}

/* Called on open() of /proc/hello_module */
static int hello_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, hello_seq_show, NULL);
}

/* file_operations for /proc entry */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops hello_proc_ops = {
    .proc_open    = hello_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations hello_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = hello_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};
#endif

/* ── Kernel memory demo ───────────────────────────────── */
static char *kmem_buf;

/* ── Init function ────────────────────────────────────── */
static int __init hello_init(void)
{
    printk(KERN_INFO "[hello_module] Loading — name=%s count=%d\n", name, count);

    /* Create /proc/hello_module */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    proc_entry = proc_create(PROC_NAME, 0444, NULL, &hello_proc_ops);
#else
    proc_entry = proc_create(PROC_NAME, 0444, NULL, &hello_proc_fops);
#endif
    if (!proc_entry) {
        printk(KERN_ERR "[hello_module] Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }
    printk(KERN_INFO "[hello_module] Created /proc/%s\n", PROC_NAME);

    /* Demonstrate kmalloc */
    kmem_buf = kmalloc(64, GFP_KERNEL);
    if (!kmem_buf) {
        printk(KERN_ERR "[hello_module] kmalloc failed!\n");
        proc_remove(proc_entry);
        return -ENOMEM;
    }
    snprintf(kmem_buf, 64, "kmalloc buffer at %p", kmem_buf);
    printk(KERN_INFO "[hello_module] %s\n", kmem_buf);
    printk(KERN_INFO "[hello_module] Module loaded successfully ✓\n");
    return 0;
}

/* ── Exit function ────────────────────────────────────── */
static void __exit hello_exit(void)
{
    proc_remove(proc_entry);
    kfree(kmem_buf);
    printk(KERN_INFO "[hello_module] Unloaded. Goodbye!\n");
}

module_init(hello_init);
module_exit(hello_exit);
