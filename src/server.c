#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include "server.h"

#define PORT 8080
#define BACKLOG 128
#define BUF_SIZE 4096
#define MAX_EVENTS 64

// Helper function: Set file descriptor to Non-Blocking mode
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

    // Make listening socket non-blocking
    if (set_nonblocking(fd) < 0) { perror("set_nonblocking server_fd"); close(fd); return -1; }

    printf("[wireflow] listening on port %d (non-blocking)\n", PORT);
    return fd;
}

void server_run(int server_fd) {
    // 1. Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        return;
    }

    struct epoll_event ev, events[MAX_EVENTS];

    // 2. Register server_fd for read events (EPOLLIN)
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl: server_fd");
        close(epoll_fd);
        return;
    }

    printf("[wireflow] epoll event loop initialized\n");
    char buf[BUF_SIZE];

    // 3. The Core Event Loop
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue; // Interrupted by signal
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int current_fd = events[i].data.fd;

            if (current_fd == server_fd) {
                // Event on server socket = NEW INCOMING CONNECTION
                while (1) {
                    int client_fd = accept(server_fd, NULL, NULL);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // Handled all pending connections
                            break;
                        }
                        perror("accept");
                        break;
                    }

                    set_nonblocking(client_fd);

                    // Register client_fd with epoll in Edge-Triggered mode (EPOLLET)
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                        perror("epoll_ctl: client_fd");
                        close(client_fd);
                    } else {
                        printf("[wireflow] client connected fd=%d\n", client_fd);
                    }
                }
            } else {
                // Event on client socket = READABLE DATA AVAILABLE
                int disconnect = 0;

                // Read aggressively until buffer empties (required for EPOLLET)
                while (1) {
                    ssize_t n = recv(current_fd, buf, sizeof(buf), 0);
                    if (n > 0) {
                        printf("[wireflow] fd=%d read %zd bytes\n", current_fd, n);
                        send(current_fd, buf, n, 0); // Echo back
                    } else if (n == 0) {
                        // Client closed connection
                        disconnect = 1;
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // Socket buffer empty, wait for next epoll event
                            break;
                        }
                        perror("recv error");
                        disconnect = 1;
                        break;
                    }
                }

                if (disconnect) {
                    printf("[wireflow] client disconnected fd=%d\n", current_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    close(current_fd);
                }
            }
        }
    }

    close(epoll_fd);
}