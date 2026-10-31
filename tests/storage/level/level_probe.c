#include "mc/storage/level/level.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_BYTES (2u * 1024u * 1024u)

static int digit(int value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static void print_hex(const uint8_t *bytes, size_t length) {
    for (size_t i = 0; i < length; ++i)
        printf("%02x", (unsigned int)bytes[i]);
}

static void print_info(const mc_level_info *info) {
    printf("{\"ok\":true,\"seed\":\"%016" PRIx64 "\",\"spawn\":[%" PRId32 ",%" PRId32 ",%" PRId32
           "],\"time\":\"%016" PRIx64 "\",\"last_played\":\"%016" PRIx64
           "\",\"size_on_disk\":\"%016" PRIx64 "\",\"level_name\":\"",
           (uint64_t)info->seed, info->spawn_x, info->spawn_y, info->spawn_z, (uint64_t)info->time,
           (uint64_t)info->last_played, (uint64_t)info->size_on_disk);
    for (size_t i = 0; i < info->level_name.length; ++i)
        printf("%04x", (unsigned int)info->level_name.units[i]);
    printf("\",\"version\":%" PRId32 ",\"rain_time\":%" PRId32
           ",\"raining\":%s,\"thunder_time\":%" PRId32
           ",\"thundering\":%s,\"has_player\":%s,\"player_dimension\":%" PRId32 "}\n",
           info->version, info->rain_time, info->raining ? "true" : "false", info->thunder_time,
           info->thundering ? "true" : "false", info->player == NULL ? "false" : "true",
           info->player_dimension);
}

int main(int argc, char **argv) {
    bool build = argc == 5 && strcmp(argv[1], "build") == 0;
    if (!(argc == 2 && strcmp(argv[1], "decode") == 0) && !build) {
        fputs("usage: mc_level_probe decode | build CLOCK_HEX keep|drop preserve|fresh\n", stderr);
        return 2;
    }
    int64_t clock = 0;
    if (build) {
        if (strlen(argv[2]) != 16 ||
            (strcmp(argv[3], "keep") != 0 && strcmp(argv[3], "drop") != 0) ||
            (strcmp(argv[4], "preserve") != 0 && strcmp(argv[4], "fresh") != 0)) {
            return 2;
        }
        uint64_t bits = 0;
        for (size_t i = 0; i < 16; ++i) {
            int value = digit((unsigned char)argv[2][i]);
            if (value < 0)
                return 2;
            bits = bits * UINT64_C(16) + (uint64_t)value;
        }
        clock = bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
    }
    uint8_t *input = malloc(PROBE_BYTES);
    if (input == NULL)
        return 2;
    size_t length = 0;
    int high;
    while ((high = getchar()) != EOF && high != '\n') {
        int low = getchar();
        high = digit(high);
        low = digit(low);
        if (high < 0 || low < 0 || length == PROBE_BYTES) {
            free(input);
            return 2;
        }
        input[length++] = (uint8_t)(high * 16 + low);
    }
    if (ferror(stdin) || (high == '\n' && getchar() != EOF)) {
        free(input);
        return 2;
    }
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_input_bytes = limits.max_output_bytes = PROBE_BYTES;
    limits.max_allocation_bytes = 32u * 1024u * 1024u;
    mc_nbt_document *source = NULL;
    size_t consumed = 0;
    mc_nbt_status nbt = mc_nbt_read_compound(input, length, &limits, &source, &consumed, NULL);
    free(input);
    if (nbt != MC_NBT_OK || consumed != length) {
        printf("{\"ok\":false,\"nbt_status\":\"%s\"}\n",
               nbt == MC_NBT_OK ? "trailing input" : mc_nbt_status_string(nbt));
        mc_nbt_document_free(source);
        return ferror(stdout) ? 2 : 0;
    }
    const mc_nbt_tag *root = mc_nbt_document_root(source);
    mc_level_info info;
    mc_level_error error;
    mc_level_status status = mc_level_decode(root, &info, &error);
    if (status == MC_LEVEL_OK && !build) {
        print_info(&info);
    } else if (status == MC_LEVEL_OK) {
        if (strcmp(argv[3], "drop") == 0)
            info.player = NULL;
        mc_nbt_document *result = NULL;
        status = mc_level_build(&info, strcmp(argv[4], "preserve") == 0 ? root : NULL, clock,
                                &limits, &result, &error);
        if (status == MC_LEVEL_OK) {
            size_t written = 0;
            nbt = mc_nbt_measure(mc_nbt_document_root(result), &limits, &written, NULL);
            uint8_t *output = nbt == MC_NBT_OK ? malloc(written) : NULL;
            if (output == NULL || mc_nbt_write(mc_nbt_document_root(result), &limits, output,
                                               written, &written, NULL) != MC_NBT_OK) {
                mc_nbt_document_free(result);
                mc_nbt_document_free(source);
                free(output);
                return 2;
            }
            fputs("{\"ok\":true,\"nbt_hex\":\"", stdout);
            print_hex(output, written);
            puts("\"}");
            free(output);
            mc_nbt_document_free(result);
        }
    }
    if (status != MC_LEVEL_OK) {
        printf("{\"ok\":false,\"status\":\"%s\",\"field\":", mc_level_status_string(status));
        if (error.field == NULL)
            fputs("null", stdout);
        else
            printf("\"%s\"", error.field);
        puts("}");
    }
    mc_nbt_document_free(source);
    return ferror(stdout) ? 2 : 0;
}
