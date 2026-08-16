#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stddef.h>
#include <sys/types.h>
#include "connection.h"

size_t buffer_available_space(const ring_buffer_t *buf);
size_t buffer_write(ring_buffer_t *buf, const char *src, size_t len);
ssize_t buffer_drain(ring_buffer_t *buf, int target_fd);

#endif // BUFFER_H