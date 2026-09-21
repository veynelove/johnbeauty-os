#include <dsa/bitmap.h>

static inline uint32_t popcount32(uint32_t x)
{
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (x * 0x01010101u) >> 24;
}

void jlos_bitmap_init(jlos_bitmap_t *bm, uint32_t *bits, uint32_t count)
{
    bm->bits = bits;
    bm->count = count;
    bm->words = JLOS_EXCEPT_CEIL(count, JLOS_BITMAP_BITS_PER_WORD);
}

void jlos_bitmap_set(jlos_bitmap_t *bm, uint32_t i)
{
    if (i < bm->count) {
        bm->bits[i / JLOS_BITMAP_BITS_PER_WORD] |= (1u << (i % JLOS_BITMAP_BITS_PER_WORD));
    }
}

void jlos_bitmap_clear(jlos_bitmap_t *bm, uint32_t i)
{
    if (i < bm->count) {
        bm->bits[i / JLOS_BITMAP_BITS_PER_WORD] &= ~(1u << (i % JLOS_BITMAP_BITS_PER_WORD));
    }
}

bool jlos_bitmap_test(const jlos_bitmap_t *bm, uint32_t i)
{
    if (i >= bm->count) {
        return false;
    }
    return (bm->bits[i % JLOS_BITMAP_BITS_PER_WORD] & (1u << (i % JLOS_BITMAP_BITS_PER_WORD))) != 0;
}

void jlos_bitmap_set_all(jlos_bitmap_t *bm)
{
    jlos_memset(bm->bits, 0xFF, bm->words * sizeof(uint32_t));
}

void jlos_bitmap_clear_all(jlos_bitmap_t *bm)
{
    jlos_memset(bm->bits, 0, bm->words * sizeof(uint32_t));
}

uint32_t jlos_bitmap_find_first_zero(const jlos_bitmap_t *bm)
{
    for (uint32_t w = 0; w < bm->words; w++) {
        if (bm->bits[w] != 0xFFFFFFFFu) {
            uint32_t inverted = ~bm->bits[w];
            uint32_t bit = w * JLOS_BITMAP_BITS_PER_WORD + __builtin_ctz(inverted);
            if (bit < bm->count) {
                return bit;
            }
        }
    }
    return bm->count;
}

uint32_t jlos_bitmap_find_next_zero(const jlos_bitmap_t *bm, uint32_t start)
{
    if (start >= bm->count) {
        return bm->count;
    }
    uint32_t w = start / JLOS_BITMAP_BITS_PER_WORD;
    uint32_t bit_int_word = start % JLOS_BITMAP_BITS_PER_WORD;
    uint32_t mask = ~((1u << bit_int_word) - 1);
    uint32_t masked = bm->bits[w] & mask;
    if (masked != mask) {
        uint32_t inverted = ~masked;
        uint32_t bit = w * JLOS_BITMAP_BITS_PER_WORD + __builtin_ctz(inverted);
        if (bit < bm->count) {
            return bit;
        }
    }
    for (w++; w < bm->words; w++) {
        if (bm->bits[w] != 0xFFFFFFFFu) {
            uint32_t inverted = ~bm->bits[w];
            uint32_t bit = w * JLOS_BITMAP_BITS_PER_WORD + __builtin_ctz(inverted);
            if (bit < bm->count) {
                return bit;
            }
        }
    }
    return bm->count;
}

uint32_t jlos_bitmap_find_first_set(const jlos_bitmap_t *bm)
{
    for (uint32_t w = 0; w < bm->count; w++) {
        if (bm->bits[w] != 0) {
            uint32_t bit = w * JLOS_BITMAP_BITS_PER_WORD + __builtin_ctz(bm->bits[w]);
            if (bit < bm->count) {
                return bit;
            }
        }
    }
    return bm->count;
}

uint32_t jlos_bitmap_count_set(const jlos_bitmap_t *bm)
{
    uint32_t total = 0;
    for (uint32_t w = 0; w < bm->words; w++) {
        total += jlos_popcount32(bm->bits[w]);
    }
    return total;
}
