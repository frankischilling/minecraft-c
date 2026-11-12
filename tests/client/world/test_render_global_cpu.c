#include "client/render_engine/internal.h"
#include "client/render_engine/textures.h"
#include "client/world/render_global_internal.h"

#include <emmintrin.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "RenderGlobal CPU check%u line%d: %s\n", checks, __LINE__,             \
                    #condition);                                                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int raw(void) {
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error error = {0};
    CHECK(mc_client_render_global_allocate(classes, NULL, &owner, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    mc_client_render_global_state state;
    CHECK(mc_client_render_global_inspect(owner, &state));
    CHECK(state.phase == MC_CLIENT_RENDER_GLOBAL_RAW && state.holds == 1 &&
          state.arena_bytes == 0 && state.arena_allocations == 0 && state.classes == classes &&
          state.fields.a == NULL && state.fields.m == NULL && state.fields.S == NULL &&
          state.fields.b == NULL && state.fields.c == NULL && state.fields.T == NULL &&
          state.fields.t == NULL && state.fields.l == NULL && state.fields.u == NULL &&
          !state.fields.w && state.fields.H == 0 && state.fields.I == 0 && state.fields.f == 0.0 &&
          state.fields.g == 0.0 && state.fields.h == 0.0 && !state.star_random_created &&
          classes->ge_phase == MC_CLIENT_RENDER_ENGINE_RAW &&
          classes->nw.phase == MC_CLIENT_RENDER_ENGINE_RAW && classes->scene_lease_count == 1);
    CHECK(mc_client_render_engine_retire_resources(classes, NULL) == MC_CLIENT_RENDER_TEXTURE_BUSY);
    CHECK(!classes->resource_retirement_started);
    mc_client_render_global *other = owner;
    CHECK(mc_client_render_global_retain(other, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_release(&owner, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL);
    CHECK(classes->scene_lease_count == 1 && other->holds == 1);
    CHECK(mc_client_render_global_release(&other, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          other == NULL);
    CHECK(classes->scene_lease_count == 0 && !classes->resource_retirement_started);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

static int prefixes(void) {
    for (size_t quota = 0; quota < 25; ++quota) {
        mc_client_render_engine_classes *classes = NULL;
        CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
        mc_client_render_global *owner = NULL;
        mc_client_render_global_error error = {0};
        const mc_client_render_engine_options options = {8u * 1024u * 1024u, quota};
        CHECK(mc_client_render_global_allocate(classes, &options, &owner, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
        const mc_client_render_global_status result =
            mc_client_render_global_construct(owner, NULL, NULL, NULL, &error);
        CHECK(result == MC_CLIENT_RENDER_GLOBAL_LIMIT ||
              result == MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED);
        mc_client_render_global_state state;
        CHECK(mc_client_render_global_inspect(owner, &state));
        CHECK(state.phase == MC_CLIENT_RENDER_GLOBAL_FAILED && !state.source_returned &&
              !state.star_random_created && !state.queries_issued && state.fields.e == 0 &&
              state.fields.t == NULL && state.fields.l == NULL && state.fields.f == 0.0 &&
              (state.fields.T == NULL) == (state.last_store_pc < 119) &&
              state.arena_allocations <= quota && classes->scene_lease_count == 1);
        if (state.fields.a != NULL && state.fields.m != NULL)
            CHECK(state.fields.a != state.fields.m && state.fields.a->count == 0 &&
                  state.fields.m->count == 0);
        if (state.fields.S != NULL)
            CHECK(state.fields.S != state.fields.a && state.fields.S != state.fields.m &&
                  state.fields.S->count == 0);
        mc_client_render_batch *const *batches =
            state.fields.T == NULL ? state.pending_T : state.fields.T;
        for (unsigned int i = 0; i < state.published_batches; ++i) {
            mc_client_render_batch_state batch;
            CHECK(batches != NULL && mc_client_render_batch_inspect(batches[i], &batch));
            CHECK(batch.phase == MC_CLIENT_RENDER_GLOBAL_READY && batch.last_store_pc == 20 &&
                  !batch.h && !batch.i && batch.a == 0 && batch.d == 0.0f && batch.g != NULL &&
                  batch.g->position == 0 && batch.g->limit == 65536 && batch.g->capacity == 65536);
            for (unsigned int j = 0; j < i; ++j)
                CHECK(batches[i] != batches[j] && batch.g != batches[j]->state.g &&
                      batch.g->bytes->data != batches[j]->state.g->bytes->data);
        }
        if (result == MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED) {
            CHECK(state.last_constructor_pc == 129 && state.last_store_pc == 124 &&
                  state.published_batches == 4 && state.constructed_batches == 4 &&
                  state.pending_T == NULL && state.pending_batch == NULL &&
                  state.fields.b != NULL && state.fields.c != NULL && state.fields.T != NULL &&
                  error.graphics.caller.pc == 129 && error.graphics.native_policy &&
                  classes->a->count == 0 && classes->b->count == 0);
            for (size_t i = 0; i < 50000; ++i)
                CHECK(state.fields.b[i] == 0);
        }
        CHECK(mc_client_render_global_construct(owner, NULL, NULL, NULL, &error) ==
              MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE);
        CHECK(mc_client_render_global_release(&owner, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              owner == NULL);
        CHECK(classes->scene_lease_count == 0);
        CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    }
    return 0;
}

static int buffer_operations(void) {
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_arena arena = {.options = {1048576, SIZE_MAX}};
    mc_client_graphics_error error = {0};
    const mc_client_graphics_site site = {"n", "<init>", "(Lnet/minecraft/client/Minecraft;Lji;)V",
                                          239};
    mc_client_direct_int_buffer *buffer = NULL;
    CHECK(mc_client_ge_d(classes, &arena, 8, site, &buffer, &error));
    CHECK(mc_client_ge_int_position(buffer, 3, site, &error));
    CHECK(mc_client_ge_int_limit(buffer, 6, site, &error));
    void *span = NULL;
    size_t count = 0;
    CHECK(mc_client_ge_int_writable_remaining(buffer, &span, &count, site, &error));
    CHECK(span == buffer->bytes->data + 12 && count == 3 && buffer->position == 3 &&
          buffer->limit == 6);
    CHECK(!mc_client_ge_int_position(buffer, 7, site, &error) &&
          error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT && buffer->position == 3);
    CHECK(mc_client_ge_int_limit(buffer, 2, site, &error) && buffer->position == 2 &&
          buffer->limit == 2);
    CHECK(!mc_client_ge_int_limit(buffer, -1, site, &error) && buffer->position == 2 &&
          buffer->limit == 2);
    CHECK(mc_client_ge_int_clear(buffer, site, &error));
    CHECK(buffer->position == 0 && buffer->limit == 8);
    mc_client_render_arena_free(&arena);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

static int delayed_fp_admission(void) {
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error error = {0};
    const mc_client_render_engine_options options = {8u * 1024u * 1024u, 0};
    CHECK(mc_client_render_global_allocate(classes, &options, &owner, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    const unsigned int saved = _mm_getcsr();
    const unsigned int changed = (saved & ~0x6000u) | 0x6000u;
    _mm_setcsr(changed);
    const mc_client_render_global_status result =
        mc_client_render_global_construct(owner, NULL, NULL, NULL, &error);
    const unsigned int observed = _mm_getcsr();
    _mm_setcsr(saved);
    CHECK(result == MC_CLIENT_RENDER_GLOBAL_LIMIT && error.source_pc == 5 &&
          owner->last_constructor_pc == 5 && owner->last_store_pc == 0 && observed == changed);
    CHECK(mc_client_render_global_release(&owner, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

int main(void) {
    CHECK(raw() == 0);
    CHECK(prefixes() == 0);
    CHECK(buffer_operations() == 0);
    CHECK(delayed_fp_admission() == 0);
    printf("Actual RenderGlobal CPU contracts: %u checks\n", checks);
    return 0;
}
