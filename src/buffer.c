#include "buffer.h"
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

size_t buffer_available_space(const ring_buffer_t *buf) {
    return BUFFER_SIZE - buf->count;
}

size_t buffer_write(ring_buffer_t *buf, const char *src, size_t len) {
    size_t capacity = BUFFER_SIZE - buf->count;
    size_t to_write = (len < capacity) ? len : capacity;
    if (to_write == 0) return 0;

    size_t first_chunk = (buf->head + to_write > BUFFER_SIZE) 
                         ? (BUFFER_SIZE - buf->head) 
                         : to_write;
    
    memcpy(&buf->data[buf->head], src, first_chunk);
    if (to_write > first_chunk) {
        memcpy(&buf->data[0], src + first_chunk, to_write - first_chunk);
    }

    buf->head = (buf->head + to_write) % BUFFER_SIZE;
    buf->count += to_write;
    return to_write;
}

ssize_t buffer_drain(ring_buffer_t *buf, int target_fd) {
    size_t total_sent = 0;

    while (buf->count > 0) {
        size_t chunk = (buf->tail + buf->count > BUFFER_SIZE) 
                       ? (BUFFER_SIZE - buf->tail) 
                       : buf->count;

        ssize_t sent = send(target_fd, &buf->data[buf->tail], chunk, MSG_NOSIGNAL);
        if (sent > 0) {
            buf->tail = (buf->tail + sent) % BUFFER_SIZE;
            buf->count -= sent;
            total_sent += sent;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return -1;
        }
    }
    return (ssize_t)total_sent;
}