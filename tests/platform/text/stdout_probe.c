#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "../../../src/platform/text/stdout.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#endif

#define REQUIRE(c)                                                                                 \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "stdout probe: %s\n", #c);                                             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const char *status_name(mc_text_stdout_status status) {
    switch (status) {
        case MC_TEXT_STDOUT_OK:
            return "OK";
        case MC_TEXT_STDOUT_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case MC_TEXT_STDOUT_NATIVE_FAILURE:
            return "NATIVE_FAILURE";
    }
    return "UNKNOWN";
}

static bool report(mc_text_stdout_status status, const char *flag) {
    return fprintf(stderr, "STATUS\t%s\t%s\n", status_name(status), flag) >= 0 &&
           fflush(stderr) == 0 && !ferror(stderr);
}

#ifndef _WIN32
static bool equal_mask(const sigset_t *left, const sigset_t *right) {
    for (int signal_number = 1; signal_number <= SIGRTMAX; ++signal_number) {
        int a = sigismember(left, signal_number), b = sigismember(right, signal_number);
        if (a < 0 || b < 0 || a != b)
            return false;
    }
    return true;
}
#endif

int main(int argc, char **argv) {
    int result = 1;
    bool io_failed = true;
    mc_text_stdout stream;
#ifndef _WIN32
    bool signal_setup = false, own_pending = false;
    sigset_t old_mask, before_mask, after_mask, pending, selected;
    struct sigaction old_action;
#endif
#ifdef _WIN32
    if (_setmode(_fileno(stderr), _O_BINARY) == -1 || _setmode(_fileno(stdin), _O_BINARY) == -1)
        return 1;
#endif
    if (argc != 2) {
        (void)fputs("stdout probe: expected one known mode\n", stderr);
        goto cleanup;
    }
    const char *mode = argv[1];
    if (strcmp(mode, "null-capture") == 0) {
        REQUIRE(report(mc_text_stdout_capture(NULL), "-"));
        result = 0;
        goto cleanup;
    }
    const bool broken = strcmp(mode, "broken-default") == 0 ||
                        strcmp(mode, "broken-prepending") == 0 ||
                        strcmp(mode, "broken-ignored") == 0;
#ifdef _WIN32
    if (broken && strcmp(mode, "broken-default") != 0) {
        (void)fputs("stdout probe: expected one known mode\n", stderr);
        goto cleanup;
    }
#endif
    const bool known = broken || strcmp(mode, "normal") == 0 || strcmp(mode, "empty") == 0 ||
                       strcmp(mode, "ascii64") == 0 || strcmp(mode, "nul") == 0 ||
                       strcmp(mode, "over64") == 0 || strcmp(mode, "non-ascii") == 0 ||
                       strcmp(mode, "null-stream") == 0 || strcmp(mode, "null-data") == 0 ||
                       strcmp(mode, "null-output") == 0 || strcmp(mode, "closed") == 0;
    if (!known) {
        (void)fputs("stdout probe: expected one known mode\n", stderr);
        goto cleanup;
    }
    REQUIRE(mc_text_stdout_capture(&stream) == MC_TEXT_STDOUT_OK);

    if (broken) {
        /* Parent closes its stdout read endpoint BEFORE releasing this gate.
         * Exactly one byte plus EOF; outer watchdog bounds these two reads. */
        REQUIRE(fgetc(stdin) == 'G' && fgetc(stdin) == EOF && !ferror(stdin));
#ifndef _WIN32
        REQUIRE(sigprocmask(SIG_SETMASK, NULL, &old_mask) == 0);
        REQUIRE(sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 0);
        REQUIRE(sigaction(SIGPIPE, NULL, &old_action) == 0);
        signal_setup = true;
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = strcmp(mode, "broken-ignored") == 0 ? SIG_IGN : SIG_DFL;
        REQUIRE(sigemptyset(&action.sa_mask) == 0 && sigaction(SIGPIPE, &action, NULL) == 0);
        selected = old_mask;
        if (strcmp(mode, "broken-prepending") == 0) {
            REQUIRE(sigaddset(&selected, SIGPIPE) == 0);
            REQUIRE(sigprocmask(SIG_SETMASK, &selected, NULL) == 0);
            REQUIRE(raise(SIGPIPE) == 0);
            own_pending = true;
        } else {
            REQUIRE(sigdelset(&selected, SIGPIPE) == 0);
            REQUIRE(sigprocmask(SIG_SETMASK, &selected, NULL) == 0);
        }
        REQUIRE(sigprocmask(SIG_SETMASK, NULL, &before_mask) == 0);
        REQUIRE(sigpending(&pending) == 0 &&
                sigismember(&pending, SIGPIPE) == (own_pending ? 1 : 0));
#endif
    }
    if (strcmp(mode, "closed") == 0) {
#ifdef _WIN32
        REQUIRE(CloseHandle((HANDLE)stream.handle) != 0);
#else
        REQUIRE(close((int)stream.handle) == 0);
#endif
    }

    static const char ascii64[] =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    static const char nul[] = {'A', '\0', 'B'};
    static const unsigned char high[] = {0x80u};
    char over64[65];
    memset(over64, 'A', sizeof(over64));
    const char *data = "hello";
    size_t length = 5;
    const mc_text_stdout *target = &stream;
    bool *output = &io_failed;
    if (strcmp(mode, "empty") == 0) {
        data = NULL;
        length = 0;
    }
    if (strcmp(mode, "ascii64") == 0) {
        data = ascii64;
        length = sizeof(ascii64) - 1;
    }
    if (strcmp(mode, "nul") == 0) {
        data = nul;
        length = sizeof(nul);
    }
    if (strcmp(mode, "over64") == 0) {
        data = over64;
        length = sizeof(over64);
    }
    if (strcmp(mode, "non-ascii") == 0) {
        data = (const char *)high;
        length = sizeof(high);
    }
    if (strcmp(mode, "null-stream") == 0)
        target = NULL;
    if (strcmp(mode, "null-data") == 0) {
        data = NULL;
        length = 1;
    }
    if (strcmp(mode, "null-output") == 0)
        output = NULL;
    if (broken || strcmp(mode, "closed") == 0)
        io_failed = false;
    mc_text_stdout_status status = mc_text_stdout_println(target, data, length, output);

#ifndef _WIN32
    if (broken) {
        REQUIRE(sigprocmask(SIG_SETMASK, NULL, &after_mask) == 0 &&
                equal_mask(&before_mask, &after_mask));
        REQUIRE(sigpending(&pending) == 0 &&
                sigismember(&pending, SIGPIPE) == (own_pending ? 1 : 0));
        struct sigaction after_action;
        REQUIRE(sigaction(SIGPIPE, NULL, &after_action) == 0 &&
                after_action.sa_handler ==
                    (strcmp(mode, "broken-ignored") == 0 ? SIG_IGN : SIG_DFL));
    }
#endif
    REQUIRE(report(status, output == NULL ? "-" : io_failed ? "1" : "0"));
    result = 0;
cleanup:
#ifndef _WIN32
    if (signal_setup) {
        /* Drain only the SIGPIPE deliberately raised by this child, before
         * restoring its prior disposition/mask. The zero timeout cannot hang. */
        if (own_pending) {
            struct timespec zero = {0, 0};
            sigset_t one;
            if (sigemptyset(&one) != 0 || sigaddset(&one, SIGPIPE) != 0 ||
                sigtimedwait(&one, NULL, &zero) != SIGPIPE)
                result = 1;
        }
        if (sigaction(SIGPIPE, &old_action, NULL) != 0 ||
            sigprocmask(SIG_SETMASK, &old_mask, NULL) != 0)
            result = 1;
    }
#endif
    return result;
}
