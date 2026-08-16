#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <stdint.h>
#include "connection.h"

void modify_epoll_events(int epoll_fd, int fd, uint32_t events);
void close_connection(int epoll_fd, connection_t *conn);
void update_socket_interest(int epoll_fd, int fd, ring_buffer_t *in_buf, ring_buffer_t *out_buf, int source_read_disabled, int is_connecting_upstream);
void handle_io(int epoll_fd, int current_fd, uint32_t events);

#endif // EVENT_LOOP_H