/*
 * Copyright 2010-2012 Paul Chote
 * This file is part of Puoko-nui, which is free software. It is made available
 * to you under the terms of version 3 of the GNU General Public License, as
 * published by the Free Software Foundation. For more information, see LICENSE.
 */

// Platform-specific shims so we can expect the same functions everywhere

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "main.h"

#ifdef _WIN32
    #include <time.h>
    #include "preferences.h"
    #include <windows.h>
#endif

#if defined(__MINGW64_VERSION_MAJOR)
/* XXX why isn't this in sec_api/time_s.h? */
# if defined(_USE_32BIT_TIME_T)
#  define gmtime_s _gmtime32_s
# else
#  define gmtime_s _gmtime64_s
# endif
#endif


#include "platform.h"

// Append a formatted string to another string
// Appends not more than size - strlen(s1) - 1 characters from
// the formatted string, and then adds a terminating `\0'
int strncatf(char *restrict str, size_t size, const char *restrict format, ...)
{
    va_list args;
    size_t len = strlen(str);
    
    va_start(args, format);
    int ret = vsnprintf(str + len, size - len, format, args);
    va_end(args);
    
    return len + ret;
}

TimerTimestamp system_time()
{
#ifdef _WIN32
    SYSTEMTIME st;
    GetSystemTime(&st);

    return (TimerTimestamp) {
        .year = st.wYear,
        .month = st.wMonth,
        .day = st.wDay,
        .hours = st.wHour,
        .minutes = st.wMinute,
        .seconds = st.wSecond,
        .milliseconds = st.wMilliseconds,
        .locked = true,
        .exposure_progress = 0
    };
#else
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);

    struct tm *st = gmtime(&tv.tv_sec);
    return (TimerTimestamp) {
        .year = st->tm_year + 1900,
        .month = st->tm_mon + 1,
        .day = st->tm_mday,
        .hours = st->tm_hour,
        .minutes = st->tm_min,
        .seconds = st->tm_sec,
        .milliseconds = tv.tv_usec / 1000,
        .locked = true,
        .exposure_progress = 0
    };
#endif
}

time_t struct_tm_to_time_t(struct tm *t)
{
#ifdef _WIN64
    return _mkgmtime(t);
#elif defined _WIN32
    return mktime(t);
#else
    return timegm(t);
#endif
}

void normalize_tm(struct tm *t)
{
    time_t b = struct_tm_to_time_t(t);
#ifdef _WIN64
    gmtime_s(t, &b);
#elif defined _WIN32
    *t = *localtime(&b);
#else
    gmtime_r(&b, t);
#endif
}

// Sleep for ms milliseconds
void millisleep(int ms)
{
    #ifdef _WIN32
        Sleep(ms);
    #else
        nanosleep(&(struct timespec){ms / 1000, (ms % 1000)*1e6}, NULL);
    #endif
}

// Cross platform equivalent of realpath()
char *canonicalize_path(const char *path)
{
#ifdef _WIN32
    char path_buf[MAX_PATH], *ptr;
    GetFullPathName(path, MAX_PATH, path_buf, &ptr);

    // Replace all '\' in path with '/'
    char *i;
    while ((i = strstr(path_buf, "\\")))
        *i = '/';
#else
    char path_buf[PATH_MAX];
    realpath(path, path_buf);
#endif
    return strdup(path_buf);
}

// Convert a canonicalized path to a native path on windows
char *platform_path(const char *path)
{
    char *ret = strdup(path);
#ifdef _WIN32
    // Replace all '/' in path with '\'
    char *i;
    while ((i = strstr(ret, "/")))
        *i = '\\';
#endif
    return ret;
}

bool file_exists(const char *path)
{
#ifdef _WIN32
    DWORD a = GetFileAttributes(path);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path, F_OK) == 0;
#endif
}

bool rename_atomically(const char *src, const char *dest, bool overwrite)
{
#ifdef _WIN32
    // Windows seemingly randomly gives sharing violation errors when moving,
    // even when nothing should hold a lock on the file.
    // Instead of moving, we copy and then delete. Failing to delete a temp
    // file is a better fail case than not moving to the final filename.
    if (!CopyFile(src, dest, !overwrite))
        return false;

    // Failing to delete is not a fatal error, so log it and return success
    if (!DeleteFile(src))
        pn_log("Failed to remove intermediate file %s", src);

    return true;
#else
    // File exists
    if (!overwrite && access(dest, F_OK) == 0)
        return false;

    return rename(src, dest) == 0;
#endif
}

bool delete_file(const char *path)
{
#ifdef _WIN32
    return DeleteFile(path);
#else
    return unlink(path) == 0;
#endif
}

// Cross platform equivalent of basename() that doesn't modify the string.
// Assumes '/' as path separator, so only use after canonicalize_path()
char *last_path_component(char *path)
{
    char *str = path;
    char *last = path;
    while (*str != '\0')
    {
        if (*str == '/')
            last = str+1;
        str++;
    }
    return last;
}

// Run a command synchronously, logging output with a given prefix
int run_command(const char *cmd, const char *log_prefix)
{
#ifdef _WIN32
    // Create pipe for stdout/stderr
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    HANDLE stdout_read, stdout_write, stdin_read, stdin_write;
    if (!CreatePipe(&stdout_read, &stdout_write, &saAttr, 0))
    {
        pn_log("Failed to create stdout pipe");
        return 1;
    }

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
    {
        pn_log("Failed to create stdout pipe");
        return 1;
    }

    if (!CreatePipe(&stdin_read, &stdin_write, &saAttr, 0))
    {
        pn_log("Failed to create stdout pipe");
        return 1;
    }

    if (!SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0))
    {
        pn_log("Failed to create stdout pipe");
        return 1;
    }

    // Create process
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = stdout_write;
    si.hStdOutput = stdout_write;
    si.hStdInput = stdin_read;
    si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcess(NULL, (char *)cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        pn_log("Failed to spawn script with errorcode: %d", GetLastError());
        return 1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(stdin_write);

    // Read output until process terminates
    CloseHandle(stdout_write);
    CHAR buffer[1024];
    DWORD bytes_read;

    for (;;)
    {
        if (!ReadFile(stdout_read, buffer, 1023, &bytes_read, NULL))
            break;

        buffer[bytes_read] = '\0';

        // Split log messages on newlines
        char *str = buffer, *end;
        while ((end = strstr(str, "\n")) != NULL)
        {
            char *next = end + 1;
            *end = '\0';
            if (strlen(str) > 0)
                pn_log("%s%s", log_prefix, str);
            str = next;
        }

        if (strlen(str) > 0)
            pn_log("%s%s", log_prefix, str);
    }

    CloseHandle(stdout_read);
    CloseHandle(stdin_read);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);

    return (int)exit_code;
#else
    FILE *process = popen(cmd, "r");
    if (!process)
    {
        pn_log("%sError invoking read process: %s", log_prefix, cmd);
        return 1;
    }

    char buffer[1024];
    while (!feof(process))
    {
        if (fgets(buffer, 1024, process) != NULL)
        {
            // Split log messages on newlines
            char *str = buffer, *end;
            while ((end = strstr(str, "\n")) != NULL)
            {
                char *next = end + 1;
                *end = '\0';
                if (strlen(str) > 0)
                    pn_log("%s%s", log_prefix, str);
                str = next;
            }

            if (strlen(str) > 0)
                pn_log("%s%s", log_prefix, str);
        }
    }

    return pclose(process);
#endif
}

int run_script(const char *script, const char *log_prefix)
{
#if (defined _WIN32)
    char *msys_bash_path = pn_preference_string(MSYS_BASH_PATH);
    char *cwd = getcwd(NULL, 0);
    char *path = canonicalize_path(cwd);
    free(cwd);

    size_t cmd_len = strlen(msys_bash_path) + strlen(path) + strlen(script) + 26;
    char *cmd = malloc(cmd_len*sizeof(char));
    if (!cmd)
    {
        pn_log("Failed to allocate command string. Skipping script.");
        return 1;
    }

    snprintf(cmd, cmd_len, "%s --login -c \"cd \\\"%s\\\" && %s", msys_bash_path, path, script);
    free(path);
    free(msys_bash_path);

    int ret = run_command(cmd, log_prefix);
    free(cmd);
    return ret;
#else
    return run_command(script, log_prefix);
#endif
}
