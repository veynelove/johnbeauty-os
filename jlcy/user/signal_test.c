#include <lib/syscall.h>

#define SIGNAL_TEST_OK  42

static volatile int g_caught = 0;

static void on_term(int sig)
{
    g_caught = sig;
}

static int test_catch(void)
{
    signal(SIGTERM, on_term);
    int32_t pid = fork();
    if (pid == 0) {
        while (!g_caught) {
            yield();
        }
        printf("signal_test: caught sig=%d\n", g_caught);
        exit(42);
    }
    if (pid < 0) {
        printf("signal_test: fork failed\n");
        return 0;
    }
    kill((uint32_t)pid, SIGTERM);
    int32_t code = 0;
    int32_t r = wait_pid((uint32_t)pid, &code);
    if (r < 0 || code != 42) {
        printf("signal_test: catch FAIL r=%d code=%d\n", r, code);
        return 0;
    }
    return 1;
}

static int test_sigkill(void)
{
    int32_t pid = fork();
    if (pid == 0) {
        for (;;) {
            yield();
        }
    }
    if (pid < 0) {
        printf("signal_test: fork failed\n");
        return 0;
    }
    kill((uint32_t)pid, SIGKILL);
    int32_t code = 0;
    int32_t r = wait_pid((uint32_t)pid, &code);
    if (r < 0 || code != SIGKILL) {
        printf("signal_test: sigkill FAIL r=%d code=%d\n", r, code);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("signal_test: pid=%u\n", get_pid());
    if (!test_catch()) {
        return 1;
    }
    if (!test_sigkill()) {
        return 2;
    }
    printf("signal_test: ALL PASSED\n");
    return SIGNAL_TEST_OK;
}
