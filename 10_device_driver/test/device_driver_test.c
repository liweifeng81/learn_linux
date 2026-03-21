#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE "/dev/mychardev0"

#define CHARDEV_MAGIC  'C'
#define CHARDEV_RESET  _IO(CHARDEV_MAGIC,  0)  /* flush buffer          */
#define CHARDEV_GSIZE  _IOR(CHARDEV_MAGIC, 1, int) /* get bytes in buffer */
#define CHARDEV_SECHO  _IOW(CHARDEV_MAGIC, 2, int) /* set echo mode       */

int main()
{
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    /* 1. write some data */
    write(fd, "hello", 5);

    /* 2. get size */
    int size;
    ioctl(fd, CHARDEV_GSIZE, &size);
    printf("buf_len = %d\n", size);
    // 2.1 read back the data
    char buf[16];
    ssize_t n = read(fd, buf, sizeof(buf)); /* read should return 0 (EOF) */
    buf[n] = '\0';
    printf("read returned size %d, content: \"%s\"\n", (int)n, buf);
    
    //2.2 write again before set
    write(fd, "h", 1);

    /* 3. set echo = 0 */
    int val = 0;
    ioctl(fd, CHARDEV_SECHO, &val);

    /* 4. reset */
    ioctl(fd, CHARDEV_RESET);

    /* 5. check size again */
    ioctl(fd, CHARDEV_GSIZE, &size);
    printf("after reset buf_len = %d\n", size);

    close(fd);
    return 0;
}