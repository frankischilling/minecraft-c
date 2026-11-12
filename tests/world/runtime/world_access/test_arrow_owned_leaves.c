/* Borrow the genuine current SourceSaving/Registry fixture; its main is not run. */
int aol_unused_query_main(int, char **);
#define MC_WORLD_COLLISION_QUERY_MAIN aol_unused_query_main
#include "test_collision_query_execution.c"
#undef MC_WORLD_COLLISION_QUERY_MAIN

#include "entity/tick/arrow_base_execution_internal.h"
#include "mc/entity/arrow/construction.h"
#include "mc/entity/tick/environment.h"

typedef struct aol_event {
    bool sound;
    const mc_registry_string *name;
    double x, y, z, vx, vy, vz;
    float volume, pitch;
    mc_java_random random;
} aol_event;

typedef struct aol_fixture {
    cq_fixture cq;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_tick_arrow_base_execution base;
    mc_entity_tick_error tick;
    mc_entity *arrow;
    mc_world_access *access;
    mc_entity_tick_execution_world_token token;
    mc_entity_bounds *box;
    aol_event events[23];
    size_t calls, primes, event_count, actor_holds, callback_bindings;
    bool observer_bad, registered;
} aol_fixture;

static bool aol_double(double a, double b) {
    uint64_t x, y;
    memcpy(&x, &a, sizeof(x));
    memcpy(&y, &b, sizeof(y));
    return x == y;
}

static bool aol_float(float a, float b) {
    uint32_t x, y;
    memcpy(&x, &a, sizeof(x));
    memcpy(&y, &b, sizeof(y));
    return x == y;
}

static bool aol_random(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state && aol_double(a->next_next_gaussian, b->next_next_gaussian) &&
           a->have_next_next_gaussian == b->have_next_next_gaussian;
}

static bool aol_name(const mc_registry_string *name, const char *text) {
    mc_registry_string_view view;
    if (!mc_registry_string_read(name, &view) || view.length != strlen(text))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint16_t)(unsigned char)text[i])
            return false;
    return true;
}

static bool aol_quiet(const mc_world_runtime_execution_result *r) {
    if (r->ownership_status != MC_WORLD_STORAGE_OK || r->selection_ready || r->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (r->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (r->closes[i].used)
            return false;
    return true;
}

static int aol_prime(aol_fixture *s) {
    fixture *f = &s->cq.source.game;
    int32_t id = -1;
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, f->worlds[0], cq_site,
                                               &s->token) == MC_WORLD_STORAGE_OK);
    ++s->primes;
    VERIFY(cq_get(&s->cq, 8, 66, 8, false, &id, &s->cq.collision, &s->cq.result) ==
               MC_WORLD_STORAGE_OK &&
           id == 0 && aol_quiet(&s->cq.result));
    VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &s->token, false) ==
               MC_WORLD_STORAGE_OK &&
           !s->cq.execution.world_open && cq_remember(&s->cq) == 0);
    return 0;
}

static int aol_arrow(aol_fixture *s, double x, double y, double z) {
    fixture *f = &s->cq.source.game;
    mc_entity_error allocation = {0};
    mc_arrow_error construction = {0};
    VERIFY(mc_arrow_allocate(f->entity_classes[0], &s->arrow, &allocation) == MC_ENTITY_OK &&
           mc_arrow_construct_position(s->arrow, f->worlds[0], x, y, z, &construction) ==
               MC_ENTITY_OK &&
           s->arrow->phase == MC_ENTITY_READY && s->arrow->holds == 1 && s->arrow->base.bt == 0 &&
           s->arrow->base.private_c && s->arrow->base.width == .5f && s->arrow->base.height == .5f);
    s->actor_holds = s->arrow->holds;
    return 0;
}

static int aol_base_call(aol_fixture *s) {
    fixture *f = &s->cq.source.game;
    const size_t bindings = f->worlds[0]->persistent_bindings;
    const size_t bounds_holds = f->entity_classes[0]->bounds_holds;
    mc_world_runtime_execution_result_init(&s->cq.result);
    mc_tick_arrow_base_execution_init(&s->base, &s->cq.execution, f->classes[0], cq_code,
                                      &s->cq.result);
    mc_entity_tick_error_init(&s->tick);
    ++s->calls;
    const bool completed =
        mc_tick_arrow_base_body_in_execution(s->arrow, f->registry[0], &s->base, NULL, &s->tick);
    VERIFY(completed && s->base.body_entered && s->base.body_completed &&
           s->tick.status == MC_ENTITY_OK && s->tick.base_bytecode == 590 &&
           !s->tick.source_exception && aol_quiet(&s->cq.result) && !s->base.world_open &&
           !s->cq.execution.world_open && s->cq.execution.first.status == MC_WORLD_STORAGE_OK &&
           !s->cq.execution.capture_pending && s->cq.execution.receipt_count == 0 &&
           f->worlds[0]->persistent_bindings == bindings && f->worlds[0]->active_calls == 0 &&
           f->entity_classes[0]->bounds_holds == bounds_holds &&
           s->arrow->holds == s->actor_holds && !s->math.failed);
    mc_math_class_state state;
    VERIFY(mc_math_classes_state(s->math_owner, &state, NULL) == MC_MATH_CLASS_OK &&
           state.phase == MC_MATH_CLASS_READY && state.holds == 2 &&
           s->math.owned == s->math_owner);
    return 0;
}

static aol_event *aol_observe(aol_fixture *s, mc_world_runtime *world, bool sound) {
    mc_entity_tick_execution_world_token current;
    mc_math_class_state state;
    if (s->event_count == COUNT(s->events)) {
        s->observer_bad = true;
        return NULL;
    }
    if (world != s->cq.source.game.worlds[0] || world->active_calls == 0 || !s->base.world_open ||
        !s->cq.execution.world_open ||
        !mc_entity_tick_execution_current_world(&s->cq.execution, &current) ||
        !cq_token_same(current, s->base.world) || s->cq.execution.world != world ||
        s->arrow->phase != MC_ENTITY_READY ||
        s->arrow->holds != s->actor_holds + (sound ? 1u : 0u) ||
        world->persistent_bindings != s->callback_bindings + 1 || s->arrow->base.bt != 2 ||
        s->arrow->base.private_c || s->arrow->base.bx || s->math.owned != s->math_owner ||
        mc_math_classes_state(s->math_owner, &state, NULL) != MC_MATH_CLASS_OK ||
        state.phase != MC_MATH_CLASS_READY || state.holds != 2 ||
        mc_world_access_free(s->access, NULL) != MC_WORLD_STORAGE_BUSY)
        s->observer_bad = true;
    aol_event *event = &s->events[s->event_count++];
    event->sound = sound;
    event->random = *s->arrow->random;
    return event;
}

static mc_world_storage_status aol_sound(void *context, mc_world_runtime *world,
                                         const mc_registry_string *name, double x, double y,
                                         double z, float volume, float pitch,
                                         mc_world_storage_error *error) {
    aol_fixture *s = context;
    aol_event *event = aol_observe(s, world, true);
    /* Metadata refusal never substitutes a Source callback failure. */
    if (event != NULL)
        *event = (aol_event){true, name, x, y, z, 0, 0, 0, volume, pitch, *s->arrow->random};
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status aol_particle(void *context, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, double vx, double vy, double vz,
                                            mc_world_storage_error *error) {
    aol_fixture *s = context;
    aol_event *event = aol_observe(s, world, false);
    if (event != NULL)
        *event = (aol_event){false, name, x, y, z, vx, vy, vz, 0, 0, *s->arrow->random};
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static int aol_splash(aol_fixture *s) {
    fixture *f = &s->cq.source.game;
    VERIFY(aol_arrow(s, 8.5, 66, 8.5) == 0 && aol_base_call(s) == 0 && s->arrow->base.bt == 1 &&
           !s->arrow->base.private_c && !s->arrow->base.bx);
    /* Genuine position operation only; no source-history/flag/Random reset. */
    VERIFY(mc_entity_set_position(s->arrow, 10.5, 66, 10.5) == MC_ENTITY_OK);
    const mc_world_access_callbacks callbacks = {.sound = aol_sound, .particle = aol_particle};
    VERIFY(mc_world_access_create(&callbacks, s, &s->access, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->access, NULL) == MC_WORLD_STORAGE_OK);
    s->registered = true;
    s->callback_bindings = f->worlds[0]->persistent_bindings;
    mc_java_random expected = *s->arrow->random;
    const mc_java_random world_random = *f->worlds[0]->fields.r;
    VERIFY(aol_base_call(s) == 0 && s->arrow->base.bt == 2 && s->arrow->base.bx &&
           !s->arrow->base.private_c && s->arrow->base.bv == 0 && s->tick.particle_work == 22 &&
           s->event_count == 23 && !s->observer_bad &&
           aol_random(f->worlds[0]->fields.r, &world_random));
    const float first = mc_java_random_next_float(&expected);
    const float second = mc_java_random_next_float(&expected);
    volatile float delta = first - second;
    volatile float scale = delta * .4f;
    volatile float pitch = 1.0f + scale;
    const aol_event *sound = &s->events[0];
    VERIFY(sound->sound && aol_name(sound->name, "random.splash") && aol_double(sound->x, 10.5) &&
           aol_double(sound->z, 10.5) &&
           aol_double(sound->y, s->arrow->base.y - (double)s->arrow->base.offset) &&
           sound->volume > 0.0f && aol_float(sound->pitch, pitch) &&
           aol_random(&sound->random, &expected));
    const double vx = s->arrow->base.velocity_x, vy = s->arrow->base.velocity_y;
    const double vz = s->arrow->base.velocity_z;
    for (size_t i = 0; i < 22; ++i) {
        const aol_event *event = &s->events[i + 1];
        const bool bubble = i < 11;
        volatile float twice_x = mc_java_random_next_float(&expected) * 2.0f;
        volatile float center_x = twice_x - 1.0f;
        volatile float ox = center_x * .5f;
        volatile float twice_z = mc_java_random_next_float(&expected) * 2.0f;
        volatile float center_z = twice_z - 1.0f;
        volatile float oz = center_z * .5f;
        double ey = vy;
        if (bubble) {
            volatile float dy = mc_java_random_next_float(&expected) * .2f;
            ey = vy - (double)dy;
        }
        VERIFY(!event->sound && aol_name(event->name, bubble ? "bubble" : "splash") &&
               aol_double(event->x, 10.5 + (double)ox) && aol_double(event->y, 67.0) &&
               aol_double(event->z, 10.5 + (double)oz) && aol_double(event->vx, vx) &&
               aol_double(event->vy, ey) && aol_double(event->vz, vz) &&
               aol_random(&event->random, &expected));
    }
    VERIFY(aol_random(s->arrow->random, &expected)); /* 2 + 11*3 + 11*2 real draws. */
    return 0;
}

static int aol_base_mode(aol_fixture *s) {
    const char *mode = s->cq.mode;
    VERIFY(aol_prime(s) == 0);
    if (strcmp(mode, "base-splash") == 0)
        return aol_splash(s);
    const bool water = strcmp(mode, "base-water") == 0;
    const bool lava = strcmp(mode, "base-lava") == 0;
    VERIFY(aol_arrow(s,
                     water  ? 10.5
                     : lava ? 13.5
                            : 8.5,
                     66,
                     water  ? 10.5
                     : lava ? 13.5
                            : 8.5) == 0);
    const mc_entity_base_fields before = s->arrow->base;
    const mc_java_random random = *s->arrow->random;
    const mc_java_random world_random = *s->cq.source.game.worlds[0]->fields.r;
    VERIFY(aol_base_call(s) == 0 && s->arrow->base.bt == 1 && s->arrow->base.aJ == before.x &&
           s->arrow->base.aK == before.y && s->arrow->base.aL == before.z &&
           !s->arrow->base.private_c && s->arrow->base.bx == water &&
           s->arrow->base.bv == (lava ? 600 : 0) && !s->arrow->base.be &&
           s->tick.particle_work == 0 && aol_random(s->arrow->random, &random) &&
           aol_random(s->cq.source.game.worlds[0]->fields.r, &world_random));
    if (water)
        VERIFY(s->arrow->base.velocity_x > 0.0 && s->arrow->base.velocity_z > 0.0 &&
               aol_double(s->arrow->base.velocity_y, 0.0));
    else
        VERIFY(aol_double(s->arrow->base.velocity_x, 0.0) &&
               aol_double(s->arrow->base.velocity_y, 0.0) &&
               aol_double(s->arrow->base.velocity_z, 0.0));
    return 0;
}

static int aol_shape(aol_fixture *s, mc_world_runtime *world, int32_t id, int32_t x, int32_t y,
                     int32_t z, bool box) {
    fixture *f = &s->cq.source.game;
    const mc_block *block = NULL;
    VERIFY(mc_registry_block_get(f->registry[0], id, &block, NULL) == MC_REGISTRY_OK &&
           block != NULL);
    VERIFY(mc_entity_tick_execution_push_world(&s->cq.execution, world, cq_site, &s->token) ==
           MC_WORLD_STORAGE_OK);
    const mc_world_collision_block_execution_code code = {
        cq_code, f->registry[0], f->classes[0], {"lm", "d", "(III)Low;", 67}};
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    mc_world_collision_error_init(&s->cq.collision);
    ++s->calls;
    const bool completed = box ? mc_world_collision_block_box_in_classes_in_execution(
                                     &s->cq.execution, f->classes[0], world, code, block, x, y, z,
                                     &s->box, &s->cq.collision, &status, &s->cq.result)
                               : mc_world_collision_block_state_shape_in_classes_in_execution(
                                     &s->cq.execution, f->classes[0], world, code, block, x, y, z,
                                     &s->cq.collision, &status, &s->cq.result);
    VERIFY(completed && status == MC_WORLD_STORAGE_OK && aol_quiet(&s->cq.result) &&
           s->cq.execution.first.status == MC_WORLD_STORAGE_OK && !s->cq.execution.capture_pending);
    if (box)
        VERIFY((id == 90) == (s->box == NULL));
    if (box && id == 1)
        VERIFY(s->box->min_x == (double)x && s->box->min_y == (double)y &&
               s->box->min_z == (double)z && s->box->max_x == (double)x + 1.0 &&
               s->box->max_y == (double)y + 1.0 && s->box->max_z == (double)z + 1.0);
    if (s->box != NULL) {
        VERIFY(mc_entity_bounds_release(f->entity_classes[0], s->box) == MC_ENTITY_OK);
        s->box = NULL;
    }
    VERIFY(mc_entity_tick_execution_pop_world(&s->cq.execution, &s->token, false) ==
               MC_WORLD_STORAGE_OK &&
           !s->cq.execution.world_open);
    return 0;
}

static int aol_collision_mode(aol_fixture *s) {
    fixture *f = &s->cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    if (strcmp(s->cq.mode, "collision-unused") == 0) {
        const size_t bindings = world->persistent_bindings;
        VERIFY(aol_shape(s, NULL, 1, 4, 66, 4, false) == 0 &&
               aol_shape(s, NULL, 1, 4, 66, 4, true) == 0 &&
               aol_shape(s, NULL, 90, 4, 66, 4, true) == 0 &&
               aol_shape(s, world, 1, 4, 66, 4, true) == 0 &&
               world->persistent_bindings == bindings && world->active_calls == 0 &&
               f->providers[0].cache->count == 0 && f->providers[0].cache->insertion_count == 0);
        return 0;
    }
    VERIFY(aol_prime(s) == 0);
    const mc_block *block = NULL;
    if (strcmp(s->cq.mode, "collision-portal") == 0) {
        VERIFY(mc_registry_block_get(f->registry[0], 90, &block, NULL) == MC_REGISTRY_OK &&
               block != NULL && block->kind == MC_BLOCK_PORTAL &&
               aol_shape(s, world, 90, 4, 66, 4, false) == 0);
        VERIFY(block->fields.bs == 0 && block->fields.bu == .375 && block->fields.bv == 1 &&
               block->fields.bx == .625);
        VERIFY(aol_shape(s, world, 90, 7, 66, 4, false) == 0 && block->fields.bs == .375 &&
               block->fields.bu == 0 && block->fields.bv == .625 && block->fields.bx == 1);
    } else if (strcmp(s->cq.mode, "collision-metadata") == 0) {
        VERIFY(mc_registry_block_get(f->registry[0], 65, &block, NULL) == MC_REGISTRY_OK &&
               block != NULL && block->kind == MC_BLOCK_LADDER &&
               aol_shape(s, world, 65, 6, 66, 8, true) == 0);
        VERIFY(block->fields.bs == 0 && block->fields.bu == .875 && block->fields.bv == 1 &&
               block->fields.bx == 1);
        VERIFY(aol_shape(s, world, 65, 6, 67, 8, true) == 0 && block->fields.bs == 0 &&
               block->fields.bu == 0 && block->fields.bv == .125 && block->fields.bx == 1);
    } else {
        VERIFY(strcmp(s->cq.mode, "collision-piston") == 0 &&
               mc_registry_block_get(f->registry[0], 36, &block, NULL) == MC_REGISTRY_OK &&
               block != NULL && block->kind == MC_BLOCK_MOVING_PISTON &&
               f->chunks[0]->l->count == 1 && aol_shape(s, world, 36, 6, 66, 6, false) == 0);
        VERIFY(block->fields.bs == -.5 && block->fields.bt == 0 && block->fields.bu == 0 &&
               block->fields.bv == .5 && block->fields.bw == 1 && block->fields.bx == 1 &&
               f->chunks[0]->l->count == 1);
    }
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 &&
           (strcmp(argv[1], "base-air") == 0 || strcmp(argv[1], "base-water") == 0 ||
            strcmp(argv[1], "base-lava") == 0 || strcmp(argv[1], "base-splash") == 0 ||
            strcmp(argv[1], "collision-unused") == 0 || strcmp(argv[1], "collision-portal") == 0 ||
            strcmp(argv[1], "collision-metadata") == 0 ||
            strcmp(argv[1], "collision-piston") == 0));
    printf("ARROW_OWNED_LEAVES_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    aol_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->cq.mode = argv[1];
    VERIFY(ss_setup(&s->cq.source, argv[2]) == 0 && cq_files(&s->cq, true) == 0 &&
           ss_open(&s->cq.source) == 0 &&
           mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->cq.execution, &s->math);
    const mc_java_random world_random = *s->cq.source.game.worlds[0]->fields.r;
    printf("ARROW_OWNED_LEAVES_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    VERIFY((strncmp(argv[1], "base-", 5) == 0 ? aol_base_mode(s) : aol_collision_mode(s)) == 0);
    VERIFY(aol_random(s->cq.source.game.worlds[0]->fields.r, &world_random));
    if (strncmp(argv[1], "collision-", 10) == 0) {
        mc_math_class_state unused;
        VERIFY(s->math.owned == NULL && !s->math.failed &&
               mc_math_classes_state(s->math_owner, &unused, NULL) == MC_MATH_CLASS_OK &&
               unused.phase == MC_MATH_CLASS_COLD && unused.holds == 1 && unused.source_a == NULL);
    }
    if (s->registered) {
        bool removed = false;
        VERIFY(mc_world_runtime_remove_world_access(s->cq.source.game.worlds[0], s->access,
                                                    &removed, NULL) == MC_WORLD_STORAGE_OK &&
               removed);
        s->registered = false;
    }
    VERIFY(mc_world_access_free(s->access, NULL) == MC_WORLD_STORAGE_OK);
    s->access = NULL;
    mc_entity_tick_error_init(
        &s->tick); /* Typed child/name borrows expire before owner retirement. */
    mc_world_collision_error_init(&s->cq.collision);
    memset(s->events, 0, sizeof(s->events));
    VERIFY(!s->base.world_open && !s->cq.execution.world_open && aol_quiet(&s->cq.result) &&
           mc_entity_tick_execution_retire(&s->cq.execution) == MC_WORLD_STORAGE_OK &&
           mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
           mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_entity_release(s->arrow);
    s->arrow = NULL;
    printf("ARROW_OWNED_LEAVES_CASE %s END calls=%zu primes=%zu\n", argv[1], s->calls, s->primes);
    fflush(stdout);
    VERIFY((s->cq.source.game.states[0] == NULL ? cq_empty_stop(&s->cq.source)
                                                : ss_stop(&s->cq.source)) == 0 &&
           cq_files(&s->cq, false) == 0 && ss_finish(&s->cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->cq.inputs[i]);
    free(s);
    puts("ARROW_OWNED_LEAVES_OPEN callback-failure/unwind/foreign-Store/full-Arrow/full-SL");
    printf("PASS: %u actual owned Arrow Base/collision checks;mode=%s\n", checks, argv[1]);
    return 0;
}
