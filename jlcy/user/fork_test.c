#include <lib/syscall.h>

#define FORK_TEST_OK    42
#define N_CHILDREN      10

static int test_fork_wait(void)
{
    int32_t pid = fork();
    if (pid == 0) {
        exit(42);
    }
    if (pid < 0) {
        printf("fork_test: fork failed\n");
        return 0;
    }
    int32_t exit_code = 0;
    int32_t ret = wait_pid((uint32_t)pid, &exit_code);
    if (ret < 0 || exit_code != 42) {
        printf("fork_test: FAIL ret=%d exit=%d\n", ret, exit_code);
        return 0;
    }
    printf("fork_test: fork+wait OK\n");
    return 1;
}

static int test_fork_pressure(void)
{
    int32_t pids[N_CHILDREN];
    for (int i = 0; i < N_CHILDREN; i++) {
        pids[i] = fork();

        if (pids[i] == 0) {
            exit(100 + i);
        }
        if (pids[i] < 0) {
            printf("fork_test: pressure fork %d failed\n", i);
            return 0;
        }
    }
    for (int i = 0; i < N_CHILDREN; i++) {
        int32_t code = 0;
        int32_t r = wait_pid((uint32_t)pids[i], &code);
        if (r < 0 || code != 100 + i) {
            printf("fork_test: pressure %d FAIL r=%d code=%d\n", i, r, code);
            return 0;
        }
    }
    printf("fork_test: 10-child pressure OK\n");
    return 1;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("fork_test: pid=%u\n", get_pid());
    if (!test_fork_wait()) {
        return 1;
    }
    if (!test_fork_pressure()) {
        return 2;
    }
    printf("fork_test: ALL PASSED\n");
    return FORK_TEST_OK;
}
