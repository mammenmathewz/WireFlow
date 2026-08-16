#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "server.h"
#include "connection.h"
#include "event_loop.h"

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void server_init(void) {
    signal(SIGPIPE, SIG_IGN);
}

int server_create_socket(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(8080),
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, 128) < 0) { perror("listen"); close(fd); return -1; }

    set_nonblocking(fd);

    printf("[wireflow] listening on :8080\n");
    return fd;
}

connection_t *connect_to_upstream(int epoll_fd, int client_fd, struct sockaddr_in *upstream_addr) {
    int upstream_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (upstream_fd < 0) return NULL;

    set_nonblocking(upstream_fd);

    int opt = 1;
    setsockopt(upstream_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(upstream_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    connection_t *conn = calloc(1, sizeof(connection_t));
    if (!conn) { close(upstream_fd); return NULL; }

    conn->client_fd   = client_fd;
    conn->upstream_fd = upstream_fd;
    conn->state       = CONN_STATE_CONNECTING;

    fd_to_conn[client_fd]   = conn;
    fd_to_conn[upstream_fd] = conn;

    int res = connect(upstream_fd, (struct sockaddr *)upstream_addr, sizeof(*upstream_addr));
    if (res < 0 && errno != EINPROGRESS) {
        close_connection(epoll_fd, conn);
        return NULL;
    }

    struct epoll_event ev;

    ev.events  = EPOLLIN | EPOLLET;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

    ev.events  = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = upstream_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, upstream_fd, &ev);

    if (res == 0)
        conn->state = CONN_STATE_ESTABLISHED;

    return conn;
}

void server_run(int server_fd) {
    server_init();

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) { perror("epoll_create1"); return; }

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    struct sockaddr_in upstream_addr = {
        .sin_family = AF_INET,
        .sin_port   = htons(UPSTREAM_PORT),
    };
    inet_pton(AF_INET, UPSTREAM_HOST, &upstream_addr.sin_addr);

    struct epoll_event events[64];
    printf("[wireflow] event loop running\n");

    while (1) {
        int n = epoll_wait(epoll_fd, events, 64, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                while (1) {
                    int client_fd = accept(server_fd, NULL, NULL);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }

                    set_nonblocking(client_fd);

                    int nodelay = 1;
                    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

                    connection_t *conn = connect_to_upstream(epoll_fd, client_fd, &upstream_addr);
                    if (!conn) { close(client_fd); continue; }

                    printf("[wireflow] connected client_fd=%d upstream_fd=%d\n",
                           conn->client_fd, conn->upstream_fd);
                }
            } else {
                handle_io(epoll_fd, fd, events[i].events);
            }
        }
    }

    close(epoll_fd);
}