/* Reuse actual GAME constructors and checked teardown. The native contract
 * main is compiled under another name and is never executed by this probe. */
#define MC_DUNGEON_TEST_MAIN mc_dungeon_loot_contract_fixture_main
#include "test_dungeon.c"
#undef MC_DUNGEON_TEST_MAIN

#include <inttypes.h>

static bool probe_seed(const char *text, int64_t *out) {
    uint64_t value = 0;
    for (size_t i = 0; i < 16u; ++i) {
        const unsigned char c = (unsigned char)text[i];
        unsigned int digit;
        if (c >= '0' && c <= '9')
            digit = (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = (unsigned int)(c - 'a') + 10u;
        else
            return false;
        value = (value << 4u) | digit;
    }
    if (text[16] != '\n' || text[17] != '\0')
        return false;
    *out =
        value <= INT64_MAX ? (int64_t)value : INT64_MIN + (int64_t)(value - (UINT64_C(1) << 63u));
    return true;
}

static bool probe_call(fixture *f, size_t side, bool loot, mc_java_random *random) {
    bool ok = false;
    mc_item_stack *stack = NULL;
    mc_tile_entity_name *name = NULL;
    mc_population_dungeon_error error;
    const mc_world_storage_status status =
        loot ? mc_population_dungeon_loot(f->worlds[side], random, NULL, &stack, &error)
             : mc_population_dungeon_mob(f->worlds[side], random, &name, &error);
    if (printf("{\"status\":%d,\"bytecode\":%u,\"loot_bytecode\":%u,\"mob_bytecode\":%u,"
               "\"registry_status\":%d,\"value\":",
               (int)status, error.bytecode, error.loot_bytecode, error.mob_bytecode,
               (int)error.registry.status) < 0)
        goto cleanup;
    if (status != MC_WORLD_STORAGE_OK || (loot ? stack == NULL : name == NULL)) {
        if (fputs("{\"kind\":\"null\"}", stdout) < 0)
            goto cleanup;
    } else if (loot) {
        mc_item_stack_fields fields;
        mc_item_stack_phase phase;
        if (mc_item_stack_fields_get(stack, &fields) != MC_ITEM_STACK_OK ||
            mc_item_stack_phase_get(stack, &phase) != MC_ITEM_STACK_OK ||
            phase != MC_ITEM_STACK_READY)
            goto cleanup;
        if (printf("{\"kind\":\"actual_Stack\",\"source_fields\":{\"id\":%" PRId32
                   ",\"count\":%" PRId32 ",\"damage\":%" PRId32 ",\"animation\":%" PRId32 "}}",
                   fields.id, fields.count, fields.damage, fields.animation) < 0)
            goto cleanup;
    } else {
        mc_tile_entity_name_view view;
        if (mc_tile_entity_name_get_view(name, &view) != MC_TILE_ENTITY_OK || view.is_null ||
            fputs("{\"kind\":\"actual_String\",\"utf16\":\"", stdout) < 0)
            goto cleanup;
        for (size_t i = 0; i < view.text.length; ++i)
            if (printf("%04x", (unsigned int)view.text.units[i]) < 0)
                goto cleanup;
        if (fputs("\"}", stdout) < 0)
            goto cleanup;
    }
    if (fputs(",\"continuation\":", stdout) < 0)
        goto cleanup;
    if (random == NULL) {
        if (fputs("null", stdout) < 0)
            goto cleanup;
    } else {
        const int32_t integer = mc_java_random_next_i32(random);
        int32_t bounded;
        if (!mc_java_random_next_bounded(random, 1000, &bounded))
            goto cleanup;
        const int64_t wide = mc_java_random_next_i64(random);
        const float single = mc_java_random_next_float(random);
        const double doubled = mc_java_random_next_double(random);
        uint32_t single_bits;
        uint64_t double_bits;
        memcpy(&single_bits, &single, sizeof(single_bits));
        memcpy(&double_bits, &doubled, sizeof(double_bits));
        if (printf("{\"integer\":%" PRId32 ",\"bounded\":%" PRId32 ",\"long_bits\":\"%016" PRIx64
                   "\",\"float_bits\":\"%08" PRIx32 "\",\"double_bits\":\"%016" PRIx64 "\"}",
                   integer, bounded, (uint64_t)wide, single_bits, double_bits) < 0)
            goto cleanup;
    }
    ok = fputs("}\n", stdout) >= 0;
cleanup:
    mc_item_stack_release(stack);
    mc_tile_entity_name_release(name);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192], line[80];
    mc_java_random random;
    bool initialized = false;
    size_t commands = 0;
    if (argc != 4 || strcmp(argv[1], "dungeon-loot") != 0 ||
        (strcmp(argv[2], "client") != 0 && strcmp(argv[2], "server") != 0) ||
        !decode_path(argv[3], root, sizeof(root)) || !mc_math_trig_init(&trig) ||
        !dg_setup(&f, root))
        goto cleanup;
    const size_t side = strcmp(argv[2], "client") == 0 ? 0u : 1u;
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (++commands > 512u)
            goto cleanup;
        const char command = line[0];
        if (command == 'L' || command == 'M') {
            int64_t seed;
            if (line[1] != ' ' || strlen(line) != 19u || !probe_seed(line + 2, &seed))
                goto cleanup;
            mc_java_random_seed(&random, seed);
            initialized = true;
        } else if (strlen(line) != 2u || line[1] != '\n' ||
                   (command != 'l' && command != 'm' && command != 'N' && command != 'n')) {
            goto cleanup;
        }
        const bool null_random = command == 'N' || command == 'n';
        if ((!initialized && !null_random) ||
            !probe_call(f, side, command == 'L' || command == 'l' || command == 'N',
                        null_random ? NULL : &random))
            goto cleanup;
    }
    if (ferror(stdin) || commands == 0)
        goto cleanup;
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Dungeon helper cleanup refused; surviving owners retained\n", stderr);
        result = 1;
    }
    return result;
}
