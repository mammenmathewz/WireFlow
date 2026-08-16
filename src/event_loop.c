#include "event_loop.h"
#include "buffer.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>

connection_t *fd_to_conn[MAX_FDS] = {NULL};

void modify_epoll_events(int epoll_fd, int fd, uint32_t events) {
    if (fd < 0 || fd >= MAX_FDS) return;
    struct epoll_event ev;
    ev.events = events | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void close_connection(int epoll_fd, connection_t *conn) {
    if (!conn) return;

    if (conn->client_fd >= 0 && conn->client_fd < MAX_FDS) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->client_fd, NULL);
        close(conn->client_fd);
        fd_to_conn[conn->client_fd] = NULL;
    }
    if (conn->upstream_fd >= 0 && conn->upstream_fd < MAX_FDS) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->upstream_fd, NULL);
        close(conn->upstream_fd);
        fd_to_conn[conn->upstream_fd] = NULL;
    }

    free(conn);
}

void update_socket_interest(int epoll_fd, int fd, ring_buffer_t *in_buf, ring_buffer_t *out_buf, int source_read_disabled, int is_connecting_upstream) {
    if (fd < 0 || fd >= MAX_FDS) return;
    uint32_t events = 0;

    if (in_buf->count < BUFFER_SIZE && !source_read_disabled) {
        events |= EPOLLIN;
    }

    if (out_buf->count > 0 || is_connecting_upstream) {
        events |= EPOLLOUT;
    }

    modify_epoll_events(epoll_fd, fd, events);
}

void handle_io(int epoll_fd, int current_fd, uint32_t events) {
    if (current_fd < 0 || current_fd >= MAX_FDS) return;
    connection_t *conn = fd_to_conn[current_fd];
    if (!conn) return;

    int is_client = (current_fd == conn->client_fd);
    int peer_fd = is_client ? conn->upstream_fd : conn->client_fd;

    ring_buffer_t *in_buf  = is_client ? &conn->client_to_upstream : &conn->upstream_to_client;
    ring_buffer_t *out_buf = is_client ? &conn->upstream_to_client : &conn->client_to_upstream;

    // Handle Errors / Hangups
    if (events & (EPOLLERR | EPOLLHUP)) {
        close_connection(epoll_fd, conn);
        return;
    }

    // 1. Handshake completion check
    if (conn->state == CONN_STATE_CONNECTING) {
        if (current_fd == conn->upstream_fd && (events & EPOLLOUT)) {
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(conn->upstream_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
                close_connection(epoll_fd, conn);
                return;
            }
            
            conn->state = CONN_STATE_ESTABLISHED;

            // Flush buffered early client data
            if (conn->client_to_upstream.count > 0) {
                if (buffer_drain(&conn->client_to_upstream, conn->upstream_fd) == -1) {
                    close_connection(epoll_fd, conn);
                    return;
                }
            }
        }
    } 
    // 2. Drain pending outbound data
    else if ((events & EPOLLOUT) && conn->state == CONN_STATE_ESTABLISHED) {
        if (buffer_drain(out_buf, current_fd) == -1) {
            close_connection(epoll_fd, conn);
            return;
        }

        if (out_buf->count < BUFFER_SIZE) {
            if (is_client && conn->upstream_read_disabled) {
                conn->upstream_read_disabled = 0;
            } else if (!is_client && conn->client_read_disabled) {
                conn->client_read_disabled = 0;
            }
        }
    }

    // 3. Read incoming data safely and flush to avoid EPOLLET stalls
    if (events & EPOLLIN) {
        while (1) {
            // Attempt to drain remaining buffer space to target before taking new data
            if (conn->state == CONN_STATE_ESTABLISHED && in_buf->count > 0) {
                if (buffer_drain(in_buf, peer_fd) == -1) {
                    close_connection(epoll_fd, conn);
                    return;
                }
            }

            size_t avail = buffer_available_space(in_buf);
            if (avail == 0) {
                if (is_client) conn->client_read_disabled = 1;
                else conn->upstream_read_disabled = 1;
                break;
            }

            char temp_buf[BUFFER_SIZE];
            size_t read_len = (avail < sizeof(temp_buf)) ? avail : sizeof(temp_buf);

            ssize_t n = recv(current_fd, temp_buf, read_len, 0);
            if (n > 0) {
                buffer_write(in_buf, temp_buf, (size_t)n);

                if (conn->state == CONN_STATE_ESTABLISHED) {
                    if (buffer_drain(in_buf, peer_fd) == -1) {
                        close_connection(epoll_fd, conn);
                        return;
                    }
                }
            } else if (n == 0) {
                close_connection(epoll_fd, conn);
                return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                close_connection(epoll_fd, conn);
                return;
            }
        }
    }

    // 4. Dynamic Epoll re-arm
    int upstream_connecting = (conn->state == CONN_STATE_CONNECTING);
    update_socket_interest(epoll_fd, conn->client_fd, &conn->client_to_upstream, &conn->upstream_to_client, conn->client_read_disabled, 0);
    update_socket_interest(epoll_fd, conn->upstream_fd, &conn->upstream_to_client, &conn->client_to_upstream, conn->upstream_read_disabled, upstream_connecting);
}