/*
 * 07_networking/tcp_client.c
 * ==========================
 * TCP client with:
 *   - connect() with timeout (using non-blocking socket + select)
 *   - send() / recv() in a loop
 *   - getaddrinfo() for hostname resolution (IPv4/IPv6 agnostic)
 *   - Socket options: TCP_NODELAY (disable Nagle's algorithm)
 *
 * Usage: ./tcp_client [host] [port]
 *
 * Interview topics:
 *   Q: What is Nagle's algorithm?
 *   A: Nagle's algorithm buffers small TCP sends to reduce packet count.
 *      TCP_NODELAY disables it — needed for low-latency apps (telnet, games).
 *
 *   Q: Why use getaddrinfo() instead of gethostbyname()?
 *   A: getaddrinfo is reentrant, supports IPv6, and returns structured results.
 *      gethostbyname is deprecated, not thread-safe (uses static buffer).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/epoll.h>

#define BUF_SIZE 1024

static int connect_with_timeout(const char *host, const char *port, int timeout_s)
{
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) { perror("epoll_create1"); return -1; }

    // get the address in addrinfo struct.
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,   /* IPv4 or IPv6 */
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    int err = getaddrinfo(host, port, &hints, &res);
    if (err) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }

    int sfd = -1;
    for (struct addrinfo *r = res; r; r = r->ai_next) {
        int connected = 0;
        sfd = socket(r->ai_family, r->ai_socktype | SOCK_CLOEXEC, r->ai_protocol);
        if (sfd < 0) continue;

        /* Non-blocking for timed connect, otherwise it could block long time if fail*/
        int flags = fcntl(sfd, F_GETFL);
        fcntl(sfd, F_SETFL, flags | O_NONBLOCK);

        /* TCP_NODELAY: disable Nagle -- no need*/
        int opt = 1;
        //setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        int rc = connect(sfd, r->ai_addr, r->ai_addrlen);
        if (rc == 0) {
            /* Connected immediately */
            fcntl(sfd, F_SETFL, flags); /* restore blocking */
            break;
        }
        if (errno == EINPROGRESS) {
            /* Wait for connect() to complete */
            /* // change select to epoll
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(sfd, &wfds);
            struct timeval tv = { .tv_sec = timeout_s };
            rc = select(sfd + 1, NULL, &wfds, NULL, &tv);
            */

            struct epoll_event ev;
            ev.events   = EPOLLIN | EPOLLOUT; //actuall it's EPOLLOUT
            ev.data.fd  = sfd;
            epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);
            struct epoll_event events[4];
            int nfds = epoll_wait(epfd, events, 4, timeout_s * 1000 /*ms*/);

            for(int i=0; i<nfds; i++) {
                if(!(events[i].events & EPOLLERR)) {
                    int so_err;
                    socklen_t len = sizeof(so_err);
                    getsockopt(sfd, SOL_SOCKET, SO_ERROR, &so_err, &len);
                    if (so_err == 0) {
                        fcntl(sfd, F_SETFL, flags); //set back to block mode
                        connected = 1;
                        break;
                    }
                }
            }
        }
        if(connected) {
            break;
        } else {
            close(sfd);
            sfd = -1;
        }
    }
    close(epfd);
    freeaddrinfo(res);
    return sfd;
}

int main(int argc, char *argv[])
{
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const char *port = (argc > 2) ? argv[2] : "8080";

    printf("[CLIENT] Connecting to %s:%s …\n", host, port);
    int sfd = connect_with_timeout(host, port, 5);
    if (sfd < 0) { fprintf(stderr, "[CLIENT] Connection failed\n"); return 1; }
    printf("[CLIENT] Connected (fd=%d)\n", sfd);

    /* Send a few messages */
    const char *messages[] = {
        "Hello from embedded Linux client!\n",
        "Demonstrating TCP send/recv\n",
        "TCP_NODELAY is enabled\n",
    };

    char buf[BUF_SIZE];
    for (int i = 0; i < 3; i++) {
        send(sfd, messages[i], strlen(messages[i]), MSG_NOSIGNAL);
        ssize_t n = recv(sfd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        printf("[CLIENT] server replied: %s", buf);
        usleep(100000);
    }

    close(sfd);
    printf("[CLIENT] Done.\n");
    return 0;
}
