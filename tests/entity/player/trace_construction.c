/* Authored CPU transport; no entropy is supplied to the real constructor. */
#define main mc_player_native_unit_main
#include "test_construction.c"
#undef main
#include "../../../src/world/runtime/runtime_internal.h"
#include <inttypes.h>
static FILE *output;
static bool first;

static FILE *open_file(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *file = NULL;
    if (fopen_s(&file, path, mode) != 0)
        return NULL;
    return file;
#else
    return fopen(path, mode);
#endif
}

static bool finish_draw(uint64_t bits, size_t digits, uint64_t words[32], size_t *count) {
    if (digits != 16 || *count == 32 || bits >= UINT64_C(0x3ff0000000000000))
        return false;
    words[(*count)++] = bits;
    return true;
}

static bool read_draw_tape(const char *path, uint64_t words[32]) {
    FILE *file = open_file(path, "rb");
    if (file == NULL)
        return false;
    size_t count = 0, digits = 0, bytes = 0;
    uint64_t bits = 0;
    bool valid = true;
    for (;;) {
        const int byte = fgetc(file);
        if (byte == EOF) {
            if (ferror(file))
                valid = false;
            break;
        }
        if (++bytes > 4096) {
            valid = false;
            break;
        }
        if (byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n') {
            if (digits != 0) {
                if (!finish_draw(bits, digits, words, &count)) {
                    valid = false;
                    break;
                }
                bits = 0;
                digits = 0;
            }
            continue;
        }
        const int digit = byte >= '0' && byte <= '9'   ? byte - '0'
                          : byte >= 'a' && byte <= 'f' ? byte - 'a' + 10
                          : byte >= 'A' && byte <= 'F' ? byte - 'A' + 10
                                                       : -1;
        if (digit < 0 || digits == 16) {
            valid = false;
            break;
        }
        bits = (bits << 4) | (uint64_t)(unsigned int)digit;
        ++digits;
    }
    if (valid && digits != 0)
        valid = finish_draw(bits, digits, words, &count);
    if (fclose(file) != 0)
        valid = false;
    return valid && count == 32;
}

static void key(const char *name) {
    fprintf(output, "%s\"%s\":", first ? "" : ",", name);
    first = false;
}

static void emit_i(const char *name, int32_t value) {
    key(name);
    fprintf(output, "%" PRId32, value);
}

static void emit_z(const char *name, bool value) {
    key(name);
    fputs(value ? "true" : "false", output);
}

static void emit_r(const char *name, const void *value) {
    key(name);
    fputs(value == NULL ? "null" : "{\"nonnull\":true}", output);
}

static void emit_s(const char *name, const mc_player_string *value) {
    key(name);
    if (value == NULL) {
        fputs("null", output);
        return;
    }
    mc_player_string_view text;
    if (!mc_player_string_read(value, &text))
        exit(3);
    fputs("{\"utf16\":[", output);
    for (size_t i = 0; i < text.length; ++i)
        fprintf(output, "%s%u", i ? "," : "", (unsigned)text.units[i]);
    fputs("]}", output);
}

static void emit_f(const char *name, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    key(name);
    fprintf(output, "{\"F\":\"%08" PRIx32 "\"}", bits);
}

static void emit_d(const char *name, double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    key(name);
    fprintf(output, "{\"D\":\"%016" PRIx64 "\"}", bits);
}

static bool run_case(fixture *f, size_t index, const int32_t input[6], const double draws[4]) {
    static const uint16_t standard[] = {'C', 'o', 'n', 's', 't', 'r', 'u', 'c',
                                        't', 'o', 'r', 'P', 'r', 'o', 'b', 'e'};
    static const uint16_t unicode[] = {'A', 0, 937, 55296, 'Z'};
    static const uint16_t shared[] = {'S', 'h', 'a', 'r', 'e', 'd', 'N', 'a', 'm', 'e'};
    mc_player_string *name = NULL, *id = NULL;
    mc_player_session *session = NULL;
    mc_entity *entity = NULL;
    const int32_t which = input[4];
    if (which != 2 && which != 5) {
        const uint16_t *text = which == 3 ? unicode : which == 4 ? shared : standard;
        size_t length = which == 1 ? 0 : which == 3 ? 5 : which == 4 ? 10 : 16;
        CHECK(mc_player_string_create(text, length, &name) == MC_PLAYER_OK);
    }
    CHECK(mc_player_string_create(NULL, 0, &id) == MC_PLAYER_OK);
    if (which != 5)
        CHECK(mc_player_session_create(f->sessions, name, id, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(f->world), input[0], input[1], input[2]) ==
          MC_WORLD_INFO_OK);
    /* An explicit oracle input writes the real source remote cell before use. */
    f->world->fields.B = input[3] == 1;
    CHECK(mc_local_player_allocate(f->entities, &entity, NULL) == MC_ENTITY_OK);
    mc_player_error error;
    const mc_player_status status = mc_local_player_construct(
        entity, input[3] == 2 ? NULL : f->world, NULL, session, input[5], NULL, &error);
    mc_local_player_snapshot state;
    CHECK(mc_local_player_get_snapshot(entity, &state) == MC_ENTITY_OK);
    fprintf(output, "{\"case\":%zu,\"status\":%d,\"failure_pc\":%u,\"fields\":{", index,
            (int)status, error.site.pc);
    first = true;
#include "trace_fields.inc"
    fputs("},\"formula_replay\":{", output);
    first = true;
    emit_f("ls.aj", mc_local_player_aj_from_draw(draws[0]));
    emit_f("ls.G", mc_local_player_g_from_draw(draws[1]));
    emit_f("ls.F", mc_local_player_f_from_draw(draws[2]));
    emit_f("initial_yaw", mc_local_player_yaw_from_draw(draws[3]));
    fputs("},\"bounds\":{", output);
    first = true;
    emit_d("eq.a", entity->bounds->min_x);
    emit_d("eq.b", entity->bounds->min_y);
    emit_d("eq.c", entity->bounds->min_z);
    emit_d("eq.d", entity->bounds->max_x);
    emit_d("eq.e", entity->bounds->max_y);
    emit_d("eq.f", entity->bounds->max_z);
    fprintf(output,
            "},\"aliases\":{\"containers_same\":%s,\"same_username\":%s,\"three_distinct_filters\":"
            "%s},\"slots\":[",
            state.player.d == state.player.e ? "true" : "false",
            session != NULL && state.player.l == name ? "true" : "false",
            state.filters[0] && state.filters[0] != state.filters[1] &&
                    state.filters[0] != state.filters[2] && state.filters[1] != state.filters[2]
                ? "true"
                : "false");
    if (state.player.d != NULL)
        for (int32_t i = 0; i < 45; ++i) {
            mc_container_slot_state slot;
            CHECK(mc_container_slot_read(state.player.d, i, &slot, NULL) == MC_INVENTORY_OK);
            const char *kind = slot.kind == MC_SLOT_CRAFTING ? "yv"
                               : slot.kind == MC_SLOT_ARMOR  ? "pv"
                                                             : "gp";
            const char *inventory = slot.inventory_kind == MC_SLOT_INVENTORY_PLAYER     ? "player"
                                    : slot.inventory_kind == MC_SLOT_INVENTORY_CRAFTING ? "crafting"
                                                                                        : "result";
            fprintf(output,
                    "%s{\"class\":\"%s\",\"index\":%" PRId32 ",\"inventory_index\":%" PRId32
                    ",\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"inventory\":\"%s\"",
                    i ? "," : "", kind, slot.container_index, slot.inventory_index, slot.x, slot.y,
                    inventory);
            if (slot.kind == MC_SLOT_ARMOR)
                fprintf(output, ",\"armor_type\":%" PRId32 ",\"same_container\":%s",
                        slot.armor_type, slot.armor_container == state.player.d ? "true" : "false");
            if (slot.kind == MC_SLOT_CRAFTING)
                fprintf(output, ",\"same_player\":%s,\"same_grid\":%s",
                        slot.player == entity ? "true" : "false",
                        slot.crafting_input != NULL ? "true" : "false");
            fputc('}', output);
        }
    fputs("]}\n", output);
    mc_player_session_retire(&session);
    mc_player_string_release(name);
    mc_player_string_release(id);
    mc_entity_release(entity);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fputs("Player probe input: expected resources world draw-tape output\n", stderr);
        return 2;
    }
    uint64_t words[32];
    if (!read_draw_tape(argv[3], words)) {
        fputs("Player probe input: invalid draw tape\n", stderr);
        return 2;
    }
    if (!mc_math_trig_init(&trig)) {
        fputs("Player probe failure: numerical profile\n", stderr);
        return 1;
    }
    output = open_file(argv[4], "wb");
    if (output == NULL) {
        fputs("Player probe output: cannot open output\n", stderr);
        return 3;
    }
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]))
        return 1;
    static const int32_t cases[8][6] = {{11, 65, -7, 0, 0, 0},
                                        {0, 0, 0, 0, 1, 0},
                                        {-31, -1, 48, 0, 2, -1},
                                        {INT32_MAX, INT32_MAX, INT32_MIN, 0, 3, INT32_MAX},
                                        {INT32_MIN, INT32_MIN, INT32_MAX, 1, 4, INT32_MIN},
                                        {0, 127, 0, 1, 0, 7},
                                        {4, 64, 8, 0, 5, 2},
                                        {0, 0, 0, 2, 0, 3}};
    for (size_t i = 0; i < 8; ++i) {
        double draws[4];
        for (size_t j = 0; j < 4; ++j)
            memcpy(&draws[j], &words[i * 4 + j], sizeof(words[0]));
        if (!run_case(&f, i, cases[i], draws))
            return 1;
    }
    bool written = fflush(output) == 0 && ferror(output) == 0;
    if (fclose(output) != 0)
        written = false;
    output = NULL;
    const bool retired = teardown(&f);
    if (!written) {
        fputs("Player probe output: write failed\n", stderr);
        return 3;
    }
    return retired ? 0 : 1;
}
