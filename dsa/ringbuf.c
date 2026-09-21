#include <dsa/ringbuf.h>

void jlos_ringbuf_init(jlos_ringbuf_t *rf, void *buffer, uint32_t size)
{
    rf->buffer = (uint8_t *)buffer;
    rf->size = size;
    rf->mask = size - 1;
    rf->in = 0;
    rf->out = 0;
}

void jlos_ringbuf_reset(jlos_ringbuf_t *rf)
{
    rf->in = 0;
    rf->out = 0;
}

uint32_t jlos_ringbuf_count(const jlos_ringbuf_t *rf)
{
    return rf->in - rf->out;
}

uint32_t jlos_ringbuf_free(const jlos_ringbuf_t *rf)
{
    return rf->size - jlos_ringbuf_count(rf);
}

bool jlos_ringbuf_is_empty(const jlos_ringbuf_t *rf)
{
    return rf->in == rf->out;
}

bool jlos_ringbuf_is_full(const jlos_ringbuf_t *rf)
{
    return jlos_ringbuf_count(rf) == rf->size;
}

uint32_t jlos_ringbuf_write(jlos_ringbuf_t *rf, const void *data, uint32_t len)
{
    uint32_t free = jlos_ringbuf_free(rf);
    if (len > free) {
        len = free;
    }
    uint32_t off = rf->in & rf->mask;
    uint32_t l = rf->size - off;
    if (l < len) {
        jlos_memcpy(rf->buffer + off, data, l);
        jlos_memcpy(rf->buffer, (const uint8_t *)data + l, len - l);
    } else {
        jlos_memcpy(rf->buffer + off, data, len);
    }
    rf->in += len;
    return len;
}

uint32_t jlos_ringbuf_read(jlos_ringbuf_t *rf, void *data, uint32_t len)
{
    uint32_t count = jlos_ringbuf_count(rf);
    if (len > count) {
        len = count;
    }
    uint32_t off = rf->out & rf->mask;
    uint32_t l = rf->size - off;
    if (l < len) {
        jlos_memcpy(data, rf->buffer + off, l);
        jlos_memcpy((uint8_t *)data + l, rf->buffer, len - l);
    } else {
        jlos_memcpy(data, rf->buffer + off, len);
    }
    rf->out += len;
    return len;
}

uint32_t jlos_ringbuf_peek(jlos_ringbuf_t *rf, void *data, uint32_t len)
{
    uint32_t count = jlos_ringbuf_count(rf);
    if (len > count) {
        len = count;
    }
    uint32_t off = rf->out & rf->mask;
    uint32_t l = rf->size - off;
    if (l < len) {
        jlos_memcpy(data, rf->buffer + off, l);
        jlos_memcpy((uint8_t *)data + l, rf->buffer, len - l);
    } else {
        jlos_memcpy(data, rf->buffer + off, len);
    }
    return len;
}
