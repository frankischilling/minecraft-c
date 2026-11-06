/* Native callback/receiver controls, separate from the original replay rows.
 * Include after test_redstone_world.c's genuine picking/Redstone helpers.
 * Cold Code Registry construction keeps its ordinary resource and RNG work.
 * The cold callback cases deliberately split the Block and code Registries.
 * They stress internal captured arguments; ordinary CV calls require matching
 * defining owners, and no original cross-loader behavior is inferred here. */
#include "mc/block/material/material.h"
#include "mc/resources/files/files.h"
#include "mc/resources/translation/translation.h"

typedef struct redstone_callback_resources {
    const mc_registry_resources *source;
    mc_cv_context *call;
    const void *entered, *replacement;
    size_t opens, rewrites;
    bool bad;
} redstone_callback_resources;

static mc_registry_status redstone_callback_open(void *context, const char *name,
                                                 mc_java_properties_input *out) {
    redstone_callback_resources *r = context;
    static const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang",
                                        "/achievement/map.txt"};
    if (r->opens >= sizeof(names) / sizeof(*names) || name == NULL ||
        strcmp(name, names[r->opens]) != 0)
        r->bad = true;
    if (r->opens++ == 0) {
        mc_cv_context *c = r->call;
        if (c->fields->c != r->entered || c->source_pc != 5 || c->depth < 2 ||
            strcmp(c->frames[c->depth - 1].class_name, "uu") != 0 ||
            strcmp(c->frames[c->depth - 1].method, "d") != 0 ||
            c->registry->blocks_phase != MC_REGISTRY_INITIALIZING)
            r->bad = true;
        /* The admitted Registry and its dependencies stay untouched. This is
         * the caller's actual mutable CV field, changed by a native callback. */
        c->fields->c = r->replacement;
        ++r->rewrites;
    }
    return r->source->open(r->source->context, name, out);
}

static mc_registry_status redstone_callback_close(void *context,
                                                  const mc_java_properties_input *input) {
    redstone_callback_resources *r = context;
    return r->source->close(r->source->context, input);
}

static bool redstone_callback_ready(void *context, const mc_java_properties_input *input) {
    redstone_callback_resources *r = context;
    return r->source->ready(r->source->context, input);
}

static bool redstone_callback_context(picking_fixture *f, mc_client_chunk_cache *cache,
                                      mc_client_render_blocks *fields, const void *receiver,
                                      mc_cv_context *out) {
    mc_client_render_blocks_construct_fields(fields, receiver, true);
    *out = (mc_cv_context){.fields = fields,
                           .cache = cache,
                           .world = f->base.world,
                           .registry = f->base.registry,
                           .models = f->models,
                           .graphics = f->graphics,
                           .application = f->app};
    CHECK(mc_cv_push(out, "g", "(Luu;III)Z"));
    return true;
}

static bool redstone_callback_receiver_error(const mc_cv_context *c, unsigned int pc,
                                             bool foreign) {
    CHECK(c->failed && c->depth == 1 && c->source_pc == pc && c->frames[0].pc == pc &&
          c->failure.source_pc == pc && c->failure.native_policy == foreign &&
          c->failure.status == (foreign ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                        : MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE) &&
          strcmp(c->failure.operation, foreign ? "actual CV IBlockAccess implementation"
                                               : "actual CV IBlockAccess receiver") == 0 &&
          strcmp(c->failure.source_class, "cv") == 0 &&
          strcmp(c->failure.source_method, "g") == 0 &&
          strcmp(c->failure.source_descriptor, "(Luu;III)Z") == 0 && !c->failure.source_returned);
    return true;
}

static bool redstone_callback_brightness(picking_fixture *f, mc_client_chunk_cache *cache,
                                         bool world, bool stairs, bool foreign) {
    const void *const receiver = world ? (const void *)f->base.world : (const void *)cache;
    const int foreign_receiver = 17;
    const void *const replacement = foreign ? &foreign_receiver : NULL;
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->base.registry, stairs ? 53 : 1, &block, NULL) ==
              MC_REGISTRY_OK &&
          block != NULL && block->kind == (stairs ? MC_BLOCK_STAIRS : MC_BLOCK_STONE));
    mc_client_render_blocks fields;
    mc_cv_context baseline, call;
    float expected = -77.0f, actual = -77.0f;
    CHECK(redstone_callback_context(f, cache, &fields, receiver, &baseline) &&
          mc_cv_brightness(&baseline, block, 8, 64, 8, 51, &expected) && !baseline.failed &&
          baseline.depth == 1);

    CHECK(redstone_callback_context(f, cache, &fields, receiver, &call));
    redstone_callback_resources resources = {.source = mc_resource_files_source(f->base.files),
                                             .call = &call,
                                             .entered = receiver,
                                             .replacement = replacement};
    CHECK(resources.source != NULL && resources.source->open != NULL &&
          resources.source->close != NULL && resources.source->ready != NULL);
    const mc_registry_resources api = {&resources, redstone_callback_open, redstone_callback_close,
                                       redstone_callback_ready};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->base.profile, f->base.materials, 0,
                                         &api};
    mc_registry_classes *code = NULL;
    CHECK(mc_registry_classes_allocate(&options, &code, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_bind_entities(code, f->base.entities, NULL) == MC_REGISTRY_OK &&
          code->blocks_phase == MC_REGISTRY_COLD && code != f->base.registry);
    call.registry = code;
    const bool completed = mc_cv_brightness(&call, block, 8, 64, 8, 51, &actual);
    if (!completed || resources.bad)
        fprintf(stderr,
                "Redstone native callback receiver=%s stairs=%d foreign=%d completed=%d "
                "status=%d pc=%u depth=%zu opens=%zu rewrites=%zu callback-bad=%d\n",
                world ? "World" : "EW", (int)stairs, (int)foreign, (int)completed,
                (int)call.failure.status, call.source_pc, call.depth, resources.opens,
                resources.rewrites, (int)resources.bad);
    CHECK(completed && !call.failed && call.depth == 1 && call.source_pc == 13 &&
          call.frames[0].pc == 51 && redstone_float_bits(actual) == redstone_float_bits(expected) &&
          !resources.bad && resources.opens == 3 && resources.rewrites == 1 &&
          fields.c == replacement && code->blocks_phase == MC_REGISTRY_READY &&
          f->base.registry->blocks_phase == MC_REGISTRY_READY &&
          (world ? call.world_error.kind == MC_CV_WORLD_MIN_BRIGHTNESS && !call.world_error.failed
                 : call.cache_error.status == MC_CLIENT_CHUNK_CACHE_OK));
    /* A new source query reads the changed field. It must not reuse the
     * brightness argument, even though that child returned successfully. */
    int32_t untouched = -777;
    CHECK(!mc_cv_block(&call, 8, 64, 8, 168, &untouched) && untouched == -777 &&
          fields.c == replacement && redstone_callback_receiver_error(&call, 168, foreign));
    CHECK(mc_registry_classes_free(code, NULL) == MC_REGISTRY_OK);
    printf("REDSTONE_CALLBACK brightness receiver=%s block=%s alias=%s captured=1 fresh=1 "
           "split-context=1\n",
           world ? "World" : "EW", stairs ? "Stairs" : "Stone", foreign ? "foreign" : "NULL");
    /* Runtime PrintStream writes bypass the fixture's buffered C stdout. */
    CHECK(fflush(stdout) == 0 && !ferror(stdout));
    return true;
}

static bool redstone_bound_controls(picking_fixture *f, mc_client_chunk_cache *cache) {
    const size_t cell = (8u << 11u) | (8u << 7u) | 64u;
    const int32_t expected_id = f->chunks[0]->data->blocks[cell];
    uint8_t expected_metadata;
    CHECK(mc_chunk_nibbles_get(f->chunks[0]->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8,
                               &expected_metadata));
    const mc_block *fluid = NULL;
    CHECK(mc_registry_block_get(f->base.registry, 8, &fluid, NULL) == MC_REGISTRY_OK &&
          fluid != NULL && fluid->kind == MC_BLOCK_FLOWING_FLUID);
    const int foreign_receiver = 19;
    for (unsigned int world = 0; world < 2; ++world) {
        const void *const receiver = world ? (const void *)f->base.world : (const void *)cache;
        mc_client_render_blocks fields;
        mc_cv_context c;
        float expected_cache = -77.0f, expected_minimum = -77.0f, expected_fluid = -77.0f;
        CHECK(redstone_callback_context(f, cache, &fields, receiver, &c) &&
              mc_cv_cache_brightness(&c, 8, 64, 8, 5, &expected_cache) &&
              mc_cv_min_brightness(&c, 8, 64, 8, 7, 13, &expected_minimum) &&
              mc_cv_brightness(&c, fluid, 8, 64, 8, 51, &expected_fluid));
        CHECK(redstone_callback_context(f, cache, &fields, &foreign_receiver, &c));
        int32_t id = -777, metadata = -777;
        float cache_value = -77.0f, minimum = -77.0f, fluid_value = -77.0f;
        CHECK(mc_cv_block_bound(&c, receiver, 8, 64, 8, 4, &id) && id == expected_id &&
              mc_cv_metadata_bound(&c, receiver, 8, 64, 8, 71, &metadata) &&
              metadata == expected_metadata &&
              mc_cv_cache_brightness_bound(&c, receiver, 8, 64, 8, 5, &cache_value) &&
              mc_cv_min_brightness_bound(&c, receiver, 8, 64, 8, 7, 13, &minimum) &&
              mc_cv_brightness_bound(&c, fluid, receiver, 8, 64, 8, 51, &fluid_value) &&
              redstone_float_bits(cache_value) == redstone_float_bits(expected_cache) &&
              redstone_float_bits(minimum) == redstone_float_bits(expected_minimum) &&
              redstone_float_bits(fluid_value) == redstone_float_bits(expected_fluid) &&
              !c.failed && c.depth == 1 && fields.c == &foreign_receiver);
    }
    for (unsigned int foreign = 0; foreign < 2; ++foreign)
        for (unsigned int query = 0; query < 4; ++query) {
            const void *const receiver = foreign ? &foreign_receiver : NULL;
            mc_client_render_blocks fields;
            mc_cv_context c;
            CHECK(redstone_callback_context(f, cache, &fields, cache, &c));
            int32_t integer = -777;
            float real = -77.0f;
            const unsigned int pc = query == 1 ? 71 : 4;
            const bool completed =
                query == 0   ? mc_cv_block_bound(&c, receiver, 8, 64, 8, pc, &integer)
                : query == 1 ? mc_cv_metadata_bound(&c, receiver, 8, 64, 8, pc, &integer)
                : query == 2 ? mc_cv_cache_brightness_bound(&c, receiver, 8, 64, 8, pc, &real)
                             : mc_cv_min_brightness_bound(&c, receiver, 8, 64, 8, 7, pc, &real);
            CHECK(!completed && integer == -777 &&
                  redstone_float_bits(real) == redstone_float_bits(-77.0f) && fields.c == cache &&
                  c.world_error.kind == MC_CV_WORLD_NONE &&
                  redstone_callback_receiver_error(&c, pc, foreign != 0));
        }
    return true;
}

static bool redstone_solid_bridge_controls(picking_fixture *f, mc_client_chunk_cache *cache) {
    mc_block *const leaves = f->base.registry->blocks.m[18];
    CHECK(leaves != NULL && leaves->kind == MC_BLOCK_LEAVES && leaves->fields.bA != NULL &&
          mc_material_c(leaves->fields.bA) && !mc_material_h(leaves->fields.bA));
    const size_t cell = (8u << 11u) | (8u << 7u) | 64u;
    const uint8_t saved_id = f->chunks[0]->data->blocks[cell];
    mc_material *const material = leaves->fields.bA;
    for (unsigned int world = 0; world < 2; ++world) {
        mc_client_render_blocks fields;
        mc_cv_context c;
        CHECK(redstone_callback_context(
            f, cache, &fields, world ? (const void *)f->base.world : (const void *)cache, &c));
        /* Declared cell input: the genuine Leaves material separates FD.h's
         * Material.h from EW.h's Material.c without replacing either owner. */
        f->chunks[0]->data->blocks[cell] = 18;
        bool value = world != 0;
        const bool completed = mc_cv_solid(&c, 8, 64, 8, 235, &value);
        f->chunks[0]->data->blocks[cell] = saved_id;
        CHECK(completed && !c.failed && value == (world == 0));
        CHECK(redstone_callback_context(
            f, cache, &fields, world ? (const void *)f->base.world : (const void *)cache, &c));
        f->chunks[0]->data->blocks[cell] = 18;
        leaves->fields.bA = NULL;
        value = true;
        const bool failed = mc_cv_solid(&c, 8, 64, 8, 296, &value);
        leaves->fields.bA = material;
        f->chunks[0]->data->blocks[cell] = saved_id;
        CHECK(!failed && value && c.failed &&
              c.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
              !c.failure.native_policy && c.source_pc == 296 && c.failure.source_pc == 296 &&
              strcmp(c.failure.source_class, "cv") == 0 &&
              strcmp(c.failure.source_method, "g") == 0 &&
              strcmp(c.failure.child.class_name, world ? "fd" : "ew") == 0 &&
              strcmp(c.failure.child.method, "h") == 0 &&
              strcmp(c.failure.child.descriptor, "(III)Z") == 0 && c.failure.child.pc == 25);
        if (world) {
            const mc_world_solid_support_error *e = &c.world_error.value.solid;
            CHECK(c.world_error.kind == MC_CV_WORLD_SOLID && c.world_error.failed &&
                  e->bytecode == 25 && e->registry.status == MC_REGISTRY_NULL_REFERENCE &&
                  c.failure.registry.status == MC_REGISTRY_NULL_REFERENCE &&
                  c.failure.world.status == e->cause.status &&
                  c.failure.world.operation == e->cause.operation &&
                  c.failure.world.system_error == e->cause.system_error);
        } else
            CHECK(c.cache_error.status == MC_CLIENT_CHUNK_CACHE_SOURCE_NULL &&
                  c.cache_error.bytecode == 25 && !c.cache_error.native_policy);
    }
    CHECK(f->chunks[0]->data->blocks[cell] == saved_id && leaves->fields.bA == material);
    return true;
}

static bool redstone_callback_controls(picking_fixture *f) {
    const size_t bindings = f->base.world->persistent_bindings;
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error error;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    mc_java_random *const random = f->base.world->fields.r;
    CHECK(random != NULL);
    const mc_java_random before = *random;
    CHECK(redstone_callback_brightness(f, cache, false, false, false) &&
          redstone_callback_brightness(f, cache, true, false, true) &&
          redstone_callback_brightness(f, cache, false, true, true) &&
          redstone_callback_brightness(f, cache, true, true, false) &&
          redstone_bound_controls(f, cache) && redstone_solid_bridge_controls(f, cache));
    CHECK(f->base.world->fields.r == random && before.state == random->state &&
          before.have_next_next_gaussian == random->have_next_next_gaussian &&
          redstone_double_bits(before.next_next_gaussian) ==
              redstone_double_bits(random->next_next_gaussian) &&
          f->base.world->persistent_bindings == bindings + 1);
    CHECK(mc_client_chunk_cache_retire(&cache, &error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && f->base.world->persistent_bindings == bindings);
    puts("REDSTONE_CALLBACK_COMPLETE native-only=1 cold-registries=4 cache-retired=1");
    return true;
}
