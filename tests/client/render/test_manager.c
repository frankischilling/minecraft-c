#include "client/render/manager/internal.h"
#include "mc/block/material/material.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "random_profile_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Authored real owners and filesystem resources. No fake App/model/renderer,
 * replacement class, callback constructor or supplied constructor seed. UNRUN. */
_Static_assert(MC_ENTITY_CLASS_KIND_ARROW == 0 && MC_ENTITY_CLASS_KIND_BOAT == 23 &&
                   MC_ENTITY_CLASS_KIND_ENTITY == 24 &&
                   MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE == 28 && MC_ENTITY_CLASS_KIND_VV == 29 &&
                   MC_ENTITY_CLASS_KIND_C == 32 && MC_ENTITY_CLASS_KIND_COUNT == 33,
               "existing29 literal selectors stay stable; only4 passive cells append");
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "manager check%u line%d: %s\n", checks, __LINE__, #x);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_materials *materials;
    mc_resource_files *resources;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_client_render_classes *models;
    mc_client_render_manager_classes *classes;
    mc_client_manager_error *error;
} fixture;

static int open_fixture(fixture *f, const char *root, const mc_client_manager_options *options) {
    memset(f, 0, sizeof(*f));
    f->error = calloc(1, sizeof(*f->error));
    CHECK(f->error != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_resource_files_create(root, &f->resources) == MC_REGISTRY_OK);
    const mc_registry_options r = {.side = MC_REGISTRY_CLIENT,
                                   .random_profile = f->profile,
                                   .materials = f->materials,
                                   .resources = mc_resource_files_source(f->resources)};
    CHECK(mc_registry_classes_allocate(&r, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &f->models) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_manager_classes_bind(f->models, f->registry, f->profile, options,
                                                &f->classes, f->error) == MC_CLIENT_MANAGER_OK);
    return 0;
}

static int close_fixture(fixture *f) {
    CHECK(mc_client_render_manager_classes_retire(&f->classes, f->error) == MC_CLIENT_MANAGER_OK);
    CHECK(f->classes == NULL);
    CHECK(mc_client_render_classes_free(f->models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_resource_files_free(f->resources) == MC_REGISTRY_OK);
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->profile);
    free(f->error);
    return 0;
}

static uint32_t bits32(float f) {
    uint32_t b;
    memcpy(&b, &f, sizeof(b));
    return b;
}

static uint64_t bits64(double d) {
    uint64_t b;
    memcpy(&b, &d, sizeof(b));
    return b;
}

static int inspect_complete(fixture *f, mc_client_render_manager *m, bool isolated_random) {
    mc_client_manager_state state;
    CHECK(mc_client_render_manager_inspect(f->classes, &state));
    CHECK(state.phase == MC_CLIENT_MANAGER_READY && state.a == m && state.candidate == m);
    CHECK(state.entries == 28 && state.completed_bw_stores == 28 && state.last_put_pc == 682 &&
          state.last_constructor_pc == 730);
    CHECK(state.fields.o != NULL && state.fields.p == NULL && state.fields.e == NULL &&
          state.fields.f == NULL && state.fields.g == NULL && state.fields.h == NULL &&
          state.fields.k == NULL && bits32(state.fields.i) == 0 && bits32(state.fields.j) == 0 &&
          bits64(state.fields.l) == 0 && bits64(state.fields.m) == 0 &&
          bits64(state.fields.n) == 0);
    CHECK(bits64(state.b) == 0 && bits64(state.c) == 0 && bits64(state.d) == 0 && state.ds_ready &&
          state.ds_i != NULL);
    static const char *const names[] = {"cloth", "chain", "iron", "diamond", "gold"};
    for (size_t i = 0; i < 5; ++i) {
        mc_player_string_view text;
        CHECK(mc_player_string_read(state.ds_i[i], &text));
        CHECK(text.length == strlen(names[i]));
        for (size_t j = 0; j < text.length; ++j)
            CHECK(text.units[j] == (uint16_t)(unsigned char)names[i][j]);
        for (size_t j = 0; j < i; ++j)
            CHECK(state.ds_i[i] != state.ds_i[j]);
    }
    static const char *const expected[MC_ENTITY_CLASS_KIND_COUNT] = {
        [MC_ENTITY_CLASS_KIND_SPIDER] = "yx",
        [MC_ENTITY_CLASS_KIND_PIG] = "me",
        [MC_ENTITY_CLASS_KIND_SHEEP] = "xy",
        [MC_ENTITY_CLASS_KIND_COW] = "va",
        [MC_ENTITY_CLASS_KIND_WOLF] = "we",
        [MC_ENTITY_CLASS_KIND_CHICKEN] = "ip",
        [MC_ENTITY_CLASS_KIND_CREEPER] = "m",
        [MC_ENTITY_CLASS_KIND_SKELETON] = "v",
        [MC_ENTITY_CLASS_KIND_ZOMBIE] = "v",
        [MC_ENTITY_CLASS_KIND_SLIME] = "mj",
        [MC_ENTITY_CLASS_KIND_PLAYER] = "ds",
        [MC_ENTITY_CLASS_KIND_GIANT] = "yg",
        [MC_ENTITY_CLASS_KIND_GHAST] = "pq",
        [MC_ENTITY_CLASS_KIND_SQUID] = "es",
        [MC_ENTITY_CLASS_KIND_LIVING_ENTITY] = "gv",
        [MC_ENTITY_CLASS_KIND_ENTITY] = "mb",
        [MC_ENTITY_CLASS_KIND_PAINTING] = "dy",
        [MC_ENTITY_CLASS_KIND_ARROW] = "mc",
        [MC_ENTITY_CLASS_KIND_SNOWBALL] = "dg",
        [MC_ENTITY_CLASS_KIND_VV] = "dg",
        [MC_ENTITY_CLASS_KIND_CF] = "kl",
        [MC_ENTITY_CLASS_KIND_ITEM] = "bb",
        [MC_ENTITY_CLASS_KIND_PRIMED_TNT] = "on",
        [MC_ENTITY_CLASS_KIND_FALLING_SAND] = "gn",
        [MC_ENTITY_CLASS_KIND_MINECART] = "tb",
        [MC_ENTITY_CLASS_KIND_BOAT] = "fe",
        [MC_ENTITY_CLASS_KIND_LX] = "pl",
        [MC_ENTITY_CLASS_KIND_C] = "pi"};
    bool seen[MC_ENTITY_CLASS_KIND_COUNT] = {false};
    const void *identities[28] = {0};
    const mc_client_biped_model *bases[28] = {0};
    size_t cvs = 0;
    mc_client_managed_renderer *bb = NULL, *dy = NULL, *dg[2] = {NULL, NULL};
    size_t dg_count = 0;
    for (size_t i = 0; i < 28; ++i) {
        const mc_entity_class_literal *key = NULL;
        mc_client_managed_renderer *r = NULL;
        CHECK(mc_client_render_manager_entry(m, i, &key, &r, f->error) == MC_CLIENT_MANAGER_OK);
        mc_entity_class_literal_view cl;
        CHECK(mc_entity_class_literal_inspect(key, &cl) == MC_ENTITY_OK);
        CHECK(cl.owner == f->entities && !seen[cl.kind]);
        seen[cl.kind] = true;
        mc_client_managed_renderer_state s;
        CHECK(mc_client_managed_renderer_inspect(r, &s));
        CHECK(expected[cl.kind] != NULL && strcmp(expected[cl.kind], s.source_class) == 0);
        CHECK(s.phase == MC_CLIENT_MODEL_READY && s.manager == m && s.bw_model != NULL &&
              s.bw_blocks != NULL);
        CHECK(s.bw_model->n == false && s.bw_model->a != NULL && s.bw_blocks->d == -1 &&
              s.bw_blocks->b && s.bw_blocks->O == 1);
        CHECK(bits32(s.shadow_opacity) == UINT32_C(0x3f800000) ||
              strcmp(s.source_class, "bb") == 0);
        identities[i] = s.identity;
        bases[i] = s.bw_model;
        for (size_t j = 0; j < i; ++j)
            CHECK(identities[i] != identities[j] && bases[i] != bases[j]);
        ++cvs;
        if (s.own_blocks != NULL) {
            ++cvs;
            CHECK(s.own_blocks != s.bw_blocks);
        }
        if (strcmp(s.source_class, "bb") == 0) {
            bb = r;
            CHECK(s.shadow_size == 0.15f && s.shadow_opacity == 0.75f);
        }
        if (strcmp(s.source_class, "dy") == 0)
            dy = r;
        if (strcmp(s.source_class, "dg") == 0) {
            CHECK(dg_count < 2);
            dg[dg_count++] = r;
        }
        if (strcmp(s.source_class, "v") == 0)
            CHECK(s.own_model == s.primary && s.secondary == NULL);
        if (strcmp(s.source_class, "ds") == 0)
            CHECK(s.own_model == s.primary && s.g != s.primary && s.h != s.primary && s.g != s.h);
        if (strcmp(s.source_class, "yx") == 0 || strcmp(s.source_class, "me") == 0 ||
            strcmp(s.source_class, "xy") == 0)
            CHECK(s.primary != NULL && s.secondary != NULL && s.primary != s.secondary);
        if (strcmp(s.source_class, "m") == 0) {
            const manager_model *inner = s.own_model;
            CHECK(inner != NULL && inner != s.primary && inner->fields.a != NULL);
            /* IL(F=2) actually inflates its first box. This is not .5. */
            const mc_entity_vector *v = inner->fields.a->j[0]->a;
            CHECK(v->x == -6.0 && v->y == -10.0); /* Actual IL.a box(-4,-8,-4,8,8,8,2). */
        }
        if (strcmp(s.source_class, "tb") == 0) {
            const manager_model *nq = s.own_model;
            CHECK(nq != NULL && nq->array_a != NULL && nq->array_a->length == 7 &&
                  nq->array_a->elements[6] == NULL);
            for (size_t j = 0; j < 6; ++j)
                CHECK(nq->array_a->elements[j] != NULL);
        }
        if (strcmp(s.source_class, "fe") == 0) {
            const manager_model *ez = s.own_model;
            CHECK(ez != NULL && ez->array_a != NULL && ez->array_a->length == 5);
            for (size_t j = 0; j < 5; ++j)
                CHECK(ez->array_a->elements[j] != NULL);
        }
        if (strcmp(s.source_class, "pq") == 0) {
            const manager_model *lo = s.primary;
            CHECK(lo != NULL && lo->array_b != NULL && lo->array_b->length == 9);
            mc_java_random length_random;
            mc_java_random_seed(&length_random, 1660);
            for (int32_t j = 0; j < 9; ++j) {
                int32_t draw = 0;
                CHECK(mc_java_random_next_bounded(&length_random, 7, &draw));
                const mc_client_model_part *part = lo->array_b->elements[j];
                CHECK(part != NULL);
                const mc_entity_vector *top = part->j[0]->a, *bottom = part->j[2]->a;
                CHECK(bottom->y - top->y == (double)(draw + 8));
            }
        }
        if (strcmp(s.source_class, "es") == 0) {
            const manager_model *wn = s.primary;
            CHECK(wn != NULL && wn->array_b->length == 8);
            CHECK(wn->array_b->elements[0]->a == 5.0f && bits32(wn->array_b->elements[0]->c) == 0);
            /* Other double-libm coordinates remain actual outputs for future
             * original bit comparison; no widened tolerance claims here. */
        }
    }
    CHECK(cvs == 31 && bb != NULL && dy != NULL && dg_count == 2 && dg[0] != dg[1]);
    CHECK(bb->bb != NULL && bb->random == NULL && dy->random != NULL);
    const mc_java_random *bb_random = mc_client_item_renderer_fields_get(bb->bb)->f;
    CHECK(bb_random != NULL && bb_random != dy->random && !bb_random->have_next_next_gaussian &&
          !dy->random->have_next_next_gaussian);
    CHECK(bits64(bb_random->next_next_gaussian) == 0 &&
          bits64(dy->random->next_next_gaussian) == 0);
    CHECK(bb_random->state <= UINT64_C(0xffffffffffff) &&
          dy->random->state <= UINT64_C(0xffffffffffff));
    const mc_item *aB = NULL, *aN = NULL;
    CHECK(mc_registry_item_static_get(f->registry, "aB", &aB, &f->error->registry) ==
          MC_REGISTRY_OK);
    CHECK(mc_registry_item_static_get(f->registry, "aN", &aN, &f->error->registry) ==
          MC_REGISTRY_OK);
    CHECK(aB != NULL && aN != NULL);
    CHECK(
        (dg[0]->icon == mc_item_fields_get(aB)->bh && dg[1]->icon == mc_item_fields_get(aN)->bh) ||
        (dg[1]->icon == mc_item_fields_get(aB)->bh && dg[0]->icon == mc_item_fields_get(aN)->bh));
    (void)isolated_random;
    return 0;
}

static int complete(const char *root, bool statistics_first) {
    fixture f;
    CHECK(open_fixture(&f, root, NULL) == 0);
    mc_client_manager_state cold;
    CHECK(mc_client_render_manager_inspect(f.classes, &cold));
    CHECK(cold.phase == MC_CLIENT_MANAGER_COLD && cold.a == NULL && cold.candidate == NULL &&
          !cold.ds_ready);
    CHECK(mc_client_render_classes_free(f.models) == MC_CLIENT_MODEL_BUSY);
    mc_entity_class_snapshot passive_before, passive_after;
    CHECK(mc_entity_classes_get_snapshot(f.entities, &passive_before) == MC_ENTITY_OK);
    const uint64_t passive_random =
        atomic_load_explicit(&f.profile->uniquifier, memory_order_relaxed);
    for (int k = MC_ENTITY_CLASS_KIND_VV; k <= MC_ENTITY_CLASS_KIND_C; ++k) {
        const mc_entity_class_literal *literal = NULL, *again_literal = NULL;
        CHECK(mc_entity_class_literal_get(f.entities, (mc_entity_class_kind)k, &literal) ==
                  MC_ENTITY_OK &&
              literal != NULL);
        CHECK(mc_entity_class_literal_get(f.entities, (mc_entity_class_kind)k, &again_literal) ==
                  MC_ENTITY_OK &&
              again_literal == literal);
    }
    CHECK(mc_entity_classes_get_snapshot(f.entities, &passive_after) == MC_ENTITY_OK &&
          passive_after.entity_class == passive_before.entity_class &&
          passive_after.next_id == passive_before.next_id &&
          passive_after.allocation_bytes == passive_before.allocation_bytes &&
          atomic_load_explicit(&f.profile->uniquifier, memory_order_relaxed) == passive_random);
    /* Real successful Registry routes. Normal Minecraft construction performs
     * Statistics active-use at186 before the later TH startup access310. */
    CHECK((statistics_first
               ? mc_registry_classes_initialize_statistics(f.registry, &f.error->registry)
               : mc_registry_classes_initialize_blocks(f.registry, &f.error->registry)) ==
          MC_REGISTRY_OK);
    const uint64_t before = atomic_load_explicit(&f.profile->uniquifier, memory_order_relaxed);
    mc_entity_class_snapshot entities;
    CHECK(mc_entity_classes_get_snapshot(f.entities, &entities) == MC_ENTITY_OK);
    const int32_t id = entities.next_id;
    mc_client_render_manager *m = NULL, *again = NULL;
    CHECK(mc_client_render_manager_static_get(f.classes, &m, f.error) == MC_CLIENT_MANAGER_OK &&
          m != NULL);
    const uint64_t after = atomic_load_explicit(&f.profile->uniquifier, memory_order_relaxed);
    CHECK(after == before * UINT64_C(1181783497276652981) * UINT64_C(1181783497276652981));
    CHECK(inspect_complete(&f, m, true) == 0);
    CHECK(mc_client_render_manager_static_get(f.classes, &again, f.error) == MC_CLIENT_MANAGER_OK &&
          again == m);
    CHECK(atomic_load_explicit(&f.profile->uniquifier, memory_order_relaxed) == after);
    CHECK(mc_entity_classes_get_snapshot(f.entities, &entities) == MC_ENTITY_OK &&
          entities.next_id == id);
    CHECK(mc_client_render_classes_free(f.models) == MC_CLIENT_MODEL_BUSY);
    mc_client_manager_consumer a = {0}, b = {0};
    CHECK(mc_client_render_manager_consumer_attach(f.classes, &a, f.error) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_consumer_attach(f.classes, &b, f.error) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&f.classes, f.error) == MC_CLIENT_MANAGER_BUSY);
    CHECK(mc_client_render_manager_consumer_detach(&a, f.error) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&f.classes, f.error) == MC_CLIENT_MANAGER_BUSY);
    CHECK(mc_client_render_manager_consumer_detach(&b, f.error) == MC_CLIENT_MANAGER_OK);
    mc_client_render_manager *external = m;
    CHECK(mc_client_render_manager_retain(external, f.error) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&f.classes, f.error) == MC_CLIENT_MANAGER_BUSY);
    CHECK(mc_client_render_manager_release(&external, f.error) == MC_CLIENT_MANAGER_OK &&
          external == NULL);
    const mc_entity_class_literal *key = NULL;
    mc_client_managed_renderer *renderer = NULL;
    CHECK(mc_client_render_manager_entry(m, 0, &key, &renderer, f.error) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_managed_renderer_retain(renderer, f.error) == MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&f.classes, f.error) == MC_CLIENT_MANAGER_BUSY);
    CHECK(mc_client_managed_renderer_release(&renderer, f.error) == MC_CLIENT_MANAGER_OK &&
          renderer == NULL);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

/* Cold Item-first is a genuine failing route, not a successful startup
 * prerequisite. The canonical Registry retains the original NULL Item cause. */
static int cold_registry_failure(const char *root) {
    fixture f;
    CHECK(open_fixture(&f, root, NULL) == 0);
    mc_client_render_manager *out = NULL;
    CHECK(mc_client_render_manager_static_get(f.classes, &out, f.error) ==
              MC_CLIENT_MANAGER_REGISTRY_FAILED &&
          out == NULL);
    CHECK(f.error->registry_delegated && !f.error->native_policy &&
          strcmp(f.error->source_class, "th") == 0 && f.error->pc == 494 && f.error->th_pc == 494 &&
          f.error->registry.status == MC_REGISTRY_NULL_REFERENCE &&
          f.error->registry.initializer_pc == 3674 &&
          strcmp(f.error->registry.constructor_class, "iz") == 0 &&
          f.error->registry.constructor_pc == 2 &&
          strcmp(f.error->registry.active_initializer_class, "ep") == 0 &&
          f.error->registry.active_initializer_pc == 23 &&
          strcmp(f.error->registry.message, "ItemStack Item constructor bf read on null") == 0);
    mc_client_manager_state before, after;
    CHECK(mc_client_render_manager_inspect(f.classes, &before));
    CHECK(before.phase == MC_CLIENT_MANAGER_FAILED && before.a == NULL &&
          before.candidate != NULL && before.fields.o != NULL && before.entries == 18 &&
          before.completed_bw_stores == 0 && before.last_put_pc == 478);
    CHECK(f.classes->candidate->allocated_renderers == 19);
    size_t renderer_bytes = 0;
    for (size_t i = 0; i < 19; ++i) {
        const mc_client_managed_renderer *r = f.classes->candidate->renderers[i];
        CHECK(r != NULL && r->manager == NULL);
        renderer_bytes += r->bytes;
    }
    mc_registry_block_view registry_before, registry_after;
    CHECK(mc_registry_classes_inspect_blocks(f.registry, &registry_before, NULL) == MC_REGISTRY_OK);
    const uint64_t random_before =
        atomic_load_explicit(&f.profile->uniquifier, memory_order_relaxed);
    const mc_client_manager_error first = *f.error;
    CHECK(mc_client_render_manager_static_get(f.classes, &out, f.error) ==
              MC_CLIENT_MANAGER_CLASS_ERRONEOUS &&
          out == NULL);
    CHECK(f.error->initial_status == MC_CLIENT_MANAGER_REGISTRY_FAILED &&
          f.error->registry_delegated && f.error->source_class == first.source_class &&
          f.error->pc == first.pc && f.error->registry.status == first.registry.status &&
          f.error->registry.message == first.registry.message &&
          f.error->registry.constructor_class == first.registry.constructor_class &&
          f.error->registry.constructor_pc == first.registry.constructor_pc &&
          f.error->registry.active_initializer_class == first.registry.active_initializer_class &&
          f.error->registry.active_initializer_pc == first.registry.active_initializer_pc);
    CHECK(mc_client_render_manager_inspect(f.classes, &after));
    CHECK(after.phase == before.phase && after.a == NULL && after.candidate == before.candidate &&
          after.fields.o == before.fields.o && after.entries == before.entries &&
          after.class_bytes == before.class_bytes && after.completed_bw_stores == 0 &&
          after.last_put_pc == before.last_put_pc);
    size_t renderer_bytes_after = 0;
    for (size_t i = 0; i < 19; ++i)
        renderer_bytes_after += f.classes->candidate->renderers[i]->bytes;
    CHECK(renderer_bytes_after == renderer_bytes &&
          atomic_load_explicit(&f.profile->uniquifier, memory_order_relaxed) == random_before);
    CHECK(mc_registry_classes_inspect_blocks(f.registry, &registry_after, NULL) == MC_REGISTRY_OK &&
          registry_after.allocation_bytes == registry_before.allocation_bytes &&
          registry_after.phase == registry_before.phase &&
          registry_after.initializer_pc == registry_before.initializer_pc);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

static int failures(const char *root) {
    const size_t quotas[] = {sizeof(mc_client_managed_renderer), 4096, 32768};
    for (size_t n = 0; n < sizeof(quotas) / sizeof(quotas[0]); ++n) {
        fixture f;
        mc_client_manager_options options = mc_client_render_manager_default_options();
        options.renderer_bytes = quotas[n];
        CHECK(open_fixture(&f, root, &options) == 0);
        mc_client_render_manager *out = NULL;
        CHECK(mc_client_render_manager_static_get(f.classes, &out, f.error) !=
                  MC_CLIENT_MANAGER_OK &&
              out == NULL);
        mc_client_manager_state state;
        CHECK(mc_client_render_manager_inspect(f.classes, &state));
        CHECK(state.phase == MC_CLIENT_MANAGER_FAILED && state.a == NULL &&
              state.candidate != NULL && state.fields.o != NULL && state.entries == 0 &&
              state.completed_bw_stores == 0 && !state.ds_ready);
        CHECK(f.classes->candidate->allocated_renderers == 1 &&
              f.classes->candidate->renderers[0]->manager == NULL);
        const int32_t old_pc = f.error->pc;
        const char *old_class = f.error->source_class;
        CHECK(mc_client_render_manager_static_get(f.classes, &out, f.error) ==
                  MC_CLIENT_MANAGER_CLASS_ERRONEOUS &&
              out == NULL);
        CHECK(f.error->pc == old_pc && f.error->source_class == old_class &&
              f.error->initial_status != MC_CLIENT_MANAGER_OK);
        CHECK(close_fixture(&f) == 0);
    }
    fixture f;
    mc_client_manager_options options = mc_client_render_manager_default_options();
    options.class_bytes = sizeof(mc_client_render_manager_classes);
    CHECK(open_fixture(&f, root, &options) == 0);
    mc_client_render_manager *out = NULL;
    CHECK(mc_client_render_manager_static_get(f.classes, &out, f.error) ==
              MC_CLIENT_MANAGER_LIMIT &&
          out == NULL);
    CHECK(f.classes->candidate == NULL && f.classes->a == NULL && f.error->pc == 0 &&
          strcmp(f.error->source_method, "<clinit>") == 0);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("manager control expects authored/explicit real resource directory\n", stderr);
        return 2;
    }
    CHECK(cold_registry_failure(argv[1]) == 0);
    CHECK(complete(argv[1], false) == 0);
    CHECK(complete(argv[1], true) == 0);
    CHECK(failures(argv[1]) == 0);
    printf("RenderManager actual constructor contracts: %u checks\n", checks);
    return 0;
}
