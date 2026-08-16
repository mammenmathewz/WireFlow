#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include "connection.h"

#define UPSTREAM_PORT 9000
#define UPSTREAM_HOST "127.0.0.1"

void server_init(void);
int server_create_socket(void);
connection_t* connect_to_upstream(int epoll_fd, int client_fd, struct sockaddr_in *upstream_addr);
void server_run(int server_fd);

#endif // SERVER_H