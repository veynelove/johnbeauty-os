#include <lib/user_syscall.h>
#include <include/fcntl.h>
#include <include/unistd.h>

#define FILE_TEST_EXIT_OK   42
#define TEST_FILE           "/test.dat"
#define HELLO_FILE          "/hello.elf"

static int check_elf_magic(void)
{
    int32_t fd = jlos_user_open(HELLO_FILE, O_RDONLY, 0);
    if (fd < 0) {
        printf("FAIL: open hello.elf ret=%d\n", fd);
        return 0;
    }
    uint8_t buf[4];
    int32_t n = jlos_user_read(fd, buf, 4);
    jlos_user_close(fd);
    if (n != 4) {
        printf("FAIL: read hello.elf n=%d\n", n);
        return 0;
    }
    if (buf[0] != 0x7F || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') {
        printf("FAIL: elf magic %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3]);
        return 0;
    }
    printf("OK: elf magic %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3]);
    return 1;
}

static int check_write_read(void)
{
    const char *data = "JLOS_FILE_TEST_OK";
    uint32_t len = 17;

    int32_t fd = jlos_user_open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        printf("FAIL: create test.dat ret=%d\n", fd);
        return 0;
    }
    int32_t w = jlos_user_write(fd, data, len);
    jlos_user_close(fd);
    if (w != (int32_t)len) {
        printf("FAIL: write ret=%d\n", w);
        return 0;
    }

    char buf[32];
    fd = jlos_user_open(TEST_FILE, O_RDONLY, 0);
    if (fd < 0) {
        printf("FAIL: reopen test.dat ret=%d\n", fd);
        return 0;
    }
    int32_t n = jlos_user_read(fd, buf, len);
    jlos_user_close(fd);
    if (n != (int32_t)len) {
        printf("FAIL: reread n=%d\n", n);
        return 0;
    }
    for (uint32_t i = 0; i < len; i++) {
        if (buf[i] != data[i]) {
            printf("FAIL: mismatch at %d got %d want %d\n", i, buf[i], data[i]);
            return 0;
        }
    }
    printf("OK: write/read match\n");
    return 1;
}

static int check_lseek(void)
{
    int32_t fd = jlos_user_open(TEST_FILE, O_RDONLY, 0);
    if (fd < 0) {
        printf("FAIL: open for lseek ret=%d\n", fd);
        return 0;
    }
    int32_t pos = jlos_user_lseek(fd, 5, SEEK_SET);
    if (pos != 5) {
        printf("FAIL: lseek ret=%d\n", pos);
        jlos_user_close(fd);
        return 0;
    }
    char buf[3];
    int32_t n = jlos_user_read(fd, buf, 2);
    jlos_user_close(fd);
    if (n != 2) {
        printf("FAIL: lseek read n=%d\n", n);
        return 0;
    }
    if (buf[0] != 'F' || buf[1] != 'I') {
        printf("FAIL: lseek data %d %d\n", buf[0], buf[1]);
        return 0;
    }
    printf("OK: lseek+read\n");
    return 1;
}

static int check_unlink(void)
{
    int32_t ret = jlos_user_unlink(TEST_FILE);
    if (ret != 0) {
        printf("FAIL: unlink ret=%d\n", ret);
        return 0;
    }
    int32_t fd = jlos_user_open(TEST_FILE, O_RDONLY, 0);
    if (fd >= 0) {
        printf("FAIL: open after unlink ret=%d\n", fd);
        jlos_user_close(fd);
        return 0;
    }
    printf("OK: unlink+reopen-fail\n");
    return 1;
}

int main(int argc, char **argv)
{
    printf("file_test: pid=%u argc=%d\n", jlos_user_get_pid(), argc);
    (void)argv;

    if (!check_elf_magic())    { return 1; }
    if (!check_write_read())   { return 1; }
    if (!check_lseek())        { return 1; }
    if (!check_unlink())       { return 1; }

    printf("file_test: ALL PASSED\n");
    return FILE_TEST_EXIT_OK;
}