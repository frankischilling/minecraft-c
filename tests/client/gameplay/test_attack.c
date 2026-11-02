/* Saved-World bootstrap, actual picking and ordinary SDK left-click attack. */
int mc_attack_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_attack_included_mining_main
#include "test_mining.c"
#undef MC_MINING_FIXTURE_MAIN
#include "entity/mob/internal.h"
#include "entity/player/player_internal.h"
#include "mc/world/runtime/world_access.h"
#include "test_attack_autosave.inc"

static struct {
    fixture *fixture;
    mc_entity *victim;
    mc_world_runtime *world;
    mc_world_access *access;
    size_t samples;
    double velocity_y;
    int32_t damage;
    bool falling_bonus, actual_call;
    size_t death_particles;
    uintptr_t particles[20];
    bool removed, bad_particle;
} attack_probe;

static mc_world_storage_status attack_probe_sound(void *context, mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, float volume, float pitch,
                                                  mc_world_storage_error *error) {
    (void)context;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    if (world == attack_probe.world && attack_probe.victim->mob->living.Y <= 0 &&
        !attack_probe.victim->mob->living.ah) {
        ++attack_probe.samples;
        attack_probe.velocity_y = attack_probe.fixture->app->h->base.velocity_y;
        const mc_client_gameplay_input *input = attack_probe.fixture->state.gameplay_input;
        mc_player_attack_state state;
        if (input != NULL && mc_local_player_attack_inspect(input->attack_call, &state) &&
            state.active && state.target == attack_probe.victim &&
            state.player == attack_probe.fixture->app->h) {
            attack_probe.actual_call = true;
            attack_probe.damage = state.damage;
            attack_probe.falling_bonus = state.falling_bonus;
        }
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_added(void *context, mc_world_runtime *world,
                                                  mc_entity *entity,
                                                  mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)entity;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_removed(void *context, mc_world_runtime *world,
                                                    mc_entity *entity,
                                                    mc_world_storage_error *error) {
    (void)context;
    if (world == attack_probe.world && entity == attack_probe.victim)
        attack_probe.removed = true;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

/* This authored observer's other notifications are literal returns. The real
 * render, sound and particle listeners remain registered and run normally. */
static mc_world_storage_status attack_probe_cell(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z,
                                                 mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_range(void *context, mc_world_runtime *world,
                                                  int32_t x0, int32_t y0, int32_t z0, int32_t x1,
                                                  int32_t y1, int32_t z1,
                                                  mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x0;
    (void)y0;
    (void)z0;
    (void)x1;
    (void)y1;
    (void)z1;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_particle(void *context, mc_world_runtime *world,
                                                     const mc_registry_string *name, double x,
                                                     double y, double z, double vx, double vy,
                                                     double vz, mc_world_storage_error *error) {
    (void)context;
    (void)x;
    (void)y;
    (void)z;
    (void)vx;
    (void)vy;
    (void)vz;
    /* A World update runs one actual Mob body at a time. Observe this victim's
     * entered death loop, after the production RenderGlobal listener returns. */
    if (world == attack_probe.world && attack_probe.victim->mob->tick_active &&
        attack_probe.victim->mob->living.ad > 20 && attack_probe.victim->base.be) {
        mc_registry_string_view text;
        static const uint16_t expected[] = {'e', 'x', 'p', 'l', 'o', 'd', 'e'};
        mc_effect_renderer_snapshot effects;
        if (!mc_registry_string_read(name, &text) || text.length != 7 ||
            memcmp(text.units, expected, sizeof(expected)) != 0 ||
            !mc_client_effect_renderer_read(attack_probe.fixture->app->effect_renderer, &effects) ||
            effects.sizes[0] == 0 || attack_probe.death_particles == 20) {
            attack_probe.bad_particle = true;
        } else {
            mc_entity *particle = mc_client_effect_renderer_particle(
                attack_probe.fixture->app->effect_renderer, 0, effects.sizes[0] - 1);
            if (particle == NULL ||
                !mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE) ||
                particle->phase != MC_ENTITY_READY || particle->base.world != world)
                attack_probe.bad_particle = true;
            const uintptr_t identity = (uintptr_t)particle;
            for (size_t i = 0; i < attack_probe.death_particles; ++i)
                if (attack_probe.particles[i] == identity)
                    attack_probe.bad_particle = true;
            attack_probe.particles[attack_probe.death_particles++] = identity;
        }
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_tile(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z, mc_tile_entity *tile,
                                                 mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)tile;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_sky(void *context, mc_world_runtime *world,
                                                mc_world_storage_error *error) {
    (void)context;
    (void)world;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status attack_probe_auxiliary(void *context, mc_world_runtime *world,
                                                      mc_entity *player, int32_t effect, int32_t x,
                                                      int32_t y, int32_t z, int32_t data,
                                                      mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)player;
    (void)effect;
    (void)x;
    (void)y;
    (void)z;
    (void)data;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static int attack_probe_remove(void) {
    if (attack_probe.access == NULL)
        return 0;
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(attack_probe.world, attack_probe.access, &removed,
                                               NULL) == MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(attack_probe.access, NULL) == MC_WORLD_STORAGE_OK);
    attack_probe.access = NULL;
    return 0;
}

static int attack_value(fixture *f, size_t index, int32_t expected) {
    mc_registry_statistic_view statistics;
    int32_t value;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.named[index] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.named[index], &value,
                                       NULL) == MC_CLIENT_STATS_OK &&
          value == expected);
    return 0;
}

static int attack_target(fixture *f, mining_work *work, mc_entity **out) {
    mc_entity_view_error *error = calloc(1, sizeof(*error));
    mc_entity_vector *eye = NULL, *look = NULL;
    mc_entity *player = f->app->h;
    CHECK(error != NULL &&
          mc_entity_living_position(player, 1.0f, &eye, error) == MC_ENTITY_VIEW_OK &&
          mc_entity_living_look_current(player, mc_client_bootstrap_math(f->bootstrap), &look,
                                        error) == MC_ENTITY_VIEW_OK &&
          eye != NULL && look != NULL);
    const double x = eye->x + look->x * 2.0, z = eye->z + look->z * 2.0;
    CHECK(mc_entity_vector_release(player->classes, look) == MC_ENTITY_OK &&
          mc_entity_vector_release(player->classes, eye) == MC_ENTITY_OK);
    free(error);
    const mc_entity_class_literal *kind = NULL;
    CHECK(mc_entity_class_literal_get(player->classes, MC_ENTITY_CLASS_KIND_SHEEP, &kind) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(player->classes, f->app->registry, kind, out, NULL) == MC_MOB_OK &&
          mc_mob_construct(*out, f->app->active_world, NULL) == MC_MOB_OK &&
          mc_entity_set_position(*out, x, 64.0, z) == MC_ENTITY_OK);
    /* Explicit component input before real World publication. The Player pose,
     * HitResult, simulation clock and subsequent victim state are untouched. */
    (*out)->mob->living.Y = 1;
    (*out)->mob->living.T = 7;
    CHECK(mc_mob_sheep_set_color(*out, 0, NULL) == MC_MOB_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->app->active_world, *out, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    for (size_t i = 0; i < 64; ++i) {
        CHECK(mining_frame(f, work) == 0);
        mc_entity_hit_result_view hit;
        if (f->app->y != NULL &&
            mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
            hit.fields.g == *out) {
            puts("ATTACK_PICK actual-source-ray=1 same-published-sheep=1 hitresult-untouched=1");
            return mining_capture(f, work, "attack-target.png");
        }
    }
    fprintf(stderr, "Actual source picking did not reach the published Sheep\n");
    return 1;
}

static int attack_run(fixture *f, mining_work *work, mc_entity **victim) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && mining_choose(f, work, NULL, &work->epochs[0]) == 0 &&
          mining_ready(f, work, false) == 0 && attack_value(f, 17, 0) == 0 &&
          attack_value(f, 20, 0) == 0);
    attack_autosave.focused_frames = true;
    CHECK(repeat_focus_state(f, "attack-ready") == 0 && attack_target(f, work, victim) == 0);
    /* A declared read-only WorldAccess observes the actual death-sound call.
     * It samples the Player before the ordinary later World/Player tick. */
    attack_probe.fixture = f;
    attack_probe.victim = *victim;
    attack_probe.world = f->app->active_world;
    const mc_world_access_callbacks observer = {.cell = attack_probe_cell,
                                                .range = attack_probe_range,
                                                .sound = attack_probe_sound,
                                                .particle = attack_probe_particle,
                                                .entity_added = attack_probe_added,
                                                .tile_entity_dirty = attack_probe_tile,
                                                .entity_removed = attack_probe_removed,
                                                .sky_changed = attack_probe_sky,
                                                .auxiliary = attack_probe_auxiliary};
    CHECK(mc_world_access_create(&observer, NULL, &attack_probe.access, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(attack_probe.world, attack_probe.access, NULL) ==
              MC_WORLD_STORAGE_OK);
    const uint64_t swings = f->state.gameplay_swing_calls;
    const size_t events = f->state.mouse_events;
    const int32_t score = f->app->h->local_player->fields.player.g;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    bool killed = false;
    size_t attempts = 0;
    for (; attempts < 12 && SDL_GetTicks() < deadline && !killed; ++attempts) {
        CHECK(f->app->r == NULL && f->app->N && save_title_focus(f) == 0 &&
              mining_button(f, true) == 0);
        work->left_down = true;
        CHECK(mining_button(f, false) == 0);
        work->left_down = false;
        const uint64_t ticks_before = f->state.ticks;
        for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
            CHECK(mining_frame(f, work) == 0);
            killed = (*victim)->mob->living.Y <= 0;
            if (killed || f->state.ticks > ticks_before)
                break;
        }
        if (!killed)
            CHECK((*victim)->mob->living.Y == 1 && f->state.gameplay_swing_calls == swings &&
                  attack_value(f, 17, 0) == 0);
    }
    const int32_t damage = 1 + (attack_probe.velocity_y < 0.0 ? 1 : 0);
    uint64_t velocity_bits;
    memcpy(&velocity_bits, &attack_probe.velocity_y, sizeof(velocity_bits));
    printf("ATTACK_OPERAND y-velocity-bits=%016" PRIx64 " expected-damage=%d actual-damage=%d "
           "falling-bonus=%d sample-count=%zu active-call=%d\n",
           velocity_bits, damage, attack_probe.damage, (int)attack_probe.falling_bonus,
           attack_probe.samples, (int)attack_probe.actual_call);
    CHECK(attack_probe.samples == 1 && attack_probe.actual_call && attack_probe.damage == damage &&
          attack_probe.falling_bonus == (attack_probe.velocity_y < 0.0) && killed &&
          (*victim)->mob->living.Y == 1 - damage && (*victim)->mob->living.ah &&
          f->state.gameplay_swing_calls == swings + 1 && f->state.mouse_events > events &&
          f->app->h->local_player->fields.player.g == mc_java_i32_add(score, 7) &&
          attack_value(f, 17, damage) == 0 && attack_value(f, 20, 1) == 0);
    size_t wool = 0;
    const mc_world_reference_list *entities = f->app->active_world->fields.b;
    for (size_t i = 0; i < entities->count; ++i) {
        mc_entity *entity = entities->items[i];
        mc_item_stack_fields stack;
        if (entity != NULL && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
            mc_item_stack_fields_get(entity->stack, &stack) == MC_ITEM_STACK_OK && stack.id == 35) {
            CHECK(stack.count == 1 && stack.damage == 0);
            ++wool;
        }
    }
    CHECK(wool == 1);
    /* Let the ordinary Timer reach the complete LS.U death loop and the next
     * actual World removal. No age, clock, Random or source field is repaired. */
    for (size_t frames = 0; frames < 2048 && SDL_GetTicks() < deadline &&
                            (attack_probe.death_particles < 20 || !attack_probe.removed);
         ++frames)
        CHECK(mining_frame(f, work) == 0);
    CHECK(!attack_probe.bad_particle && attack_probe.death_particles == 20 &&
          attack_probe.removed && (*victim)->base.be && (*victim)->mob->living.ad > 20);
    printf("ATTACK_DEATH age=%d particles=20 distinct-gy=20 actual-world-remove=1 normal-timer=1\n",
           (*victim)->mob->living.ad);
    CHECK(attack_probe_remove() == 0);
    const size_t autosaves_after_death = attack_autosave.returns;
    /* Reach the next actual periodic save after removal. Earlier saves remain
     * audited. Only ordinary Timer-driven frames advance the World. */
    for (size_t frames = 0; frames < 2048 && SDL_GetTicks() < deadline &&
                            attack_autosave.returns == autosaves_after_death;
         ++frames)
        CHECK(mining_frame(f, work) == 0);
    CHECK(!attack_autosave.failed && !attack_autosave.active &&
          attack_autosave.returns > autosaves_after_death &&
          attack_autosave.entries == attack_autosave.returns);
    printf("ATTACK_AUTOSAVE_POST_DEATH event=%zu world-remove=1 player-score=%d\n",
           attack_autosave.returns - 1, f->app->h->local_player->fields.player.g);
    for (size_t i = 0; i < 3; ++i)
        CHECK(mining_frame(f, work) == 0);
    CHECK(mining_capture(f, work, "attack-returned.png") == 0 && !attack_autosave.failed &&
          !attack_autosave.active && attack_autosave.entries == attack_autosave.returns);
    printf("ATTACK_INPUT sdk-left=1 attempts=%zu mouse-progress=1 swing=1 health-nonpositive=1 "
           "dying=1 "
           "score-added=7 mob-kills=1 damage-dealt=%d wool-items=1 returned-world-hud=3\n",
           attempts, damage);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    mining_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    mining_fixture = f;
    attack_autosave.fixture = f;
    attack_autosave.work = work;
    mining_extra_before_frame = attack_autosave_frame_before;
    mining_extra_after_frame = attack_autosave_frame_after;
    work->directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    mc_entity *victim = NULL;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        mc_client_frame_state host;
        if (f->app != NULL &&
            mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
            host.frame_window != NULL && host.canvas_window != NULL) {
            frame_id = SDL_GetWindowID(host.frame_window);
            canvas_id = SDL_GetWindowID(host.canvas_window);
            result = attack_run(f, work, &victim);
        }
    }
    if (f->app != NULL && work->left_down)
        (void)mining_button(f, false);
    if (attack_probe.access != NULL && attack_probe_remove() != 0)
        result = 1;
    mc_entity_release(victim);
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    if (closed != MC_CLIENT_BOOTSTRAP_OK || f->bootstrap != NULL || f->loop != NULL ||
        frame_id == 0 || canvas_id == 0 || SDL_GetWindowFromID(frame_id) != NULL ||
        SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("ATTACK_AUTOSAVE_SUMMARY entries=%zu returns=%zu passive-observer=1 normal-timer=1\n"
               "ATTACK_FINISH cleanup=1 windows-absent=1 checks=%u\n",
               attack_autosave.entries, attack_autosave.returns, checks);
    free(work);
    free(f);
    return result;
}
