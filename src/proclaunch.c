#include "proclaunch.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#endif

#ifdef _WIN32

static char *build_cmdline(const char *const argv[]) {
    size_t total = 1;
    for (int i = 0; argv[i]; i++) {
        total += strlen(argv[i]) + 3;
    }
    char *cmd = (char *)malloc(total);
    if (!cmd) return NULL;
    cmd[0] = '\0';
    for (int i = 0; argv[i]; i++) {
        if (i > 0) strcat(cmd, " ");
        strcat(cmd, "\"");
        strcat(cmd, argv[i]);
        strcat(cmd, "\"");
    }
    return cmd;
}

int pl_start(const char *const argv[], pl_process *proc) {
    char *cmd = build_cmdline(argv);
    if (!cmd) return -1;

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmd);
        return -1;
    }

    free(cmd);
    CloseHandle(pi.hThread);
    proc->handle = (intptr_t)pi.hProcess;
    proc->pid = (long)pi.dwProcessId;
    return 0;
}

int pl_wait(pl_process *proc, int *exit_code) {
    HANDLE h = (HANDLE)proc->handle;
    WaitForSingleObject(h, INFINITE);
    DWORD code;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    *exit_code = (int)code;
    proc->handle = 0;
    return 0;
}
int pl_poll(pl_process *proc, int *exit_code) {
    HANDLE h = (HANDLE)proc->handle;
    DWORD result = WaitForSingleObject(h, 0);
    if (result == WAIT_TIMEOUT) return 0;
    if (result != WAIT_OBJECT_0) return -1;

    DWORD code;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);

    *exit_code = (int)code;
    proc->handle = 0;
    return 1;
}

#else

int pl_start(const char *const argv[], pl_process *proc) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);

    }
    proc->pid = pid;
    proc->handle = 0;
    return 0;
}

int pl_wait(pl_process *proc, int *exit_code) {
    int status;
    while (waitpid(proc->pid, &status, 0) == -1) {
    return 0;
}

int pl_poll(pl_process *proc, int *exit_code) {
    if (result < 0) return -1;
    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        *exit_code = 128 + WTERMSIG(status);
    } else {
        *exit_code = -1;

#endif    }
    return 1;
}
    int status;
    pid_t result = waitpid(proc->pid, &status, WNOHANG);
    if (result == 0) return 0;
        if (errno != EINTR) return -1;
        *exit_code = -1;
    }
    } else if (WIFSIGNALED(status)) {
        *exit_code = 128 + WTERMSIG(status);
    } else {
    }
    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);

