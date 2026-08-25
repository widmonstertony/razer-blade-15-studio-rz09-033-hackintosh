#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <Security/Security.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

static char g_helper_path[PATH_MAX];
static io_connect_t g_power_root = IO_OBJECT_NULL;
static atomic_ulong g_power_generation = 0;

static void log_line(const char *message) {
    time_t now = time(NULL);
    struct tm local;
    char stamp[32];
    localtime_r(&now, &local);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S %z", &local);
    printf("%s %s\n", stamp, message);
    fflush(stdout);
}

static int resolve_helper_path(void) {
    char executable[PATH_MAX];
    uint32_t size = sizeof(executable);
    if (_NSGetExecutablePath(executable, &size) != 0) return 1;

    char resolved[PATH_MAX];
    if (realpath(executable, resolved) == NULL) return 1;
    char directory_buffer[PATH_MAX];
    if (strlcpy(directory_buffer, resolved, sizeof(directory_buffer)) >= sizeof(directory_buffer)) {
        return 1;
    }
    const char *directory = dirname(directory_buffer);
    if (snprintf(g_helper_path, sizeof(g_helper_path), "%s/DisplayModeNudgeV2", directory) >=
        (int)sizeof(g_helper_path)) {
        return 1;
    }
    return 0;
}

static int verify_helper(void) {
    struct stat info;
    if (lstat(g_helper_path, &info) != 0 || !S_ISREG(info.st_mode) ||
        S_ISLNK(info.st_mode) || info.st_uid != getuid() ||
        (info.st_mode & (S_IWGRP | S_IWOTH)) != 0 || access(g_helper_path, X_OK) != 0) {
        log_line("HELPER_VERIFY_FAIL: ownership, mode, file type, or executability rejected");
        return 1;
    }

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, (const UInt8 *)g_helper_path, strlen(g_helper_path), false);
    if (url == NULL) {
        log_line("HELPER_VERIFY_FAIL: cannot create helper URL");
        return 1;
    }
    SecStaticCodeRef code = NULL;
    OSStatus create_status = SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &code);
    CFRelease(url);
    if (create_status != errSecSuccess || code == NULL) {
        log_line("HELPER_VERIFY_FAIL: cannot create static code object");
        if (code != NULL) CFRelease(code);
        return 1;
    }
    OSStatus verify_status = SecStaticCodeCheckValidity(code, kSecCSCheckAllArchitectures, NULL);
    CFRelease(code);
    if (verify_status != errSecSuccess) {
        log_line("HELPER_VERIFY_FAIL: code signature is invalid");
        return 1;
    }
    return 0;
}

static int run_helper(const char *action) {
    if (verify_helper() != 0) return 70;
    char *const arguments[] = { g_helper_path, (char *)action, NULL };
    pid_t child = 0;
    int spawn_result = posix_spawn(&child, g_helper_path, NULL, NULL, arguments, environ);
    if (spawn_result != 0) {
        log_line("HELPER_SPAWN_FAIL");
        return 71;
    }
    int status = 0;
    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) {
            log_line("HELPER_WAIT_FAIL");
            return 72;
        }
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 73;
}

struct wake_job {
    unsigned long generation;
};

static void *wake_worker(void *opaque) {
    struct wake_job *job = opaque;
    unsigned long generation = job->generation;
    free(job);

    sleep(8);
    if (atomic_load(&g_power_generation) != generation) {
        log_line("WAKE_RESCUE_CANCEL: a newer power transition superseded this wake");
        return NULL;
    }

    for (int attempt = 1; attempt <= 6; attempt++) {
        char message[128];
        snprintf(message, sizeof(message), "WAKE_RESCUE_ATTEMPT=%d", attempt);
        log_line(message);
        int result = run_helper("--nudge");
        if (result == 0) {
            log_line("WAKE_RESCUE_PASS: exact display transaction completed");
            return NULL;
        }
        snprintf(message, sizeof(message), "WAKE_RESCUE_HELPER_EXIT=%d", result);
        log_line(message);
        if (result == 2 || result == 3 || result >= 70) {
            log_line("WAKE_RESCUE_STOP: protected restore or helper identity failure");
            return NULL;
        }
        sleep(2);
        if (atomic_load(&g_power_generation) != generation) {
            log_line("WAKE_RESCUE_CANCEL: power state changed during retry window");
            return NULL;
        }
    }
    log_line("WAKE_RESCUE_FAIL: six protected attempts exhausted");
    return NULL;
}

static void power_callback(void *refcon, io_service_t service, natural_t message_type,
                           void *message_argument) {
    (void)refcon;
    (void)service;
    switch (message_type) {
        case kIOMessageCanSystemSleep:
        case kIOMessageSystemWillSleep:
            atomic_fetch_add(&g_power_generation, 1);
            IOAllowPowerChange(g_power_root, (long)message_argument);
            break;
        case kIOMessageSystemHasPoweredOn: {
            unsigned long generation = atomic_fetch_add(&g_power_generation, 1) + 1;
            log_line("POWER_EVENT: SystemHasPoweredOn; scheduling protected rescue in 8 seconds");
            struct wake_job *job = calloc(1, sizeof(*job));
            if (job == NULL) {
                log_line("WAKE_RESCUE_FAIL: cannot allocate wake job");
                break;
            }
            job->generation = generation;
            pthread_t thread;
            if (pthread_create(&thread, NULL, wake_worker, job) != 0) {
                free(job);
                log_line("WAKE_RESCUE_FAIL: cannot create wake worker");
                break;
            }
            pthread_detach(thread);
            break;
        }
        default:
            break;
    }
}

static int run_agent(void) {
    IONotificationPortRef notification_port = NULL;
    io_object_t notifier = IO_OBJECT_NULL;
    g_power_root = IORegisterForSystemPower(NULL, &notification_port, power_callback, &notifier);
    if (g_power_root == IO_OBJECT_NULL || notification_port == NULL) {
        log_line("AGENT_START_FAIL: IORegisterForSystemPower failed");
        return 1;
    }

    CFRunLoopSourceRef source = IONotificationPortGetRunLoopSource(notification_port);
    if (source == NULL) {
        log_line("AGENT_START_FAIL: no IOKit run-loop source");
        IODeregisterForSystemPower(&notifier);
        IOServiceClose(g_power_root);
        IONotificationPortDestroy(notification_port);
        return 1;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    log_line("AGENT_READY: event-driven OLED wake rescue registered; idle until real wake");
    CFRunLoopRun();

    IODeregisterForSystemPower(&notifier);
    IOServiceClose(g_power_root);
    IONotificationPortDestroy(notification_port);
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (resolve_helper_path() != 0) {
        fprintf(stderr, "ERROR: cannot resolve sibling DisplayModeNudgeV2\n");
        return 64;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: OLEDWakeRescueAgent --probe|--test-once|--run\n");
        return 64;
    }
    if (strcmp(argv[1], "--probe") == 0) {
        int result = run_helper("--probe");
        if (result == 0) log_line("AGENT_PROBE_PASS");
        return result;
    }
    if (strcmp(argv[1], "--test-once") == 0) {
        int result = run_helper("--nudge");
        if (result == 0) log_line("AGENT_TEST_ONCE_PASS");
        return result;
    }
    if (strcmp(argv[1], "--run") == 0) return run_agent();
    fprintf(stderr, "usage: OLEDWakeRescueAgent --probe|--test-once|--run\n");
    return 64;
}
