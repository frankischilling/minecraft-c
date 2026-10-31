/* Genuine GAME Worlds and serialized Chunk inputs; the included fluid main is
 * not run. These are native contracts, not substituted original observations. */
#define main mc_random_blocks_fluid_fixture_main
#include "test_fluids.c"
#undef main

#include "../../../src/block/registry/runtime_internal.h"
#include "mc/entity/vector/vector.h"
#include "mc/world/chunk/object.h"
#include "mc/world/runtime/world_access.h"
#include <inttypes.h>

static input_cell random_block_input(int32_t x, int32_t y, int32_t z) {
    if (z == 9 && x == 9) {
        if (y == 12 || y == 20)
            return (input_cell){2, 0};
        if (y == 13)
            return (input_cell){1, 0};
        if (y == 56 || y == 60 || y == 68)
            return (input_cell){79, 0};
    }
    if (z == 9 && x == 22 && (y == 56 || y == 64 || y == 72))
        return (input_cell){80, 0};
    if (x >= 21 && x <= 23 && z >= 8 && z <= 10 && y >= 77 && y <= 81)
        return (input_cell){x == 22 && z == 9 && y == 80 ? 2 : 3, 0};
    if (z == 9 && y == 24) {
        if (x >= 9 && x <= 12)
            return (input_cell){18, x == 9 ? 9 : 0};
        if (x == 13)
            return (input_cell){17, 0};
    }
    if (x == 8 && y == 24 && z == 9)
        return (input_cell){18, 1};
    if (x == 22 && y == 40 && z == 22)
        return (input_cell){18, 11};
    if (x == 12 && z == 12 && (y == 100 || y == 104))
        return (input_cell){18, y == 100 ? 8 : 10};
    return (input_cell){0, 0};
}

static input_cell random_block_light(int32_t x, int32_t y, int32_t z) {
    if (x == 9 && z == 9 && y == 56)
        return (input_cell){15, 8};
    if (x == 9 && z == 9 && (y == 60 || y == 68))
        return (input_cell){0, 9};
    if (x == 22 && z == 9 && y == 56)
        return (input_cell){15, 11};
    if (x == 22 && z == 9 && (y == 64 || y == 72))
        return (input_cell){0, 12};
    if (x >= 20 && x <= 24 && z >= 7 && z <= 11 && y >= 77 && y <= 83)
        return (input_cell){0, 15};
    return (input_cell){0, 0};
}

static bool random_block_call(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                              int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_mutation_error_init(error);
    const bool result = mc_world_mutation_block_tick(world, block, x, y, z, random, error);
    mc_world_runtime_call_leave(world);
    return result;
}

static bool random_block_clear(const mc_world_mutation_error *e) {
    return e->status == MC_WORLD_STORAGE_OK && e->hook_class == NULL && e->hook_method == NULL &&
           e->hook_bytecode == 0;
}

static bool n_array_contract(fixture *f) {
    bool ok = false, value = false;
    mc_registry_classes *r = f->registry[0], *limited = NULL;
    mc_registry_block_n_view captured = {0}, missing = {0};
    mc_registry_error e = {0};
    CHECK(mc_registry_block_n_get(r, &captured, &e) == MC_REGISTRY_OK &&
          captured.identity == r->blocks.n && captured.length == 256);
    const int32_t active[] = {2, 18, 79, 80};
    for (size_t i = 0; i < COUNT(active); ++i)
        CHECK(mc_registry_block_n_at(&captured, active[i], &value, &e) == MC_REGISTRY_OK && value);
    CHECK(mc_registry_block_n_at(&captured, 1, &value, &e) == MC_REGISTRY_OK && !value);
    bool *const array = r->blocks.n;
    r->blocks.n = NULL;
    const mc_registry_status old = mc_registry_block_n_at(&captured, 2, &value, &e);
    const mc_registry_status get_null = mc_registry_block_n_get(r, &missing, &e);
    r->blocks.n = array;
    CHECK(old == MC_REGISTRY_OK && value && get_null == MC_REGISTRY_OK &&
          missing.identity == NULL && missing.length == 0);
    value = true;
    CHECK(mc_registry_block_n_at(&missing, -1, &value, &e) == MC_REGISTRY_NULL_REFERENCE && value &&
          e.constructor_pc == -1 && e.initializer_pc == -1);
    CHECK(mc_registry_block_n_at(&captured, 256, &value, &e) == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          value);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials,
                                         sizeof(mc_registry_classes) + 1024, &f->sources[0].api};
    CHECK(mc_registry_classes_allocate(&options, &limited, NULL) == MC_REGISTRY_OK);
    const uint64_t before = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    mc_registry_block_n_view unchanged = captured;
    CHECK(mc_registry_block_n_get(limited, &unchanged, &e) == MC_REGISTRY_LIMIT &&
          limited->blocks_phase == MC_REGISTRY_FAILED && unchanged.identity == captured.identity &&
          unchanged.classes == r && unchanged.length == captured.length);
    const mc_registry_error first = limited->block_failure;
    const size_t bytes = limited->allocation_bytes;
    CHECK(mc_registry_block_n_get(limited, &unchanged, &e) == MC_REGISTRY_FAILED_INITIALIZATION &&
          e.status == MC_REGISTRY_FAILED_INITIALIZATION &&
          e.initializer_pc == first.initializer_pc && e.message == first.message &&
          limited->block_failure.status == first.status &&
          limited->block_failure.message == first.message && limited->allocation_bytes == bytes &&
          unchanged.identity == captured.identity &&
          atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == before);
    puts("BLOCK_RANDOM n active-use=1 captured-reference=1 null-before-index=1 failed-latched=1");
    ok = true;
cleanup:
    if (mc_registry_classes_free(limited, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

static bool grass_contract(fixture *f, const mc_block *grass) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[0];
    mc_registry_classes *r = f->registry[0];
    mc_world_mutation_error e = mc_world_mutation_empty_error();
    const mc_java_random world_before = *w->fields.r;
    const mc_java_random math_before = math_snapshot(f);
    mc_java_random random = {0}, expected = {0};
    mc_java_random_seed(&random, 17);
    w->fields.B = true;
    const bool remote = random_block_call(w, grass, 9, 12, 9, NULL, &e);
    w->fields.B = false;
    CHECK(remote && random_block_clear(&e) && at(w, 9, 12, 9, 2, 0));
    int32_t nonzero_seed = 0, zero_seed = 0;
    bool found_nonzero = false, found_zero = false;
    for (int32_t seed = 0; seed < 10000 && (!found_zero || !found_nonzero); ++seed) {
        int32_t draw;
        mc_java_random_seed(&expected, seed);
        CHECK(mc_java_random_next_bounded(&expected, 4, &draw));
        if (draw == 0 && !found_zero) {
            zero_seed = seed;
            found_zero = true;
        } else if (draw != 0 && !found_nonzero) {
            nonzero_seed = seed;
            found_nonzero = true;
        }
    }
    CHECK(found_zero && found_nonzero);
    int32_t ignored;
    mc_java_random_seed(&random, nonzero_seed);
    expected = random;
    CHECK(mc_java_random_next_bounded(&expected, 4, &ignored) &&
          random_block_call(w, grass, 9, 12, 9, &random, &e) && random_block_clear(&e) &&
          at(w, 9, 12, 9, 2, 0) && mc_test_java_random_equal(&random, &expected));
    mc_java_random_seed(&random, zero_seed);
    expected = random;
    CHECK(mc_java_random_next_bounded(&expected, 4, &ignored) && ignored == 0 &&
          random_block_call(w, grass, 9, 12, 9, &random, &e) && random_block_clear(&e) &&
          at(w, 9, 12, 9, 3, 0) && mc_test_java_random_equal(&random, &expected));
    CHECK(random_block_call(w, grass, 9, 20, 9, NULL, &e) && random_block_clear(&e) &&
          at(w, 9, 20, 9, 2, 0));
    int32_t *const opacity = r->blocks.q;
    r->blocks.q = NULL;
    const bool null_q = random_block_call(w, grass, 9, 20, 9, NULL, &e);
    r->blocks.q = opacity;
    CHECK(!null_q && e.hook_bytecode == 35 && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          e.query.status == MC_WORLD_STORAGE_OK && at(w, 9, 20, 9, 2, 0));
    int32_t spread_seed = -1, dx = 0, dy = 0, dz = 0;
    for (int32_t seed = 0; seed < 10000; ++seed) {
        mc_java_random_seed(&expected, seed);
        CHECK(mc_java_random_next_bounded(&expected, 3, &dx) &&
              mc_java_random_next_bounded(&expected, 5, &dy) &&
              mc_java_random_next_bounded(&expected, 3, &dz));
        if (dy == 4) {
            spread_seed = seed;
            break;
        }
    }
    CHECK(spread_seed >= 0);
    mc_java_random_seed(&random, spread_seed);
    r->blocks.q = NULL;
    const bool late_q = random_block_call(w, grass, 22, 80, 9, &random, &e);
    r->blocks.q = opacity;
    CHECK(!late_q && e.hook_bytecode == 174 && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          mc_test_java_random_equal(&random, &expected) && at(w, 21 + dx, 81, 8 + dz, 3, 0));
    mc_java_random_seed(&random, spread_seed);
    CHECK(random_block_call(w, grass, 22, 80, 9, &random, &e) && random_block_clear(&e) &&
          mc_test_java_random_equal(&random, &expected) && at(w, 21 + dx, 81, 8 + dz, 2, 0) &&
          mc_test_java_random_equal(w->fields.r, &world_before) && math_is(f, &math_before) &&
          w->fields.b->count == 0 && w->active_calls == 0 && w->local_leases == 0);
    puts("BLOCK_RANDOM grass dark-gate=1 decay=1 spread=1 supplied-random=1 q-order=1");
    ok = true;
cleanup:
    return ok;
}

static bool leaves_contract(fixture *f, const mc_block *leaves) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[0], *edge = f->worlds[2];
    mc_registry_classes *r = f->registry[0];
    mc_world_mutation_error e = mc_world_mutation_empty_error();
    mc_java_random random = {0};
    mc_java_random_seed(&random, 17);
    const mc_java_random supplied = random;
    CHECK(leaves->subtype.bk.a == NULL && random_block_call(w, leaves, 8, 24, 9, &random, &e) &&
          random_block_clear(&e) && leaves->subtype.bk.a == NULL && at(w, 8, 24, 9, 18, 1));
    const size_t maximum = r->options.max_allocation_bytes;
    r->options.max_allocation_bytes = r->allocation_bytes + r->managed_current_bytes;
    const bool exhausted = random_block_call(edge, leaves, 12, 100, 12, &random, &e);
    r->options.max_allocation_bytes = maximum;
    CHECK(!exhausted && e.hook_bytecode == 68 && e.registry.status == MC_REGISTRY_LIMIT &&
          e.registry.initializer_pc == -1 && e.registry.constructor_pc == -1 &&
          r->blocks_phase == MC_REGISTRY_READY && leaves->subtype.bk.a == NULL &&
          at(edge, 12, 100, 12, 18, 8));
    const size_t before = r->allocation_bytes;
    CHECK(random_block_call(edge, leaves, 12, 100, 12, &random, &e) && random_block_clear(&e) &&
          leaves->subtype.bk.a != NULL && r->allocation_bytes > before + 32768u * sizeof(int32_t) &&
          at(edge, 12, 100, 12, 18, 0));
    int32_t *const retained = leaves->subtype.bk.a;
    for (size_t i = 0; i < 32768; ++i)
        CHECK(retained[i] == 0);
    CHECK(random_block_call(w, leaves, 9, 24, 9, &random, &e) && random_block_clear(&e) &&
          leaves->subtype.bk.a == retained && at(w, 9, 24, 9, 18, 1));
    for (int32_t i = 0; i < 32768; ++i) {
        const int32_t dx = i / 1024 - 16, dy = (i / 32) % 32 - 16, dz = i % 32 - 16;
        int32_t expected = 0;
        if (dx >= -4 && dx <= 4 && dy >= -4 && dy <= 4 && dz >= -4 && dz <= 4)
            expected = dy == 0 && dz == 0 && dx >= 0 && dx <= 4 ? 4 - dx
                       : dx == -1 && dy == 0 && dz == 0         ? -2
                                                                : -1;
        CHECK(retained[i] == expected);
    }
    mc_java_random_reseed(w->fields.r, 89);
    mc_java_random expected = *w->fields.r;
    int32_t draw;
    CHECK(mc_java_random_next_bounded(&expected, 20, &draw) && draw == 16 &&
          random_block_call(w, leaves, 22, 40, 22, &random, &e) && random_block_clear(&e) &&
          at(w, 22, 40, 22, 0, 0) && leaves->subtype.bk.a == retained &&
          retained[16 * 1024 + 16 * 32 + 16] == -2 &&
          mc_test_java_random_equal(w->fields.r, &expected));
    mc_java_random_reseed(edge->fields.r, 89);
    expected = *edge->fields.r;
    CHECK(mc_java_random_next_bounded(&expected, 20, &draw) &&
          random_block_call(edge, leaves, 12, 104, 12, &random, &e) && random_block_clear(&e) &&
          at(edge, 12, 104, 12, 0, 0) && leaves->subtype.bk.a == retained &&
          retained[16 * 1024 + 16 * 32 + 16] == -2 &&
          mc_test_java_random_equal(edge->fields.r, &expected) &&
          mc_test_java_random_equal(&random, &supplied) && w->fields.b->count == 0 &&
          edge->fields.b->count == 0 && w->active_calls == 0 && edge->active_calls == 0 &&
          w->local_leases == 0 && edge->local_leases == 0);
    puts("BLOCK_RANDOM leaves allocation-prefix=1 scratch-exact=1 distance4=1 "
         "retained-stale-center=1");
    ok = true;
cleanup:
    return ok;
}

static bool melt_contract(fixture *f, const mc_block *ice, const mc_block *snow) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[0];
    mc_registry_classes *r = f->registry[0];
    mc_world_mutation_error e = mc_world_mutation_empty_error();
    mc_java_random parameter = {0};
    mc_java_random_seed(&parameter, 17);
    const mc_java_random supplied = parameter;
    const mc_java_random world_before = *w->fields.r;
    CHECK(r->blocks.q[79] == 3 && random_block_call(w, ice, 9, 56, 9, NULL, &e) &&
          random_block_clear(&e) && at(w, 9, 56, 9, 79, 0));
    int32_t *const opacity = r->blocks.q;
    r->blocks.q = NULL;
    const bool no_q = random_block_call(w, ice, 9, 68, 9, NULL, &e);
    r->blocks.q = opacity;
    CHECK(!no_q && e.hook_bytecode == 20 && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          at(w, 9, 68, 9, 79, 0));
    mc_java_random *const actual = w->fields.r;
    w->fields.r = NULL;
    const bool zero_drop = random_block_call(w, ice, 9, 60, 9, &parameter, &e);
    w->fields.r = actual;
    CHECK(zero_drop && random_block_clear(&e) && at(w, 9, 60, 9, 9, 0) &&
          mc_test_java_random_equal(w->fields.r, &world_before) &&
          random_block_call(w, snow, 22, 56, 9, NULL, &e) && random_block_clear(&e) &&
          at(w, 22, 56, 9, 80, 0));
    w->fields.B = true;
    w->fields.r = NULL;
    const bool remote = random_block_call(w, snow, 22, 64, 9, &parameter, &e);
    w->fields.r = actual;
    w->fields.B = false;
    CHECK(remote && random_block_clear(&e) && at(w, 22, 64, 9, 0, 0) && w->fields.b->count == 0 &&
          mc_test_java_random_equal(w->fields.r, &world_before));
    mc_java_random_reseed(w->fields.r, 89);
    mc_java_random expected = *w->fields.r, math = math_snapshot(f);
    for (size_t i = 0; i < 16; ++i) {
        (void)mc_java_random_next_float(&expected);
        (void)mc_java_random_next_double(&math);
    }
    CHECK(random_block_call(w, snow, 22, 72, 9, &parameter, &e) && random_block_clear(&e) &&
          at(w, 22, 72, 9, 0, 0) && w->fields.b->count == 4 &&
          mc_test_java_random_equal(w->fields.r, &expected) && math_is(f, &math) &&
          mc_test_java_random_equal(&parameter, &supplied));
    for (size_t i = 0; i < 4; ++i) {
        mc_entity_snapshot entity = {0};
        mc_item_stack *stack = NULL;
        mc_item_stack_fields item = {0};
        CHECK(mc_entity_get_snapshot(w->entity_membership.entries[i], &entity) == MC_ENTITY_OK &&
              entity.phase == MC_ENTITY_READY && entity.base.world == w && entity.item.c == 10 &&
              mc_item_entity_stack_get(w->entity_membership.entries[i], &stack) == MC_ENTITY_OK);
        const bool valid = mc_item_stack_fields_get(stack, &item) == MC_ITEM_STACK_OK &&
                           item.id == 332 && item.count == 1 && item.damage == 0;
        mc_item_stack_release(stack);
        CHECK(valid);
    }
    CHECK(w->active_calls == 0 && w->local_leases == 0);
    puts("BLOCK_RANDOM melt ice-opacity-threshold=1 still-water=1 snow11-12=1 remote-write=1 "
         "items4=1");
    ok = true;
cleanup:
    return ok;
}

static FILE *leaf_open(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *out = NULL;
    return fopen_s(&out, path, mode) == 0 ? out : NULL;
#else
    return fopen(path, mode);
#endif
}

static bool leaf_word(FILE *out, int32_t value) {
    const uint32_t word = (uint32_t)value;
    const uint8_t bytes[4] = {(uint8_t)(word >> 24), (uint8_t)(word >> 16), (uint8_t)(word >> 8),
                              (uint8_t)word};
    return fwrite(bytes, 1, 4, out) == 4;
}

static bool leaf_array(FILE *out, const uint8_t *data, size_t length) {
    return data == NULL ? leaf_word(out, -1)
                        : length <= INT32_MAX && leaf_word(out, (int32_t)length) &&
                              fwrite(data, 1, length, out) == length;
}

static uint64_t leaf_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

typedef struct leaf_observer {
    mc_world_runtime *world;
    FILE *output;
    unsigned int index;
    size_t cells, ranges;
    bool bad;
} leaf_observer;

static mc_world_storage_status leaf_cell(void *owner, mc_world_runtime *world, int32_t x, int32_t y,
                                         int32_t z, mc_world_storage_error *error) {
    leaf_observer *o = owner;
    o->bad |= world != o->world;
    ++o->cells;
    fprintf(o->output, "CELL %u %" PRId32 " %" PRId32 " %" PRId32 "\n", o->index, x, y, z);
    *error = o->index == 5 ? (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                                      "authored reached cell failure", 907}
                           : (mc_world_storage_error){0};
    return error->status;
}

static mc_world_storage_status leaf_range(void *owner, mc_world_runtime *world, int32_t x,
                                          int32_t y, int32_t z, int32_t a, int32_t b, int32_t c,
                                          mc_world_storage_error *error) {
    leaf_observer *o = owner;
    o->bad |= world != o->world;
    ++o->ranges;
    fprintf(o->output,
            "RANGE %u %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 "\n",
            o->index, x, y, z, a, b, c);
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status leaf_added(void *owner, mc_world_runtime *world, mc_entity *entity,
                                          mc_world_storage_error *error) {
    leaf_observer *o = owner;
    mc_entity_snapshot state = {0};
    o->bad |= world != o->world || mc_entity_get_snapshot(entity, &state) != MC_ENTITY_OK;
    if (o->bad) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "leaf observer actual Entity", 0};
        return error->status;
    }
    fprintf(o->output, "ADD %u hl %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", o->index,
            leaf_bits(state.base.x), leaf_bits(state.base.y), leaf_bits(state.base.z));
    *error = o->index == 17 ? (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                                       "authored reached Entity failure", 908}
                            : (mc_world_storage_error){0};
    return error->status;
}

static bool leaf_close_world(fixture *f) {
    if (f->worlds[0] == NULL)
        return true;
    if (!remember(f, 0) ||
        mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_chunk_cache *cache = f->providers[0].cache;
    if (mc_chunk_pending_clear(f->providers[0].pending) != MC_CHUNK_ACQUIRE_OK ||
        !mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                             cache->insertion_capacity) ||
        mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    for (size_t i = 0; i < COUNT(f->children); ++i)
        if (f->children[i].state != NULL) {
            if (mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->children[i] = (child){0};
        }
    if (mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_free(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    f->worlds[0] = NULL;
    f->bound[0] = false;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    if (mc_world_store_free_checked(&f->stores[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    memset(&f->providers[0], 0, sizeof(f->providers[0]));
    return true;
}

static bool leaf_streams(const char *directory, unsigned int index, mc_world_chunk *object,
                         const int32_t *scratch) {
    bool okay = false;
    FILE *files[4] = {0};
    const char *const names[4] = {"raw", "nibbles", "height", "scratch"};
    for (size_t i = 0; i < COUNT(files); ++i) {
        char path[8192];
        const int length =
            snprintf(path, sizeof(path), "%s/leaf%u-%s.bin", directory, index, names[i]);
        if (length <= 0 || (size_t)length >= sizeof(path) ||
            (files[i] = leaf_open(path, "wb")) == NULL)
            goto done;
    }
    mc_world_chunk_fields fields;
    if (mc_world_chunk_fields_get(object, &fields) != MC_WORLD_STORAGE_OK ||
        !leaf_array(files[0], fields.b, MC_CHUNK_BLOCKS) ||
        !leaf_array(files[2], fields.h, MC_CHUNK_HEIGHTMAP_BYTES))
        goto done;
    const mc_world_chunk_nibble *arrays[] = {fields.e, fields.f, fields.g};
    for (size_t i = 0; i < COUNT(arrays); ++i) {
        mc_world_chunk_nibble_fields words;
        if (mc_world_chunk_nibble_fields_get(arrays[i], &words) != MC_WORLD_STORAGE_OK ||
            !leaf_array(files[1], words.a, words.length))
            goto done;
    }
    if (scratch != NULL)
        for (size_t i = 0; i < 32u * 32u * 32u; ++i)
            if (!leaf_word(files[3], scratch[i]))
                goto done;
    okay = true;
done:
    for (size_t i = 0; i < COUNT(files); ++i)
        if (files[i] != NULL && fclose(files[i]) != 0)
            okay = false;
    return okay;
}

static int leaf_probe(const char *root, const char *directory) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    mc_world_access *access = NULL;
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    FILE *output = NULL;
    const int32_t *first_scratch = NULL;
    static const char *const names[] = {"grass_no_decay",
                                        "grass_decay",
                                        "grass_dark_air",
                                        "grass_spread",
                                        "grass_light4",
                                        "grass_callback_failure",
                                        "leaves_flag0",
                                        "leaves_fresh_absent",
                                        "leaves_log_distance4",
                                        "leaves_isolated",
                                        "leaves_reused_absent",
                                        "ice8",
                                        "ice9",
                                        "ice_remote9",
                                        "snow11",
                                        "snow_remote12",
                                        "snow_drop4",
                                        "snow_add_failure"};
    const uint16_t title[] = {'L', 'e', 'a', 'f'};
    char path[8192];
    const int n = snprintf(path, sizeof(path), "%s/native.txt", directory);
    CHECK(f != NULL && error != NULL && n > 0 && (size_t)n < sizeof(path) &&
          (output = leaf_open(path, "wb")) != NULL);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *source = &f->sources[0];
    source->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    source->streams[1] = source->streams[2] = (stream){.bytes = language};
    source->api = (mc_registry_resources){source, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &source->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entities[0]) == MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    for (unsigned int index = 0; index < 18; ++index) {
        char case_root[8192];
        const int length = snprintf(case_root, sizeof(case_root), "%s/case%u", root, index);
        CHECK(length > 0 && (size_t)length < sizeof(case_root) && open_store(f, 0, case_root) &&
              create_world(f, 0));
        mc_world_runtime *w = f->worlds[0];
        mc_chunk_state *chunk = NULL;
        CHECK(mc_world_runtime_get(w, 0, 0, &chunk) == MC_CHUNK_ACQUIRE_OK && chunk != NULL &&
              remember(f, 0) && f->providers[0].cache->count == 1);
        mc_world_chunk *object = mc_world_store_chunk_object(f->stores[0], chunk);
        CHECK(object != NULL);
        const int32_t block_id = index < 6 ? 2 : index < 11 ? 18 : index < 14 ? 79 : 80;
        const int32_t x = index == 7 || index == 10 ? 12 : 8;
        w->fields.q = 1;
        w->fields.f = w->fields.m = 0;
        w->fields.g = 17;
        w->fields.Q = 6001;
        w->fields.i = w->fields.j = w->fields.k = w->fields.l = 0.0f;
        w->fields.B = index == 13 || index == 15;
        mc_java_random_reseed(w->fields.r, 89);
        mc_java_random parameter;
        mc_java_random_seed(&parameter, index == 1 ? 4096 : 0);
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(f->registry[0], block_id, &block, NULL) == MC_REGISTRY_OK &&
              block);
        leaf_observer observer = {.world = w, .output = output, .index = index};
        const mc_world_access_callbacks callbacks = {
            .cell = leaf_cell, .range = leaf_range, .entity_added = leaf_added};
        CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(w, access, NULL) == MC_WORLD_STORAGE_OK &&
              mc_entity_bounds_pool_reset(f->entities[0], NULL) == MC_ENTITY_OK &&
              mc_entity_vector_pool_reset(f->entities[0], NULL) == MC_ENTITY_OK);
        const bool returned = random_block_call(w, block, x, 64, 8, &parameter, error);
        CHECK(!observer.bad && !ferror(output) && returned == (index != 5 && index != 17));
        const int32_t *scratch = f->registry[0]->blocks.m[18]->subtype.bk.a;
        if (scratch != NULL && first_scratch == NULL)
            first_scratch = scratch;
        CHECK(scratch == NULL || scratch == first_scratch);
        uint8_t metadata = 0, target_meta = 0;
        CHECK(mc_chunk_nibbles_get(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, 64, 8,
                                   &metadata) &&
              mc_chunk_nibbles_get(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8,
                                   &target_meta));
        fprintf(output,
                "BLOCK_TICK %u %s %" PRId32 " %s %d %012" PRIx64 " %012" PRIx64 " %u %" PRId32
                " %u %" PRId32 " %zu %d %" PRId32 "\n",
                index, block->constructor_class, block_id, names[index], returned ? 1 : -1,
                parameter.state, w->fields.r->state, (unsigned)chunk->data->blocks[cell(x, 64, 8)],
                metadata, (unsigned)chunk->data->blocks[cell(7, 64, 8)], target_meta,
                w->fields.b->count, scratch != NULL,
                scratch == NULL ? 0 : scratch[16 * 1024 + 16 * 32 + 16]);
        fprintf(output, "LEAF_STATE %u %zu %zu %d %" PRId32 " %u\n", index, observer.cells,
                observer.ranges, scratch == NULL || scratch == first_scratch,
                f->registry[0]->blocks.q[79], error->hook_bytecode);
        for (size_t i = 0; i < w->entity_membership.count; ++i) {
            mc_entity_snapshot entity = {0};
            mc_item_stack_fields item = {0};
            CHECK(
                mc_entity_get_snapshot(w->entity_membership.entries[i], &entity) == MC_ENTITY_OK &&
                entity.stack && mc_item_stack_fields_get(entity.stack, &item) == MC_ITEM_STACK_OK);
            fprintf(output,
                    "DROPPED %u %zu %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %016" PRIx64
                    " %016" PRIx64 " %016" PRIx64 "\n",
                    index, i, item.id, item.count, item.damage, entity.item.c,
                    leaf_bits(entity.base.x), leaf_bits(entity.base.y), leaf_bits(entity.base.z));
        }
        CHECK(leaf_streams(directory, index, object, scratch));
        bool removed;
        CHECK(mc_world_runtime_remove_world_access(w, access, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
        access = NULL;
        CHECK(leaf_close_world(f));
    }
    ok = true;
cleanup:
    if (access != NULL && f != NULL && f->worlds[0] != NULL) {
        bool removed = false;
        (void)mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL);
        (void)mc_world_access_free(access, NULL);
    }
    free(error);
    if (output != NULL && fclose(output) != 0)
        ok = false;
    if (!cleanup_fixture(f))
        ok = false;
    if (ok)
        printf("PASS: %u World random Block probe checks (18 profiles)\n", checks);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc == 4 && strcmp(argv[1], "world-random-blocks-probe") == 0) {
        char directory[8192];
        if (!decode_path(argv[2], root, sizeof(root)) ||
            !decode_path(argv[3], directory, sizeof(directory)) || !mc_math_trig_init(&trig))
            return 1;
        return leaf_probe(root, directory);
    }
    const uint16_t title[] = {'R', 'a', 'n', 'd', 'o', 'm'};
    if (argc != 3 || strcmp(argv[1], "world-random-blocks") != 0 ||
        !decode_path(argv[2], root, sizeof root) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    CHECK(open_store(f, 0, root) && open_store(f, 2, root));
    resources *source = &f->sources[0];
    source->streams[0] = (stream){.bytes = language, .length = sizeof language - 1};
    source->streams[1] = source->streams[2] = (stream){.bytes = language};
    source->api = (mc_registry_resources){source, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials,
                                         8u * 1024u * 1024u, &source->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entities[0]) == MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          create_world(f, 0) && create_world(f, 2));
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z)
            CHECK(seed_record_from(f, 0, x, z, random_block_input, random_block_light));
    CHECK(seed_record_from(f, 2, 0, 0, random_block_input, random_block_light));
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state *state = NULL;
            CHECK(mc_world_runtime_get(f->worlds[0], x, z, &state) == MC_CHUNK_ACQUIRE_OK &&
                  state != NULL && remember(f, 0));
            f->chunks[0][x][z] = state;
        }
    mc_chunk_state *edge = NULL;
    CHECK(mc_world_runtime_get(f->worlds[2], 0, 0, &edge) == MC_CHUNK_ACQUIRE_OK && edge != NULL &&
          remember(f, 2));
    f->chunks[2][0][0] = edge;
    double warmup;
    CHECK(mc_java_math_random(f->profile, &warmup) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_block *grass = NULL, *leaves = NULL, *ice = NULL, *snow = NULL;
    CHECK(mc_registry_block_get(f->registry[0], 2, &grass, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->registry[0], 18, &leaves, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->registry[0], 79, &ice, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_get(f->registry[0], 80, &snow, NULL) == MC_REGISTRY_OK &&
          n_array_contract(f) && grass_contract(f, grass) && leaves_contract(f, leaves) &&
          melt_contract(f, ice, snow));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("random Block cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u World random Block checks (world-random-blocks)\n", checks);
    return result;
}
