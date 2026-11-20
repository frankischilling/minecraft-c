#include "client/startup/application_construction_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/resources/files/files.h"
#include "random_profile_internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

/* Authored contracts against actual source owners. This source has not been
 * compiled/run. Resource root is the user's imported original resources. */
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "App constructor check%u line%d: %s\n", checks, __LINE__, #x);         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_client_application_constructor_services services;
    mc_materials *materials;
    mc_resource_files *files;
    mc_client_render_engine_classes *graphics;
    mc_client_application_construction_error *error;
} fixture;

static int fixture_open(fixture *f, const char *resources) {
    memset(f, 0, sizeof(*f));
    f->error = calloc(1, sizeof(*f->error));
    CHECK(f->error != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->services.profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    const mc_registry_options opts = {.side = MC_REGISTRY_CLIENT,
                                      .random_profile = f->services.profile,
                                      .materials = f->materials,
                                      .resources = mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&opts, &f->services.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->services.profile, NULL,
                                   &f->services.entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->services.entities, f->services.profile,
                                          &f->services.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_loading_classes_create(f->graphics, &f->services.gui_classes) ==
          MC_CLIENT_LOADING_OK);
    CHECK(mc_client_application_classes_create(NULL, &f->services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_sound_backend_create(f->services.profile, NULL, &f->services.sound_backend, NULL) ==
          MC_SOUND_BACKEND_OK);
    mc_sound_manager_backend backend;
    CHECK(mc_sound_backend_manager_interface(f->services.sound_backend, &backend));
    CHECK(mc_sound_manager_shared_create(&backend, &f->services.sound_shared, NULL) == MC_SOUND_OK);
    return 0;
}

static int fixture_close(fixture *f) {
    CHECK(mc_client_application_classes_release(&f->services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_loading_classes_release(f->services.gui_classes) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_render_engine_classes_release(f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_sound_manager_shared_retire(&f->services.sound_shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_backend_retire(&f->services.sound_backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(mc_client_render_classes_free(f->services.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(f->services.entities) == MC_ENTITY_OK);
    CHECK(mc_registry_classes_free(f->services.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_resource_files_free(f->files) == MC_REGISTRY_OK);
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->services.profile);
    free(f->error);
    return 0;
}

static int raw_fields(const client_application *a) {
    CHECK(a->controller == NULL);         /* c:Lob; */
    CHECK(a->Q == false);                 /* Q:Z */
    CHECK(a->R == false);                 /* R:Z */
    CHECK(a->d == 0);                     /* d:I */
    CHECK(a->e == 0);                     /* e:I */
    CHECK(a->S == NULL);                  /* S:Lcx; */
    CHECK(a->timer == NULL);              /* T:Lpy; */
    CHECK(a->active_world == NULL);       /* f:Lfd; */
    CHECK(a->g == NULL);                  /* g:Ln; */
    CHECK(a->h == NULL);                  /* h:Ldc; */
    CHECK(a->i == NULL);                  /* i:Lls; */
    CHECK(a->effect_renderer == NULL);    /* j:Ldn; */
    CHECK(a->session == NULL);            /* k:Lgr; */
    CHECK(a->l == NULL);                  /* l:Ljava/lang/String; */
    CHECK(a->m == NULL);                  /* m:Ljava/awt/Canvas; */
    CHECK(a->n == false);                 /* n:Z */
    CHECK(atomic_load(&a->o) == false);   /* o:Z */
    CHECK(a->render_engine == NULL);      /* p:Lji; */
    CHECK(a->font_renderer == NULL);      /* q:Lsj; */
    CHECK(a->r == NULL);                  /* r:Lda; */
    CHECK(a->loading_screen == NULL);     /* s:Lmk; */
    CHECK(a->t == NULL);                  /* t:Lpx; */
    CHECK(a->U == NULL);                  /* U:Lcz; */
    CHECK(a->V == 0);                     /* V:I */
    CHECK(a->W == 0);                     /* W:I */
    CHECK(a->X == 0);                     /* X:I */
    CHECK(a->Y == 0);                     /* Y:I */
    CHECK(a->achievement_notice == NULL); /* u:Lvu; */
    CHECK(a->v == NULL);                  /* v:Luq; */
    CHECK(a->w == false);                 /* w:Z */
    CHECK(a->x == NULL);                  /* x:Lfh; */
    CHECK(a->y == NULL);                  /* y:Lvf; */
    CHECK(a->settings == NULL);           /* z:Lkv; */
    CHECK(a->A == NULL);                  /* A:Lnet/minecraft/client/MinecraftApplet; */
    CHECK(a->B == NULL);                  /* B:Lyo; */
    CHECK(a->C == NULL);                  /* C:Lvy; */
    CHECK(a->D == NULL);                  /* D:Lik; */
    CHECK(a->game_directory == NULL);     /* Z:Ljava/io/File; */
    CHECK(a->aa == NULL);                 /* aa:Lnl; */
    CHECK(a->stats_writer == NULL);       /* I:Lxi; */
    CHECK(a->ab == NULL);                 /* ab:Ljava/lang/String; */
    CHECK(a->ac == 0);                    /* ac:I */
    CHECK(a->water_effect == NULL);       /* ad:Lvs; */
    CHECK(a->ae == NULL);                 /* ae:Lcg; */
    CHECK(atomic_load(&a->J) == false);   /* J:Z */
    CHECK(a->K == NULL);                  /* K:Ljava/lang/String; */
    CHECK(a->L == false);                 /* L:Z */
    CHECK(a->M == 0);                     /* M:J */
    CHECK(a->N == false);                 /* N:Z */
    CHECK(a->ag == 0);                    /* ag:I */
    CHECK(a->O == false);                 /* O:Z */
    CHECK(a->P == 0);                     /* P:J */
    CHECK(a->ah == 0);                    /* ah:I */
    return 0;
}

static int finish_app(fixture *f, client_application **app) {
    const mc_client_application_construction_status status =
        mc_client_application_constructor_retire(app, f->error);
    CHECK(status == MC_CLIENT_APP_OK || status == MC_CLIENT_APP_BUSY);
    if (status == MC_CLIENT_APP_BUSY) {
        CHECK(*app != NULL && (*app)->constructor_phase == MC_CLIENT_APP_RETIRING);
        CHECK(!(*app)->source_namespace_borrowed && atomic_load(&(*app)->source_static_holds) == 1);
        CHECK(mc_client_application_classes_release(&f->services.application_classes, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK);
        CHECK(mc_client_application_constructor_retire(app, f->error) == MC_CLIENT_APP_OK);
    }
    CHECK(*app == NULL);
    CHECK(mc_client_application_constructor_retire(app, f->error) == MC_CLIENT_APP_OK);
    return 0;
}

static int allocation_and_prefixes(const char *root) {
    const int32_t expected_pc[] = {15, 87, 108, 119, 130};
    const int32_t expected_store[] = {11, 83, 100, 111, 122};
    for (size_t i = 0; i < sizeof(expected_pc) / sizeof(expected_pc[0]); ++i) {
        fixture f;
        CHECK(fixture_open(&f, root) == 0);
        const uint64_t before = atomic_load(&f.services.profile->uniquifier);
        mc_client_application_constructor_options opts = {0};
        if (i == 0)
            opts.timer_bytes = 1;
        if (i == 1)
            opts.biped_bytes = 1;
        if (i == 2)
            opts.sound_pool_bytes = 1;
        if (i == 3)
            opts.water.max_array_bytes = 1;
        if (i == 4)
            opts.lava.max_array_bytes = 1024;
        client_application *app = NULL;
        CHECK(mc_client_application_constructor_allocate(&f.services, &opts, &app, f.error) ==
              MC_CLIENT_APP_OK);
        CHECK(raw_fields(app) == 0 && atomic_load(&f.services.profile->uniquifier) == before);
        CHECK(app->construction->worker == NULL && !app->clock.initialized);
        const mc_client_application_constructor_arguments args = {.width = 854, .height = 480};
        CHECK(mc_client_application_constructor_construct(app, &args, f.error) ==
              MC_CLIENT_APP_CHILD_FAILED);
        CHECK(f.error->minecraft_pc == expected_pc[i] &&
              app->constructor_phase == MC_CLIENT_APP_FAILED);
        CHECK(app->construction->last_source_store == expected_store[i] && !atomic_load(&app->J));
        CHECK(app->h == NULL && app->i == NULL && app->controller == NULL && app->session == NULL);
        mc_client_application_class_state state;
        CHECK(mc_client_application_classes_inspect(f.services.application_classes, &state));
        CHECK(state.a == NULL && state.native_app_borrows == 1 && state.native_holds == 1);
        CHECK(mc_client_application_constructor_construct(app, &args, f.error) ==
              MC_CLIENT_APP_INVALID_PHASE);
        CHECK(finish_app(&f, &app) == 0 && fixture_close(&f) == 0);
    }
    return 0;
}

static int normal_fields(fixture *f, client_application *a, int32_t width, int32_t height,
                         bool fullscreen) {
    CHECK(a->constructor_phase == MC_CLIENT_APP_READY && atomic_load(&a->J) && !atomic_load(&a->o));
    CHECK(a->timer != NULL && a->loading_screen != NULL && a->achievement_notice != NULL &&
          a->x != NULL);
    CHECK(a->B != NULL && a->water_effect != NULL && a->ae != NULL && a->K != NULL);
    CHECK(a->K == mc_client_loading_classes_empty(f->services.gui_classes));
    mc_client_loading_state loading;
    CHECK(mc_client_loading_screen_inspect(a->loading_screen, &loading));
    CHECK(loading.fields.a == a->K && loading.fields.c == a->K && loading.fields.b == a);
    CHECK(a->d == width && a->e == height && a->Q == fullscreen && a->Y == height && !a->n);
    CHECK(a->display_width == 0 && a->display_height == 0 && !a->running && !a->captured);
    CHECK(a->options.width == 0 && a->options.height == 0 && a->M == -1 && !a->L && !a->N && !a->O);
    CHECK(a->controller == NULL && a->h == NULL && a->i == NULL && a->active_world == NULL);
    CHECK(a->session == NULL && a->stats_writer == NULL && a->render_engine == NULL &&
          a->font_renderer == NULL);
    CHECK(a->statistics_active_use_completed && a->construction->last_source_store == 264);
    mc_timer_constructor_state timer;
    CHECK(mc_timer_constructor_inspect(a->timer, &timer));
    CHECK(timer.phase == MC_TIMER_NATIVE_READY && timer.fields.a == 20.0f &&
          timer.fields.c == 0.0f && timer.fields.e == 0.0f);
    mc_client_biped_state biped;
    CHECK(mc_client_biped_inspect(a->x, &biped) && biped.phase == MC_CLIENT_BIPED_READY);
    const mc_client_achievement_notice_fields *notice =
        mc_client_achievement_notice_fields_get(a->achievement_notice);
    CHECK(notice != NULL && notice->a == a && notice->h != NULL);
    CHECK(mc_client_item_renderer_fields_get(notice->h)->base_model != biped.fields);
    mc_client_timer_hack_state worker;
    CHECK(mc_client_timer_hack_inspect(a->construction->worker, &worker));
    CHECK(worker.a == a && worker.daemon && worker.started && worker.last_constructor_pc == 16);
    CHECK(worker.sleep_millis == MC_CLIENT_TIMER_HACK_SLEEP_MILLIS);
    CHECK(a->source_namespace_borrowed && atomic_load(&a->timer_hack_holds) == 1);
    mc_sound_backend_state backend;
    CHECK(mc_sound_backend_read_state(f->services.sound_backend, &backend));
    CHECK(!backend.constructed && !backend.device_open && !backend.command_worker_started &&
          !backend.stream_worker_started);
    return 0;
}

static int global_and_lifetimes(const char *root) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    client_application *a = NULL, *b = NULL;
    const mc_client_application_constructor_arguments args = {
        .width = -7, .height = 0, .fullscreen = true};
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &a, f.error) ==
          MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_construct(a, &args, f.error) == MC_CLIENT_APP_OK);
    CHECK(normal_fields(&f, a, -7, 0, true) == 0);
    mc_client_application_class_state state;
    CHECK(mc_client_application_classes_inspect(f.services.application_classes, &state));
    CHECK(state.a == a && state.native_app_borrows == 1 && state.applet_true == NULL &&
          state.applet_standalone == NULL);
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &b, f.error) ==
          MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_construct(b, &args, f.error) == MC_CLIENT_APP_OK);
    CHECK(b->x != a->x && b->timer != a->timer && b->K == a->K);
    CHECK(atomic_load(&a->source_static_holds) == 0 && atomic_load(&b->source_static_holds) == 1);
    CHECK(mc_client_application_constructor_retire(&b, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(b != NULL && b->constructor_phase == MC_CLIENT_APP_RETIRING && atomic_load(&b->J));
    CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_application_classes_inspect(f.services.application_classes, &state));
    CHECK(state.a == b && state.native_app_borrows == 1 && state.native_holds == 1);
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_OK && a == NULL);
    CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_constructor_retire(&b, f.error) == MC_CLIENT_APP_OK && b == NULL);
    CHECK(fixture_close(&f) == 0);
    return 0;
}

static int retained_mk_and_stop(const char *root) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    client_application *a = NULL;
    const mc_client_application_constructor_arguments args = {.width = 854, .height = 480};
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &a, f.error) ==
          MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_construct(a, &args, f.error) == MC_CLIENT_APP_OK);
    mc_client_loading_screen *external = a->loading_screen;
    CHECK(mc_client_loading_screen_retain(external) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(a != NULL && a->loading_screen == external && a->source_namespace_borrowed &&
          atomic_load(&a->J));
    const size_t before = atomic_load(&a->timer_hack_holds);
    CHECK(before == 0);
    mc_client_timer_hack *worker = NULL;
    mc_client_application_owner_error owner_error;
    CHECK(mc_client_timer_hack_allocate(&worker, &owner_error) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_timer_hack_construct(worker, a, a->K, &owner_error) ==
          MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE);
    mc_client_timer_hack_state worker_state;
    CHECK(mc_client_timer_hack_inspect(worker, &worker_state) &&
          worker_state.phase == MC_CLIENT_TIMER_HACK_RAW);
    CHECK(worker_state.a == NULL && !worker_state.started &&
          atomic_load(&a->timer_hack_holds) == before);
    CHECK(mc_client_timer_hack_retire(&worker, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_client_statistics_owner_error *statistics = calloc(1, sizeof(*statistics));
    CHECK(statistics != NULL);
    CHECK(mc_client_application_statistics_active_use(a, statistics) ==
          MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE);
    CHECK(mc_client_application_publish_session(a, NULL, statistics) ==
          MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE);
    CHECK(a->session == NULL && a->stats_writer == NULL && atomic_load(&a->J));
    free(statistics);
    CHECK(mc_client_loading_screen_release(external) == MC_CLIENT_LOADING_OK);
    CHECK(finish_app(&f, &a) == 0 && fixture_close(&f) == 0);
    return 0;
}

static int native_fp_prefix(const char *root) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    client_application *a = NULL;
    const mc_client_application_constructor_arguments args = {.width = 854, .height = 480};
    const unsigned original = _mm_getcsr();
    _mm_setcsr(original | 0x8000u);
    const mc_client_application_construction_status allocation =
        mc_client_application_constructor_allocate(&f.services, NULL, &a, f.error);
    mc_client_application_construction_status construction = MC_CLIENT_APP_INVALID_PHASE;
    if (allocation == MC_CLIENT_APP_OK)
        construction = mc_client_application_constructor_construct(a, &args, f.error);
    _mm_setcsr(original);
    CHECK(allocation == MC_CLIENT_APP_OK && construction == MC_CLIENT_APP_CHILD_FAILED);
    CHECK(f.error->minecraft_pc == 21 && f.error->child == MC_CLIENT_APP_CHILD_TIMER &&
          !atomic_load(&a->J));
    CHECK(a->timer == NULL && a->construction->timer_candidate != NULL &&
          a->loading_screen == NULL);
    mc_timer_constructor_state timer;
    CHECK(mc_timer_constructor_inspect(a->construction->timer_candidate, &timer));
    CHECK(timer.phase == MC_TIMER_NATIVE_FAILED && timer.last_store_pc == 28 &&
          timer.fields.a == 20.0f);
    CHECK(timer.fields.d == 1.0f && timer.fields.j == 1.0 && timer.fields.c == 0.0f &&
          timer.fields.e == 0.0f);
    CHECK(finish_app(&f, &a) == 0 && fixture_close(&f) == 0);
    return 0;
}

static int retained_sound_prefix(const char *root) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    const mc_java_random_constructor_input input = {
        atomic_load(&f.services.profile->uniquifier) * UINT64_C(1181783497276652981), INT64_C(7)};
    /* One declared primitive Random constructor input: Notice's actual bb.f.
     * Exhaustion occurs in yo's first real hr, without replacing either child. */
    CHECK(mc_java_random_profile_bind_constructor_inputs(f.services.profile, &input, 1) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    client_application *a = NULL;
    const mc_client_application_constructor_arguments args = {.width = 854, .height = 480};
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &a, f.error) ==
          MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_construct(a, &args, f.error) ==
          MC_CLIENT_APP_CHILD_FAILED);
    CHECK(f.error->child == MC_CLIENT_APP_CHILD_SOUND && f.error->minecraft_pc == 108);
    CHECK(a->B == NULL && a->construction->sound_candidate != NULL && !atomic_load(&a->J));
    mc_sound_manager_constructor_state state;
    CHECK(mc_sound_manager_read_construction(a->construction->sound_candidate, &state));
    CHECK(state.phase == MC_SOUND_MANAGER_FAILED && state.pending_pool != NULL &&
          state.published_pool_mask == 0);
    CHECK(state.failure.frame_count == 2 && strcmp(state.failure.frames[0].class_name, "yo") == 0);
    CHECK(strcmp(state.failure.frames[1].class_name, "hr") == 0);
    CHECK(!state.pending_prefix.random_published && state.pools[0] == NULL);
    CHECK(finish_app(&f, &a) == 0 && fixture_close(&f) == 0);
    return 0;
}

static int source_global_failure(const char *root) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    client_application *a = NULL, *b = NULL;
    const mc_client_application_constructor_arguments args = {.width = 854, .height = 480};
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &a, f.error) ==
          MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_construct(a, &args, f.error) == MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &b, f.error) ==
          MC_CLIENT_APP_OK);
    /* Explicit native counter-limit fault, not a source field or child result. */
    atomic_store(&b->source_static_holds, SIZE_MAX);
    CHECK(mc_client_application_constructor_construct(b, &args, f.error) ==
          MC_CLIENT_APP_CHILD_FAILED);
    CHECK(f.error->minecraft_pc == 264 &&
          f.error->cause.owner.status == MC_CLIENT_APPLICATION_OWNER_LIMIT);
    CHECK(b->constructor_phase == MC_CLIENT_APP_FAILED && atomic_load(&b->J) &&
          b->construction->worker != NULL);
    CHECK(b->construction->last_source_store == 260 && !b->n && b->d == 854 && b->e == 480);
    mc_client_application_class_state state;
    CHECK(mc_client_application_classes_inspect(f.services.application_classes, &state));
    CHECK(state.a == a && state.native_app_borrows == 2 &&
          atomic_load(&a->source_static_holds) == 1);
    atomic_store(&b->source_static_holds, 0); /* End the bounded native fault. */
    CHECK(mc_client_application_constructor_retire(&b, f.error) == MC_CLIENT_APP_OK && b == NULL);
    CHECK(finish_app(&f, &a) == 0 && fixture_close(&f) == 0);
    return 0;
}

static int publication_helper_policy(const char *root) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    client_application *a = NULL;
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &a, f.error) ==
          MC_CLIENT_APP_OK);
    /* A declared prepared-state fixture for the private publication kernel.
     * No whole constructor, children or constructor return is claimed here. */
    a->constructor_phase = MC_CLIENT_APP_CONSTRUCTING;
    CHECK(mc_client_application_classes_publish(f.services.application_classes, a, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(atomic_load(&a->source_static_holds) == 1);
    atomic_store(&a->source_static_holds, SIZE_MAX);
    mc_client_application_owner_error error;
    CHECK(mc_client_application_classes_publish(f.services.application_classes, a, &error) ==
          MC_CLIENT_APPLICATION_OWNER_LIMIT);
    CHECK(atomic_load(&a->source_static_holds) == SIZE_MAX);
    atomic_store(&a->source_static_holds, 1);
    CHECK(mc_client_application_classes_publish(f.services.application_classes, a, &error) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(atomic_load(&a->source_static_holds) == 1);
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(a != NULL && a->constructor_phase == MC_CLIENT_APP_CONSTRUCTING &&
          a->source_namespace_borrowed);
    a->constructor_phase = MC_CLIENT_APP_RAW; /* Restore prepared native phase. */
    a->water_effect_constructing = true;
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(a != NULL && a->constructor_phase == MC_CLIENT_APP_RAW && a->source_namespace_borrowed);
    a->water_effect_constructing = false;
    a->construction->retirement_active = true;
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(a->constructor_phase == MC_CLIENT_APP_RAW && a->source_namespace_borrowed);
    a->construction->retirement_active = false;
    CHECK(finish_app(&f, &a) == 0 && fixture_close(&f) == 0);
    return 0;
}

static int lava_constructor_seam(const char *root) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    mc_client_lava_texture_effect *effect = NULL, *legacy = NULL;
    mc_lava_texture_error error;
    CHECK(mc_client_lava_texture_allocate(f.services.registry, f.services.profile, NULL, NULL,
                                          &legacy, &error) == MC_LAVA_TEXTURE_INVALID_ARGUMENT &&
          legacy == NULL);
    CHECK(mc_client_lava_texture_allocate_constructor(f.services.registry, f.services.profile, NULL,
                                                      &effect, &error) == MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_construct(effect, &error) == MC_LAVA_TEXTURE_OK);
    CHECK(atomic_load(&f.services.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD);
    mc_lava_texture_state before, after;
    CHECK(mc_client_lava_texture_read(effect, &before) && before.phase == MC_LAVA_TEXTURE_READY);
    CHECK(before.array_bytes == 5120 && before.fields.a_length == 1024 &&
          before.fields.g_length == 256);
    const uint64_t random_before = atomic_load(&f.services.profile->uniquifier);
    CHECK(mc_client_lava_texture_animate(effect, &error) == MC_LAVA_TEXTURE_INVALID_PHASE);
    CHECK(error.source_pc == -1 && error.cg_pc == -1 && error.source_class == NULL);
    CHECK(mc_client_lava_texture_read(effect, &after) && after.fields.g == before.fields.g);
    CHECK(atomic_load(&f.services.profile->uniquifier) == random_before);
    mc_math_trig *trig = malloc(sizeof(*trig));
    CHECK(trig != NULL && mc_math_trig_init(trig));
    const unsigned fp = _mm_getcsr();
    _mm_setcsr(fp | 0x8000u);
    const mc_lava_texture_status bound = mc_client_lava_texture_bind_trig(effect, trig, &error);
    _mm_setcsr(fp);
    CHECK(bound == MC_LAVA_TEXTURE_OK &&
          atomic_load(&f.services.profile->uniquifier) == random_before);
    CHECK(atomic_load(&f.services.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD);
    CHECK(mc_client_lava_texture_bind_trig(effect, trig, &error) == MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_allocate(f.services.registry, f.services.profile, trig, NULL,
                                          &legacy, &error) == MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_construct(legacy, &error) == MC_LAVA_TEXTURE_OK);
    CHECK(mc_client_lava_texture_read(legacy, &after));
    CHECK(after.fields.b == before.fields.b && after.fields.d == before.fields.d &&
          after.fields.e == before.fields.e);
    CHECK(memcmp(after.fields.a, before.fields.a, 1024) == 0 &&
          memcmp(after.fields.g, before.fields.g, 1024) == 0);
    CHECK(memcmp(after.fields.h, before.fields.h, 1024) == 0 &&
          memcmp(after.fields.i, before.fields.i, 1024) == 0);
    CHECK(memcmp(after.fields.j, before.fields.j, 1024) == 0);
    CHECK(mc_client_lava_texture_animate(effect, &error) == MC_LAVA_TEXTURE_OK);
    CHECK(atomic_load(&f.services.profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_READY);
    CHECK(mc_client_lava_texture_retire(&effect, &error) == MC_LAVA_TEXTURE_OK && effect == NULL);
    CHECK(mc_client_lava_texture_retire(&legacy, &error) == MC_LAVA_TEXTURE_OK && legacy == NULL);
    free(trig);
    CHECK(fixture_close(&f) == 0);
    return 0;
}

/* Optional genuine later-owner profile. All Apps here are actual RAW owners;
 * attaching a Display/splash is a native ownership fixture, not execution of
 * Minecraft construction/startup. Run identical controls with old/new bodies. */
static bool later_owner_cleanup(fixture *f, client_application **app, mc_client_display **display,
                                mc_client_string *splash, mc_client_assets **assets) {
    bool ok = true;
    if (*display != NULL) {
        mc_client_display_error error;
        const bool destroyed = mc_client_display_destroy(*display, &error);
        ok = destroyed;
        /* Actual by-value destructor: BUSY preserves, terminal errors consume. */
        if (destroyed || error.status != MC_CLIENT_DISPLAY_BUSY) {
            *display = NULL;
            if (*app != NULL)
                (*app)->display = NULL;
        }
    }
    mc_client_string_free(splash);
    if (*app != NULL)
        (*app)->splash = *splash; /* Clear only after genuine free. */
    mc_client_assets_free(*assets);
    *assets = NULL;
    if (*display == NULL && *app != NULL)
        ok = mc_client_application_constructor_retire(app, f->error) == MC_CLIENT_APP_OK && ok;
    if (*app == NULL && *display == NULL)
        ok = fixture_close(f) == 0 && ok;
    else
        ok = false; /* Keep actual BUSY owners, without claiming cleanup. */
    return ok;
}

static int later_owner_case(const char *root, unsigned kind) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    client_application *app = NULL;
    mc_client_display *saved_display = NULL;
    mc_client_string saved_splash = {0};
    mc_client_assets *assets = NULL;
    if (mc_client_application_constructor_allocate(&f.services, NULL, &app, f.error) !=
        MC_CLIENT_APP_OK) {
        fprintf(stderr, "LATER-OWNER %u actual RAW App refused\n", kind);
        (void)fixture_close(&f);
        return 1;
    }
    if (raw_fields(app) != 0) {
        (void)later_owner_cleanup(&f, &app, &saved_display, &saved_splash, &assets);
        return 1;
    }
    if ((kind & 1u) != 0) {
        if (!mc_client_display_create("App later-owner retirement control", 64, 64, true,
                                      &saved_display, &app->display_error)) {
            fprintf(stderr,
                    "LATER-OWNER %u real Display refused status=%d operation=%s detail=%s "
                    "gl_error=%" PRIu32 "\n",
                    kind, (int)app->display_error.status,
                    app->display_error.operation == NULL ? "<none>" : app->display_error.operation,
                    app->display_error.detail, app->display_error.gl_error);
            const bool cleaned =
                later_owner_cleanup(&f, &app, &saved_display, &saved_splash, &assets);
            fprintf(stderr, "LATER-OWNER-REFUSAL-CLEANUP %u ok=%d\n", kind, (int)cleaned);
            return 1; /* Genuine refusal, never positive/skipped parity. */
        }
        if (saved_display == NULL || mc_client_display_info_get(saved_display) == NULL) {
            (void)later_owner_cleanup(&f, &app, &saved_display, &saved_splash, &assets);
            return 1;
        }
        app->display = saved_display; /* One genuine owned occurrence, saved alias only. */
    }
    if ((kind & 2u) != 0) {
        const mc_client_assets_status created = mc_client_assets_create(
            mc_resource_files_source(f.files), f.services.registry, &assets);
        if (created != MC_CLIENT_ASSETS_OK) {
            fprintf(stderr, "LATER-OWNER %u actual Assets refused status=%d\n", kind, (int)created);
            (void)later_owner_cleanup(&f, &app, &saved_display, &saved_splash, &assets);
            return 1;
        }
        mc_java_random random;
        mc_java_random_seed(&random, INT64_C(17)); /* Explicit native Assets fixture. */
        mc_client_assets_error asset_error;
        const mc_client_assets_status selected =
            mc_client_assets_choose_splash(assets, &random, &saved_splash, &asset_error);
        if (selected != MC_CLIENT_ASSETS_OK) {
            fprintf(stderr, "LATER-OWNER %u actual splash refused status=%d\n", kind,
                    (int)selected);
            (void)later_owner_cleanup(&f, &app, &saved_display, &saved_splash, &assets);
            return 1;
        }
        if (saved_splash.units == NULL) {
            (void)later_owner_cleanup(&f, &app, &saved_display, &saved_splash, &assets);
            return 1;
        } /* Real separate owner, including owned missingno. */
        app->splash = saved_splash;
    }
    /* Capture the actual observed failure BEFORE cleanup. The old body can free
     * App here. Saved owners allow genuine cleanup without reading freed App. */
    const mc_client_application_construction_status observed =
        mc_client_application_constructor_retire(&app, f.error);
    const bool app_preserved = app != NULL;
    const bool retained_prefix =
        app_preserved && app->constructor_phase == MC_CLIENT_APP_RETIRING &&
        app->source_namespace_borrowed && app->construction->source_stores == 0 &&
        !atomic_load(&app->J) && app->construction->registry_hold &&
        app->construction->entity_hold && app->construction->render_hold &&
        app->construction->gui_hold && app->display == saved_display &&
        app->splash.units == saved_splash.units;
    const bool error_preserved = f.error->status == MC_CLIENT_APP_UNIMPLEMENTED &&
                                 f.error->child == MC_CLIENT_APP_CHILD_PLATFORM &&
                                 f.error->minecraft_pc == -1;
    printf("LATER-OWNER %u observed=%d app=%d prefix=%d error=%d\n", kind, (int)observed,
           (int)app_preserved, (int)retained_prefix, (int)error_preserved);

    const bool cleanup_ok = later_owner_cleanup(&f, &app, &saved_display, &saved_splash, &assets);
    printf("LATER-OWNER-CLEANUP %u app=%d display=%d splash=%d ok=%d\n", kind, app != NULL,
           saved_display != NULL, saved_splash.units != NULL, (int)cleanup_ok);
    CHECK(cleanup_ok);
    CHECK(observed == MC_CLIENT_APP_UNIMPLEMENTED && app_preserved && retained_prefix &&
          error_preserved);
    return 0;
}

static int later_owners(const char *root) {
    int failures = 0;
    /* Preserve all three actual old/new observations before final failure. */
    for (unsigned kind = 1; kind <= 3; ++kind)
        failures += later_owner_case(root, kind);
    printf("Later-owner profile: %u checks; %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[2], "later-owners") == 0)
        return later_owners(argv[1]);
    CHECK(argc == 2);
    CHECK(allocation_and_prefixes(argv[1]) == 0);
    CHECK(global_and_lifetimes(argv[1]) == 0);
    CHECK(retained_mk_and_stop(argv[1]) == 0);
    CHECK(native_fp_prefix(argv[1]) == 0);
    CHECK(retained_sound_prefix(argv[1]) == 0);
    CHECK(source_global_failure(argv[1]) == 0);
    CHECK(publication_helper_policy(argv[1]) == 0);
    CHECK(lava_constructor_seam(argv[1]) == 0);
    printf("Actual App constructor contracts: %u checks\n", checks);
    return 0;
}
