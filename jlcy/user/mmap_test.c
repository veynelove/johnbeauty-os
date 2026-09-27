#include <lib/syscall.h>

#define MMAP_TEST_OK 42
#define PAGE_SIZE    4096
#define FIXED_VADDR  0x50000000

static int test_anon_rw(void)
{
    uint32_t len = 2 * PAGE_SIZE;
    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if ((int32_t)p < 0) {
        printf("mmap_test: anon map FAIL ret=%d\n", (int32_t)p);
        return 0;
    }
    if ((uint32_t)p & (PAGE_SIZE - 1)) {
        printf("mmap_test: addr not aligned p=%u\n", (uint32_t)p);
        return 0;
    }

    char *c = (char *)p;
    for (uint32_t i = 0; i < len; i++) {
        c[i] = (char)(i & 0xFF);
    }
    for (uint32_t i = 0; i < len; i++) {
        if (c[i] != (char)(i & 0xFF)) {
            printf("mmap_test: data mismatch off=%u\n", i);
            return 0;
        }
    }

    if (munmap(p, len) < 0) {
        printf("mmap_test: munmap FAIL\n");
        return 0;
    }
    return 1;
}

static int test_multi_fixed(void)
{
    void *a = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    void *b = mmap(NULL, 3 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if ((int32_t)a < 0 || (int32_t)b < 0) {
        printf("mmap_test: multi map FAIL a=%u b=%u\n", (uint32_t)a, (uint32_t)b);
        return 0;
    }

    unsigned char *ca = (unsigned char *)a;
    unsigned char *cb = (unsigned char *)b;
    ca[0] = 0x5A;
    cb[3 * PAGE_SIZE - 1] = 0xA5;
    if (ca[0] != 0x5A || cb[3 * PAGE_SIZE - 1] != 0xA5) {
        printf("mmap_test: multi data FAIL\n");
        return 0;
    }

    void *f = mmap((void *)FIXED_VADDR, PAGE_SIZE, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (f != (void *)FIXED_VADDR) {
        printf("mmap_test: fixed map FAIL f=%u\n", (uint32_t)f);
        return 0;
    }
    unsigned char *cf = (unsigned char *)f;
    cf[0] = 0x33;
    if (cf[0] != 0x33) {
        printf("mmap_test: fixed data FAIL\n");
        return 0;
    }

    munmap(a, PAGE_SIZE);
    munmap(b, 3 * PAGE_SIZE);
    munmap(f, PAGE_SIZE);
    return 1;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("mmap_test: pid=%u\n", get_pid());
    if (!test_anon_rw()) {
        return 1;
    }
    if (!test_multi_fixed()) {
        return 2;
    }
    printf("mmap_test: ALL PASSED\n");
    return MMAP_TEST_OK;
}