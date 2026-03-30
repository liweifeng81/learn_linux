#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

static void separator(const char *t)
{
    printf("\n══════════════════════════════════════════\n  %s\n══════════════════════════════════════════\n", t);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: uart send and receive                          *
 * ─────────────────────────────────────────────────────── */
static void demo_uart_send_receive(void)
{
    separator("UART demo1: send and receive data");

    const char *uart_dev = "/dev/ttyUSB0";
    int fd = open(uart_dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open uart");
        return; 
    }

    //set baudrate, parity, stop bits, etc. by termios if needed
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return;
    }
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        return;
    }

    const char *msg = "Hello UART\n";
    write(fd, msg, strlen(msg));
    printf("Sent: %s", msg);
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Received: %s", buf);
    } else {
        perror("read uart");
    }
    close(fd);

}


int main(void)
{
    demo_uart_send_receive();
    return 0;
}