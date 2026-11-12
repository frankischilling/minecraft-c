/* Actual GAME World, Chunks and canonical Mob owners. The imported main is
 * never called; mutable branch inputs below are declared fixture inputs. */
int mc_mount_unused_random_main(int, char **);
#define main mc_mount_unused_random_main
#include "../../world/runtime/test_world_random_tick.c"
#undef main

#include "entity/mob/internal.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/mount/mount.h"
#include "mc/world/runtime/entity_tick.h"
#include "world/runtime/entity_tick_internal.h"
#include "world/runtime/terminal_inventory_internal.h"
#include "world/storage/generated_internal.h"

static bool mount_actor(random_fixture *f, mc_entity_class_kind kind, mc_entity **out) {
    const mc_entity_class_literal *literal;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(f->entities, f->registry, literal, out, &error) == MC_MOB_OK &&
          mc_mob_construct(*out, f->world, &error) == MC_MOB_OK &&
          mc_entity_set_position(*out, .5, 64., .5) == MC_ENTITY_OK);
    return true;
}

static bool mount_controls(random_fixture *f, mc_entity *p, mc_entity *q, mc_entity *a,
                           mc_entity *b) {
    mc_entity_mount_error e;
    CHECK(mc_entity_mount(NULL, a, NULL, 0, &e) == MC_ENTITY_STATE_ERROR && e.source_null);
    p->base.private_d = 12.;
    p->base.private_e = -19.;
    CHECK(mc_entity_mount(p, NULL, NULL, 0, &e) == MC_ENTITY_OK && e.bytecode == 79 &&
          double_bits(p->base.private_d) == 0 && double_bits(p->base.private_e) == 0);
    const size_t ph = p->holds, ah = a->holds;
    const uint64_t random = p->random->state, other = a->random->state;
    CHECK(mc_entity_mount(p, a, NULL, 0, &e) == MC_ENTITY_OK && e.bytecode == 175 &&
          p->base.aH == a && p->mount_hold == a && a->base.aG == p && a->passenger_hold == p &&
          p->holds == ph + 1 && a->holds == ah + 1 && p->random->state == random &&
          a->random->state == other);
    /* Same-target arm clears links BEFORE positioning. */
    const double y = a->bounds->min_y + (double)a->base.height + (double)p->base.offset;
    p->base.yaw = -31.f;
    p->base.aT = 47.f;
    CHECK(mc_entity_mount(p, a, NULL, 0, &e) == MC_ENTITY_OK && e.bytecode == 134 &&
          p->base.aH == NULL && a->base.aG == NULL && p->mount_hold == NULL &&
          a->passenger_hold == NULL && p->holds == ph && a->holds == ah && p->base.x == a->base.x &&
          p->base.y == y && p->base.aK == y && p->base.bm == y && p->base.z == a->base.z &&
          p->base.yaw == -31.f && p->base.aT == 47.f);
    CHECK(mc_entity_mount(p, a, NULL, 0, &e) == MC_ENTITY_OK &&
          mc_entity_mount(q, b, NULL, 0, &e) == MC_ENTITY_OK &&
          mc_entity_mount(p, b, NULL, 0, &e) == MC_ENTITY_OK && a->base.aG == NULL &&
          a->passenger_hold == NULL && q->base.aH == NULL && q->mount_hold == NULL &&
          p->base.aH == b && b->base.aG == p);
    /* NULL dismount positions before clearing either reference. A genuine
     * nullable source bounds input must retain that completed source prefix. */
    mc_entity_bounds *saved = b->bounds;
    b->bounds = NULL;
    const mc_entity_status null_bounds = mc_entity_mount(p, NULL, NULL, 0, &e);
    b->bounds = saved;
    CHECK(null_bounds == MC_ENTITY_STATE_ERROR && e.source_null && e.bytecode == 36 &&
          p->base.aH == b && b->base.aG == p && p->mount_hold == b && b->passenger_hold == p);
    CHECK(mc_entity_mount(p, NULL, NULL, 0, &e) == MC_ENTITY_OK && p->base.aH == NULL &&
          b->base.aG == NULL);
    CHECK(mc_entity_mount(p, b, NULL, 0, &e) == MC_ENTITY_OK);
    saved = b->bounds;
    b->bounds = NULL;
    const mc_entity_status toggled = mc_entity_mount(p, b, NULL, 0, &e);
    b->bounds = saved;
    CHECK(toggled == MC_ENTITY_STATE_ERROR && e.source_null && e.bytecode == 110 &&
          p->base.aH == NULL && b->base.aG == NULL && p->mount_hold == NULL &&
          b->passenger_hold == NULL);
    /* A source link without a native occurrence requires an actual held
     * caller scope; refusal precedes any dereference or reference mutation. */
    p->base.aH = a;
    CHECK(mc_entity_mount(p, NULL, NULL, 0, &e) == MC_ENTITY_STATE_ERROR && e.native_policy &&
          p->base.aH == a);
    mc_entity *actors[] = {p, a};
    CHECK(mc_entity_mount(p, NULL, actors, COUNT(actors), &e) == MC_ENTITY_OK &&
          p->base.aH == NULL);
    CHECK(mc_entity_mount(p, p, NULL, 0, &e) == MC_ENTITY_OK && p->base.aH == p &&
          p->base.aG == p && p->holds == ph + 2 &&
          mc_entity_mount(p, NULL, NULL, 0, &e) == MC_ENTITY_OK && p->holds == ph);
    (void)f;
    return true;
}

static bool jockey_tick(random_fixture *f, mc_entity *rider, mc_entity *spider) {
    f->world->fields.q = 1; /* Declared nonpeaceful input. */
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
            mc_world_chunk *object = NULL;
            CHECK(blocks != NULL &&
                  mc_world_store_create_generated(f->store, f->world, 0, x, z, &blocks,
                                                  MC_CHUNK_BLOCKS, &object,
                                                  NULL) == MC_WORLD_STORAGE_OK &&
                  blocks == NULL);
            mc_chunk_state *state = mc_world_chunk_state(object);
            CHECK(state != NULL && mc_chunk_cache_publish(f->provider.cache, x, z, state));
            f->owned[f->owned_count++] = state;
        }
    CHECK(mc_entity_set_position(rider, .5, 64., .5) == MC_ENTITY_OK &&
          mc_entity_set_position(spider, .5, 64., .5) == MC_ENTITY_OK);
    bool inserted;
    CHECK(mc_world_runtime_insert_entity(f->world, spider, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted &&
          mc_world_runtime_insert_entity(f->world, rider, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted && mc_entity_mount(rider, spider, NULL, 0, NULL) == MC_ENTITY_OK);
    mc_world_entity_tick_error *e = calloc(1, sizeof(*e));
    CHECK(e != NULL);
    const mc_world_entity_tick_context context = {
        .math = f->math,
        .player = {
            .travel = {.code_registry = f->registry, .trig = mc_math_classes_table(f->math)}}};
    const int32_t before = rider->base.bt, spider_before = spider->base.bt;
    const mc_world_storage_status status =
        mc_world_runtime_tick_entities(f->world, &context, NULL, e);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Jockey tick refused: %s at%u riding%s:%u mount%s:%u\n",
                e->operation ? e->operation : "-", e->bytecode,
                e->riding_class ? e->riding_class : "-", e->riding_bytecode,
                e->mount_class ? e->mount_class : "-", e->mount_bytecode);
    const double mounted_y =
        (spider->base.y + ((double)spider->base.height * .75 - .5)) + (double)rider->base.offset;
    /* The public success contract clears the error, including work counters.
     * Exact actor tick counts and placement prove the ordinary loop result. */
    const bool returned = status == MC_WORLD_STORAGE_OK && rider->base.bt == before + 1 &&
                          spider->base.bt == spider_before + 1 && rider->base.aH == spider &&
                          spider->base.aG == rider && e->passenger_calls == 0 &&
                          rider->base.x == spider->base.x && rider->base.z == spider->base.z &&
                          rider->base.y == mounted_y && float_bits(rider->mob->living.K) == 0;
    if (!returned)
        fprintf(
            stderr,
            "Jockey tick state status=%d rider-ticks=%" PRId32 "/%" PRId32 " spider-ticks=%" PRId32
            "/%" PRId32 " rider-mounted=%d spider-passenger=%d calls=%zu\n"
            "Jockey positions rider=%a,%a,%a spider=%a,%a,%a expected-y=%a K=%08" PRIx32
            " chunked=%d,%d dead=%d,%d loaded=%d update=%u presence=%u\n",
            (int)status, rider->base.bt, before + 1, spider->base.bt, spider_before + 1,
            rider->base.aH == spider, spider->base.aG == rider, e->passenger_calls, rider->base.x,
            rider->base.y, rider->base.z, spider->base.x, spider->base.y, spider->base.z, mounted_y,
            float_bits(rider->mob->living.K), rider->base.bF, spider->base.bF, rider->base.be,
            spider->base.be, (int)e->loaded_index, e->update_bytecode, e->presence_bytecode);
    CHECK(returned);
    /* Observe the actual source tail chain before the public error reset.
     * This is the same production body on the same genuine World/actors. */
    wt_scope *scope = calloc(1, sizeof(*scope));
    CHECK(scope != NULL);
    scope->world = f->world;
    scope->context = &context;
    scope->error.max_loop_work = MC_WORLD_ENTITY_TICK_DEFAULT_LOOP_WORK;
    scope->error.max_passenger_calls = MC_WORLD_ENTITY_TICK_DEFAULT_PASSENGER_CALLS;
    scope->error.max_temporary_bytes = MC_WORLD_ENTITY_TICK_DEFAULT_TEMPORARY_BYTES;
    CHECK(mc_world_runtime_call_enter(f->world, NULL) == MC_WORLD_STORAGE_OK);
    const bool updated = wt_update(scope, spider, true, true);
    mc_world_runtime_call_leave(f->world);
    CHECK(updated && scope->error.passenger_calls == 1 && scope->parent_count == 0 &&
          rider->base.bt == before + 2 && spider->base.bt == spider_before + 2 &&
          rider->base.aH == spider && spider->base.aG == rider && rider->base.x == spider->base.x &&
          rider->base.z == spider->base.z &&
          rider->base.y == (spider->base.y + ((double)spider->base.height * .75 - .5)) +
                               (double)rider->base.offset &&
          float_bits(rider->mob->living.K) == 0);
    /* Direct LS riding with dead mount returns SN's short arm, then executes
     * the LS J/K tail without invoking an active Mob tick. */
    spider->base.be = true;
    rider->mob->living.K = -0.75f;
    CHECK(wt_riding(scope, rider, 114) && rider->base.aH == NULL && rider->mount_hold == NULL &&
          rider->mob->living.J == -0.75f && float_bits(rider->mob->living.K) == 0);
    (void)mc_entity_mount_passenger_store(spider, NULL);
    spider->base.be = false;
    CHECK(mc_entity_mount(rider, spider, NULL, 0, NULL) == MC_ENTITY_OK);
    /* This direct internal caller owns the optional diagnostic reservation,
     * just as the public World runner owns its own invocation's scratch. */
    free(scope->mob_workspace);
    free(scope->parents);
    free(scope);
    free(e);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    random_fixture *f = calloc(1, sizeof(*f));
    mc_entity *rider = NULL, *other = NULL, *spider = NULL, *second = NULL;
    if (f == NULL || !setup(f, argv[1], argv[2]) ||
        !mount_actor(f, MC_ENTITY_CLASS_KIND_SKELETON, &rider) ||
        !mount_actor(f, MC_ENTITY_CLASS_KIND_SKELETON, &other) ||
        !mount_actor(f, MC_ENTITY_CLASS_KIND_SPIDER, &spider) ||
        !mount_actor(f, MC_ENTITY_CLASS_KIND_SPIDER, &second) ||
        !mount_controls(f, rider, other, spider, second) || !jockey_tick(f, rider, spider)) {
        fputs("Entity mount failure: actual graph retained; no cleanup credit\n", stderr);
        return 1;
    }
    /* Preserve the actual reciprocal graph after dropping external roots.
     * Existing checked container retirement must inventory and break it. */
    mc_entity_release(rider);
    mc_entity_release(other);
    mc_entity_release(spider);
    mc_entity_release(second);
    if (!teardown(f))
        return 1;
    free(f);
    printf("PASS: %u Entity mount source-order/ownership checks\n", checks);
    return 0;
}
