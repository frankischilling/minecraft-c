/* Native ownership controls. The local source inclusion exposes the retained
 * coordinator's private join without adding any production test callback.
 * Aliases keep this copy separate from the library's complete CQ entry. */
#define mc_world_nightmare_classes_ensure mc_eligibility_classes_ensure
#define mc_world_nightmare_default_limits mc_eligibility_default_limits
#define mc_world_nightmare_begin mc_eligibility_begin
#define mc_world_nightmare_inspect mc_eligibility_inspect
#define mc_world_nightmare_retire mc_eligibility_retire
#include "../../../src/world/runtime/nightmare.c"
#undef mc_world_nightmare_classes_ensure
#undef mc_world_nightmare_default_limits
#undef mc_world_nightmare_begin
#undef mc_world_nightmare_inspect
#undef mc_world_nightmare_retire

int mc_eligibility_unused_spawn_main(int, char **);
#define main mc_eligibility_unused_spawn_main
#include "../../entity/test_mob_spawning.c"
#undef main

#include "../../../src/world/runtime/world_access_internal.h"

typedef struct eligibility_receiver {
    mc_world_access *access;
    mc_world_access_receipt *receipt;
    size_t calls;
} eligibility_receiver;

static mc_world_storage_status eligibility_added(void *context, mc_world_runtime *world,
                                                 mc_entity *entity, mc_world_storage_error *error,
                                                 mc_world_access_failure *failure) {
    eligibility_receiver *receiver = context;
    (void)world;
    (void)entity;
    ++receiver->calls;
    const mc_world_storage_status status =
        mc_world_access_receipt_create(receiver->access, NULL, &receiver->receipt, failure, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    receiver->receipt->exception_class = "java/lang/IllegalStateException";
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Authored eligibility source callback failure", 71};
    return error->status;
}

static mc_world_nightmare_call *eligibility_owner(void) {
    mc_world_nightmare_call *c = calloc(1, sizeof(*c));
    if (c != NULL) {
        c->state.source_pc = 394;
        c->current_child = MC_WORLD_NIGHTMARE_ELIGIBILITY;
        c->state.failure = &c->error;
        c->state.cleanup = &c->cleanup;
    }
    return c;
}

/* These envelopes deliberately author only the already-returned child join.
 * They do not claim a full CQ/loader/activation execution or original result. */
static mc_mob_spawn_failure_view eligibility_envelope(mc_world_nightmare_call *c,
                                                      mc_mob_spawn_child child,
                                                      mc_world_storage_error cause) {
    mc_mob_spawn_error *e = &c->eligibility_error;
    e->status = cause.status;
    e->operation = cause.operation;
    e->native_policy = true;
    e->site = (mc_mob_site){"ls", "d", "()Z", 8};
    e->child = (mc_mob_site){"fd", "a", "(Leq;)Z", 3};
    e->active_child = child;
    mc_world_query_error *query = NULL;
    switch (child) {
        case MC_MOB_SPAWN_CHILD_QUERY:
            query = &e->detail.query;
            break;
        case MC_MOB_SPAWN_CHILD_BRIGHTNESS:
            e->detail.brightness.status = cause.status;
            e->detail.brightness.operation = cause.operation;
            e->detail.brightness.cause = cause;
            e->detail.brightness.query_reached = true;
            query = &e->detail.brightness.query;
            break;
        case MC_MOB_SPAWN_CHILD_NO_ENTITY:
            e->detail.no_entity.status = cause.status;
            e->detail.no_entity.operation = cause.operation;
            query = &e->detail.no_entity.nearby.query;
            break;
        case MC_MOB_SPAWN_CHILD_COLLISION:
            query = &e->detail.collision.nearby.query;
            break;
        case MC_MOB_SPAWN_CHILD_MATERIAL:
            e->detail.material.status = cause.status;
            e->detail.material.operation = cause.operation;
            e->detail.material.cause = cause;
            query = &e->detail.material.block.nearby.query;
            break;
        case MC_MOB_SPAWN_CHILD_ACQUISITION:
            e->detail.acquisition.status = MC_CHUNK_ACQUIRE_CALLBACK_FAILED;
            e->detail.acquisition.cause = cause;
            break;
        case MC_MOB_SPAWN_CHILD_NONE:
        case MC_MOB_SPAWN_CHILD_CHUNK_RANDOM:
            break;
    }
    if (query != NULL) {
        query->status = cause.status;
        query->operation = cause.operation;
        query->cause = cause;
    }
    return mc_mob_spawn_error_project(e);
}

static bool eligibility_loader(mc_mob_spawn_child child) {
    mc_world_nightmare_call *c = eligibility_owner();
    CHECK(c != NULL);
    const mc_world_storage_error cause = {MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Authored eligibility loader envelope", 19};
    const mc_mob_spawn_failure_view view = eligibility_envelope(c, child, cause);
    mc_world_source_chunk_error producer = {0};
    producer.source.status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    producer.source.source_exception = true;
    producer.source.exception_class = "java/lang/NullPointerException";
    producer.source.operation = cause.operation;
    producer.source.frames[0] = (mc_world_source_site){"ld", "a", "(Lfd;II)Llm;", 11};
    producer.source.frame_count = 1;
    producer.storage = cause;
    const mc_world_source_chunk_error **borrow =
        view.query != NULL ? &view.query->source_chunk : &view.acquisition->source_chunk;
    *borrow = &producer;
    CHECK(!eligibility_failed(c, cause.status) && c->eligibility_source_copied &&
          *borrow == &c->eligibility_source && *borrow != &producer &&
          c->error.eligibility == &c->eligibility_error && c->error.pc == 394 &&
          c->error.child_pc == 8 && c->error.cause.system_error == 19 &&
          c->error.source_exception && !c->error.native_policy &&
          strcmp(c->error.exception_class, "java/lang/NullPointerException") == 0);
    const mc_world_nightmare_error original = c->error;
    memset(&producer, 0, sizeof(producer));
    c->state.active = true;
    CHECK(mc_eligibility_retire(&c, NULL) == MC_WORLD_STORAGE_BUSY && c != NULL &&
          memcmp(&c->error, &original, sizeof(original)) == 0 &&
          (*borrow)->source.frames[0].pc == 11 && (*borrow)->storage.system_error == 19);
    c->state.active = false;
    CHECK(mc_eligibility_retire(&c, NULL) == MC_WORLD_STORAGE_OK && c == NULL);
    return true;
}

static bool eligibility_receipt(mc_mob_spawn_child child, unsigned int slot, bool ceiling) {
    eligibility_receiver receiver = {0};
    const mc_world_access_source_callbacks callbacks = {.entity_added = eligibility_added};
    CHECK(mc_world_access_source_create(&callbacks, &receiver, &receiver.access, NULL) ==
          MC_WORLD_STORAGE_OK);
    mc_world_storage_error cause;
    mc_world_access_failure actual;
    CHECK(mc_world_access_call_entity_added(receiver.access, NULL, NULL, &cause, &actual) ==
              MC_WORLD_STORAGE_IO_ERROR &&
          receiver.calls == 1 && actual.receipt == receiver.receipt);
    mc_world_nightmare_call *c = eligibility_owner();
    CHECK(c != NULL);
    const mc_mob_spawn_failure_view view = eligibility_envelope(c, child, cause);
    mc_world_entity_batch_error *batch =
        view.nearby != NULL ? &view.nearby->batch : &view.acquisition->batch;
    mc_live_tile_entity_error *tile =
        view.nearby != NULL ? &view.nearby->tile_entity : &view.acquisition->tile_entity;
    if (slot == 0) {
        batch->publication.status = cause.status;
        batch->publication.operation = cause.operation;
        batch->publication.world_access_failure = actual;
    } else if (slot == 1) {
        tile->status = cause.status;
        tile->operation = cause.operation;
        tile->world_access_failure = actual;
    } else {
        mc_world_entity_error *publication =
            slot == 2 ? &tile->publication : &tile->batch.publication;
        publication->status = cause.status;
        publication->operation = cause.operation;
        publication->world_access_failure = actual;
    }
    if (ceiling)
        receiver.receipt->holds = SIZE_MAX;
    CHECK(!eligibility_failed(c, cause.status) && c->error.source_exception &&
          !c->error.native_policy && c->error.cause.system_error == 71 &&
          strcmp(c->error.exception_class, "java/lang/IllegalStateException") == 0 &&
          c->eligibility_capture_pending == ceiling && c->receipt_count == (ceiling ? 0u : 1u));
    const mc_world_nightmare_error original = c->error;
    CHECK(mc_world_access_receipt_retire(&receiver.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
          receiver.receipt != NULL);
    if (ceiling) {
        mc_world_nightmare_cleanup cleanup;
        CHECK(mc_eligibility_retire(&c, &cleanup) == MC_WORLD_STORAGE_LIMIT && c != NULL &&
              cleanup.child == MC_WORLD_NIGHTMARE_ELIGIBILITY &&
              memcmp(&c->error, &original, sizeof(original)) == 0 && receiver.calls == 1);
        receiver.receipt->holds = 1;
    }
    CHECK(mc_eligibility_retire(&c, NULL) == MC_WORLD_STORAGE_OK && c == NULL &&
          receiver.receipt->holds == 1 && receiver.calls == 1 &&
          mc_world_access_receipt_retire(&receiver.receipt, NULL) == MC_WORLD_STORAGE_OK &&
          receiver.receipt == NULL &&
          mc_world_access_free(receiver.access, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool eligibility_inactive(void) {
    const mc_world_storage_error old = {MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                        "Authored prior query failure", 0};
    for (unsigned int mode = 0; mode < 2; ++mode) {
        mc_world_nightmare_call *c = eligibility_owner();
        CHECK(c != NULL);
        const mc_mob_spawn_failure_view view =
            eligibility_envelope(c, MC_MOB_SPAWN_CHILD_MATERIAL, old);
        mc_world_source_chunk_error producer = {0};
        producer.source.status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
        producer.source.source_exception = true;
        producer.source.exception_class = "java/lang/NullPointerException";
        view.query->source_chunk = &producer;
        c->eligibility_error.status = MC_WORLD_STORAGE_LIMIT;
        c->eligibility_error.operation = "Authored eligibility native policy ceiling";
        if (mode == 0)
            c->eligibility_error.active_child = MC_MOB_SPAWN_CHILD_NONE;
        else {
            c->eligibility_error.detail.material.status = MC_WORLD_STORAGE_LIMIT;
            c->eligibility_error.detail.material.operation = c->eligibility_error.operation;
            c->eligibility_error.detail.material.cause =
                (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, c->eligibility_error.operation, 0};
            c->eligibility_error.detail.material.registry.status = MC_REGISTRY_NULL_REFERENCE;
        }
        CHECK(!eligibility_failed(c, MC_WORLD_STORAGE_LIMIT) && c->error.native_policy &&
              !c->error.source_exception && !c->error.source_error &&
              c->error.exception_class == NULL && !c->eligibility_source_copied &&
              c->receipt_count == 0 && !c->eligibility_capture_pending);
        CHECK(mc_eligibility_retire(&c, NULL) == MC_WORLD_STORAGE_OK && c == NULL);
    }
    return true;
}

static bool eligibility_actual(const char *resources, const char *directory) {
    spawn_fixture f = {0};
    CHECK(spawn_setup(&f, resources, directory, 23, 512));
    mc_entity *actor = NULL;
    CHECK(spawn_actor(&f, 0, &actor) &&
          mc_entity_set_position(actor, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
    mc_world_nightmare_call *c = eligibility_owner();
    CHECK(c != NULL && mc_entity_retain(actor) == MC_ENTITY_OK);
    c->context = (mc_world_nightmare_context){f.world_classes, f.world, f.math};
    c->state.mob = actor;
    bool out = true;
    mc_world_chunk_nibble *sky = f.chunk->f;
    f.chunk->f = NULL;
    const bool returned = eligible(c, &out);
    f.chunk->f = sky;
    CHECK(!returned && out && c->eligibility_error.active_child == MC_MOB_SPAWN_CHILD_QUERY &&
          c->error.eligibility == &c->eligibility_error && c->error.pc == 394 &&
          c->error.child_pc == 37 && c->eligibility_error.site.pc == 37 &&
          c->eligibility_error.child.class_name != NULL &&
          mc_mob_spawn_error_project(&c->eligibility_error).query ==
              &c->eligibility_error.detail.query);
    CHECK(mc_eligibility_retire(&c, NULL) == MC_WORLD_STORAGE_OK && c == NULL);
    mc_java_random *random = actor->random;
    actor->random = NULL;
    mc_mob_spawn_state state;
    mc_mob_spawn_error error;
    const mc_world_storage_status failed =
        mc_mob_spawn_eligible(actor, f.math, f.world_classes, &out, &state, &error);
    actor->random = random;
    CHECK(failed != MC_WORLD_STORAGE_OK && error.site.pc == 46 && error.source_exception &&
          error.active_child == MC_MOB_SPAWN_CHILD_NONE &&
          mc_mob_spawn_error_project(&error).query == NULL);
    CHECK(mc_world_runtime_retire_entity_containers(f.world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_mob_retire(&actor, NULL) == MC_MOB_OK && spawn_teardown(&f));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    static const mc_mob_spawn_child children[] = {
        MC_MOB_SPAWN_CHILD_QUERY,     MC_MOB_SPAWN_CHILD_BRIGHTNESS,
        MC_MOB_SPAWN_CHILD_NO_ENTITY, MC_MOB_SPAWN_CHILD_COLLISION,
        MC_MOB_SPAWN_CHILD_MATERIAL,  MC_MOB_SPAWN_CHILD_ACQUISITION};
    for (size_t i = 0; i < sizeof(children) / sizeof(*children); ++i)
        if (!eligibility_loader(children[i]))
            return 1;
    for (size_t i = 2; i < sizeof(children) / sizeof(*children); ++i)
        for (unsigned int slot = 0; slot < 4; ++slot)
            if (!eligibility_receipt(children[i], slot, false))
                return 1;
    if (!eligibility_receipt(MC_MOB_SPAWN_CHILD_NO_ENTITY, 0, true) || !eligibility_inactive() ||
        !eligibility_actual(argv[1], argv[2]))
        return 1;
    printf("Nightmare eligibility: %u checks\n", checks);
    return 0;
}
