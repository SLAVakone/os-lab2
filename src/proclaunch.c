#define _POSIX_C_SOURCE 200809L
#include "proclaunch.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

static char *build_cmdline(const char *const argv[]) {
    size_t total = 1;
    for (int i = 0; argv[i]; i++) total += 2 * strlen(argv[i]) + 3;
    char *cmd = (char *)malloc(total);
    if (!cmd) return NULL;
    char *p = cmd;
    for (int i = 0; argv[i]; i++) {
        const char *a = argv[i];
        if (i > 0) *p++ = ' ';
        if (*a != '\0' && !strpbrk(a, " \t\n\v\"")) {
            size_t n = strlen(a); memcpy(p, a, n); p += n; continue;
        }
        *p++ = '"';
        for (;;) {
            size_t bs = 0; while (*a == '\\') { a++; bs++; }
            if (*a == '\0') { for (size_t k = 0; k < bs * 2; k++) *p++ = '\\'; break; }
            else if (*a == '"') { for (size_t k = 0; k < bs * 2 + 1; k++) *p++ = '\\'; *p++ = '"'; }
            else { for (size_t k = 0; k < bs; k++) *p++ = '\\'; *p++ = *a; }
            a++;
        }
        *p++ = '"';
    }
    *p = '\0';
    return cmd;
}

int pl_start(const char *const argv[], pl_process *proc) {
    if (!argv || !argv[0] || !proc) return -1;
    char *cmd = build_cmdline(argv);
    if (!cmd) return -1;
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si); ZeroMemory(&pi, sizeof pi);
    si.cb = sizeof si;
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    free(cmd);
    if (!ok) return -1;
    CloseHandle(pi.hThread);
    proc->handle = (intptr_t)pi.hProcess;
    proc->pid = (long)pi.dwProcessId;
    proc->finished = 0; proc->exit_code = 0;
    return 0;
}

static int win_finish(pl_process *proc) {
    HANDLE h = (HANDLE)proc->handle; DWORD code = 0;
    if (!GetExitCodeProcess(h, &code)) { CloseHandle(h); proc->handle = 0; return -1; }
    CloseHandle(h); proc->handle = 0; proc->exit_code = (int)code; proc->finished = 1; return 0;
}

int pl_wait(pl_process *proc, int *exit_code) {
    if (!proc) return -1;
    if (!proc->finished) {
        if (!proc->handle) return -1;
        if (WaitForSingleObject((HANDLE)proc->handle, INFINITE) != WAIT_OBJECT_0) return -1;
        if (win_finish(proc) != 0) return -1;
    }
    if (exit_code) *exit_code = proc->exit_code; return 0;
}

int pl_poll(pl_process *proc, int *exit_code) {
    if (!proc) return -1;
    if (!proc->finished) {
        if (!proc->handle) return -1;
        DWORD r = WaitForSingleObject((HANDLE)proc->handle, 0);
        if (r == WAIT_TIMEOUT) return 0;
        if (r != WAIT_OBJECT_0) return -1;
        if (win_finish(proc) != 0) return -1;
    }
    if (exit_code) *exit_code = proc->exit_code; return 1;
}

#else
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int pl_start(const char *const argv[], pl_process *proc) {
    if (!argv || !argv[0] || !proc) return -1;
    int fds[2];
    if (pipe(fds) != 0) return -1;
    fcntl(fds[0], F_SETFD, FD_CLOEXEC); fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return -1; }
    if (pid == 0) {
        close(fds[0]); execvp(argv[0], (char *const *)argv);
        int e = errno; ssize_t w = write(fds[1], &e, sizeof e); (void)w; _exit(127);
    }
    close(fds[1]); int child_errno = 0; ssize_t n;
    do { n = read(fds[0], &child_errno, sizeof child_errno); } while (n < 0 && errno == EINTR);
    close(fds[0]);
    if (n > 0) { int st; while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {} return -1; }
    proc->handle = 0; proc->pid = (long)pid; proc->finished = 0; proc->exit_code = 0; return 0;
}

static int decode_status(int status) {
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status); return -1;
}

int pl_wait(pl_process *proc, int *exit_code) {
    if (!proc) return -1;
    if (!proc->finished) {
        int status; pid_t r;
        do { r = waitpid((pid_t)proc->pid, &status, 0); } while (r < 0 && errno == EINTR);
        if (r < 0) return -1;
        proc->exit_code = decode_status(status); proc->finished = 1;
    }
    if (exit_code) *exit_code = proc->exit_code; return 0;
}

int pl_poll(pl_process *proc, int *exit_code) {
    if (!proc) return -1;
    if (!proc->finished) {
        int status; pid_t r;
        do { r = waitpid((pid_t)proc->pid, &status, WNOHANG); } while (r < 0 && errno == EINTR);
        if (r < 0) return -1;
        if (r == 0) return 0;
        proc->exit_code = decode_status(status); proc->finished = 1;
    }
    if (exit_code) *exit_code = proc->exit_code; return 1;
}
#endif
