#include "proclaunch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep(ms); }
#else
#include <time.h>
static void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
#endif

/* Режим ребёнка: спит N мс и возвращает заданный код */
int child_mode(int exit_code, int sleep_time) {
    sleep_ms(sleep_time);
    return exit_code;
}

int run_test(const char *name, const char *argv[], int expected_code) {
    pl_process proc;
    if (pl_start(argv, &proc) != 0) {
        printf("[FAIL] %s: pl_start failed\n", name);
        return 0;
    }

    int code;
    if (pl_wait(&proc, &code) != 0) {
        printf("[FAIL] %s: pl_wait failed\n", name);
        return 0;
    }

    if (code == expected_code) {
        printf("[ OK ] %s (code=%d)\n", name, code);
        return 1;
    } else {
        printf("[FAIL] %s: expected %d, got %d\n", name, expected_code, code);
        return 0;
    }
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    /* Режим ребёнка */
    if (argc >= 4 && strcmp(argv[1], "--child") == 0) {
        return child_mode(atoi(argv[2]), atoi(argv[3]));
    }

    const char *self = argv[0];
    int passed = 0;
    int total = 5;

    /* Тест 1: Родитель работает, пока ребёнок спит 1с, код 42 */
    {
        const char *args[] = { self, "--child", "42", "1000", NULL };
        pl_process proc;
        if (pl_start(args, &proc) == 0) {
            int polls = 0;
            for (int i = 0; i < 20; i++) {
                int code;
                int res = pl_poll(&proc, &code);
                if (res == 1) break;
                if (res == 0) polls++;
                sleep_ms(100);
            }
            int code;
            pl_wait(&proc, &code);
            if (polls > 3 && code == 42) {
                printf("[ OK ] Test 1: parent worked while child slept (%d polls, code=42)\n", polls);
                passed++;
            } else {
                printf("[FAIL] Test 1: polls=%d, code=%d\n", polls, code);
            }
        } else {
            printf("[FAIL] Test 1: pl_start failed\n");
        }
    }

    /* Тест 2: Код возврата 7 */
    {
        const char *args[] = { self, "--child", "7", "300", NULL };
        if (run_test("Test 2", args, 7)) passed++;
    }

    /* Тест 3: Аргумент с пробелами */
    {
        const char *args[] = { self, "--child", "hello \"big\" world\\", "0", NULL };
        if (run_test("Test 3", args, 0)) passed++;
    }

    /* Тест 4: Несуществующая программа */
    {
        const char *args[] = { "no_such_program_12345", NULL };
        pl_process proc;
        if (pl_start(args, &proc) == -1) {
            printf("[ OK ] Test 4: nonexistent program returned -1\n");
            passed++;
        } else {
            int code;
            pl_wait(&proc, &code);
            printf("[FAIL] Test 4: expected -1, got start success\n");
        }
    }

    /* Тест 5: Код 300 (Linux обрезает до 8 бит = 44) */
    {
        const char *args[] = { self, "--child", "300", "0", NULL };
#ifdef _WIN32
        if (run_test("Test 5", args, 300)) passed++;
#else
        if (run_test("Test 5", args, 44)) passed++;
#endif
    }

    printf("\n%d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
