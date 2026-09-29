#include <lib/syscall.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    timespec_t before;
    clock_gettime(CLOCK_MONOTONIC, &before);

    timespec_t req;
    req.tv_sec = 0;
    req.tv_nsec = 100000000;
    int32_t ret = nanosleep(&req);

    timespec_t after;
    clock_gettime(CLOCK_MONOTONIC, &after);

    if (ret != 0) {
        return 1;
    }

    uint64_t elapsed = (uint64_t)(after.tv_sec - before.tv_sec) * 1000000000ULL
                     + (uint64_t)after.tv_nsec - (uint64_t)before.tv_nsec;
    if (elapsed < 50000000) {
        return 2;
    }

    return 42;
}