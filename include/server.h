#ifndef SERVER_H
#define SERVER_H

#define UPSTREAM_PORT 9000
#define UPSTREAM_HOST "127.0.0.1"

int server_create_socket();
int connect_to_upstream();
void server_run(int server_fd);

#endif