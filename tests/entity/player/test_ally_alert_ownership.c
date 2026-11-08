/* Real World/App and source-receipt producers, with explicitly authored
 * returned-query child envelopes. This is a component ownership/classification
 * test, not a complete source loader/activation or original-JAR execution. */
int mc_ally_ownership_unused_statistics_main(int, char **);
#define main mc_ally_ownership_unused_statistics_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "mc/entity/player/ally_alert.h"

mc_entity_status mc_ally_ownership_begin(mc_entity *, mc_entity *, bool,
                                         mc_player_ally_alert_call **);
bool mc_ally_ownership_inspect(const mc_player_ally_alert_call *, mc_player_ally_alert_state *);
mc_entity_status mc_ally_ownership_retire(mc_player_ally_alert_call **,
                                          mc_player_ally_alert_cleanup *);
#define mc_player_ally_alert_begin mc_ally_ownership_begin
#define mc_player_ally_alert_inspect mc_ally_ownership_inspect
#define mc_player_ally_alert_retire mc_ally_ownership_retire
#include "../../../src/entity/player/ally_alert.c"
#undef mc_player_ally_alert_begin
#undef mc_player_ally_alert_inspect
#undef mc_player_ally_alert_retire

typedef struct component_producer {
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    mc_world_storage_error cause;
    size_t calls;
} component_producer;

static mc_world_storage_status component_added(void *context, mc_world_runtime *world,
                                               mc_entity *entity, mc_world_storage_error *error,
                                               mc_world_access_failure *failure) {
    component_producer *p = context;
    (void)world;
    (void)entity;
    ++p->calls;
    const mc_world_storage_status status =
        mc_world_access_receipt_create(p->receiver, NULL, &p->receipt, failure, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    p->receipt->exception_class = "java/lang/IllegalStateException";
    *error = p->cause;
    return error->status;
}

static bool component_owner(fixture *f, mc_world_storage_error cause,
                            mc_player_ally_alert_call **out) {
    mc_player_ally_alert_call *c = calloc(1, sizeof(*c));
    CHECK(c != NULL && mc_entity_retain(f->player) == MC_ENTITY_OK &&
          mc_world_runtime_binding_retain(f->world));
    c->player_held = c->world_held = c->query_failed = true;
    c->state = (mc_player_ally_alert_state){
        .player = f->player, .world = f->world, .error = &c->error, .cleanup = &c->cleanup};
    mc_world_nearby_error_init(&c->error.nearby);
    c->error.nearby.query.status = cause.status;
    c->error.nearby.query.operation = cause.operation;
    c->error.nearby.query.cause = cause;
    at(c, 115);
    CHECK(!fail(c, MC_ENTITY_WORLD_FAILED, "Player ally alert actual Wolf query"));
    *out = c;
    return true;
}

static bool component_loader(fixture *f, unsigned int mode) {
    const size_t player_holds = f->player->holds, bindings = f->world->persistent_bindings;
    const mc_world_storage_error first = {MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Authored original loader source cause", 59};
    const mc_world_storage_error current =
        mode == 0 ? first
        : mode == 1
            ? (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                       "ld active or incomplete source save/read", 0}
            : (mc_world_storage_error){first.status, "Authored new native loader policy", 0};
    mc_player_ally_alert_call *c = NULL;
    CHECK(component_owner(f, current, &c));
    mc_world_source_chunk_error producer = {0};
    producer.storage = first;
    producer.source.status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    producer.source.source_exception = true;
    producer.source.operation = first.operation;
    producer.source.exception_class = "java/lang/NullPointerException";
    producer.source.frames[0] = (mc_world_source_site){"ld", "a", "(Lfd;II)Llm;", 18};
    producer.source.frame_count = 1;
    c->error.nearby.query.source_chunk = &producer;
    CHECK(capture_query(c) && c->source_copied &&
          c->error.nearby.query.source_chunk == &c->source &&
          c->error.nearby.query.source_chunk != &producer);
    classify_query(c);
    CHECK(c->error.status == MC_ENTITY_WORLD_FAILED && c->error.site.pc == 115 &&
          c->error.nearby.query.cause.status == current.status &&
          c->error.nearby.query.cause.operation == current.operation &&
          c->error.nearby.query.cause.system_error == current.system_error &&
          c->error.source_exception == (mode == 0) && !c->error.source_error &&
          c->error.native_policy == (mode != 0));
    CHECK(mode == 0 ? strcmp(c->error.exception_class, "java/lang/NullPointerException") == 0
                    : c->error.exception_class == NULL);
    memset(&producer, 0, sizeof(producer));
    CHECK(c->error.nearby.query.source_chunk->source.frames[0].pc == 18 &&
          c->error.nearby.query.source_chunk->storage.system_error == 59);
    mc_player_ally_alert_error *snapshot = malloc(sizeof(*snapshot));
    CHECK(snapshot != NULL);
    *snapshot = c->error;
    const size_t held = f->world->persistent_bindings;
    f->world->persistent_bindings = 0; /* Authored native cleanup metadata fault. */
    mc_player_ally_alert_cleanup cleanup;
    const mc_entity_status refused = mc_ally_ownership_retire(&c, &cleanup);
    f->world->persistent_bindings = held;
    CHECK(refused == MC_ENTITY_STATE_ERROR && c != NULL &&
          memcmp(&c->error, snapshot, sizeof(*snapshot)) == 0 &&
          c->error.nearby.query.source_chunk->source.frames[0].pc == 18);
    CHECK(mc_ally_ownership_retire(&c, &cleanup) == MC_ENTITY_OK && c == NULL &&
          f->player->holds == player_holds && f->world->persistent_bindings == bindings);
    free(snapshot);
    return true;
}

static void component_receipt_slot(mc_world_nearby_error *e, unsigned int slot,
                                   mc_world_storage_error cause, mc_world_access_failure actual) {
    if (slot == 1) {
        e->tile_entity.status = cause.status;
        e->tile_entity.operation = cause.operation;
        e->tile_entity.world_access_failure = actual;
        return;
    }
    mc_world_entity_error *publication = slot == 0   ? &e->batch.publication
                                         : slot == 2 ? &e->tile_entity.publication
                                                     : &e->tile_entity.batch.publication;
    publication->status = cause.status;
    publication->operation = cause.operation;
    publication->cause = cause;
    publication->world_access_failure = actual;
}

static bool component_receipt(fixture *f, unsigned int slot, unsigned int mode) {
    const size_t player_holds = f->player->holds, bindings = f->world->persistent_bindings;
    component_producer p = {
        .cause = {MC_WORLD_STORAGE_IO_ERROR, "Authored ally source callback cause", 43}};
    const mc_world_access_source_callbacks callbacks = {.entity_added = component_added};
    CHECK(mc_world_access_source_create(&callbacks, &p, &p.receiver, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_storage_error cause;
    mc_world_access_failure actual;
    CHECK(mc_world_access_call_entity_added(p.receiver, f->world, f->player, &cause, &actual) ==
              p.cause.status &&
          actual.receipt == p.receipt && actual.receiver == p.receiver && p.calls == 1);
    const mc_world_storage_error current =
        mode == 1
            ? (mc_world_storage_error){MC_WORLD_STORAGE_BUSY, "Authored new native query BUSY", 0}
        : mode == 2
            ? (mc_world_storage_error){cause.status, "Authored new native query operation", 0}
            : cause;
    mc_player_ally_alert_call *c = NULL;
    CHECK(component_owner(f, current, &c));
    component_receipt_slot(&c->error.nearby, slot, cause, actual);
    if (mode == 3)
        p.receipt->holds = SIZE_MAX; /* Authored native capture refusal. */
    CHECK(capture_query(c) == (mode != 3) && c->receipt_count == (mode == 3 ? 0u : 1u));
    classify_query(c);
    const bool source = mode == 0 || mode == 3;
    CHECK(c->error.status == MC_ENTITY_WORLD_FAILED && c->error.source_exception == source &&
          !c->error.source_error && c->error.native_policy != source &&
          c->error.nearby.query.cause.status == current.status &&
          c->error.nearby.query.cause.operation == current.operation &&
          c->error.nearby.query.cause.system_error == current.system_error);
    CHECK(source ? strcmp(c->error.exception_class, "java/lang/IllegalStateException") == 0
                 : c->error.exception_class == NULL);
    mc_player_ally_alert_error *snapshot = malloc(sizeof(*snapshot));
    CHECK(snapshot != NULL);
    *snapshot = c->error;
    CHECK(mc_world_access_receipt_retire(&p.receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
          p.receipt != NULL);
    mc_player_ally_alert_cleanup cleanup;
    if (mode == 3) {
        CHECK(mc_ally_ownership_retire(&c, &cleanup) == MC_ENTITY_LIMIT && c != NULL &&
              cleanup.world == MC_WORLD_STORAGE_LIMIT &&
              memcmp(&c->error, snapshot, sizeof(*snapshot)) == 0 && p.calls == 1 &&
              c->error.source_exception && c->error.nearby.query.cause.system_error == 43);
        p.receipt->holds = 1; /* Restore the producer's one genuine base hold. */
    } else {
        c->state.active = true;
        CHECK(mc_ally_ownership_retire(&c, &cleanup) == MC_ENTITY_BUSY && c != NULL &&
              memcmp(&c->error, snapshot, sizeof(*snapshot)) == 0 && p.calls == 1);
        c->state.active = false;
    }
    CHECK(mc_ally_ownership_retire(&c, &cleanup) == MC_ENTITY_OK && c == NULL &&
          p.receipt->holds == 1 && p.calls == 1 && f->player->holds == player_holds &&
          f->world->persistent_bindings == bindings &&
          mc_world_access_receipt_retire(&p.receipt, NULL) == MC_WORLD_STORAGE_OK &&
          p.receipt == NULL && mc_world_access_free(p.receiver, NULL) == MC_WORLD_STORAGE_OK);
    free(snapshot);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "ally-ownership", true))
        return 1;
    for (unsigned int mode = 0; mode < 3; ++mode)
        if (!component_loader(f, mode))
            return 1;
    for (unsigned int slot = 0; slot < 4; ++slot)
        for (unsigned int mode = 0; mode < 4; ++mode)
            if (!component_receipt(f, slot, mode))
                return 1;
    if (!teardown(f))
        return 1;
    free(f);
    printf("Ally alert ownership: %zu checks; 19 authored child envelopes\n", checks);
    return 0;
}
