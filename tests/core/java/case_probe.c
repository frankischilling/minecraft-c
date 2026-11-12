#include "mc/core/java/format.h"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define BLOCKS 256u
#define MAX_BYTES (1024u * 1024u)
#define MAX_RESULT_UNITS 8192u

typedef struct arena {
    void *blocks[BLOCKS];
    size_t capacity[BLOCKS], cursor, bytes;
    bool failed;
} arena;

/* Each call receives distinct live malloc blocks. Only after a complete
 * iteration has stopped using its result may reset reuse them; old contents
 * are not cleared. No pool storage is an input or output slot. */
static void *allocate(void *context, size_t bytes) {
    arena *a = context;
    if (a->cursor == BLOCKS || bytes > MAX_BYTES) {
        a->failed = true;
        return NULL;
    }
    size_t slot = a->cursor++, needed = bytes == 0 ? 1 : bytes;
    if (needed > a->capacity[slot]) {
        if (a->bytes - a->capacity[slot] > MAX_BYTES - needed) {
            a->failed = true;
            return NULL;
        }
        void *block = malloc(needed);
        if (block == NULL) {
            a->failed = true;
            return NULL;
        }
        free(a->blocks[slot]);
        a->blocks[slot] = block;
        a->bytes = a->bytes - a->capacity[slot] + needed;
        a->capacity[slot] = needed;
    }
    return a->blocks[slot];
}

static void retire(arena *a) {
    for (size_t i = 0; i < BLOCKS; ++i)
        free(a->blocks[i]);
}

static bool hex(const mc_java_format_text *text) {
    static const char digits[] = "0123456789abcdef";
    if (text->length > MAX_RESULT_UNITS || (text->length != 0 && text->units == NULL))
        return false;
    for (size_t i = 0; i < text->length; ++i)
        for (int shift = 12; shift >= 0; shift -= 4)
            if (fputc(digits[(text->units[i] >> shift) & 15], stdout) == EOF)
                return false;
    return true;
}

int main(int argc, char **argv) {
    (void)argv;
    int result = 1;
    arena a = {0};
    uint32_t point = 0, observed = 0, changed = 0;
    size_t maximum = 0;
    mc_java_format_status status = MC_JAVA_FORMAT_OK;
    static const uint16_t format_units[] = {'%', 'S'}, lf[] = {10};
    const mc_java_format_text pattern = {format_units, 2};
    const mc_java_format_options options = {
        {&a, allocate}, MC_JAVA_FORMAT_LOCALE_ENGLISH, {lf, 1}, MAX_RESULT_UNITS};
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) < 0 || _setmode(_fileno(stderr), _O_BINARY) < 0)
        goto cleanup;
#endif
#ifdef SIGPIPE
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        goto cleanup;
#endif
    if (argc != 1 || fputs("STRING_UPPER_EN_V1\n", stdout) == EOF)
        goto cleanup;
    for (point = 0; point <= UINT32_C(0x10ffff); ++point) {
        uint16_t units[2];
        size_t length = 1;
        if (point < UINT32_C(0x10000))
            units[0] = (uint16_t)point;
        else {
            uint32_t supplement = point - UINT32_C(0x10000);
            units[0] = (uint16_t)(UINT32_C(0xd800) + (supplement >> 10));
            units[1] = (uint16_t)(UINT32_C(0xdc00) + (supplement & UINT32_C(0x3ff)));
            length = 2;
        }
        const mc_java_format_text input = {units, length};
        const mc_java_format_text *arguments[] = {&input};
        mc_java_format_text upper = {0};
        /* The previous iteration has emitted everything and retained no
         * borrowed result/input view. New allocations may now reuse slots. */
        a.cursor = 0;
        status = mc_java_format_strings(&options, &pattern, arguments, 1, &upper, NULL);
        if (status != MC_JAVA_FORMAT_OK || a.failed || upper.length > MAX_RESULT_UNITS ||
            (upper.length != 0 && upper.units == NULL))
            goto cleanup;
        ++observed;
        bool equal = upper.length == length;
        for (size_t i = 0; equal && i < length; ++i)
            equal = upper.units[i] == units[i];
        if (!equal) {
            if (printf("MAP\t%" PRIu32 "\t", point) < 0 || !hex(&upper) ||
                fputc('\n', stdout) == EOF)
                goto cleanup;
            ++changed;
            if (upper.length > maximum)
                maximum = upper.length;
        }
    }
    if (observed != UINT32_C(0x110000) ||
        printf("END\t%" PRIu32 "\t%" PRIu32 "\t%zu\n", observed, changed, maximum) < 0 ||
        fflush(stdout) != 0 || ferror(stdout))
        goto cleanup;
    result = 0;
cleanup:
    retire(&a);
    if (result != 0)
        fprintf(stderr, "String uppercase probe failed: point=%" PRIu32 " status=%d\n", point,
                (int)status);
    return result;
}
