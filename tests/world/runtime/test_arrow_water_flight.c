/* Actual default World dispatch through Arrow Base and its water-flight tail. */
#define MC_ARROW_NATURAL_FLIGHT_MAIN mc_arrow_water_unused_flight_main
#include "test_arrow_natural_flight.c"
#undef MC_ARROW_NATURAL_FLIGHT_MAIN

#include "mc/entity/arrow/tick.h"
#include "mc/world/runtime/world_access.h"

typedef struct aw_event {
    const mc_registry_string *name;
    double x, y, z, vx, vy, vz;
    float volume, pitch;
    mc_java_random random;
    unsigned int parent_pc, completed_bubbles;
    bool sound, wet;
} aw_event;

typedef struct aw_fixture {
    nf_fixture natural;
    mc_world_access *access;
    bool registered, null_registered, bad;
    size_t count, total, fail_at;
    unsigned int completed_ticks, failed_pc, failed_bubbles, classification;
    aw_event events[27];
} aw_fixture;

static bool aw_name(const mc_registry_string *name, const char *text) {
    mc_registry_string_view view;
    if (!mc_registry_string_read(name, &view) || view.length != strlen(text))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint16_t)(unsigned char)text[i])
            return false;
    return true;
}

static uint32_t aw_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool aw_random_equal(const mc_java_random *left, const mc_java_random *right) {
    return left->state == right->state &&
           left->have_next_next_gaussian == right->have_next_next_gaussian &&
           ad_bits(left->next_next_gaussian) == ad_bits(right->next_next_gaussian);
}

static aw_event *aw_record(aw_fixture *s, mc_world_runtime *world, bool sound,
                           mc_world_storage_error *error) {
    mc_entity *const arrow = s->natural.arrow;
    mc_arrow_tick_state state = {0};
    if (world != s->natural.world->worlds[0] || arrow->arrow->tick_call == NULL ||
        !mc_arrow_tick_inspect(arrow->arrow->tick_call, &state) || !state.active ||
        state.phase != MC_ARROW_TICK_ACTIVE || state.receiver != arrow ||
        world->active_calls == 0 || mc_world_access_free(s->access, NULL) != MC_WORLD_STORAGE_BUSY)
        s->bad = true;
    if (s->count == COUNT(s->events)) {
        s->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "Arrow water event ceiling", 0};
        return NULL;
    }
    aw_event *const event = &s->events[s->count++];
    *event = (aw_event){0}; /* Separate callback signatures have absent scalar slots. */
    ++s->total;
    event->sound = sound;
    event->random = *arrow->random;
    event->parent_pc = state.source_pc;
    event->completed_bubbles = state.bubble_calls;
    event->wet = arrow->base.bx;
    return event;
}

static mc_world_storage_status aw_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *error) {
    aw_fixture *s = context;
    aw_event *const event = aw_record(s, world, true, error);
    if (event == NULL)
        return error->status;
    event->name = name;
    event->x = x;
    event->y = y;
    event->z = z;
    event->volume = volume;
    event->pitch = pitch;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status aw_particle(void *context, mc_world_runtime *world,
                                           const mc_registry_string *name, double x, double y,
                                           double z, double vx, double vy, double vz,
                                           mc_world_storage_error *error) {
    aw_fixture *s = context;
    aw_event *const event = aw_record(s, world, false, error);
    if (event == NULL)
        return error->status;
    event->name = name;
    event->x = x;
    event->y = y;
    event->z = z;
    event->vx = vx;
    event->vy = vy;
    event->vz = vz;
    if (s->fail_at != 0 && s->count == s->fail_at) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Arrow water particle failure", 317};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

/* Full source cells prevent a flow gradient in the reached interior. Setters
 * run before observer registration; no actor field or Random is repaired. */
static bool aw_prepare(aw_fixture *s, int32_t id) {
    bool ok = false;
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    for (int32_t x = 6; x <= 14; ++x)
        for (int32_t y = 64; y <= 68; ++y)
            for (int32_t z = 6; z <= 10; ++z) {
                bool changed = false;
                CHECK(mc_world_runtime_block_metadata_set_quiet(s->natural.world->worlds[0], x, y,
                                                                z, id, 0, &changed,
                                                                error) == MC_WORLD_STORAGE_OK &&
                      changed);
            }
    ok = true;
cleanup:
    free(error);
    return ok;
}

/* SN.U entry splash consumes 57 nextFloat draws, then SL.w_ emits four
 * identical nonrandom bubbles after movement and before .8f drag/gravity. */
static bool aw_splash(aw_fixture *s, const mc_entity_base_fields *before, double surface,
                      mc_java_random *expected) {
    bool ok = false;
    volatile double xx = before->velocity_x * before->velocity_x;
    volatile double scaled_x = xx * (double).2f;
    volatile double yy = before->velocity_y * before->velocity_y;
    volatile double xy = scaled_x + yy;
    volatile double zz = before->velocity_z * before->velocity_z;
    volatile double scaled_z = zz * (double).2f;
    volatile double squared = xy + scaled_z;
    volatile float root = (float)sqrt(squared);
    volatile float volume = root * .2f;
    const float first = mc_java_random_next_float(expected);
    const float second = mc_java_random_next_float(expected);
    volatile float difference = first - second;
    volatile float scaled_pitch = difference * .4f;
    volatile float pitch = 1.0f + scaled_pitch;
    const aw_event *const sound = &s->events[0];
    CHECK(sound->sound && aw_name(sound->name, "random.splash") && sound->parent_pc == 1 &&
          !sound->wet && ad_bits(sound->x) == ad_bits(before->x) &&
          ad_bits(sound->y) == ad_bits(before->y - (double)before->offset) &&
          ad_bits(sound->z) == ad_bits(before->z) &&
          aw_float_bits(sound->volume) == aw_float_bits(volume) &&
          aw_float_bits(sound->pitch) == aw_float_bits(pitch) &&
          aw_random_equal(&sound->random, expected));
    for (size_t i = 0; i < 22; ++i) {
        const aw_event *const event = &s->events[i + 1];
        const bool bubble = i < 11;
        volatile float twice_x = mc_java_random_next_float(expected) * 2.0f;
        volatile float centered_x = twice_x - 1.0f;
        volatile float ox = centered_x * .5f;
        volatile float twice_z = mc_java_random_next_float(expected) * 2.0f;
        volatile float centered_z = twice_z - 1.0f;
        volatile float oz = centered_z * .5f;
        double vy = before->velocity_y;
        if (bubble) {
            volatile float dy = mc_java_random_next_float(expected) * .2f;
            volatile double next = vy - (double)dy;
            vy = next;
        }
        CHECK(!event->sound && aw_name(event->name, bubble ? "bubble" : "splash") &&
              event->parent_pc == 1 && !event->wet &&
              ad_bits(event->x) == ad_bits(before->x + (double)ox) &&
              ad_bits(event->y) == ad_bits(surface) &&
              ad_bits(event->z) == ad_bits(before->z + (double)oz) &&
              ad_bits(event->vx) == ad_bits(before->velocity_x) &&
              ad_bits(event->vy) == ad_bits(vy) &&
              ad_bits(event->vz) == ad_bits(before->velocity_z) &&
              aw_random_equal(&event->random, expected));
    }
    ok = true;
cleanup:
    return ok;
}

/* Raw values from this actual returned World invocation. Independent original
 * processes keep their own entropy; these records never reseed either actor. */
static bool aw_observe(const aw_fixture *s) {
    bool ok = false;
    const mc_entity *const p = s->natural.arrow;
    const mc_entity_bounds *const b = p->bounds;
    CHECK(printf("ARROW_WATER_STATE\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64
                 "\t%016" PRIx64 "\t%016" PRIx64 "\t%08" PRIx32 "\t%08" PRIx32 "\t%08" PRIx32
                 "\t%08" PRIx32 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64
                 "\t%016" PRIx64 "\t%016" PRIx64 "\t%" PRId32 "\t%" PRId32 "\t%s\t%" PRId32
                 "\t%s\t%s\t%s"
                 "\t%016" PRIx64 "\t%s\t%016" PRIx64 "\n",
                 ad_bits(p->base.x), ad_bits(p->base.y), ad_bits(p->base.z),
                 ad_bits(p->base.velocity_x), ad_bits(p->base.velocity_y),
                 ad_bits(p->base.velocity_z), aw_float_bits(p->base.yaw), aw_float_bits(p->base.aT),
                 aw_float_bits(p->base.aU), aw_float_bits(p->base.aV), ad_bits(b->min_x),
                 ad_bits(b->min_y), ad_bits(b->min_z), ad_bits(b->max_x), ad_bits(b->max_y),
                 ad_bits(b->max_z), p->base.bt, p->arrow->fields.k,
                 p->arrow->fields.i ? "true" : "false", p->arrow->fields.b,
                 p->base.bx ? "true" : "false", p->base.be ? "true" : "false",
                 p->base.private_c ? "true" : "false", p->random->state,
                 p->random->have_next_next_gaussian ? "true" : "false",
                 ad_bits(p->random->next_next_gaussian)) > 0);
    for (size_t i = 0; i < s->count; ++i) {
        const aw_event *const e = &s->events[i];
        mc_registry_string_view name;
        char text[32];
        CHECK(mc_registry_string_read(e->name, &name) && name.length < sizeof(text));
        for (size_t j = 0; j < name.length; ++j) {
            CHECK(name.units[j] > 32 && name.units[j] < 127);
            text[j] = (char)name.units[j];
        }
        text[name.length] = '\0';
        CHECK(printf("ARROW_WATER_EVENT\t%" PRId32 "\t%zu\t%s\t%s"
                     "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64 "\t%016" PRIx64
                     "\t%016" PRIx64 "\t%08" PRIx32 "\t%08" PRIx32 "\t%s\t%016" PRIx64
                     "\t%s\t%016" PRIx64 "\t%u\t%u\n",
                     p->base.bt, s->total - s->count + i + 1u, e->sound ? "true" : "false", text,
                     ad_bits(e->x), ad_bits(e->y), ad_bits(e->z), ad_bits(e->vx), ad_bits(e->vy),
                     ad_bits(e->vz), aw_float_bits(e->volume), aw_float_bits(e->pitch),
                     e->wet ? "true" : "false", e->random.state,
                     e->random.have_next_next_gaussian ? "true" : "false",
                     ad_bits(e->random.next_next_gaussian), e->parent_pc,
                     e->completed_bubbles) > 0);
    }
    CHECK(fflush(stdout) == 0 && !ferror(stdout));
    ok = true;
cleanup:
    return ok;
}

static bool aw_tick(aw_fixture *s, bool water, bool splash, bool failure, bool null_failure) {
    bool ok = false;
    mc_entity *const p = s->natural.arrow;
    mc_world_runtime *const world = s->natural.world->worlds[0];
    const mc_entity_base_fields before = p->base;
    const double surface = floor(p->bounds->min_y) + 1.0;
    mc_java_random expected = *p->random;
    const mc_java_random world_random = *world->fields.r;
    const size_t holds = p->holds, bindings = world->persistent_bindings;
    const size_t borrows = s->natural.world->classes[0]->native_borrows;
    const size_t local_leases = world->local_leases;
    volatile double x = before.x + before.velocity_x;
    volatile double y = before.y + before.velocity_y;
    volatile double z = before.z + before.velocity_z;
    volatile double quarter_x = before.velocity_x * (double).25f;
    volatile double quarter_y = before.velocity_y * (double).25f;
    volatile double quarter_z = before.velocity_z * (double).25f;
    volatile double bubble_x = x - quarter_x;
    volatile double bubble_y = y - quarter_y;
    volatile double bubble_z = z - quarter_z;
    const float drag = water ? .8f : .99f;
    volatile double vx = before.velocity_x * (double)drag;
    volatile double scaled_vy = before.velocity_y * (double)drag;
    volatile double vy = scaled_vy - (double).03f;
    volatile double vz = before.velocity_z * (double)drag;
    s->count = 0;
    ++nf_calls;
    const mc_world_storage_status status =
        mc_world_runtime_tick_entities(world, &tick_context, NULL, tick_error);
    const size_t prefix = splash ? 23u : 0u;
    const size_t tail = failure ? (null_failure ? 1u : s->fail_at) : (water ? 4u : 0u);
    if ((status == MC_WORLD_STORAGE_OK) != !failure || s->bad || s->count != prefix + tail)
        fprintf(stderr,
                "Arrow water tick status=%u tick=%" PRId32 " bad=%u callbacks=%zu expected=%zu "
                "operation=%s child=%u\n",
                (unsigned int)status, p->base.bt, (unsigned int)s->bad, s->count, prefix + tail,
                tick_error->operation == NULL ? "-" : tick_error->operation,
                (unsigned int)tick_error->child);
    CHECK((status == MC_WORLD_STORAGE_OK) == !failure && !s->bad && s->count == prefix + tail);
    if (splash)
        CHECK(aw_splash(s, &before, surface, &expected));
    for (size_t i = 0; i < tail; ++i) {
        const aw_event *const event = &s->events[prefix + i];
        CHECK(!event->sound && aw_name(event->name, "bubble") && event->wet &&
              event->parent_pc == 1459 && event->completed_bubbles == i &&
              ad_bits(event->x) == ad_bits(bubble_x) && ad_bits(event->y) == ad_bits(bubble_y) &&
              ad_bits(event->z) == ad_bits(bubble_z) &&
              ad_bits(event->vx) == ad_bits(before.velocity_x) &&
              ad_bits(event->vy) == ad_bits(before.velocity_y) &&
              ad_bits(event->vz) == ad_bits(before.velocity_z) &&
              aw_random_equal(&event->random, &expected));
    }
    CHECK(p->base.bt == before.bt + 1 && p->arrow->fields.k == before.bt + 1 &&
          p->base.bx == water && !p->base.private_c && !p->base.be && !p->arrow->fields.i &&
          ad_bits(p->base.x) == ad_bits(x) && ad_bits(p->base.y) == ad_bits(y) &&
          ad_bits(p->base.z) == ad_bits(z) && aw_random_equal(p->random, &expected) &&
          aw_random_equal(world->fields.r, &world_random));
    if (failure) {
        mc_arrow_tick_state state = {0};
        CHECK(tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_ARROW &&
              tick_error->child_error.arrow.call != NULL &&
              tick_error->child_error.arrow.call == p->arrow->tick_call &&
              mc_arrow_tick_inspect(tick_error->child_error.arrow.call, &state) &&
              state.phase == MC_ARROW_TICK_FAILED && !state.source_returned &&
              state.source_pc == 1459 && state.bubble_calls == (null_failure ? 0u : 2u) &&
              state.failure != NULL && state.failure->child == MC_ARROW_TICK_CHILD_PARTICLE &&
              state.failure->particle != NULL &&
              ad_bits(p->base.velocity_x) == ad_bits(before.velocity_x) &&
              ad_bits(p->base.velocity_y) == ad_bits(before.velocity_y) &&
              ad_bits(p->base.velocity_z) == ad_bits(before.velocity_z));
        const mc_arrow_tick_error *const error = state.failure;
        if (null_failure) {
            if (error->classification != MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION ||
                !error->source_exception || error->native_policy ||
                error->exception_class == NULL ||
                strcmp(error->exception_class, "java/lang/NullPointerException") != 0 ||
                !error->particle->world_access_failure.source_null_receiver)
                fprintf(stderr,
                        "Arrow water null classification=%u source_exception=%u native=%u "
                        "class=%s source_null=%u operation=%s cause=%u\n",
                        (unsigned int)error->classification, (unsigned int)error->source_exception,
                        (unsigned int)error->native_policy,
                        error->exception_class == NULL ? "-" : error->exception_class,
                        (unsigned int)error->particle->world_access_failure.source_null_receiver,
                        error->operation == NULL ? "-" : error->operation,
                        (unsigned int)error->particle->cause.status);
            CHECK(error->classification == MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION &&
                  error->source_exception && !error->native_policy &&
                  error->exception_class != NULL &&
                  strcmp(error->exception_class, "java/lang/NullPointerException") == 0 &&
                  error->particle->world_access_failure.source_null_receiver);
        } else
            CHECK(error->classification == MC_ARROW_TICK_FAILURE_UNRESOLVED &&
                  !error->source_exception && !error->source_error && !error->native_policy &&
                  error->particle->cause.status == MC_WORLD_STORAGE_IO_ERROR &&
                  error->particle->cause.system_error == 317);
        s->failed_pc = state.source_pc;
        s->failed_bubbles = state.bubble_calls;
        s->classification = (unsigned int)error->classification;
        CHECK(mc_world_entity_tick_error_retire(tick_error));
    } else {
        ++s->completed_ticks;
        CHECK(ad_bits(p->base.velocity_x) == ad_bits(vx) &&
              ad_bits(p->base.velocity_y) == ad_bits(vy) &&
              ad_bits(p->base.velocity_z) == ad_bits(vz) &&
              tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    }
    CHECK(p->arrow->tick_call == NULL && p->holds == holds &&
          world->persistent_bindings == bindings && world->local_leases == local_leases &&
          s->natural.world->classes[0]->native_borrows == borrows && world->fields.b->count == 1 &&
          world->fields.b->items[0] == p && s->natural.world->chunks[0]->entities[4].count == 1 &&
          s->natural.world->chunks[0]->entities[4].items[0] == p);
    CHECK(aw_observe(s));
    ok = true;
cleanup:
    return ok;
}

static bool aw_cleanup(aw_fixture *s) {
    if (tick_error != NULL && !mc_world_entity_tick_error_retire(tick_error))
        return false;
    mc_world_runtime *const world = s->natural.world == NULL ? NULL : s->natural.world->worlds[0];
    bool removed = false;
    if (s->null_registered) {
        if (mc_world_runtime_remove_world_access(world, NULL, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            return false;
        s->null_registered = false;
    }
    if (s->registered) {
        if (mc_world_runtime_remove_world_access(world, s->access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            return false;
        s->registered = false;
    }
    if (mc_world_access_free(s->access, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    s->access = NULL;
    return nf_cleanup(&s->natural);
}

int main(int argc, char **argv) {
    int result = 1;
    aw_fixture s = {0};
    char root[8192];
    if (argc != 3 ||
        (strcmp(argv[1], "water-first") != 0 && strcmp(argv[1], "water-multi") != 0 &&
         strcmp(argv[1], "dry-to-water") != 0 && strcmp(argv[1], "water-to-air") != 0 &&
         strcmp(argv[1], "particle-failure") != 0 && strcmp(argv[1], "null-callback") != 0) ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    const bool splash = strcmp(argv[1], "dry-to-water") == 0;
    const bool native_failure = strcmp(argv[1], "particle-failure") == 0;
    const bool null_failure = strcmp(argv[1], "null-callback") == 0;
    printf("ARROW_WATER_FLIGHT_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    CHECK(nf_setup(&s.natural, root, NULL) && nf_construct(&s.natural));
    if (splash)
        CHECK(nf_first_air(s.natural.world, s.natural.arrow));
    CHECK(aw_prepare(&s, 9));
    const mc_world_access_callbacks callbacks = {.sound = aw_sound, .particle = aw_particle};
    CHECK(mc_world_access_create(&callbacks, &s, &s.access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(s.natural.world->worlds[0], s.access, NULL) ==
              MC_WORLD_STORAGE_OK);
    s.registered = true;
    if (null_failure) {
        CHECK(mc_world_runtime_add_world_access(s.natural.world->worlds[0], NULL, NULL) ==
              MC_WORLD_STORAGE_OK);
        s.null_registered = true;
    }
    if (native_failure)
        s.fail_at = 3;
    printf("ARROW_WATER_FLIGHT_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    const unsigned int count = strcmp(argv[1], "water-multi") == 0 ? 8u : 1u;
    for (unsigned int i = 0; i < count; ++i) {
        /* The unchanged original's .5f Arrow box is inverted by SN.k_'s
         * -.4d expansion. In this exact eight-tick scene, the Y-cell loop
         * is empty at ticks5..7 despite the surrounding source water. Tick8
         * re-enters and emits the actual Base splash again. */
        const bool water = count != 8u || i < 4u || i == 7u;
        const bool entry_splash = splash || (count == 8u && i == 7u);
        CHECK(aw_tick(&s, water, entry_splash, native_failure || null_failure, null_failure));
    }
    if (strcmp(argv[1], "water-to-air") == 0) {
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(s.natural.world->worlds[0], s.access, &removed,
                                                   NULL) == MC_WORLD_STORAGE_OK &&
              removed);
        s.registered = false;
        CHECK(aw_prepare(&s, 0));
        CHECK(mc_world_runtime_add_world_access(s.natural.world->worlds[0], s.access, NULL) ==
              MC_WORLD_STORAGE_OK);
        s.registered = true;
        CHECK(aw_tick(&s, false, false, false, false));
    }
    result = 0;
cleanup:
    if (!aw_cleanup(&s)) {
        fputs("Arrow water cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0) {
        printf("ARROW_WATER_FLIGHT_OBSERVATION mode=%s calls=%zu completed=%u callbacks=%zu "
               "failed_pc=%u failed_bubbles=%u classification=%u\n",
               argv[1], nf_calls, s.completed_ticks, s.total, s.failed_pc, s.failed_bubbles,
               s.classification);
        printf("ARROW_WATER_FLIGHT_CASE %s END\n", argv[1]);
        puts("ARROW_WATER_FLIGHT_OPEN original-execution/visible-bubbles/full-victim/pickup/rider");
        printf("PASS: %u actual Arrow water flight checks;mode=%s\n", checks, argv[1]);
    }
    return result;
}
