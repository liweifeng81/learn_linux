#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

static void separator(const char *t)
{
    printf("\n══════════════════════════════════════════\n  %s\n══════════════════════════════════════════\n", t);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 1: gpiod_line_set_value                           *
 * ─────────────────────────────────────────────────────── */
static void demo_toggle_gpio(void)
{
    separator("GPIO demo1: toggle GPIO 17 (BCM numbering) every second");

    struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        perror("gpiod_chip_open");
        return; 
    }
    struct gpiod_line *line = gpiod_chip_get_line(chip, 17);
    if (!line) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return;
    }
    /* Request the line as output and set initial value to 0 */
    gpiod_line_request_output(line, "my-app", 0);

    for (int i = 0; i < 4; i++) {
        gpiod_line_set_value(line, i % 2); /* toggle value */
        printf("Set GPIO 17 to %d\n", i % 2);
        sleep(1);
    }
    gpiod_line_release(line);
    gpiod_chip_close(chip);
}


/* ─────────────────────────────────────────────────────── *
 * Demo 2: wait for gpio events                           *
 * ─────────────────────────────────────────────────────── */
static int demo_gpio_events(void)
{
    separator("GPIO demo: wait for edges on GPIO 17 (BCM numbering)");

    struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {                          // ← missing check
        perror("gpiod_chip_open");
        return -1;
    }

    struct gpiod_line *line = gpiod_chip_get_line(chip, 17);
    if (!line) {                          // ← check this too
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return -1;
    }
    /* Request edge detection — this is your "ISR registration" */
    struct gpiod_line_request_config cfg = {
        .consumer      = "my-app",
        .request_type  = GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES,
        .flags         = 0,
    };
    gpiod_line_request(line, &cfg, 0);

    /* Block waiting for edge events — like epoll_wait */
    while (1) {
        struct gpiod_line_event event;
        int ret = gpiod_line_event_wait(line, NULL); /* NULL = block forever */
        if (ret == 1) {
            gpiod_line_event_read(line, &event);
            if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
                printf("Rising edge at %ld.%09ld\n",
                       event.ts.tv_sec, event.ts.tv_nsec);
            else
                printf("Falling edge at %ld.%09ld\n",
                       event.ts.tv_sec, event.ts.tv_nsec);
        }
    }

    gpiod_line_release(line);
    gpiod_chip_close(chip);
}

/* ─────────────────────────────────────────────────────── *
 * Demo 3: watchdog demo                                 *
 * ─────────────────────────────────────────────────────── */
void watchdog_demo(){
    if (access("/dev/watchdog", F_OK) != 0) { // R_OK, W_OK, X_OK
        //install softdog module if not present
        int ret = system("modprobe softdog soft_margin=10");
        if (ret != 0) {
            fprintf(stderr, "failed to load softdog module\n");
            return;
        }
    }
    int fd = open("/dev/watchdog", O_WRONLY);
    if (fd == -1) {
        perror("open");
        return;
    }
    printf("Watchdog demo: if you kill the process within 5s, system will reboot after 10s\n");
    for (int i = 0; i < 5; i++) {
        write(fd, "\0", 1); // "kick" the watchdog
        printf("Kicked the watchdog\n");
        sleep(1);
    }
    write(fd, "V", 1); // "V" to disable the watchdog before closing
    close(fd);
}
int main(void)
{
    demo_toggle_gpio();
    watchdog_demo();
    demo_gpio_events();
    return 0;
}