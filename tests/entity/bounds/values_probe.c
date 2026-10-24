#include "mc/entity/bounds/bounds.h"

#include <emmintrin.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

_Static_assert(sizeof(double) == sizeof(uint64_t), "binary64 storage required");

static bool parse_bits(const char *text, double *out) {
    if (strlen(text) != 16u) {
        return false;
    }
    uint64_t bits = 0;
    for (size_t i = 0; i < 16u; ++i) {
        unsigned int digit;
        if (text[i] >= '0' && text[i] <= '9') {
            digit = (unsigned int)(text[i] - '0');
        } else if (text[i] >= 'a' && text[i] <= 'f') {
            digit = (unsigned int)(text[i] - 'a') + 10u;
        } else {
            return false;
        }
        bits = (bits << 4u) | (uint64_t)digit;
    }
    memcpy(out, &bits, sizeof(bits));
    return true;
}

static uint64_t raw(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool print_box(const mc_entity_bounds *box) {
    return printf("\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64
                  "\t%016" PRIx64,
                  raw(box->min_x), raw(box->min_y), raw(box->min_z), raw(box->max_x),
                  raw(box->max_y), raw(box->max_z)) >= 0;
}

/* Every row starts with a real pool reset and acquisition. The alias mode
 * resets immediately before a pooled transform, forcing that transform to
 * reuse its source's held slot. Both initial and returned holds are released.
 * The reset row adds another native hold before reuse; the client clear row
 * observes the still-held original after the List drops its reference. */
static bool run_row(mc_entity_classes *classes, char **tokens) {
    const char *operation = tokens[1];
    const bool alias = strcmp(tokens[2], "1") == 0;
    if (!alias && strcmp(tokens[2], "0") != 0) {
        return false;
    }
    double values[15];
    for (size_t i = 0; i < 15u; ++i) {
        if (!parse_bits(tokens[i + 3u], &values[i])) {
            return false;
        }
    }
    mc_entity_bounds_error error = {0};
    mc_entity_bounds *initial = NULL, *result = NULL;
    bool result_hold = false, extra_hold = false, success = false;
    const mc_entity_bounds other = {values[6], values[7],  values[8],
                                    values[9], values[10], values[11]};
    uint64_t scalar = 0;
    if (mc_entity_bounds_pool_reset(classes, &error) != MC_ENTITY_OK ||
        mc_entity_bounds_pool_get(classes, values[0], values[1], values[2], values[3], values[4],
                                  values[5], &initial, &error) != MC_ENTITY_OK) {
        goto cleanup;
    }
    result = initial;
    mc_entity_status status = MC_ENTITY_OK;
    if (strcmp(operation, "get") == 0) {
        /* The actual factory above is the operation under observation. */
    } else if (strcmp(operation, "reset") == 0 || strcmp(operation, "clear") == 0) {
        status = mc_entity_bounds_retain(classes, initial);
        if (status == MC_ENTITY_OK) {
            extra_hold = true;
            status = strcmp(operation, "reset") == 0
                         ? mc_entity_bounds_pool_reset(classes, &error)
                         : mc_entity_bounds_pool_clear_client(classes, &error);
        }
        if (status == MC_ENTITY_OK) {
            status =
                mc_entity_bounds_pool_get(classes, other.min_x, other.min_y, other.min_z,
                                          other.max_x, other.max_y, other.max_z, &result, &error);
            result_hold = status == MC_ENTITY_OK;
        }
    } else if (strcmp(operation, "sweep") == 0 || strcmp(operation, "expand") == 0 ||
               strcmp(operation, "shift") == 0 || strcmp(operation, "contract") == 0 ||
               strcmp(operation, "copy") == 0) {
        if (alias) {
            status = mc_entity_bounds_pool_reset(classes, &error);
        }
        if (status == MC_ENTITY_OK) {
            if (strcmp(operation, "sweep") == 0) {
                status = mc_entity_bounds_sweep(classes, initial, values[12], values[13],
                                                values[14], &result, &error);
            } else if (strcmp(operation, "expand") == 0) {
                status = mc_entity_bounds_expand(classes, initial, values[12], values[13],
                                                 values[14], &result, &error);
            } else if (strcmp(operation, "shift") == 0) {
                status = mc_entity_bounds_shift(classes, initial, values[12], values[13],
                                                values[14], &result, &error);
            } else if (strcmp(operation, "contract") == 0) {
                status = mc_entity_bounds_contract(classes, initial, values[12], values[13],
                                                   values[14], &result, &error);
            } else {
                status = mc_entity_bounds_copy(classes, initial, &result, &error);
            }
            result_hold = status == MC_ENTITY_OK;
        }
    } else if (strcmp(operation, "translate") == 0) {
        result = mc_entity_bounds_translate(initial, values[12], values[13], values[14]);
    } else if (strcmp(operation, "assign") == 0) {
        result = mc_entity_bounds_assign(initial, alias ? initial : &other);
    } else if (strcmp(operation, "clip_x") == 0) {
        scalar = raw(mc_entity_bounds_clip_x(initial, alias ? initial : &other, values[12]));
    } else if (strcmp(operation, "clip_y") == 0) {
        scalar = raw(mc_entity_bounds_clip_y(initial, alias ? initial : &other, values[13]));
    } else if (strcmp(operation, "clip_z") == 0) {
        scalar = raw(mc_entity_bounds_clip_z(initial, alias ? initial : &other, values[14]));
    } else if (strcmp(operation, "intersects") == 0) {
        scalar = mc_entity_bounds_intersects(initial, alias ? initial : &other) ? UINT64_C(1)
                                                                                : UINT64_C(0);
    } else {
        goto cleanup;
    }
    if (status != MC_ENTITY_OK) {
        fprintf(stderr, "bounds_values_probe: row %s: status %d, %s PC %u\n", tokens[0],
                (int)status, error.operation == NULL ? "no operation" : error.operation,
                (unsigned int)error.bytecode);
        goto cleanup;
    }
    success = printf("%s\t%s", tokens[0], operation) >= 0 && print_box(result) &&
              print_box(initial) &&
              printf("\t%016" PRIx64 "\t%d\n", scalar, result == initial ? 1 : 0) >= 0;
cleanup:
    if (result_hold && mc_entity_bounds_release(classes, result) != MC_ENTITY_OK) {
        success = false;
    }
    if (extra_hold && mc_entity_bounds_release(classes, initial) != MC_ENTITY_OK) {
        success = false;
    }
    if (initial != NULL && mc_entity_bounds_release(classes, initial) != MC_ENTITY_OK) {
        success = false;
    }
    return success;
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "CLIENT") != 0 && strcmp(argv[1], "SERVER") != 0)) {
        fprintf(stderr, "bounds_values_probe: expected CLIENT or SERVER\n");
        return 1;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fprintf(stderr, "bounds_values_probe: strict SSE environment required\n");
        return 1;
    }
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1) {
        fprintf(stderr, "bounds_values_probe: binary streams unavailable\n");
        return 1;
    }
#endif
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    int exit_code = 1;
    if (mc_java_random_profile_create(MC_ENTITY_DEFAULT_ALLOCATION_BYTES, &profile) !=
            MC_JAVA_RANDOM_PROFILE_OK ||
        mc_entity_classes_create(strcmp(argv[1], "CLIENT") == 0 ? MC_ENTITY_CLIENT
                                                                : MC_ENTITY_SERVER,
                                 profile, NULL, &classes) != MC_ENTITY_OK) {
        fprintf(stderr, "bounds_values_probe: owner creation failed\n");
        goto cleanup;
    }
    char line[1024];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (strchr(line, '\n') == NULL) {
            fprintf(stderr, "bounds_values_probe: unterminated or oversized input\n");
            goto cleanup;
        }
        char *tokens[18];
        size_t count = 0;
        char *cursor = line;
        while (*cursor != '\0') {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
                ++cursor;
            }
            if (*cursor == '\0') {
                break;
            }
            if (count == sizeof(tokens) / sizeof(tokens[0])) {
                fprintf(stderr, "bounds_values_probe: too many input fields\n");
                goto cleanup;
            }
            tokens[count++] = cursor;
            while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' &&
                   *cursor != '\n') {
                ++cursor;
            }
            if (*cursor != '\0') {
                *cursor++ = '\0';
            }
        }
        if (count != sizeof(tokens) / sizeof(tokens[0]) || !run_row(classes, tokens)) {
            fprintf(stderr, "bounds_values_probe: invalid or failed row\n");
            goto cleanup;
        }
    }
    if (ferror(stdin) || fflush(stdout) != 0) {
        fprintf(stderr, "bounds_values_probe: stream failure\n");
        goto cleanup;
    }
    exit_code = 0;
cleanup:
    if (classes != NULL && mc_entity_classes_free(classes) != MC_ENTITY_OK) {
        fprintf(stderr, "bounds_values_probe: classes remain busy\n");
        exit_code = 1;
    }
    mc_java_random_profile_free(profile);
    return exit_code;
}
