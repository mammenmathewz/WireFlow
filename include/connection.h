#ifndef CONNECTION_H
#define CONNECTION_H

#include "buffer.h"

typedef enum {
    CONN_STATE_UNUSED = 0,
    CONN_STATE_CONNECTING,
    CONN_STATE_ESTABLISHED,
    CONN_STATE_CLOSED
} connection_state_t;

typedef struct {
    int client_fd;
    int upstream_fd;
    connection_state_t state;
    ring_buffer_t client_to_upstream;
    ring_buffer_t upstream_to_client;
} connection_t;

void connection_init(connection_t *conn, int client_fd, int upstream_fd);
void connection_close(connection_t *conn);

#endif