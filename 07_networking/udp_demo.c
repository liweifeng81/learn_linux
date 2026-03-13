/*
 * 07_networking/udp_demo.c
 * ========================
 * UDP server + client in a single file (fork to create both).
 * Demonstrates:
 *   - recvfrom() / sendto() for connectionless sockets
 *   - "Connected" UDP (connect() on UDP for peering)
 *   - SO_BROADCAST for subnet broadcast
 *   - IP_TTL and IP_MULTICAST_TTL options
 *
 * Interview topics:
 *   Q: When does UDP make sense over TCP?
 *   A: Real-time: VoIP, video streaming, gaming.  DNS, DHCP, TFTP.
 *      Tolerate packet loss but need low latency.
 *
 *   Q: Can UDP recv more data than the datagram?
 *   A: No — excess bytes are SILENTLY DISCARDED (unlike TCP which is a stream).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT    9090
#define BUF_SZ  512
#define N_MSGS  4

int main(void)
{
    printf("=== Embedded Linux Demo: UDP ===\n\n");

    /* Server socket */
    int srv = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in saddr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(PORT),
    };
    bind(srv, (struct sockaddr *)&saddr, sizeof(saddr));
    printf("[SERVER] UDP socket bound to port %d\n", PORT);

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        /* ── CLIENT ──────────────────────────────────────── */
        close(srv);
        usleep(50000); /* let server settle */

        int cfd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        struct sockaddr_in dst = {
            .sin_family      = AF_INET,
            .sin_port        = htons(PORT),
        };
        inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);

        /* "Connect" UDP socket — restricts traffic to this peer */
        connect(cfd, (struct sockaddr *)&dst, sizeof(dst));
        printf("[CLIENT] UDP connected to 127.0.0.1:%d\n", PORT);

        for (int i = 0; i < N_MSGS; i++) {
            char msg[BUF_SZ];
            snprintf(msg, sizeof(msg), "UDP datagram #%d from PID %d", i, getpid());
            send(cfd, msg, strlen(msg) + 1, 0); /* use send() since connected */
            printf("[CLIENT] sent: \"%s\"\n", msg);

            char reply[BUF_SZ];
            ssize_t n = recv(cfd, reply, sizeof(reply) - 1, 0);
            if (n > 0) {
                reply[n] = '\0';
                printf("[CLIENT] got echo: \"%s\"\n", reply);
            }
            usleep(200000);
        }
        close(cfd);
        exit(0);
    } else {
        /* ── SERVER ──────────────────────────────────────── */
        char buf[BUF_SZ];
        struct sockaddr_in peer;
        socklen_t plen;
        for (int i = 0; i < N_MSGS; i++) {
            plen = sizeof(peer);
            ssize_t n = recvfrom(srv, buf, sizeof(buf) - 1, 0,
                                 (struct sockaddr *)&peer, &plen);
            if (n < 0) { perror("recvfrom"); break; }
            buf[n] = '\0';
            char peer_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
            printf("[SERVER] from %s:%d: \"%s\"\n",
                   peer_ip, ntohs(peer.sin_port), buf);
            /* Echo back */
            sendto(srv, buf, n, 0, (struct sockaddr *)&peer, plen);
        }
        waitpid(pid, NULL, 0);
        close(srv);
        printf("\n[DONE] UDP demo complete.\n");
    }
    return 0;
}
