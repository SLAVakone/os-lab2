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
    struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000L; nanosleep(&ts, NULL);
}
#endif

static const char *TRICKY_ARG = "hello \"big\" world\\";

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc >= 4 && strcmp(argv[1], "--child") == 0) {
        sleep_ms(atoi(argv[3])); return atoi(argv[2]);
    }
    if (argc >= 3 && strcmp(argv[1], "--argcheck") == 0) {
        return strcmp(argv[2], TRICKY_ARG) == 0 ? 0 : 1;
    }

    const char *self = argv[0]; int passed = 0; int total = 5;

    {
        const char *args[] = { self, "--child", "42", "1000", NULL }; pl_process proc;
        if (pl_start(args, &proc) == 0) {
            int polls = 0, code = -1, finished = 0;
            for (int i = 0; i < 30; i++) {
                int res = pl_poll(&proc, &code);
                if (res == 1) { finished = 1; break; } if (res < 0) break;
                polls++; sleep_ms(100);
            }
            if (!finished) pl_wait(&proc, &code);
            if (polls > 3 && code == 42) { printf("[ OK ] Test 1: parent worked while child slept (%d polls, code=42)\n", polls); passed++; }
            else { printf("[FAIL] Test 1: polls=%d, code=%d\n", polls, code); }
        } else { printf("[FAIL] Test 1: pl_start failed\n"); }
    }
    {
        const char *args[] = { self, "--child", "7", "300", NULL }; pl_process proc;
        if (pl_start(args, &proc) == 0) {
            int code = -1; pl_wait(&proc, &code);
            if (code == 7) { printf("[ OK ] Test 2 (code=7)\n"); passed++; }
            else { printf("[FAIL] Test 2: expected 7, got %d\n", code); }
        } else { printf("[FAIL] Test 2: pl_start failed\n"); }
    }
    {
        const char *args[] = { self, "--argcheck", TRICKY_ARG, NULL }; pl_process proc;
        if (pl_start(args, &proc) == 0) {
            int code = -1; pl_wait(&proc, &code);
            if (code == 0) { printf("[ OK ] Test 3: argument passed intact\n"); passed++; }
            else { printf("[FAIL] Test 3: child got a distorted argument (code=%d)\n", code); }
        } else { printf("[FAIL] Test 3: pl_start failed\n"); }
    }
    {
        const char *args[] = { "no_such_program_12345", NULL }; pl_process proc;
        if (pl_start(args, &proc) == -1) { printf("[ OK ] Test 4: nonexistent program returned -1\n"); passed++; }
        else { int code; pl_wait(&proc, &code); printf("[FAIL] Test 4: expected -1, got start success\n"); }
    }
    {
        const char *args[] = { self, "--child", "300", "0", NULL }; pl_process proc;
#ifdef _WIN32
        const int expected = 300;
#else
        const int expected = 44;
#endif
        if (pl_start(args, &proc) == 0) {
            int code = -1; pl_wait(&proc, &code);
            if (code == expected) { printf("[ OK ] Test 5 (code=%d)\n", code); passed++; }
            else { printf("[FAIL] Test 5: expected %d, got %d\n", expected, code); }
        } else { printf("[FAIL] Test 5: pl_start failed\n"); }
    }
    printf("\n%d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
