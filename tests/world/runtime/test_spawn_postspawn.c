/* Actual CQ postspawn helper over genuine published Mob/World components. */
int mc_postspawn_unused_random_main(int, char **);
#define main mc_postspawn_unused_random_main
#include "test_world_random_tick.c"
#undef main

#include "../../../src/entity/mob/internal.h"
#include "../../../src/world/runtime/spawn_postspawn_internal.h"
#include "../../../src/world/runtime/world_access_internal.h"

typedef struct post_fixture {
    random_fixture base;
    mc_world_spawn_postspawn_work work;
    mc_entity *actor, *jockey;
    mc_world_access_receipt *receipt;
    size_t added;
    bool refuse, reentry, callback_bad, capture_limit;
} post_fixture;

static bool post_actor(post_fixture *f, mc_entity_class_kind kind) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->base.entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(f->base.entities, f->base.registry, literal, &f->actor, &error) ==
              MC_MOB_OK &&
          mc_mob_construct(f->actor, f->base.world, &error) == MC_MOB_OK &&
          mc_entity_set_position(f->actor, 8.5, 64, 8.5) == MC_ENTITY_OK);
    f->actor->base.yaw = 37.0f;
    bool published = false;
    CHECK(mc_world_runtime_insert_entity(f->base.world, f->actor, &published, NULL) ==
              MC_WORLD_STORAGE_OK &&
          published);
    return true;
}

static mc_world_storage_status post_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                          mc_world_storage_error *error) {
    post_fixture *f = context;
    ++f->added;
    f->callback_bad = f->callback_bad || world != f->base.world || !f->work.active ||
                      f->work.pc != 49 || entity != f->work.jockey;
    mc_world_storage_error cleanup, admission;
    const mc_entity *const actual = f->work.jockey;
    const bool retired =
        mc_world_spawn_postspawn_retire(&f->work, &cleanup) == MC_WORLD_STORAGE_BUSY;
    const bool begun =
        mc_world_spawn_postspawn_begin(world, f->base.registry, f->actor, 0, 0, 0, &f->work,
                                       &admission) == MC_WORLD_STORAGE_BUSY;
    f->reentry = retired && begun && f->work.active && f->work.jockey == actual;
    if (f->refuse) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Authored postspawn publication refusal", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status post_added_source(void *context, mc_world_runtime *world,
                                                 mc_entity *entity, mc_world_storage_error *error,
                                                 mc_world_access_failure *failure) {
    post_fixture *f = context;
    const mc_world_storage_status status = post_added(context, world, entity, error);
    const mc_world_storage_error original = *error;
    const mc_world_storage_status receipt =
        mc_world_access_receipt_create(f->base.access, NULL, &f->receipt, failure, error);
    if (receipt != MC_WORLD_STORAGE_OK)
        return receipt;
    f->receipt->exception_class = "java/lang/IllegalStateException";
    if (f->capture_limit)
        f->receipt->holds = SIZE_MAX;
    *error = original;
    return status;
}

static bool post_teardown(post_fixture *f) {
    CHECK(mc_world_spawn_postspawn_retire(&f->work, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_access_receipt_retire(&f->receipt, NULL) == MC_WORLD_STORAGE_OK);
    if (f->jockey != NULL && f->jockey->base.aH != NULL) {
        mc_entity *scope[] = {f->jockey, f->actor};
        mc_entity_mount_error error;
        CHECK(mc_entity_mount(f->jockey, NULL, scope, COUNT(scope), &error) == MC_ENTITY_OK);
    }
    mc_entity_release(f->jockey);
    mc_entity_release(f->actor);
    f->jockey = f->actor = NULL;
    if (f->base.access != NULL) {
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->base.world, f->base.access, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed && mc_world_access_free(f->base.access, NULL) == MC_WORLD_STORAGE_OK);
        f->base.access = NULL;
    }
    return teardown(&f->base);
}

static bool post_case(const char *resources, const char *directory, unsigned int mode) {
    post_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && setup(&f->base, resources, directory));
    mc_chunk_state *chunk = NULL;
    CHECK(load_chunk(&f->base, 0, 0, &chunk) &&
          mc_chunk_cache_publish(f->base.provider.cache, 0, 0, chunk));
    const mc_entity_class_kind kind = mode < 4 || mode == 12 || mode == 13 || mode >= 15
                                          ? MC_ENTITY_CLASS_KIND_SPIDER
                                      : mode < 10 || mode == 14 ? MC_ENTITY_CLASS_KIND_SHEEP
                                                                : MC_ENTITY_CLASS_KIND_COW;
    CHECK(post_actor(f, kind));
    static const int64_t seeds[] = {0, 18, 18, 18, 18, 2, 6, 14, 238, 0, 0, 0, 0, 0, 0, 18, 18};
    mc_java_random_reseed(f->base.world->fields.r, seeds[mode]);
    mc_java_random expected = *f->base.world->fields.r;
    const mc_java_random own = *f->actor->random;
    mc_java_random *const world_random = f->base.world->fields.r;
    const size_t entities = f->base.world->fields.b->count;
    mc_world_storage_error admission, cleanup;
    if (mode == 1 || mode == 2) {
        const mc_world_access_callbacks callbacks = {.entity_added = post_added};
        CHECK(mc_world_access_create(&callbacks, f, &f->base.access, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(f->base.world, f->base.access, NULL) ==
                  MC_WORLD_STORAGE_OK);
        f->refuse = mode == 2;
    }
    if (mode >= 15) {
        const mc_world_access_source_callbacks callbacks = {.entity_added = post_added_source};
        CHECK(mc_world_access_source_create(&callbacks, f, &f->base.access, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(f->base.world, f->base.access, NULL) ==
                  MC_WORLD_STORAGE_OK);
        f->refuse = true;
        f->capture_limit = mode == 16;
    }
    int32_t draw = -1;
    if (mode < 10 || mode >= 15)
        CHECK(mc_java_random_next_bounded(&expected, 100, &draw));
    static const int32_t colors[] = {15, 7, 8, 12, 6, 0};
    if (mode >= 4 && mode < 10 && (mode == 8 || mode == 9))
        CHECK(mc_java_random_next_bounded(&expected, 500, &draw));
    if (mode == 13)
        f->base.world->fields.r = NULL;
    const mc_world_storage_status status = mc_world_spawn_postspawn_begin(
        mode == 10 || mode == 12 || mode == 14 ? NULL : f->base.world,
        mode == 3 ? NULL : f->base.registry, mode == 11 ? NULL : f->actor, 8.5f, 64.0f, 8.5f,
        &f->work, &admission);
    if (mode == 13)
        f->base.world->fields.r = world_random;
    CHECK(admission.status == MC_WORLD_STORAGE_OK && !f->work.active && f->work.entered &&
          memcmp(f->actor->random, &own, sizeof(own)) == 0 &&
          memcmp(f->base.world->fields.r, &expected, sizeof(expected)) == 0);
    if (mode >= 15) {
        CHECK(status == MC_WORLD_STORAGE_IO_ERROR && !f->work.returned && !f->work.native_policy &&
              f->work.exception_class != NULL &&
              strcmp(f->work.exception_class, "java/lang/IllegalStateException") == 0 &&
              f->work.pc == 49 && f->receipt != NULL && f->added == 1 && f->reentry &&
              !f->callback_bad && f->work.jockey != NULL && f->work.jockey->base.aH == NULL);
        const mc_world_storage_status first = f->work.status;
        const char *const operation = f->work.operation;
        if (mode == 15) {
            CHECK(!f->work.capture_pending && f->work.retained_publication.receipt == f->receipt &&
                  f->receipt->holds == 2 &&
                  mc_world_access_receipt_retire(&f->receipt, &cleanup) == MC_WORLD_STORAGE_BUSY &&
                  f->receipt != NULL);
        } else {
            CHECK(f->work.capture_pending &&
                  f->work.capture_error.status == MC_WORLD_STORAGE_LIMIT &&
                  f->work.retained_publication.receipt == NULL &&
                  mc_world_spawn_postspawn_retire(&f->work, &cleanup) == MC_WORLD_STORAGE_LIMIT &&
                  f->work.entered && f->work.status == first && f->work.operation == operation &&
                  f->added == 1);
            f->receipt->holds = 1;
        }
        CHECK(mc_world_spawn_postspawn_retire(&f->work, &cleanup) == MC_WORLD_STORAGE_OK &&
              !f->work.entered && f->work.publication.world_access_failure.receipt == NULL &&
              f->work.retained_publication.receipt == NULL && f->receipt->holds == 1 &&
              f->work.status == first && f->work.operation == operation && f->added == 1 &&
              mc_world_access_receipt_retire(&f->receipt, &cleanup) == MC_WORLD_STORAGE_OK &&
              f->receipt == NULL);
    } else if (mode >= 12) {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !f->work.returned &&
              !f->work.native_policy && f->work.exception_class != NULL &&
              strcmp(f->work.exception_class, "java/lang/NullPointerException") == 0 &&
              f->work.pc == (mode == 12   ? 8u
                             : mode == 13 ? 13u
                                          : 74u) &&
              f->work.random_calls == 0 && f->work.jockey == NULL &&
              f->base.world->fields.b->count == entities);
    } else if (mode == 2) {
        CHECK(status == MC_WORLD_STORAGE_IO_ERROR && !f->work.returned && f->work.pc == 49 &&
              f->work.jockey != NULL && f->work.jockey->base.aH == NULL &&
              f->actor->base.aG == NULL && f->added == 1 && f->reentry && !f->callback_bad &&
              f->base.world->fields.b->count == entities + 1 && f->work.publications == 0);
        const mc_world_storage_status first = f->work.status;
        const char *const operation = f->work.operation;
        const size_t holds = f->work.jockey->holds;
        f->work.jockey->holds = 0;
        const mc_world_storage_status refused = mc_world_spawn_postspawn_retire(&f->work, &cleanup);
        f->work.jockey->holds = holds;
        CHECK(refused == MC_WORLD_STORAGE_UNSUPPORTED_STATE && f->work.entered &&
              f->work.status == first && f->work.operation == operation && f->added == 1);
        f->jockey = f->work.jockey;
        CHECK(mc_entity_retain(f->jockey) == MC_ENTITY_OK &&
              mc_world_spawn_postspawn_retire(&f->work, &cleanup) == MC_WORLD_STORAGE_OK &&
              !f->work.entered && f->work.jockey == NULL && f->work.status == first &&
              f->work.operation == operation && f->added == 1 && f->jockey->base.aH == NULL);
    } else if (mode == 3) {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !f->work.returned &&
              f->work.pc == 19 && f->work.jockey == NULL &&
              f->base.world->fields.b->count == entities);
    } else {
        CHECK(status == MC_WORLD_STORAGE_OK && f->work.returned && f->work.pc == 83);
        if (mode == 1) {
            f->jockey = f->work.jockey;
            CHECK(f->jockey != NULL && mc_entity_retain(f->jockey) == MC_ENTITY_OK &&
                  mc_entity_is_kind(f->jockey, MC_ENTITY_CLASS_KIND_SKELETON) &&
                  f->jockey->base.aH == f->actor && f->actor->base.aG == f->jockey &&
                  f->jockey->base.x == 8.5 && f->jockey->base.z == 8.5 &&
                  f->jockey->base.yaw == 37.0f && f->work.publications == 1 &&
                  f->base.world->fields.b->count == entities + 1 && f->added == 1 && f->reentry &&
                  !f->callback_bad);
        } else {
            CHECK(f->work.jockey == NULL && f->work.publications == 0 &&
                  f->base.world->fields.b->count == entities);
            if (mode >= 4 && mode < 10) {
                mc_mob_state sheep;
                CHECK(mc_mob_inspect(f->actor, &sheep) &&
                      ((uint8_t)sheep.watcher_16 & 15u) == (uint8_t)colors[mode - 4] &&
                      f->work.random_calls == (mode >= 8 ? 2u : 1u));
            }
        }
    }
    CHECK(post_teardown(f));
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    for (unsigned int mode = 0; mode < 17; ++mode) {
        char directory[8192];
        const int length = snprintf(directory, sizeof(directory), "%s/case%u", argv[2], mode);
        if (length <= 0 || (size_t)length >= sizeof(directory) ||
            !post_case(argv[1], directory, mode))
            return 1;
    }
    printf("World postspawn: %u checks\n", checks);
    return 0;
}
