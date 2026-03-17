#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "server.h"

#define PORT 8080
#define BACKLOG 10
#define BUF_SIZE 4096

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

    printf("[wireflow] listening on port %d\n", PORT);
    return fd;
}

void server_run(int server_fd) {
    char buf[BUF_SIZE];
    int  conn_count = 0;

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) { perror("accept"); continue; }

        conn_count++;
        printf("[wireflow] client connected fd=%d  total=%d\n", client_fd, conn_count);

        ssize_t n;
        while ((n = recv(client_fd, buf, sizeof(buf), 0)) > 0) {
            printf("[wireflow] fd=%d  bytes=%zd\n", client_fd, n);
            send(client_fd, buf, n, 0);
        }

        conn_count--;
        printf("[wireflow] client disconnected fd=%d  total=%d\n", client_fd, conn_count);
        close(client_fd);
    }
}