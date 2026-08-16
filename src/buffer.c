#include <string.h>
#include "buffer.h"

void buffer_init(ring_buffer_t *buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

size_t buffer_available_capacity(const ring_buffer_t *buf) {
    return BUFFER_SIZE - buf->count;
}

size_t buffer_write(ring_buffer_t *buf, const char *src, size_t len) {
    size_t capacity = buffer_available_capacity(buf);
    if (len > capacity) {
        len = capacity; // Write as much as possible without overflowing
    }

    for (size_t i = 0; i < len; i++) {
        buf->data[buf->head] = src[i];
        buf->head = (buf->head + 1) % BUFFER_SIZE;
        buf->count++;
    }

    return len;
}

size_t buffer_read(ring_buffer_t *buf, char *dest, size_t len) {
    if (len > buf->count) {
        len = buf->count; // Read only available data
    }

    for (size_t i = 0; i < len; i++) {
        dest[i] = buf->data[buf->tail];
        buf->tail = (buf->tail + 1) % BUFFER_SIZE;
        buf->count--;
    }

    return len;
}