#include <unistd.h>
#include "connection.h"

void connection_init(connection_t *conn, int client_fd, int upstream_fd) {
    conn->client_fd = client_fd;
    conn->upstream_fd = upstream_fd;
    conn->state = CONN_STATE_ESTABLISHED;
    buffer_init(&conn->client_to_upstream);
    buffer_init(&conn->upstream_to_client);
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