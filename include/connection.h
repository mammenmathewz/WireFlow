#ifndef CONNECTION_H
#define CONNECTION_H

#include <stddef.h>
#include <sys/types.h>

#define MAX_FDS 65536
#define BUFFER_SIZE 8192

typedef enum {
    CONN_STATE_CONNECTING,
    CONN_STATE_ESTABLISHED,
    CONN_STATE_CLOSED
} conn_state_t;

typedef struct {
    char data[BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} ring_buffer_t;

typedef struct {
    int client_fd;
    int upstream_fd;
    conn_state_t state;
    ring_buffer_t client_to_upstream;
    ring_buffer_t upstream_to_client;
    int client_read_disabled;
    int upstream_read_disabled;
} connection_t;

extern connection_t *fd_to_conn[MAX_FDS];

void connection_init(connection_t *conn, int client_fd, int upstream_fd);
void connection_close(connection_t *conn);

#endif // CONNECTION_H