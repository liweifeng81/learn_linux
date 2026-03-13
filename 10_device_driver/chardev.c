/*
 * 10_device_driver/chardev.c
 * ==========================
 * Character device driver skeleton demonstrating:
 *   - alloc_chrdev_region / unregister_chrdev_region
 *   - cdev_init / cdev_add / cdev_del
 *   - file_operations: open, release, read, write, llseek, unlocked_ioctl
 *   - copy_to_user / copy_from_user (safe user↔kernel memory transfer)
 *   - kmalloc / kfree (kernel heap)
 *   - class_create / device_create (auto /dev node via udev)
 *   - Kernel circular buffer
 *   - mutex for concurrency protection
 *   - Custom IOCTL commands
 *
 * Build:   make (kbuild Makefile)
 * Load:    sudo insmod chardev.ko
 * Test:    echo "hello" > /dev/chardev0
 *          cat /dev/chardev0
 *          sudo rmmod chardev
 *
 * Interview topics:
 *   Q: Difference between character and block device?
 *   A: Character: byte stream, sequential access (serial, keyboard, sensors).
 *      Block: random access at block granularity (disks, flash).
 *      Network devices are a third class (not accessible via /dev).
 *
 *   Q: What is copy_from_user() / copy_to_user()?
 *   A: Safe memcpy between kernel and user space. Checks for valid user
 *      addresses and handles page faults. Never use memcpy directly
 *      with user pointers in the kernel.
 *
 *   Q: What is minor/major number?
 *   A: Major identifies the driver; minor identifies the specific device
 *      instance. MKDEV(major, minor) combines them.
 *
 *   Q: What is udev?
 *   A: Userspace device manager. Listens to kernel uevent netlink messages
 *      (from class_create/device_create) and creates /dev nodes dynamically.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Demo");
MODULE_DESCRIPTION("Character device driver skeleton");
MODULE_VERSION("1.0");

/* ── Configuration ────────────────────────────────────── */
#define DRIVER_NAME   "chardev"
#define DEVICE_NAME   "chardev0"
#define BUF_SIZE      4096
#define NUM_DEVICES   1

/* ── Custom IOCTL commands ────────────────────────────── */
#define CHARDEV_MAGIC  'C'
#define CHARDEV_RESET  _IO(CHARDEV_MAGIC,  0)  /* flush buffer          */
#define CHARDEV_GSIZE  _IOR(CHARDEV_MAGIC, 1, int) /* get bytes in buffer */
#define CHARDEV_SECHO  _IOW(CHARDEV_MAGIC, 2, int) /* set echo mode       */

/* ── Device state ─────────────────────────────────────── */
struct chardev_data {
    struct cdev       cdev;
    struct mutex      lock;
    char             *buf;
    size_t            buf_len;  /* bytes currently in buffer */
    loff_t            rd_pos;   /* read position */
    int               echo;     /* echo writes to kernel log */
};

static dev_t            g_devno;
static struct class    *g_class;
static struct chardev_data g_dev;

/* ── open ─────────────────────────────────────────────── */
static int chardev_open(struct inode *inode, struct file *filp)
{
    struct chardev_data *dev =
        container_of(inode->i_cdev, struct chardev_data, cdev);
    filp->private_data = dev;
    printk(KERN_INFO "[chardev] open() — PID %d\n", current->pid);
    return 0;
}

/* ── release ──────────────────────────────────────────── */
static int chardev_release(struct inode *inode, struct file *filp)
{
    (void)inode; (void)filp;
    printk(KERN_INFO "[chardev] release()\n");
    return 0;
}

/* ── read ─────────────────────────────────────────────── */
static ssize_t chardev_read(struct file *filp, char __user *ubuf,
                             size_t count, loff_t *ppos)
{
    struct chardev_data *dev = filp->private_data;
    ssize_t n;

    mutex_lock(&dev->lock);

    if (dev->buf_len == 0) {
        mutex_unlock(&dev->lock);
        return 0; /* EOF */
    }

    n = min(count, dev->buf_len);
    if (copy_to_user(ubuf, dev->buf, n)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    /* Shift remaining data left (simple ring-less buffer) */
    memmove(dev->buf, dev->buf + n, dev->buf_len - n);
    dev->buf_len -= n;
    *ppos += n;

    printk(KERN_INFO "[chardev] read() %zd bytes\n", n);
    mutex_unlock(&dev->lock);
    return n;
}

/* ── write ────────────────────────────────────────────── */
static ssize_t chardev_write(struct file *filp, const char __user *ubuf,
                              size_t count, loff_t *ppos)
{
    struct chardev_data *dev = filp->private_data;
    size_t space, n;

    mutex_lock(&dev->lock);

    space = BUF_SIZE - dev->buf_len;
    if (space == 0) {
        mutex_unlock(&dev->lock);
        return -ENOSPC;
    }

    n = min(count, space);
    if (copy_from_user(dev->buf + dev->buf_len, ubuf, n)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    dev->buf_len += n;
    *ppos += n;

    if (dev->echo)
        printk(KERN_INFO "[chardev] write() %zd bytes: \"%.*s\"\n",
               n, (int)n, dev->buf + dev->buf_len - n);
    else
        printk(KERN_INFO "[chardev] write() %zd bytes\n", n);

    mutex_unlock(&dev->lock);
    return n;
}

/* ── llseek ───────────────────────────────────────────── */
static loff_t chardev_llseek(struct file *filp, loff_t offset, int whence)
{
    (void)filp; (void)offset; (void)whence;
    return -ESPIPE; /* not seekable */
}

/* ── ioctl ────────────────────────────────────────────── */
static long chardev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct chardev_data *dev = filp->private_data;
    int val;

    switch (cmd) {
    case CHARDEV_RESET:
        mutex_lock(&dev->lock);
        dev->buf_len = 0;
        memset(dev->buf, 0, BUF_SIZE);
        mutex_unlock(&dev->lock);
        printk(KERN_INFO "[chardev] ioctl RESET\n");
        return 0;

    case CHARDEV_GSIZE:
        mutex_lock(&dev->lock);
        val = (int)dev->buf_len;
        mutex_unlock(&dev->lock);
        if (copy_to_user((int __user *)arg, &val, sizeof(val)))
            return -EFAULT;
        return 0;

    case CHARDEV_SECHO:
        if (copy_from_user(&val, (int __user *)arg, sizeof(val)))
            return -EFAULT;
        mutex_lock(&dev->lock);
        dev->echo = val;
        mutex_unlock(&dev->lock);
        printk(KERN_INFO "[chardev] ioctl SECHO = %d\n", val);
        return 0;

    default:
        return -ENOTTY;
    }
}

/* ── file_operations table ────────────────────────────── */
static const struct file_operations chardev_fops = {
    .owner          = THIS_MODULE,
    .open           = chardev_open,
    .release        = chardev_release,
    .read           = chardev_read,
    .write          = chardev_write,
    .llseek         = chardev_llseek,
    .unlocked_ioctl = chardev_ioctl,
};

/* ── Module init ──────────────────────────────────────── */
static int __init chardev_init(void)
{
    int ret;

    /* Dynamically allocate major:minor */
    ret = alloc_chrdev_region(&g_devno, 0, NUM_DEVICES, DRIVER_NAME);
    if (ret < 0) {
        printk(KERN_ERR "[chardev] alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }
    printk(KERN_INFO "[chardev] major=%d minor=%d\n",
           MAJOR(g_devno), MINOR(g_devno));

    /* Init cdev */
    cdev_init(&g_dev.cdev, &chardev_fops);
    g_dev.cdev.owner = THIS_MODULE;
    mutex_init(&g_dev.lock);

    g_dev.buf = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!g_dev.buf) { ret = -ENOMEM; goto err_chrdev; }
    g_dev.buf_len = 0;
    g_dev.echo    = 1;

    ret = cdev_add(&g_dev.cdev, g_devno, NUM_DEVICES);
    if (ret < 0) { printk(KERN_ERR "[chardev] cdev_add failed\n"); goto err_buf; }

    /* Create /dev/chardev0 via udev */
    g_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(g_class)) { ret = PTR_ERR(g_class); goto err_cdev; }

    if (IS_ERR(device_create(g_class, NULL, g_devno, NULL, DEVICE_NAME))) {
        printk(KERN_ERR "[chardev] device_create failed\n");
        ret = -ENOMEM;
        goto err_class;
    }

    printk(KERN_INFO "[chardev] Loaded — /dev/%s ready\n", DEVICE_NAME);
    return 0;

err_class: class_destroy(g_class);
err_cdev:  cdev_del(&g_dev.cdev);
err_buf:   kfree(g_dev.buf);
err_chrdev:unregister_chrdev_region(g_devno, NUM_DEVICES);
    return ret;
}

/* ── Module exit ──────────────────────────────────────── */
static void __exit chardev_exit(void)
{
    device_destroy(g_class, g_devno);
    class_destroy(g_class);
    cdev_del(&g_dev.cdev);
    kfree(g_dev.buf);
    unregister_chrdev_region(g_devno, NUM_DEVICES);
    printk(KERN_INFO "[chardev] Unloaded\n");
}

module_init(chardev_init);
module_exit(chardev_exit);
