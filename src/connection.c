#include "connection.h"
#include <unistd.h>
#include <string.h>

void connection_init(connection_t *conn, int client_fd, int upstream_fd) {
    memset(conn, 0, sizeof(connection_t));
    conn->client_fd = client_fd;
    conn->upstream_fd = upstream_fd;
    conn->state = CONN_STATE_ESTABLISHED;
}

void connection_close(connection_t *conn) {
    if (conn->client_fd >= 0) {
        close(conn->client_fd);
        conn->client_fd = -1;
    }
    if (conn->upstream_fd >= 0) {
        close(conn->upstream_fd);
        conn->upstream_fd = -1;
    }
    conn->state = CONN_STATE_CLOSED;
}