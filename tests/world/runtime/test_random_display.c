/* Reuse genuine World/class/Store construction and checked terminal owners.
 * The included FD.n fixture main is never executed by this probe. */
int mc_random_display_world_fixture_main(int argc, char **argv);
#define main mc_random_display_world_fixture_main
#include "test_world_random_tick.c"
#undef main
#include "../../support/java_random.h"
#include "mc/world/runtime/block_random_display.h"
#include "mc/world/runtime/random_display.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Random display check%u line%d: %s\n", checks, __LINE__, #c);          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct display_input {
    unsigned int id;
    int32_t block, metadata, x, y, z;
    int64_t argument_seed, world_seed;
    bool argument_null;
    char scenario[96], scene[64], callback[64];
} display_input;

typedef struct data_observation {
    mc_chunk_state *state;
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
} data_observation;

typedef struct display_fixture {
    random_fixture base;
    display_input input;
    mc_world_random_display_call *sampler;
    mc_world_random_display_block_error *error;
    mc_world_access *observer;
    mc_java_random parameter, replacement;
    mc_java_random *original_random;
    mc_chunk_provider *original_provider;
    mc_block **original_m;
    mc_block *original_stone, *wrapper, *original_wrapped;
    mc_block *fire;
    int32_t *original_fire_array;
    bool m_control, entry_control, wrapper_control, fire_control;
    bool sampler_mode, changed, registered, callback_bad;
    const mc_java_random *local_identity;
    size_t particles, sounds, events;
    data_observation *before;
    size_t before_count;
    FILE *out;
} display_fixture;

static bool read_inputs(const char *path, bool sampler, display_input *values, size_t *count) {
    FILE *source = open_file(path, "rb");
    CHECK(source != NULL);
    char line[1024];
    CHECK(fgets(line, sizeof(line), source) != NULL);
    size_t used = 0;
    while (fgets(line, sizeof(line), source)) {
        CHECK(used < 128);
        display_input *p = &values[used];
        int fields;
        if (sampler) {
#ifdef _MSC_VER
            fields = sscanf_s(line, "%u %95s %d %d %d %d %63s %63s", &p->id, p->scenario,
                              (unsigned int)sizeof(p->scenario), &p->block, &p->x, &p->y, &p->z,
                              p->scene, (unsigned int)sizeof(p->scene), p->callback,
                              (unsigned int)sizeof(p->callback));
#else
            fields = sscanf(line, "%u %95s %d %d %d %d %63s %63s", &p->id, p->scenario, &p->block,
                            &p->x, &p->y, &p->z, p->scene, p->callback);
#endif
            CHECK(fields == 8);
        } else {
            char argument[32];
#ifdef _MSC_VER
            fields = sscanf_s(
                line, "%u %d %95s %d %31s %" SCNd64 " %d %d %d %63s %63s", &p->id, &p->block,
                p->scenario, (unsigned int)sizeof(p->scenario), &p->metadata, argument,
                (unsigned int)sizeof(argument), &p->world_seed, &p->x, &p->y, &p->z, p->scene,
                (unsigned int)sizeof(p->scene), p->callback, (unsigned int)sizeof(p->callback));
#else
            fields = sscanf(line, "%u %d %95s %d %31s %" SCNd64 " %d %d %d %63s %63s", &p->id,
                            &p->block, p->scenario, &p->metadata, argument, &p->world_seed, &p->x,
                            &p->y, &p->z, p->scene, p->callback);
#endif
            CHECK(fields == 11);
            p->argument_null = !strcmp(argument, "NULL");
            if (!p->argument_null) {
                char *end = NULL;
                p->argument_seed = strtoll(argument, &end, 10);
                CHECK(end && !*end);
            }
        }
        CHECK(p->id == used && p->block >= 0 && p->block < 256);
        ++used;
    }
    const bool clean = !ferror(source) && fclose(source) == 0;
    CHECK(clean && used == (sampler ? 12u : 99u));
    *count = used;
    return true;
}

static bool source_domain(int32_t x, int32_t y, int32_t z) {
    return x >= -32000000 && x < 32000000 && z >= -32000000 && z <= 32000000 && y >= 0 && y < 128;
}

static bool input_cell(display_fixture *f, int32_t x, int32_t y, int32_t z, int32_t id,
                       int32_t metadata) {
    if (!source_domain(x, y, z))
        return true;
    mc_chunk_state *chunk = NULL;
    CHECK(mc_chunk_cache_lookup(f->base.provider.cache, mc_java_i32_shr(x, 4),
                                mc_java_i32_shr(z, 4), &chunk) &&
          chunk);
    const int32_t lx = (int32_t)((uint32_t)x & 15u), lz = (int32_t)((uint32_t)z & 15u);
    const size_t index = (size_t)((lx << 11) | (lz << 7) | y);
    chunk->data->blocks[index] = (uint8_t)id;
    CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, lx, y, lz,
                               (uint8_t)metadata));
    if (id) {
        const size_t column = (size_t)((lz << 4) | lx);
        if (chunk->data->heightmap[column] < y + 1)
            chunk->data->heightmap[column] = (uint8_t)(y + 1);
    }
    return true;
}

static bool prepare_world(display_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory));
    const bool fallback = f->sampler_mode && !strcmp(f->input.scene, "fallback");
    const int32_t fill = f->sampler_mode ? f->input.block : 0;
    for (int32_t x = -1; x <= 1; ++x)
        for (int32_t z = -1; z <= 1; ++z) {
            mc_chunk_state *chunk = &f->base.world->fallback;
            if (!fallback)
                CHECK(load_chunk(&f->base, x, z, &chunk));
            memset(chunk->data->blocks, fill, MC_CHUNK_BLOCKS);
            memset(chunk->data->heightmap, fill ? 128 : 0, MC_CHUNK_HEIGHTMAP_BYTES);
            chunk->data->minimum_height = fill ? 128 : 0;
            chunk->terrain_populated = true;
            CHECK(mc_chunk_cache_publish(f->base.provider.cache, x, z, chunk));
        }
    mc_world_runtime *w = f->base.world;
    w->fields.B = false;
    w->fields.q = 1;
    w->fields.f = 0;
    w->fields.g = 17;
    w->fields.Q = 6001;
    f->original_random = w->fields.r;
    f->original_provider = w->fields.v;
    mc_java_random_reseed(w->fields.r, f->sampler_mode ? 0 : f->input.world_seed);
    mc_java_random_seed(&f->parameter, f->input.argument_seed);
    mc_java_random_seed(&f->replacement, 123);
    if (!f->sampler_mode) {
        CHECK(input_cell(f, f->input.x, f->input.y, f->input.z, f->input.block, f->input.metadata));
        static const int32_t neighbors[6][3] = {{0, 1, 0},  {0, -1, 0}, {0, 0, 1},
                                                {0, 0, -1}, {1, 0, 0},  {-1, 0, 0}};
        if (!strcmp(f->input.scene, "opaque_six"))
            for (size_t i = 0; i < COUNT(neighbors); ++i)
                CHECK(input_cell(f, mc_java_i32_add(f->input.x, neighbors[i][0]),
                                 mc_java_i32_add(f->input.y, neighbors[i][1]),
                                 mc_java_i32_add(f->input.z, neighbors[i][2]), 1, 0));
        if (!strcmp(f->input.scene, "above_stone"))
            CHECK(input_cell(f, f->input.x, mc_java_i32_add(f->input.y, 1), f->input.z, 1, 0));
        if (!strcmp(f->input.scene, "below_stone") || !strcmp(f->input.scene, "below_leaves"))
            CHECK(input_cell(f, f->input.x, mc_java_i32_sub(f->input.y, 1), f->input.z,
                             !strcmp(f->input.scene, "below_stone") ? 1 : 18, 0));
        if (!strcmp(f->input.scene, "portal_left") || !strcmp(f->input.scene, "portal_right"))
            CHECK(input_cell(
                f, mc_java_i32_add(f->input.x, !strcmp(f->input.scene, "portal_left") ? -1 : 1),
                f->input.y, f->input.z, 90, 0));
        if (!strncmp(f->input.scene, "fire_faces_", 11)) {
            const unsigned int mask = (unsigned int)strtoul(f->input.scene + 11, NULL, 10);
            static const int32_t faces[5][3] = {
                {-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}};
            for (size_t i = 0; i < COUNT(faces); ++i)
                if (mask & (1u << i))
                    CHECK(input_cell(f, mc_java_i32_add(f->input.x, faces[i][0]),
                                     mc_java_i32_add(f->input.y, faces[i][1]),
                                     mc_java_i32_add(f->input.z, faces[i][2]), 5, 0));
        }
    }
    return true;
}

static bool observe_data(display_fixture *f) {
    f->before_count = f->base.owned_count ? f->base.owned_count : 1;
    f->before = calloc(f->before_count, sizeof(*f->before));
    CHECK(f->before != NULL);
    for (size_t i = 0; i < f->before_count; ++i) {
        data_observation *p = &f->before[i];
        p->state = f->base.owned_count ? f->base.owned[i] : &f->base.world->fallback;
        memcpy(&p->data, p->state->data, sizeof(p->data));
        memcpy(p->blocks, p->state->data->blocks, sizeof(p->blocks));
    }
    return true;
}

static bool data_unchanged(display_fixture *f) {
    /* Observation-only input photograph. It never supplies a query, selected
     * Block, sampled coordinate or Random value to source execution. */
    for (size_t i = 0; i < f->before_count; ++i)
        CHECK(!memcmp(&f->before[i].data, f->before[i].state->data, sizeof(mc_chunk_data)) &&
              !memcmp(f->before[i].blocks, f->before[i].state->data->blocks, MC_CHUNK_BLOCKS));
    return true;
}

static void event_name(display_fixture *f, const mc_registry_string *name) {
    mc_registry_string_view text = {0};
    if (!mc_registry_string_read(name, &text)) {
        f->callback_bad = true;
        return;
    }
    for (size_t i = 0; i < text.length; ++i) {
        if (text.units[i] <= 32 || text.units[i] >= 127)
            f->callback_bad = true;
        fputc((int)text.units[i], f->out);
    }
}

static bool callback_guards(display_fixture *f, mc_world_runtime *w) {
    if (w != f->base.world || w->local_leases != 0 || w->active_calls == 0 ||
        mc_world_store_entity_containers_validate(f->base.store, w, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (f->sampler_mode) {
        mc_world_random_display_state state = {0};
        mc_world_random_display_error retired;
        if (!mc_world_runtime_random_display_inspect(f->sampler, &state) || !state.active ||
            !state.local_random_constructed || !state.source_random ||
            state.source_random == f->original_random || state.source_random == &f->replacement ||
            mc_world_runtime_random_display_retire(&f->sampler, &retired) !=
                MC_WORLD_STORAGE_BUSY ||
            !f->sampler)
            return false;
        if (!f->local_identity)
            f->local_identity = state.source_random;
        if (f->local_identity != state.source_random)
            return false;
        if (f->input.id == 12 && f->events == 0) {
            mc_java_random expected = state.local_random_initial;
            for (size_t i = 0; i < 3; ++i)
                (void)mc_java_random_next_float(&expected);
            if (!mc_test_java_random_equal(&expected, &state.local_random))
                return false;
        }
    }
    return true;
}

static mc_world_storage_status after_event(display_fixture *f, bool particle,
                                           mc_world_storage_error *error) {
    if (particle && !f->changed) {
        if (!strcmp(f->input.callback, "swap_random")) {
            f->base.world->fields.r = &f->replacement;
            f->changed = true;
        } else if (!strcmp(f->input.callback, "null_provider")) {
            f->base.world->fields.v = NULL;
            f->changed = true;
        } else if (!strcmp(f->input.callback, "clear_observers")) {
            bool removed = false;
            if (mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) !=
                    MC_WORLD_STORAGE_OK ||
                !removed || mc_world_access_free(f->observer, NULL) != MC_WORLD_STORAGE_BUSY)
                f->callback_bad = true;
            f->registered = false;
            f->changed = true;
        }
    }
    if ((particle && !strcmp(f->input.callback, "fail_particle")) ||
        (!particle && !strcmp(f->input.callback, "fail_sound"))) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored native display observer failure", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status display_particle(void *owner, mc_world_runtime *w,
                                                const mc_registry_string *name, double x, double y,
                                                double z, double a, double b, double c,
                                                mc_world_storage_error *error) {
    display_fixture *f = owner;
    if (!callback_guards(f, w))
        f->callback_bad = true;
    fprintf(f->out, "%s_PARTICLE %u %zu ", f->sampler_mode ? "SAMPLER" : "DISPLAY", f->input.id,
            f->events++);
    event_name(f, name);
    fprintf(f->out,
            " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
            " %016" PRIx64 "\n",
            double_bits(x), double_bits(y), double_bits(z), double_bits(a), double_bits(b),
            double_bits(c));
    ++f->particles;
    return after_event(f, true, error);
}

static mc_world_storage_status display_sound(void *owner, mc_world_runtime *w,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    display_fixture *f = owner;
    if (!callback_guards(f, w))
        f->callback_bad = true;
    fprintf(f->out, "%s_SOUND %u %zu ", f->sampler_mode ? "SAMPLER" : "DISPLAY", f->input.id,
            f->events++);
    event_name(f, name);
    fprintf(f->out, " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32 " %08" PRIx32 "\n",
            double_bits(x), double_bits(y), double_bits(z), float_bits(volume), float_bits(pitch));
    ++f->sounds;
    return after_event(f, false, error);
}

static bool attach_observer(display_fixture *f) {
    const mc_world_access_callbacks callbacks = {.sound = display_sound,
                                                 .particle = display_particle};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->registered = true;
    return true;
}

static void random_word(FILE *out, const mc_java_random *random) {
    if (random)
        fprintf(out, "%012" PRIx64, random->state);
    else
        fputc('-', out);
}

static bool controls_intact(const display_fixture *f) {
    const mc_world_runtime *w = f->base.world;
    if (!w || !f->original_random)
        return true;
    const bool null_random =
        !strcmp(f->input.scene, "random_null") || !strcmp(f->input.scene, "world_random_null");
    return w->fields.r == (null_random ? NULL
                           : f->changed && !strcmp(f->input.callback, "swap_random")
                               ? &f->replacement
                               : f->original_random) &&
           w->fields.v == (f->changed && !strcmp(f->input.callback, "null_provider")
                               ? NULL
                               : f->original_provider) &&
           (!f->entry_control || f->base.registry->blocks.m[1] == NULL) &&
           (!f->m_control || f->base.registry->blocks.m == NULL) &&
           (!f->wrapper_control ||
            f->wrapper->subtype.ss.a == (!strcmp(f->input.scene, "wrapper_null")
                                             ? NULL
                                             : f->base.registry->blocks.m[50])) &&
           (!f->fire_control || f->fire->subtype.yq.a == NULL);
}

static bool install_controls(display_fixture *f) {
    mc_registry_classes *r = f->base.registry;
    if (!strcmp(f->input.scene, "random_null") || !strcmp(f->input.scene, "world_random_null"))
        f->base.world->fields.r = NULL;
    if (!strcmp(f->input.scene, "entry_null")) {
        f->original_stone = r->blocks.m[1];
        f->entry_control = true;
        r->blocks.m[1] = NULL;
    }
    if (!strncmp(f->input.scene, "wrapper_", 8)) {
        f->wrapper = r->blocks.m[f->input.block];
        f->original_wrapped = f->wrapper->subtype.ss.a;
        f->wrapper_control = true;
        f->wrapper->subtype.ss.a = !strcmp(f->input.scene, "wrapper_null") ? NULL : r->blocks.m[50];
    }
    if (!strcmp(f->input.scene, "fire_array_null")) {
        f->fire = r->blocks.m[51];
        f->original_fire_array = f->fire->subtype.yq.a;
        f->fire_control = true;
        f->fire->subtype.yq.a = NULL;
    }
    if (!strcmp(f->input.scene, "m_null")) {
        f->original_m = r->blocks.m;
        f->m_control = true;
        r->blocks.m = NULL;
    }
    return true;
}

static bool sampler_case(display_fixture *f) {
    mc_world_runtime *w = f->base.world;
    const uint64_t u = atomic_load(&f->base.profile->uniquifier);
    const unsigned int phase = atomic_load(&f->base.profile->holder_phase);
    const uint64_t math = f->base.profile->math ? atomic_load(&f->base.profile->math->state) : 0;
    const size_t bindings = w->persistent_bindings;
    CHECK(observe_data(f) && install_controls(f));
    mc_world_random_display_error admission;
    const mc_world_storage_status status = mc_world_runtime_random_display_begin(
        w, f->input.x, f->input.y, f->input.z, &f->sampler, &admission);
    mc_world_random_display_state state = {0};
    CHECK(mc_world_runtime_random_display_inspect(f->sampler, &state));
    fprintf(f->out, "SAMPLER_STATE %u %s %d ", f->input.id, f->input.scenario,
            status == MC_WORLD_STORAGE_OK ? 1 : -1);
    random_word(f->out, w->fields.r);
    fputc(' ', f->out);
    random_word(f->out, f->original_random);
    fprintf(f->out, " %d %d %zu %zu %zu\n", w->fields.g, w->fields.Q, f->particles, f->sounds,
            f->events);
    fprintf(f->out, "SAMPLER_NATIVE %u %u %zu %zu %zu %zu %zu %012" PRIx64 " %012" PRIx64 "\n",
            f->input.id, state.source_pc, state.samples_started, state.samples_returned,
            state.world_random_calls, state.display_calls_started, state.display_calls_returned,
            state.local_random_initial.state, state.local_random.state);
    CHECK(fflush(f->out) == 0 && !f->callback_bad && controls_intact(f));
    CHECK(state.local_random_allocated && state.local_random_constructed && !state.active &&
          state.source_random && state.source_random != f->original_random &&
          state.source_random != &f->replacement &&
          (!f->local_identity || state.source_random == f->local_identity));
    CHECK(atomic_load(&f->base.profile->uniquifier) == u * UINT64_C(1181783497276652981) &&
          atomic_load(&f->base.profile->holder_phase) == phase &&
          (!f->base.profile->math || atomic_load(&f->base.profile->math->state) == math) &&
          w->fields.g == 17 && w->fields.Q == 6001);
    size_t samples = 1000, returned = 1000, draws = 6000,
           calls = f->input.block && f->input.id != 5 ? 1000 : 0;
    unsigned int pc = 134;
    if (f->input.id == 4 || f->input.id == 8 || f->input.id == 13)
        calls = 0;
    if (f->input.id == 6) {
        samples = 1;
        returned = draws = calls = 0;
        pc = 31;
    }
    if (f->input.id == 7 || f->input.id == 10) {
        samples = 1;
        returned = 0;
        draws = 6;
        calls = 1;
        pc = 125;
    }
    if (f->input.id == 11) {
        samples = 2;
        returned = 1;
        draws = 12;
        calls = 1;
        pc = 100;
    }
    if (f->input.id == 14) {
        samples = 1;
        returned = 0;
        draws = 6;
        calls = 0;
        pc = 115;
    }
    CHECK(state.source_pc == pc && state.returned == (pc == 134) &&
          (status == MC_WORLD_STORAGE_OK) == (pc == 134) && state.samples_started == samples &&
          state.samples_returned == returned && state.world_random_calls == draws &&
          state.display_calls_started == calls &&
          state.display_calls_returned == (f->input.id == 7 || f->input.id == 10 ? 0 : calls));
    mc_java_random expected;
    int32_t last[6] = {0};
    mc_java_random_seed(&expected, 0);
    for (size_t i = 0; i < (f->input.id == 9 ? 6u : draws); ++i) {
        CHECK(mc_java_random_next_bounded(&expected, 16, &last[i % 6]));
    }
    CHECK(mc_test_java_random_equal(&expected, f->original_random));
    if (f->input.id == 9) {
        mc_java_random_seed(&expected, 123);
        for (size_t i = 0; i < 5994; ++i) {
            CHECK(mc_java_random_next_bounded(&expected, 16, &last[i % 6]));
        }
        CHECK(mc_test_java_random_equal(&expected, &f->replacement));
    }
    if (draws) {
        CHECK(state.coordinates_stored[0] && state.coordinates_stored[1] &&
              state.coordinates_stored[2] && !memcmp(last, state.draws, sizeof(last)) &&
              state.x == mc_java_i32_sub(mc_java_i32_add(f->input.x, last[0]), last[1]) &&
              state.y == mc_java_i32_sub(mc_java_i32_add(f->input.y, last[2]), last[3]) &&
              state.z == mc_java_i32_sub(mc_java_i32_add(f->input.z, last[4]), last[5]));
    } else
        CHECK(!state.coordinates_stored[0] && !state.coordinates_stored[1] &&
              !state.coordinates_stored[2]);
    expected = state.local_random_initial;
    if (f->input.id == 12)
        for (size_t i = 0; i < 3000; ++i)
            (void)mc_java_random_next_float(&expected);
    CHECK(mc_test_java_random_equal(&expected, &state.local_random));
    CHECK(f->particles == (f->input.id == 9    ? 2000u
                           : f->input.id == 10 ? 1u
                           : f->input.id == 11 ? 2u
                           : f->input.id == 12 ? 1000u
                                               : 0u) &&
          f->sounds == 0);
    CHECK(data_unchanged(f) && w->active_calls == 0 && w->local_leases == 0 &&
          w->persistent_bindings == bindings + 1);
    if (pc != 134) {
        CHECK(state.error && state.error->status == status);
        if (f->input.id == 6 || f->input.id == 7 || f->input.id == 14)
            CHECK(!state.error->native_policy && state.error->exception_class &&
                  !strcmp(state.error->exception_class, "java/lang/NullPointerException"));
    }
    CHECK(controls_intact(f) &&
          mc_world_runtime_random_display_retire(&f->sampler, NULL) == MC_WORLD_STORAGE_OK &&
          !f->sampler && controls_intact(f) && w->persistent_bindings == bindings &&
          w->active_calls == 0 && w->local_leases == 0);
    return true;
}

static size_t argument_steps(const display_fixture *f) {
    const display_input *p = &f->input;
    if (p->argument_null || p->block == 1 || p->block == 53 || p->block == 67 || p->block == 73 ||
        p->block == 74 || p->block == 50 || p->block == 75 || p->block == 61 || p->block == 93)
        return 0;
    if (p->block == 9 || p->block == 8)
        return p->argument_seed == 5120 && p->metadata > 0 && p->metadata < 8 ? 3 : 1;
    if (p->block == 11 || p->block == 10)
        return !strcmp(p->scene, "above_stone") ? 0 : p->argument_seed == 18 ? 3 : 1;
    if (p->block == 76)
        return 3;
    if (p->block == 62)
        return 2;
    if (p->block == 55)
        return p->metadata ? 2 : 0;
    if (p->block == 94)
        return 4;
    if (p->block == 90)
        return p->argument_seed == 18 ? 34 : 33;
    if (p->block == 51)
        return 1u + (p->argument_seed == 0 ? 2u : 0u) + f->particles * 3u;
    return SIZE_MAX;
}

static bool direct_case(display_fixture *f) {
    mc_world_runtime *w = f->base.world;
    mc_block *actor = f->base.registry->blocks.m[f->input.block];
    CHECK(actor && actor->phase == MC_BLOCK_CONSTRUCTED);
    const uint64_t u = atomic_load(&f->base.profile->uniquifier);
    const unsigned int phase = atomic_load(&f->base.profile->holder_phase);
    const size_t bindings = w->persistent_bindings;
    CHECK(observe_data(f) && install_controls(f));
    const mc_world_storage_status status = mc_world_runtime_block_random_display(
        w, actor, f->input.x, f->input.y, f->input.z, f->input.argument_null ? NULL : &f->parameter,
        f->error);
    fprintf(f->out, "DISPLAY_STATE %u %d %s %s %d ", f->input.id, f->input.block,
            actor->constructor_class, f->input.scenario, status == MC_WORLD_STORAGE_OK ? 1 : -1);
    random_word(f->out, f->input.argument_null ? NULL : &f->parameter);
    fputc(' ', f->out);
    random_word(f->out, w->fields.r);
    fputc(' ', f->out);
    random_word(f->out, f->original_random);
    fprintf(f->out, " %zu %zu %zu\n", f->particles, f->sounds, f->events);
    fprintf(f->out, "DISPLAY_NATIVE %u %s %u %u %zu %u\n", f->input.id,
            f->error->site.class_name ? f->error->site.class_name : "-", f->error->site.pc,
            f->error->child_site.pc, f->error->wrapped_calls, f->error->native_policy ? 1u : 0u);
    CHECK(fflush(f->out) == 0 && !f->callback_bad && controls_intact(f));
    const bool failure = f->input.id == 3 || f->input.id == 9 || f->input.id == 11 ||
                         f->input.id == 89 || f->input.id == 90 || f->input.id == 92 ||
                         f->input.id == 93 || f->input.id == 94;
    CHECK((status != MC_WORLD_STORAGE_OK) == failure && data_unchanged(f));
    CHECK(atomic_load(&f->base.profile->uniquifier) == u &&
          atomic_load(&f->base.profile->holder_phase) == phase && w->fields.g == 17 &&
          w->fields.Q == 6001 && w->persistent_bindings == bindings && w->active_calls == 0 &&
          w->local_leases == 0);
    mc_java_random expected;
    mc_java_random_seed(&expected, f->input.world_seed);
    const size_t world_steps = f->input.block == 74 ? f->input.id == 9    ? 0u
                                                      : f->input.id == 11 ? 3u
                                                                          : 18u
                                                    : 0u;
    for (size_t i = 0; i < world_steps; ++i)
        (void)mc_java_random_next_float(&expected);
    CHECK(mc_test_java_random_equal(&expected, f->original_random));
    if (!f->input.argument_null) {
        const size_t steps = argument_steps(f);
        CHECK(steps != SIZE_MAX);
        mc_java_random_seed(&expected, f->input.argument_seed);
        for (size_t i = 0; i < steps; ++i)
            (void)mc_java_random_next_i32(&expected);
        CHECK(mc_test_java_random_equal(&expected, &f->parameter));
    }
    if (f->input.id == 3 || f->input.id == 9 || f->input.id == 89 || f->input.id == 94)
        CHECK(!f->error->native_policy && f->error->exception_class &&
              !strcmp(f->error->exception_class, "java/lang/NullPointerException"));
    return true;
}

static bool display_teardown(display_fixture *f) {
    CHECK(controls_intact(f));
    CHECK(mc_world_runtime_random_display_retire(&f->sampler, NULL) == MC_WORLD_STORAGE_OK &&
          controls_intact(f));
    /* Only final fixture disposal restores caller-owned source inputs. Neither
     * direct display return nor retained sampler retirement is credited with
     * repaired fields, replayed draws or a substituted successful query. */
    if (f->base.world) {
        f->base.world->fields.r = f->original_random;
        f->base.world->fields.v = f->original_provider;
    }
    if (f->m_control)
        f->base.registry->blocks.m = f->original_m;
    if (f->entry_control)
        f->base.registry->blocks.m[1] = f->original_stone;
    if (f->wrapper_control)
        f->wrapper->subtype.ss.a = f->original_wrapped;
    if (f->fire_control)
        f->fire->subtype.yq.a = f->original_fire_array;
    CHECK(teardown(&f->base));
    if (f->observer)
        CHECK(mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    free(f->before);
    free(f->error);
    return true;
}

static bool inactive_controls(display_fixture *f) {
    const int32_t ids[] = {1, 53, 67, 73, 75, 61, 93};
    for (size_t i = 0; i < COUNT(ids); ++i) {
        const mc_block *actor = f->base.registry->blocks.m[ids[i]];
        CHECK(mc_world_runtime_block_random_display(NULL, actor, INT32_MIN, INT32_MIN, INT32_MAX,
                                                    NULL, f->error) == MC_WORLD_STORAGE_OK &&
              f->error->status == MC_WORLD_STORAGE_OK);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 6)
        return 2;
    display_input direct[128] = {0}, sampler[128] = {0};
    size_t direct_count = 0, sampler_count = 0;
    if (!read_inputs(argv[4], false, direct, &direct_count) ||
        !read_inputs(argv[5], true, sampler, &sampler_count))
        return 2;
    sampler[sampler_count++] = (display_input){.id = 12,
                                               .block = 76,
                                               .x = 8,
                                               .y = 64,
                                               .z = 8,
                                               .scenario = "native_local_draws",
                                               .scene = "normal",
                                               .callback = "none"};
    sampler[sampler_count++] = (display_input){.id = 13,
                                               .block = 0,
                                               .x = 8,
                                               .y = 64,
                                               .z = 8,
                                               .scenario = "native_air_m_null",
                                               .scene = "m_null",
                                               .callback = "none"};
    sampler[sampler_count++] = (display_input){.id = 14,
                                               .block = 1,
                                               .x = 8,
                                               .y = 64,
                                               .z = 8,
                                               .scenario = "native_positive_m_null",
                                               .scene = "m_null",
                                               .callback = "none"};
    FILE *out = open_file(argv[3], "wb");
    if (!out)
        return 2;
    for (size_t group = 0; group < 2; ++group) {
        const size_t count = group ? direct_count : sampler_count;
        for (size_t index = 0; index < count; ++index) {
            display_fixture *f = calloc(1, sizeof(*f));
            if (!f)
                return 2;
            f->sampler_mode = group == 0;
            f->input = group ? direct[index] : sampler[index];
            f->out = out;
            f->error = calloc(1, sizeof(*f->error));
            char path[8192];
            const int n = snprintf(path, sizeof(path), "%s/%s%zu", argv[2],
                                   group ? "display" : "sampler", index);
            const bool passed =
                f->error && n > 0 && (size_t)n < sizeof(path) && prepare_world(f, argv[1], path) &&
                attach_observer(f) &&
                (group ? direct_case(f) && (index != 0 || inactive_controls(f)) : sampler_case(f));
            if (!display_teardown(f)) {
                fputs("Random display cleanup refused; live dependencies retained\n", stderr);
                return 1;
            }
            free(f);
            if (!passed)
                return 1;
        }
    }
    if (fclose(out))
        return 1;
    printf("Random display: %u checks;99 direct and12 paired samplers plus3 native controls\n",
           checks);
    return 0;
}
