#include "application_state.h"
#include "mc/block/registry/runtime.h"
#include "millis.h"
#include "random_profile_internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line%d: %s\n", __LINE__, #x);                                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint32_t fb(float x) {
    uint32_t b;
    memcpy(&b, &x, sizeof(b));
    return b;
}

static uint64_t db(double x) {
    uint64_t b;
    memcpy(&b, &x, sizeof(b));
    return b;
}

static mc_game_settings_status binding_code(mc_game_settings *settings, mc_key_binding *binding,
                                            int32_t value) {
    mc_game_settings_text name;
    int32_t old_code;
    if (!mc_key_binding_read(binding, &name, &old_code))
        return MC_GAME_SETTINGS_INVALID_ARGUMENT;
    return mc_game_settings_binding_set(settings, binding, name, value, NULL);
}

static int failure_controls(void) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *classes = NULL;
    mc_client_item_renderer *item = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, NULL, 0) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(entities, profile, &classes) == MC_CLIENT_MODEL_OK);
    mc_entity_vector *pooled = NULL, *reused = NULL;
    CHECK(mc_entity_vector_pool_get(entities, 1, 2, 3, &pooled, NULL) == MC_ENTITY_OK);
    CHECK(mc_client_item_renderer_allocate(classes, 0, &item) == MC_CLIENT_MODEL_OK);
    mc_client_model_error error;
    CHECK(mc_client_item_renderer_construct(item, &error) == MC_CLIENT_MODEL_RANDOM_FAILED);
    CHECK(error.source_pc == 20 && strcmp(error.source_class, "bb") == 0);
    const mc_client_item_renderer_fields *fields = mc_client_item_renderer_fields_get(item);
    CHECK(fields->base_model != NULL && fields->base_blocks != NULL && fields->e != NULL &&
          fields->base_blocks != fields->e);
    CHECK(fields->f == NULL && !fields->a && fb(fields->shadow_size) == 0 &&
          fields->shadow_opacity == 1.0f);
    CHECK(mc_client_item_renderer_construct(item, NULL) == MC_CLIENT_MODEL_INVALID_PHASE);
    mc_entity_vector_snapshot vectors;
    CHECK(mc_entity_vector_get_snapshot(entities, &vectors) == MC_ENTITY_OK &&
          vectors.holds == 73 && vectors.size == 1 && vectors.cursor == 1);
    CHECK(mc_entity_vector_pool_reset(entities, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_get(entities, 9, 8, 7, &reused, NULL) == MC_ENTITY_OK &&
          reused == pooled);
    CHECK(fields->base_model->a->j[0]->a != pooled && fields->base_model->a->j[0]->a->x == -4.0);
    CHECK(mc_entity_vector_release(entities, pooled) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_release(entities, reused) == MC_ENTITY_OK);
    CHECK(mc_client_item_renderer_free(&item) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_vector_get_snapshot(entities, &vectors) == MC_ENTITY_OK && vectors.holds == 0);
    for (size_t limit = 1024; limit <= 16384; limit += 1024) {
        CHECK(mc_client_item_renderer_allocate(classes, limit, &item) == MC_CLIENT_MODEL_OK);
        CHECK(mc_client_item_renderer_construct(item, &error) == MC_CLIENT_MODEL_LIMIT);
        CHECK(mc_client_item_renderer_phase(item) == MC_CLIENT_MODEL_FAILED);
        CHECK(mc_client_item_renderer_free(&item) == MC_CLIENT_MODEL_OK);
        CHECK(mc_entity_vector_get_snapshot(entities, &vectors) == MC_ENTITY_OK &&
              vectors.holds == 0);
    }
    CHECK(mc_client_render_classes_free(classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    return 0;
}

static void text(const mc_registry_string *s) {
    if (s == NULL) {
        printf("null");
        return;
    }
    mc_registry_string_view v;
    (void)mc_registry_string_read(s, &v);
    for (size_t i = 0; i < v.length; ++i)
        printf("%04x", v.units[i]);
}

static void state(const char *label, const mc_client_achievement_notice *notice) {
    const mc_client_achievement_notice_fields *f = mc_client_achievement_notice_fields_get(notice);
    printf("STATE %s ", label);
    text(f->d);
    putchar(' ');
    text(f->e);
    printf(" %d %d\n", f->f == NULL ? -1 : mc_statistic_fields_get(f->f)->e, f->i ? 1 : 0);
}

static void geometry(const mc_client_item_renderer *item) {
    const mc_client_item_renderer_fields *f = mc_client_item_renderer_fields_get(item);
    printf("MODEL %08" PRIx32 " %08" PRIx32 " %d %d %d %d\n", fb(f->shadow_size),
           fb(f->shadow_opacity), f->a, f->base_blocks != f->e, f->render_manager == NULL,
           f->f != NULL && !f->f->have_next_next_gaussian && db(f->f->next_next_gaussian) == 0);
    const mc_client_biped_model *m = f->base_model;
    printf("BIPED %08" PRIx32 " %d %d %d %d\n", fb(m->m), m->n, m->j, m->k, m->l);
    const mc_client_model_part *parts[] = {m->a, m->b, m->c, m->d, m->e, m->f, m->g, m->h, m->i};
    for (size_t p = 0; p < 9; ++p) {
        const mc_client_model_part *part = parts[p];
        printf("PART %zu %d %d %d %d %d %d %d %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
               " %08" PRIx32 " %08" PRIx32 "\n",
               p, part->l, part->m, part->g, part->h, part->i, part->n, part->o, fb(part->a),
               fb(part->b), fb(part->c), fb(part->d), fb(part->e), fb(part->f));
        for (size_t i = 0; i < 8; ++i) {
            const mc_client_model_vertex *v = part->j[i];
            printf("VERTEX %zu %zu %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32
                   " %08" PRIx32 "\n",
                   p, i, db(v->a->x), db(v->a->y), db(v->a->z), fb(v->b), fb(v->c));
        }
        for (size_t q = 0; q < 6; ++q) {
            const mc_client_model_quad *quad = part->k[q];
            printf("QUAD %zu %zu %d %d", p, q, quad->b, quad->c);
            for (size_t i = 0; i < 4; ++i) {
                const mc_client_model_vertex *v = quad->a[i];
                int index = -1;
                bool distinct = true;
                for (size_t j = 0; j < 8; ++j) {
                    if (v->a == part->j[j]->a)
                        index = (int)j;
                    if (v == part->j[j])
                        distinct = false;
                }
                printf(" %d %d %08" PRIx32 " %08" PRIx32, index, distinct, fb(v->b), fb(v->c));
            }
            putchar('\n');
        }
    }
    const mc_client_render_blocks *blocks[] = {f->base_blocks, f->e};
    for (size_t i = 0; i < 2; ++i) {
        const mc_client_render_blocks *b = blocks[i];
        bool zero = b->c == NULL && !b->e && !b->f && b->g == 0 && b->h == 0 && b->i == 0 &&
                    b->j == 0 && b->k == 0 && b->l == 0 && !b->m;
        for (size_t j = 0; j < 27; ++j)
            zero = zero && fb(b->n_to_N[j]) == 0;
        for (size_t j = 0; j < 12; ++j)
            zero = zero && fb(b->P_to_aa[j]) == 0 && !b->ab_to_am[j];
        printf("BLOCKS %zu %d %d %d %d\n", i, b->d, b->b, b->O, zero);
    }
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(failure_controls() == 0);
    client_application app = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &app.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, app.profile, NULL, &app.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_materials_create(&app.materials));
    CHECK(mc_resource_files_create(argv[2], &app.files) == MC_REGISTRY_OK);
    const mc_registry_options opts = {.side = MC_REGISTRY_CLIENT,
                                      .random_profile = app.profile,
                                      .materials = app.materials,
                                      .resources = mc_resource_files_source(app.files)};
    CHECK(mc_registry_classes_allocate(&opts, &app.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_client_render_classes_create(app.entities, app.profile, &app.render_classes) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_client_achievement_notice_allocate(app.render_classes, app.registry, app.profile,
                                                &app.achievement_notice,
                                                NULL) == MC_CLIENT_NOTICE_OK);
    CHECK(mc_client_render_classes_free(app.render_classes) == MC_CLIENT_MODEL_BUSY);
    CHECK(mc_registry_classes_free(app.registry, NULL) == MC_REGISTRY_BUSY);
    uint64_t before = atomic_load(&app.profile->uniquifier);
    CHECK(mc_client_achievement_notice_construct(app.achievement_notice,
                                                 strcmp(argv[1], "geometry") == 0 ? NULL : &app,
                                                 NULL) == MC_CLIENT_NOTICE_OK);
    CHECK(atomic_load(&app.profile->uniquifier) == before * UINT64_C(1181783497276652981));
    mc_entity_vector_snapshot vectors;
    CHECK(mc_entity_vector_get_snapshot(app.entities, &vectors) == MC_ENTITY_OK &&
          vectors.holds == 72 && vectors.size == 0 && vectors.cursor == 0);
    const mc_client_achievement_notice_fields *fields =
        mc_client_achievement_notice_fields_get(app.achievement_notice);
    CHECK(fields->a == (strcmp(argv[1], "geometry") == 0 ? NULL : &app));
    CHECK(fields->b == 0 && fields->c == 0 && fields->d == NULL && fields->e == NULL &&
          fields->f == NULL && fields->g == 0 && !fields->i && fb(fields->k) == 0);
    geometry(fields->h);
    if (strcmp(argv[1], "geometry") != 0) {
        /* Minecraft's vs constructor first reads uu.B; its later jl.a()
         * active use also precedes the diagnostic Settings/kh assignment. */
        CHECK(mc_registry_classes_initialize_blocks(app.registry, NULL) == MC_REGISTRY_OK);
        CHECK(mc_registry_classes_initialize_statistics(app.registry, NULL) == MC_REGISTRY_OK);
        CHECK(mc_game_settings_create_defaults(NULL, &app.settings, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(mc_client_application_install_inventory_callback(&app, NULL) == MC_CLIENT_NOTICE_OK);
        mc_registry_achievement_view v;
        CHECK(mc_registry_classes_inspect_achievements(app.registry, &v, NULL) == MC_REGISTRY_OK);
        const mc_statistic *inventory = v.named[0], *wood = v.named[1];
        mc_client_notice_error error;
        int64_t clock_before, clock_after;
        CHECK(mc_platform_current_time_millis(&clock_before) == MC_PLATFORM_MILLIS_OK);
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, inventory, &error) ==
              MC_CLIENT_NOTICE_OK);
        CHECK(mc_platform_current_time_millis(&clock_after) == MC_PLATFORM_MILLIS_OK);
        CHECK(mc_client_achievement_notice_fields_get(app.achievement_notice)->g >=
                  clock_before - 2500 &&
              mc_client_achievement_notice_fields_get(app.achievement_notice)->g <=
                  clock_after - 2500);
        state("E", app.achievement_notice);
        mc_game_settings_state settings;
        CHECK(mc_game_settings_read_state(app.settings, &settings));
        CHECK(binding_code(app.settings, settings.named[5], 21) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_set_binding_array(app.settings, NULL, 0, true, NULL) ==
              MC_GAME_SETTINGS_OK);
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, inventory, &error) ==
              MC_CLIENT_NOTICE_OK);
        state("Y_NULL_W", app.achievement_notice);
        mc_game_settings *previous_settings = app.settings;
        CHECK(mc_game_settings_create_defaults(NULL, &app.settings, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_retire(&previous_settings, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_read_state(app.settings, &settings));
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, inventory, &error) ==
              MC_CLIENT_NOTICE_OK);
        state("SETTINGS_REPLACED", app.achievement_notice);
        CHECK(binding_code(app.settings, settings.named[5], 84) == MC_GAME_SETTINGS_OK);
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, inventory, &error) ==
              MC_CLIENT_NOTICE_OK);
        state("HOLE", app.achievement_notice);
        CHECK(binding_code(app.settings, settings.named[5], 256) == MC_GAME_SETTINGS_OK);
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, inventory, &error) ==
                  MC_CLIENT_NOTICE_SOURCE_EXCEPTION &&
              error.source_pc == 4);
        state("KEY_INDEX", app.achievement_notice);
        mc_game_settings *held_settings = app.settings;
        app.settings = NULL;
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, inventory, &error) ==
                  MC_CLIENT_NOTICE_SOURCE_EXCEPTION &&
              error.source_pc == 14);
        state("NULL_SETTINGS", app.achievement_notice);
        app.settings = held_settings;
        CHECK(mc_game_settings_set_named_binding(app.settings, 5, NULL, NULL) ==
              MC_GAME_SETTINGS_OK);
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, inventory, &error) ==
                  MC_CLIENT_NOTICE_SOURCE_EXCEPTION &&
              error.source_pc == 17);
        state("NULL_BINDING", app.achievement_notice);
        CHECK(mc_game_settings_set_named_binding(app.settings, 5, settings.named[5], NULL) ==
              MC_GAME_SETTINGS_OK);
        const mc_client_achievement_notice_fields saved =
            *mc_client_achievement_notice_fields_get(app.achievement_notice);
        const mc_achievement_description_callback *null_callback;
        CHECK(mc_client_inventory_callback_create(app.registry, NULL, &null_callback, NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_client_achievement_callback_set(app.registry, wood, null_callback, NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, wood, &error) ==
                  MC_CLIENT_NOTICE_SOURCE_EXCEPTION &&
              error.source_pc == 11 && error.notice_pc == 10);
        fields = mc_client_achievement_notice_fields_get(app.achievement_notice);
        CHECK(fields->d == mc_statistic_fields_get(wood)->f && fields->e == saved.e &&
              fields->f == saved.f && fields->g == saved.g && fields->i == saved.i);
        state("NULL_CLIENT_PREFIX", app.achievement_notice);
        CHECK(mc_client_achievement_callback_set(app.registry, wood, NULL, NULL) == MC_REGISTRY_OK);
        mc_client_description_result plain = {0};
        CHECK(mc_client_achievement_description(app.registry, wood, &plain, &error) ==
                  MC_CLIENT_NOTICE_OK &&
              plain.string == mc_statistic_achievement_fields_get(wood)->l && plain.owner == NULL);
        const mc_client_achievement_notice_fields before_null_complete =
            *mc_client_achievement_notice_fields_get(app.achievement_notice);
        CHECK(mc_client_achievement_notice_remind(app.achievement_notice, NULL, &error) ==
                  MC_CLIENT_NOTICE_SOURCE_EXCEPTION &&
              error.source_pc == 2);
        CHECK(mc_client_achievement_notice_fields_get(app.achievement_notice)->d ==
              before_null_complete.d);
        CHECK(mc_client_achievement_notice_complete(app.achievement_notice, NULL, &error) ==
                  MC_CLIENT_NOTICE_SOURCE_EXCEPTION &&
              error.source_pc == 11);
        fields = mc_client_achievement_notice_fields_get(app.achievement_notice);
        CHECK(fields->e == before_null_complete.e && fields->f == before_null_complete.f &&
              fields->g == before_null_complete.g && fields->i == before_null_complete.i);
        state("NULL_COMPLETE_PREFIX", app.achievement_notice);
        CHECK(mc_platform_current_time_millis(&clock_before) == MC_PLATFORM_MILLIS_OK);
        CHECK(mc_client_achievement_notice_complete(app.achievement_notice, wood, &error) ==
              MC_CLIENT_NOTICE_OK);
        CHECK(mc_platform_current_time_millis(&clock_after) == MC_PLATFORM_MILLIS_OK);
        CHECK(mc_client_achievement_notice_fields_get(app.achievement_notice)->g >= clock_before &&
              mc_client_achievement_notice_fields_get(app.achievement_notice)->g <= clock_after);
        state("COMPLETE", app.achievement_notice);
        CHECK(binding_code(app.settings, settings.named[5], 18) == MC_GAME_SETTINGS_OK);
        mc_registry_block_view before_repeat, after_repeat;
        CHECK(mc_registry_classes_inspect_blocks(app.registry, &before_repeat, NULL) ==
              MC_REGISTRY_OK);
        for (size_t n = 0; n < 1000; ++n)
            CHECK(mc_client_achievement_notice_remind(app.achievement_notice, inventory, &error) ==
                  MC_CLIENT_NOTICE_OK);
        CHECK(mc_registry_classes_inspect_blocks(app.registry, &after_repeat, NULL) ==
              MC_REGISTRY_OK);
        CHECK(before_repeat.allocation_bytes == after_repeat.allocation_bytes);
        printf("OWNERSHIP repeated1000\n");
        CHECK(mc_client_achievement_notice_draw(app.achievement_notice, &error) ==
              MC_CLIENT_NOTICE_UNIMPLEMENTED_DEPENDENCY);
    }
    CHECK(mc_client_achievement_notice_free(&app.achievement_notice, NULL) == MC_CLIENT_NOTICE_OK);
    CHECK(mc_entity_vector_get_snapshot(app.entities, &vectors) == MC_ENTITY_OK &&
          vectors.holds == 0);
    CHECK(mc_client_render_classes_free(app.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_game_settings_retire(&app.settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_registry_classes_free(app.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(app.entities) == MC_ENTITY_OK);
    mc_materials_destroy(app.materials);
    CHECK(mc_resource_files_free(app.files) == MC_REGISTRY_OK);
    mc_java_random_profile_free(app.profile);
    printf("LIFETIME clean\n");
    return 0;
}
