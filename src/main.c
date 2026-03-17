#include <stdio.h>
#include "server.h"

int main() {
    printf("[wireflow] starting...\n");

    int server_fd = server_create_socket();
    if (server_fd < 0) return 1;

    server_run(server_fd);
    return 0;
}