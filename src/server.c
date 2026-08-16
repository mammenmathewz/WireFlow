#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"
#include "connection.h"

#define PORT 8080
#define BACKLOG 128
#define MAX_EVENTS 64
#define MAX_CONNECTIONS 1024

static connection_t conn_table[MAX_CONNECTIONS];

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int server_create_socket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(PORT),
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, BACKLOG) < 0)                              { perror("listen"); close(fd); return -1; }
    if (set_nonblocking(fd) < 0)                              { perror("set_nonblocking server_fd"); close(fd); return -1; }

    printf("[wireflow] listening on port %d (proxy active)\n", PORT);
    return fd;
}

int connect_to_upstream() {
    int ufd = socket(AF_INET, SOCK_STREAM, 0);
    if (ufd < 0) return -1;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(UPSTREAM_PORT),
    };
    inet_pton(AF_INET, UPSTREAM_HOST, &addr.sin_addr);

    if (connect(ufd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect to upstream failed");
        close(ufd);
        return -1;
    }

    set_nonblocking(ufd);
    return ufd;
}

void server_run(int server_fd) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create1"); return; }

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    printf("[wireflow] proxy event loop initialized\n");

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int current_fd = events[i].data.fd;

            if (current_fd == server_fd) {
                // Accept client
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd < 0) continue;

                set_nonblocking(client_fd);

                // Connect to upstream backend
                int upstream_fd = connect_to_upstream();
                if (upstream_fd < 0) {
                    printf("[wireflow] failed to connect to upstream for client fd=%d\n", client_fd);
                    close(client_fd);
                    continue;
                }

                // Initialize connection mapping
                connection_t *conn = &conn_table[client_fd];
                connection_init(conn, client_fd, upstream_fd);

                // Add both FDs to epoll
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = upstream_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, upstream_fd, &ev);

                printf("[wireflow] mapped client fd=%d <---> upstream fd=%d\n", client_fd, upstream_fd);

            } else {
                // Forwarding logic
                char tmp[4096];
                ssize_t n = recv(current_fd, tmp, sizeof(tmp), 0);

                if (n <= 0) {
                    printf("[wireflow] socket fd=%d disconnected\n", current_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    close(current_fd);
                    continue;
                }

                // Determine target socket (Client -> Upstream OR Upstream -> Client)
                int target_fd = -1;
                for (int c = 0; c < MAX_CONNECTIONS; c++) {
                    if (conn_table[c].client_fd == current_fd) {
                        target_fd = conn_table[c].upstream_fd;
                        break;
                    } else if (conn_table[c].upstream_fd == current_fd) {
                        target_fd = conn_table[c].client_fd;
                        break;
                    }
                }

                if (target_fd > 0) {
                    send(target_fd, tmp, n, 0);
                    printf("[wireflow] forwarded %zd bytes: fd=%d ---> fd=%d\n", n, current_fd, target_fd);
                }
            }
        }
    }

    close(epoll_fd);
}