#ifndef _JLOS_DSA_BITMAP_H
#define _JLOS_DSA_BITMAP_H

#include <common/types.h>

#define JLOS_BITMAP_BITS_PER_WORD 32

typedef struct {
    uint32_t *bits;
    uint32_t count;
    uint32_t words;
} jlos_bitmap_t;

void jlos_bitmap_init(jlos_bitmap_t *bm, uint32_t *bits, uint32_t count);
void jlos_bitmap_set(jlos_bitmap_t *bm, uint32_t i);
void jlos_bitmap_clear(jlos_bitmap_t *bm, uint32_t i);

bool jlos_bitmap_test(const jlos_bitmap_t *bm, uint32_t i);

void jlos_bitmap_set_all(jlos_bitmap_t *bm);
void jlos_bitmap_clear_all(jlos_bitmap_t *bm);

uint32_t jlos_bitmap_find_first_zero(const jlos_bitmap_t *bm);
uint32_t jlos_bitmap_find_next_zero(const jlos_bitmap_t *bm, uint32_t start);
uint32_t jlos_bitmap_find_first_set(const jlos_bitmap_t *bm);

uint32_t jlos_bitmap_count_set(const jlos_bitmap_t *bm);

#endif
