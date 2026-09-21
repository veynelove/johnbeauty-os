#ifndef _JLOS_DSA_RINGBUF_H
#define _JLOS_DSA_RINGBUF_H

#include <common/types.h>

typedef struct {
    uint8_t             *buffer;
    uint32_t            size;
    uint32_t            mask;
    volatile uint32_t   in;
    volatile uint32_t   out;
} jlos_ringbuf_t;

void jlos_ringbuf_init(jlos_ringbuf_t *rf, void *buffer, uint32_t size);
void jlos_ringbuf_reset(jlos_ringbuf_t *rf);

uint32_t jlos_ringbuf_count(const jlos_ringbuf_t *rf);
uint32_t jlos_ringbuf_free(const jlos_ringbuf_t *rf);

bool jlos_ringbuf_is_empty(const jlos_ringbuf_t *rf);
bool jlos_ringbuf_is_full(const jlos_ringbuf_t *rf);

uint32_t jlos_ringbuf_write(jlos_ringbuf_t *rf, const void *data, uint32_t len);
uint32_t jlos_ringbuf_read(jlos_ringbuf_t *rf, void *data, uint32_t len);
uint32_t jlos_ringbuf_peek(jlos_ringbuf_t *rf, void *data, uint32_t len);

#endif
