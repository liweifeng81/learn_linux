/*
 * 07_networking/tcp_server.c
 * ==========================
 * Multi-client TCP server using epoll (event-driven, non-blocking).
 * Demonstrates:
 *   - socket() / bind() / listen() / accept()
 *   - Non-blocking sockets with O_NONBLOCK
 *   - epoll for scalable event notification (Level-Triggered)
 *   - SO_REUSEADDR / SO_REUSEPORT socket options
 *   - Graceful client disconnect handling
 *   - TCP keep-alive option (SO_KEEPALIVE)
 *   - getsockname() / getpeername() — endpoint info
 *
 * Usage:  ./tcp_server [port]   (default 8080)
 *         ./tcp_client [port]   (to test)
 *
 * Interview topics:
 *   Q: What does SO_REUSEADDR do?
 *   A: Allows binding to a port in TIME_WAIT state — essential to avoid
 *      "Address already in use" when restarting a server quickly.
 *
 *   Q: What is the TCP 3-way handshake?
 *   A: SYN → SYN-ACK → ACK. Establishes sequence numbers before data exchange.
 *
 *   Q: What is TCP TIME_WAIT?
 *   A: After close(), the socket stays in TIME_WAIT for 2×MSL (≈60s) to ensure
 *      delayed packets from the old connection don't corrupt a new one.
 *
 *   Q: Difference between TCP and UDP?
 *   A: TCP: reliable, ordered, connection-oriented, flow-control, congestion-control.
 *      UDP: unreliable, unordered, connectionless, lower latency.
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
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_EVENTS  64
#define BUF_SIZE   1024
#define DEFAULT_PORT 8080

static volatile int g_running = 1;

static void sigterm_handler(int s) { (void)s; g_running = 0; }

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int create_server_socket(int port)
{
    int sfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sfd < 0) { perror("socket"); return -1; }

    /* Reuse address immediately after server restart */
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    /* TCP keep-alive: detect dead peers */
    setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons((uint16_t)port),
    };
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(sfd); return -1;
    }
    if (listen(sfd, SOMAXCONN) < 0) {
        perror("listen"); close(sfd); return -1;
    }
    set_nonblocking(sfd);

    printf("[SERVER] Listening on port %d (PID %d)\n", port, getpid());
    printf("[SERVER] Connect with:  nc 127.0.0.1 %d\n", port);
    printf("[SERVER] Stop with:     Ctrl-C or SIGTERM\n\n");
    return sfd;
}

static void handle_new_connection(int epfd, int sfd)
{
    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    int cfd = accept4(sfd, (struct sockaddr *)&peer, &plen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (cfd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept4");
        return;
    }
    char peer_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
    printf("[SERVER] New client: %s:%d  (fd=%d)\n", peer_ip, ntohs(peer.sin_port), cfd);

    struct epoll_event ev = {
        .events  = EPOLLIN | EPOLLRDHUP,
        .data.fd = cfd,
    };
    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
}

static void handle_client(int epfd, int cfd)
{
    char buf[BUF_SIZE];
    ssize_t n = recv(cfd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        /* n==0: client closed; n<0: error */
        printf("[SERVER] Client fd=%d disconnected\n", cfd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
        close(cfd);
        return;
    }
    buf[n] = '\0';
    printf("[SERVER] fd=%d received (%zd bytes): %s", cfd, n, buf);

    /* Echo back with prefix */
    char resp[BUF_SIZE + 16];
    int rlen = snprintf(resp, sizeof(resp), "ECHO: %s", buf);
    send(cfd, resp, rlen, MSG_NOSIGNAL);
}

int main(int argc, char *argv[])
{
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);
    signal(SIGPIPE, SIG_IGN); /* ignore broken pipe */

    int sfd  = create_server_socket(port);
    if (sfd < 0) return 1;

    int epfd = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = sfd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);

    struct epoll_event events[MAX_EVENTS];

    while (g_running) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1000 /*ms*/);
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            if (fd == sfd) {
                handle_new_connection(epfd, sfd);
            } else {
                if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    printf("[SERVER] fd=%d: peer closed / error\n", fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                } else {
                    handle_client(epfd, fd);
                }
            }
        }
    }

    printf("\n[SERVER] Shutting down.\n");
    close(epfd);
    close(sfd);
    return 0;
}
