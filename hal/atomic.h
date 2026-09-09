#ifndef _JLOS_HAL_ATOMIC_H
#define _JLOS_HAL_ATOMIC_H

#include <common/types.h>

#define JLOS_ATOMIC_INIT(i) {(i)}

typedef struct {
    volatile int counter;
} jlos_atomic_t;

static inline int jlos_atomic_read(const jlos_atomic_t *v)         { return __atomic_load_n(&v->counter, __ATOMIC_SEQ_CST); }
static inline void jlos_atomic_set(jlos_atomic_t *v, int i)        { __atomic_store_n(&v->counter, i, __ATOMIC_SEQ_CST); }

static inline int jlos_atomic_fetch_add(jlos_atomic_t *v, int i)   { return __atomic_fetch_add(&v->counter, i, __ATOMIC_SEQ_CST); }
static inline int jlos_atomic_fetch_sub(jlos_atomic_t *v, int i)   { return __atomic_fetch_sub(&v->counter, i, __ATOMIC_SEQ_CST); }
static inline int jlos_atomic_inc(jlos_atomic_t *v)                { return __atomic_fetch_add(&v->counter, 1, __ATOMIC_SEQ_CST); }
static inline int jlos_atomic_dec(jlos_atomic_t *v)                { return __atomic_fetch_sub(&v->counter, 1, __ATOMIC_SEQ_CST); }

static inline int jlos_atomic_add_return(jlos_atomic_t *v, int i)  { return __atomic_add_fetch(&v->counter, i, __ATOMIC_SEQ_CST); }
static inline int jlos_atomic_inc_return(jlos_atomic_t *v)         { return __atomic_add_fetch(&v->counter, 1, __ATOMIC_SEQ_CST); }
static inline int jlos_atomic_dec_return(jlos_atomic_t *v)         { return __atomic_sub_fetch(&v->counter, 1, __ATOMIC_SEQ_CST); }

static inline bool jlos_atomic_cmpxchg(jlos_atomic_t *v, int *old, int nv)
{
    return __atomic_compare_exchange_n(&v->counter, old, nv, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
static inline int jlos_atomic_xchg(jlos_atomic_t *v, int nv)       { return __atomic_exchange_n(&v->counter, nv, __ATOMIC_SEQ_CST); }

static inline int jlos_atomic_add_unless(jlos_atomic_t *v, int add, int unless)
{
    int old = jlos_atomic_read(v);
    for (;;) {
        if (old == unless) return old;
        if (jlos_atomic_cmpxchg(v, &old, old + add)) return old;
    }
}

#endif
