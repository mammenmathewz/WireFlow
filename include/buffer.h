#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <sys/types.h>

#define BUFFER_SIZE 8192

typedef struct {
    char data[BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} ring_buffer_t;

void buffer_init(ring_buffer_t *buf);
size_t buffer_write(ring_buffer_t *buf, const char *src, size_t len);
size_t buffer_read(ring_buffer_t *buf, char *dest, size_t len);
size_t buffer_available_capacity(const ring_buffer_t *buf);

#endif