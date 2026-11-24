/* Saved supported TallGrass, real hand/Shears input, Seeds drawing and pickup. */
static int tall_inventory_opened;
#define MC_WORLD_SELECTION_OPEN_INVENTORY_EXPECTED(initial) (1 + tall_inventory_opened)
int mc_tall_unused_leaf_main(int, char **);
#define MC_LEAF_FIXTURE_MAIN mc_tall_unused_leaf_main
#include "test_leaf_harvest.c"
#undef MC_LEAF_FIXTURE_MAIN

#include "client/gui/source_inventory_drawing.h"
#include "client/gui/source_inventory_screen_internal.h"
#include "inventory/player/source_internal.h"
#include "mc/client/world/entity_renderer.h"
#include "mc/world/runtime/world_access.h"

enum { TALL_PLANTS = 64, TALL_FIRST_Z = 2 };

typedef struct tall_birth {
    uintptr_t entity, stack;
    int32_t entity_id, target_z, age, delay;
    mc_item_stack_fields payload;
    double x, y, z;
    size_t world_occurrences, chunk_occurrences, removals;
} tall_birth;

typedef struct tall_observer {
    mc_world_runtime *world;
    mc_world_access *access;
    bool registered, armed, bad;
    int32_t target_z;
    size_t count, unrelated;
    tall_birth births[TALL_PLANTS];
} tall_observer;

/* Observe the actual publication, before any Item tick moves the birth point.
 * No actor or Stack hold changes its source lifetime. Remembered addresses
 * below are identities only; a current World occurrence is needed to read one. */
static bool tall_membership(mc_world_runtime *world, const mc_entity *entity,
                            size_t *world_occurrences, size_t *chunk_occurrences) {
    *world_occurrences = *chunk_occurrences = 0;
    const mc_world_reference_list *list = world->fields.b;
    if (list == NULL || list->count > list->capacity || list->count > 4096)
        return false;
    for (size_t i = 0; i < list->count; ++i)
        *world_occurrences += list->items[i] == entity ? 1u : 0u;
    mc_chunk_state *state = NULL;
    if (!mc_chunk_cache_lookup(world->acquisition.cache, entity->base.bG, entity->base.bI, &state))
        return false;
    mc_world_chunk *chunk = mc_world_store_chunk_object(world->fields.w, state);
    mc_world_chunk_fields fields;
    const mc_world_chunk_entity_list *slice = NULL;
    size_t count;
    if (chunk == NULL || mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK ||
        entity->base.bH < 0 || entity->base.bH >= 8 ||
        mc_world_chunk_slice_at(fields.m, (size_t)entity->base.bH, &slice) != MC_WORLD_STORAGE_OK ||
        mc_world_chunk_entity_count(slice, &count) != MC_WORLD_STORAGE_OK || count > 4096)
        return false;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *member = NULL;
        if (mc_world_chunk_entity_at(slice, i, &member) != MC_WORLD_STORAGE_OK)
            return false;
        *chunk_occurrences += member == entity ? 1u : 0u;
    }
    return true;
}

static mc_world_storage_status tall_observe_cell(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z,
                                                 mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    if (error != NULL)
        *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tall_observe_range(void *context, mc_world_runtime *world, int32_t x,
                                                  int32_t y, int32_t z, int32_t max_x,
                                                  int32_t max_y, int32_t max_z,
                                                  mc_world_storage_error *error) {
    (void)max_x;
    (void)max_y;
    (void)max_z;
    return tall_observe_cell(context, world, x, y, z, error);
}

static mc_world_storage_status tall_observe_sound(void *context, mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, float volume, float pitch,
                                                  mc_world_storage_error *error) {
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    return tall_observe_cell(context, world, 0, 0, 0, error);
}

static mc_world_storage_status tall_observe_particle(void *context, mc_world_runtime *world,
                                                     const mc_registry_string *name, double x,
                                                     double y, double z, double dx, double dy,
                                                     double dz, mc_world_storage_error *error) {
    (void)dx;
    (void)dy;
    (void)dz;
    return tall_observe_sound(context, world, name, x, y, z, 0, 0, error);
}

static mc_world_storage_status tall_observe_added(void *context, mc_world_runtime *world,
                                                  mc_entity *entity,
                                                  mc_world_storage_error *error) {
    tall_observer *observer = context;
    mc_item_stack_fields payload;
    if (entity == NULL || !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) ||
        mc_item_stack_fields_get(entity->stack, &payload) != MC_ITEM_STACK_OK || payload.id != 295)
        return tall_observe_cell(context, world, 0, 0, 0, error);
    /* Exact protected-spawn extrema: nextFloat is in [0, 1-2^-24],
     * its product with .7f rounds to float, then the margin is added as
     * double before the integer Block coordinate, exactly as drops.c. */
    const float spread = .7f;
    const float edge = 1.0f - spread;
    const float max_draw = 16777215.0f / 16777216.0f;
    const float max_scaled = max_draw * spread;
    const double low = (double)edge * .5;
    const double high = (double)max_scaled + low;
    const bool associated = observer->armed && world == observer->world && entity->base.x >= low &&
                            entity->base.x <= high && entity->base.y >= 64.0 + low &&
                            entity->base.y <= 64.0 + high &&
                            entity->base.z >= (double)observer->target_z + low &&
                            entity->base.z <= (double)observer->target_z + high;
    if (!associated) {
        ++observer->unrelated;
        fprintf(stderr,
                "TALL_SDK_UNRELATED_SEED entity=%" PRIuPTR " entity-id=%" PRId32
                " armed=%d target-z=%" PRId32 " pos=%.17g,%.17g,%.17g age=%" PRId32
                " delay=%" PRId32 "\n",
                (uintptr_t)entity, entity->base.id, (int)observer->armed, observer->target_z,
                entity->base.x, entity->base.y, entity->base.z, entity->item.b, entity->item.c);
        return tall_observe_cell(context, world, 0, 0, 0, error);
    }
    if (observer->count == TALL_PLANTS) {
        observer->bad = true;
        return tall_observe_cell(context, world, 0, 0, 0, error);
    }
    tall_birth *birth = &observer->births[observer->count++];
    *birth = (tall_birth){.entity = (uintptr_t)entity,
                          .stack = (uintptr_t)entity->stack,
                          .entity_id = entity->base.id,
                          .target_z = observer->target_z,
                          .age = entity->item.b,
                          .delay = entity->item.c,
                          .payload = payload,
                          .x = entity->base.x,
                          .y = entity->base.y,
                          .z = entity->base.z};
    observer->bad |=
        !tall_membership(world, entity, &birth->world_occurrences, &birth->chunk_occurrences) ||
        birth->world_occurrences != 1 || birth->chunk_occurrences != 1 ||
        entity->base.world != world || entity->phase != MC_ENTITY_READY || entity->base.be ||
        !entity->base.bF || payload.count != 1 || payload.damage != 0 || birth->age != 0 ||
        birth->delay != 10;
    for (size_t i = 0; i + 1 < observer->count; ++i)
        observer->bad |= observer->births[i].target_z == birth->target_z ||
                         (observer->births[i].entity == birth->entity &&
                          observer->births[i].entity_id == birth->entity_id);
    printf("TALL_SDK_BIRTH target-z=%" PRId32 " entity=%" PRIuPTR " entity-id=%" PRId32
           " stack=%" PRIuPTR " item-id=%" PRId32 " count=%" PRId32 " damage=%" PRId32
           " age=%" PRId32 " delay=%" PRId32 " x=%.17g y=%.17g z=%.17g world=%zu chunk=%zu\n",
           birth->target_z, birth->entity, birth->entity_id, birth->stack, payload.id,
           payload.count, payload.damage, birth->age, birth->delay, birth->x, birth->y, birth->z,
           birth->world_occurrences, birth->chunk_occurrences);
    return tall_observe_cell(context, world, 0, 0, 0, error);
}

static mc_world_storage_status tall_observe_removed(void *context, mc_world_runtime *world,
                                                    mc_entity *entity,
                                                    mc_world_storage_error *error) {
    tall_observer *observer = context;
    for (size_t i = 0; i < observer->count; ++i) {
        tall_birth *birth = &observer->births[i];
        if (entity != NULL && birth->entity == (uintptr_t)entity &&
            birth->entity_id == entity->base.id) {
            ++birth->removals;
            observer->bad |= world != observer->world || birth->removals != 1 || !entity->base.be;
            printf("TALL_SDK_REMOVAL entity=%" PRIuPTR " entity-id=%" PRId32 " dead=%d\n",
                   birth->entity, birth->entity_id, (int)entity->base.be);
        }
    }
    return tall_observe_cell(context, world, 0, 0, 0, error);
}

static mc_world_storage_status tall_observe_tile(void *context, mc_world_runtime *world, int32_t x,
                                                 int32_t y, int32_t z, mc_tile_entity *tile,
                                                 mc_world_storage_error *error) {
    (void)tile;
    return tall_observe_cell(context, world, x, y, z, error);
}

static mc_world_storage_status tall_observe_sky(void *context, mc_world_runtime *world,
                                                mc_world_storage_error *error) {
    return tall_observe_cell(context, world, 0, 0, 0, error);
}

static mc_world_storage_status tall_observe_auxiliary(void *context, mc_world_runtime *world,
                                                      mc_entity *player, int32_t effect, int32_t x,
                                                      int32_t y, int32_t z, int32_t data,
                                                      mc_world_storage_error *error) {
    (void)player;
    (void)effect;
    (void)data;
    return tall_observe_cell(context, world, x, y, z, error);
}

static int tall_observer_start(fixture *f, tall_observer *observer) {
    observer->world = f->app->active_world;
    const mc_world_access_callbacks callbacks = {.cell = tall_observe_cell,
                                                 .range = tall_observe_range,
                                                 .sound = tall_observe_sound,
                                                 .particle = tall_observe_particle,
                                                 .entity_added = tall_observe_added,
                                                 .tile_entity_dirty = tall_observe_tile,
                                                 .entity_removed = tall_observe_removed,
                                                 .sky_changed = tall_observe_sky,
                                                 .auxiliary = tall_observe_auxiliary};
    CHECK(mc_world_access_create(&callbacks, observer, &observer->access, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(observer->world, observer->access, NULL) ==
          MC_WORLD_STORAGE_OK);
    observer->registered = true;
    return 0;
}

static int tall_observer_remove(tall_observer *observer) {
    if (observer->access == NULL)
        return 0;
    observer->armed = false;
    if (observer->registered) {
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(observer->world, observer->access, &removed,
                                                   NULL) == MC_WORLD_STORAGE_OK &&
              removed);
        observer->registered = false;
    }
    CHECK(mc_world_access_free(observer->access, NULL) == MC_WORLD_STORAGE_OK);
    observer->access = NULL;
    printf("TALL_SDK_OBSERVER removed=1 freed=1 births=%zu unrelated=%zu\n", observer->count,
           observer->unrelated);
    return 0;
}

static size_t tall_removed_count(const tall_observer *observer) {
    size_t count = 0;
    for (size_t i = 0; i < observer->count; ++i)
        count += observer->births[i].removals;
    return count;
}

static int tall_cell(fixture *f, int32_t z, int32_t *out) {
    CHECK(mc_world_runtime_block_get(f->app->active_world, 0, 64, z, out, NULL) ==
          MC_WORLD_STORAGE_OK);
    return 0;
}

static bool tall_picked(fixture *f, int32_t z) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    return f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
           mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
           hit.fields.a == kinds.a && hit.fields.b == 0 && hit.fields.c == 64 && hit.fields.d == z;
}

static int tall_key(fixture *f, SDL_Scancode code) {
    CHECK(gameplay_key(f, code, true) == 0 && gameplay_key(f, code, false) == 0 &&
          transfer(f) == 0);
    return 0;
}

static int tall_slot(fixture *f, int32_t index, const mc_item_stack **out,
                     mc_item_stack_fields *fields) {
    CHECK(mc_inventory_player_source_get(f->app->h->local_player->fields.player.c, index, out,
                                         NULL) == MC_INVENTORY_OK);
    *fields = (mc_item_stack_fields){0};
    if (*out != NULL)
        CHECK(mc_item_stack_fields_get(*out, fields) == MC_ITEM_STACK_OK);
    return 0;
}

static int tall_shears(fixture *f) {
    const mc_item_stack *stack = NULL;
    mc_item_stack_fields fields;
    CHECK(tall_slot(f, 1, &stack, &fields) == 0 && stack != NULL && fields.id == 359 &&
          fields.count == 1 && fields.damage == 238);
    return 0;
}

static int tall_inventory_seeds(fixture *f, int32_t *out) {
    *out = 0;
    for (int32_t slot = 0; slot < 36; ++slot) {
        const mc_item_stack *stack = NULL;
        mc_item_stack_fields fields;
        CHECK(tall_slot(f, slot, &stack, &fields) == 0);
        if (stack == NULL)
            continue;
        CHECK((slot == 1 && fields.id == 359 && fields.count == 1 && fields.damage == 238) ||
              (slot == 0 && fields.id == 295 && fields.count >= 1 && fields.count <= 2 &&
               fields.damage == 0));
        if (fields.id == 295)
            *out += fields.count;
    }
    return 0;
}

static int tall_item(fixture *f, const tall_observer *observer, size_t *count, mc_entity **out) {
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    CHECK(!observer->bad && f->app->active_world == observer->world && list != NULL &&
          list->count <= list->capacity && list->count <= 4096);
    *count = 0;
    *out = NULL;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *entity = list->items[i];
        if (entity == NULL)
            continue;
        const tall_birth *birth = NULL;
        for (size_t j = 0; j < observer->count; ++j)
            if (observer->births[j].entity == (uintptr_t)entity &&
                observer->births[j].entity_id == entity->base.id)
                birth = &observer->births[j];
        if (birth == NULL || entity->base.be)
            continue;
        mc_item_stack_fields fields;
        CHECK(birth->removals == 0 && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
              (uintptr_t)entity->stack == birth->stack && entity->stack != NULL &&
              mc_item_stack_fields_get(entity->stack, &fields) == MC_ITEM_STACK_OK &&
              fields.id == 295 && fields.count == 1 && fields.damage == 0 &&
              entity->base.world == f->app->active_world && entity->phase == MC_ENTITY_READY &&
              entity->base.bF && entity->item.c >= 0 && entity->item.c <= 10);
        size_t world_occurrences, chunk_occurrences;
        CHECK(tall_membership(observer->world, entity, &world_occurrences, &chunk_occurrences) &&
              world_occurrences == 1 && chunk_occurrences == 1);
        ++*count;
        *out = entity;
    }
    CHECK(*count <= 2);
    return 0;
}

static mc_entity *tall_world_item(fixture *f, uintptr_t wanted, int32_t entity_id,
                                  size_t *occurrences) {
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    *occurrences = 0;
    if (list == NULL || list->count > list->capacity || list->count > 4096)
        return NULL;
    mc_entity *found = NULL;
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity *candidate = list->items[i];
        if (candidate != NULL && (uintptr_t)candidate == wanted &&
            candidate->base.id == entity_id) {
            ++*occurrences;
            found = candidate;
        }
    }
    return *occurrences == 1 ? found : NULL;
}

static void tall_draw_receipt(const char *name, const mc_client_render_global_draw_receipt *draw,
                              uintptr_t wanted) {
    fprintf(stderr,
            "TALL_SDK_DRAW_RECEIPT name=%s valid=%d returned=%d retired=%d actor=%" PRIuPTR
            " wanted=%" PRIuPTR " same=%d kind=%" PRId32 " renderer=%s primary=%d post=%d"
            " living-pc=%u manager-pc=%u post-pc=%u attempt=%zu\n",
            name, (int)draw->valid, (int)draw->returned, (int)draw->retired, draw->actor_identity,
            wanted, (int)(draw->actor_identity == wanted), draw->actor_kind,
            draw->renderer_class_name == NULL ? "none" : draw->renderer_class_name,
            (int)draw->primary_returned, (int)draw->post_draw_returned, draw->living_source_pc,
            draw->th_source_pc, draw->post_draw_source_pc, draw->attempt);
}

static void tall_draw_failure(fixture *f, mining_work *work, uintptr_t wanted, int32_t entity_id,
                              size_t samples, size_t world_absent, size_t returned_item_frames) {
    size_t occurrences;
    mc_entity *item = tall_world_item(f, wanted, entity_id, &occurrences);
    int32_t seeds = -1;
    (void)tall_inventory_seeds(f, &seeds);
    fprintf(stderr,
            "TALL_SDK_DRAW_FAILURE wanted=%" PRIuPTR
            " samples=%zu world-absent-frames=%zu item-draw-frames=%zu world-occurrences=%zu"
            " inventory-seeds=%" PRId32 "\n",
            wanted, samples, world_absent, returned_item_frames, occurrences, seeds);
    const mc_client_render_global_entities_state *entities =
        f->state.world_frame == NULL ? NULL : f->state.world_frame->entities;
    if (entities != NULL) {
        fprintf(stderr,
                "TALL_SDK_DRAW_WORLD returned=%d delayed=%d index=%" PRId32
                " visited=%zu distance-checks=%zu frustum-checks=%zu presence-checks=%zu"
                " camera-skips=%zu draws=%zu/%zu/%zu item-draws=%zu pc=%u\n",
                (int)entities->returned, (int)entities->delayed, entities->index,
                entities->entities_visited, entities->distance_checks, entities->frustum_checks,
                entities->presence_checks, entities->camera_skips, entities->draws_attempted,
                entities->draws_returned, entities->draws_retired, entities->item_draws_returned,
                entities->source_pc);
        tall_draw_receipt("first-item", &entities->first_item_draw, wanted);
        tall_draw_receipt("first-entity", &entities->first_entity_draw, wanted);
        tall_draw_receipt("last", &entities->last_draw, wanted);
    }
    /* A remembered address is an identity only. Dereference the actor only
     * after finding its still-owned occurrence in the current World List. */
    if (item != NULL) {
        mc_item_stack_fields stack = {0};
        const mc_item_stack_status status = mc_item_stack_fields_get(item->stack, &stack);
        fprintf(stderr,
                "TALL_SDK_DRAW_ITEM actor=%" PRIuPTR " id=%" PRId32
                " dead=%d same-world=%d pos=%.17g,%.17g,%.17g age=%" PRId32 " delay=%" PRId32
                " chunk=%" PRId32 ",%" PRId32 ",%" PRId32 " stack-status=%d stack=%" PRId32
                ",%" PRId32 ",%" PRId32 "\n",
                (uintptr_t)item, item->base.id, (int)item->base.be,
                (int)(item->base.world == f->app->active_world), item->base.x, item->base.y,
                item->base.z, item->item.b, item->item.c, item->base.bG, item->base.bH,
                item->base.bI, (int)status, stack.id, stack.count, stack.damage);
        size_t chunk_occurrences = 0;
        mc_chunk_state *state = NULL;
        if (mc_chunk_cache_lookup(f->app->active_world->acquisition.cache, item->base.bG,
                                  item->base.bI, &state)) {
            mc_world_chunk *chunk =
                mc_world_store_chunk_object(f->app->active_world->fields.w, state);
            mc_world_chunk_fields fields;
            const mc_world_chunk_entity_list *slice = NULL;
            size_t count;
            if (chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
                item->base.bH >= 0 && item->base.bH < 8 &&
                mc_world_chunk_slice_at(fields.m, (size_t)item->base.bH, &slice) ==
                    MC_WORLD_STORAGE_OK &&
                mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK && count <= 4096)
                for (size_t i = 0; i < count; ++i) {
                    mc_entity *member = NULL;
                    if (mc_world_chunk_entity_at(slice, i, &member) == MC_WORLD_STORAGE_OK)
                        chunk_occurrences += (uintptr_t)member == wanted ? 1u : 0u;
                }
        }
        fprintf(stderr, "TALL_SDK_DRAW_CHUNK occurrences=%zu\n", chunk_occurrences);
        mc_client_source_frustum_state frustum;
        if (entities != NULL && entities->frustum != NULL && item->bounds != NULL &&
            mc_client_source_frustum_inspect(entities->frustum, &frustum)) {
            const mc_entity_bounds *bounds = item->bounds;
            bool visible = false;
            mc_client_frustum_error error;
            const mc_client_frustum_status checked = mc_client_frustum_box(
                frustum.a, bounds->min_x - frustum.b, bounds->min_y - frustum.c,
                bounds->min_z - frustum.d, bounds->max_x - frustum.b, bounds->max_y - frustum.c,
                bounds->max_z - frustum.d, &visible, &error);
            fprintf(stderr,
                    "TALL_SDK_DRAW_FRUSTUM diagnostic-recheck=1 status=%d visible=%d"
                    " origin=%.17g,%.17g,%.17g\n",
                    (int)checked, (int)visible, frustum.b, frustum.c, frustum.d);
        }
    }
    const int captured = mining_capture(f, work, "tall-grass-draw-failure.png");
    fprintf(stderr, "TALL_SDK_DRAW_FAILURE_CAPTURE returned=%d\n", captured == 0 ? 1 : 0);
    (void)fflush(stderr);
}

static int tall_draw(fixture *f, mining_work *work, mc_entity *item) {
    const uintptr_t wanted = (uintptr_t)item;
    const int32_t entity_id = item->base.id;
    fprintf(stderr,
            "TALL_SDK_DRAW_ENTRY wanted=%" PRIuPTR " id=%" PRId32
            " pos=%.17g,%.17g,%.17g age=%" PRId32 " delay=%" PRId32 "\n",
            wanted, item->base.id, item->base.x, item->base.y, item->base.z, item->item.b,
            item->item.c);
    size_t samples = 0, world_absent = 0, returned_item_frames = 0;
    const uint64_t deadline = SDL_GetTicks() + 15000;
    do {
        ++samples;
        size_t occurrences;
        mc_entity *live = tall_world_item(f, wanted, entity_id, &occurrences);
        world_absent += live == NULL ? 1u : 0u;
        const mc_client_render_global_entities_state *entities =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->entities;
        returned_item_frames +=
            entities != NULL && entities->returned && entities->item_draws_returned > 0 ? 1u : 0u;
        const mc_client_render_global_draw_receipt *receipt = NULL;
        if (live != NULL && !live->base.be && entities != NULL && entities->returned &&
            entities->item_draws_returned > 0) {
            if (leaf_drawn(&entities->first_item_draw, live))
                receipt = &entities->first_item_draw;
            else if (leaf_drawn(&entities->first_entity_draw, live))
                receipt = &entities->first_entity_draw;
            else if (leaf_drawn(&entities->last_draw, live))
                receipt = &entities->last_draw;
        }
        if (receipt != NULL) {
            printf("TALL_SDK_DRAW entity=%" PRIuPTR " entity-id=%" PRId32
                   " source-pc=%u valid=%d returned=%d retired=%d primary=%d post=%d renderer=%s\n",
                   receipt->actor_identity, entity_id, receipt->living_source_pc,
                   (int)receipt->valid, (int)receipt->returned, (int)receipt->retired,
                   (int)receipt->primary_returned, (int)receipt->post_draw_returned,
                   receipt->renderer_class_name);
            return leaf_focus(f);
        }
        CHECK(leaf_frame(f) == 0 && f->app->r == NULL && f->app->N);
    } while (SDL_GetTicks() < deadline);
    tall_draw_failure(f, work, wanted, entity_id, samples, world_absent, returned_item_frames);
    fprintf(stderr, "TallGrass Seed Item did not return and retire its actual BB draw\n");
    return 1;
}

static void tall_failure(fixture *f, const mining_work *work, const char *stage, int32_t z,
                         int returned) {
    native_focus_receipt(f, stage);
    mc_entity_hit_result_view hit = {0};
    const bool picked =
        f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK;
    fprintf(stderr,
            "TALL_SDK_FAILURE stage=%s requested-z=%" PRId32
            " frame-result=%d frame-returned=%d failed=%d running=%d screen=%d source-focus=%d"
            " paused=%d forward-down=%d picked=%d target=%" PRId32 ",%" PRId32 ",%" PRId32
            " ticks=%" PRIu64 " mouse-events=%zu keyboard-events=%zu\n",
            stage, z, returned, (int)f->state.clocks.frame_returned, (int)f->state.failed,
            (int)f->state.running, f->app->r == NULL ? -1 : (int)f->app->r->kind, (int)f->app->N,
            (int)atomic_load(&f->app->o), (int)work->forward_down, (int)picked,
            picked ? hit.fields.b : -1, picked ? hit.fields.c : -1, picked ? hit.fields.d : -1,
            f->state.ticks, f->state.mouse_events, f->state.keyboard_events);
    const mc_entity *player = f->app->h;
    if (player != NULL)
        fprintf(stderr,
                "TALL_SDK_FAILURE_PLAYER pos=%.17g,%.17g,%.17g yaw=%.9g pitch=%.9g dead=%d\n",
                player->base.x, player->base.y, player->base.z, (double)player->base.yaw,
                (double)player->base.aT, (int)player->base.be);
    mc_client_sdk_input_state input;
    if (mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input))
        fprintf(stderr,
                "TALL_SDK_FAILURE_KEY key=%" PRId32 " pressed=%d repeat=%d pending-native=%" PRIuPTR
                "\n",
                input.current_key.key, (int)input.current_key.pressed,
                (int)input.current_key.repeat, (uintptr_t)input.pending_native_event);
    (void)fflush(stderr);
}

static int tall_walk_to_pick(fixture *f, mining_work *work, int32_t z) {
    if (tall_picked(f, z))
        return 0;
    CHECK(gameplay_key(f, SDL_SCANCODE_W, true) == 0 && transfer(f) == 0);
    work->forward_down = true;
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (!tall_picked(f, z) && SDL_GetTicks() < deadline) {
        const int returned = leaf_frame(f);
        const bool valid = returned == 0 && f->app->r == NULL && f->app->N;
        if (!valid)
            tall_failure(f, work, "tall-grass-walk-failure", z, returned);
        CHECK(valid);
    }
    CHECK(gameplay_key(f, SDL_SCANCODE_W, false) == 0 && transfer(f) == 0);
    work->forward_down = false;
    CHECK(leaf_frame(f) == 0 && tall_picked(f, z));
    return 0;
}

static int tall_click(fixture *f, tall_observer *observer, int32_t z, uint64_t expected_mined) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (unsigned int attempt = 0; attempt < 12 && SDL_GetTicks() < deadline; ++attempt) {
        if (f->app->r != NULL || !f->app->N)
            CHECK(leaf_resume(f) == 0);
        leaf_mouse queue;
        CHECK(leaf_mouse_read(f, 0, &queue) == 0);
        while ((queue.read.position != queue.read.limit || queue.queue.position != 0) &&
               SDL_GetTicks() < deadline) {
            CHECK(leaf_frame(f) == 0);
            CHECK(leaf_mouse_read(f, 0, &queue) == 0);
        }
        int32_t before;
        CHECK(queue.read.position == queue.read.limit && queue.queue.position == 0 &&
              tall_cell(f, z, &before) == 0 && before == 31 && tall_picked(f, z) &&
              !observer->bad && !observer->armed);
        observer->target_z = z;
        observer->armed = true;
        uint64_t timestamp = 0;
        for (unsigned int i = 0; i < 2; ++i) {
            SDL_Event event = {0};
            event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            event.common.timestamp = SDL_GetTicksNS();
            event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
            event.button.button = SDL_BUTTON_LEFT;
            event.button.down = i == 0;
            timestamp = event.common.timestamp;
            CHECK(SDL_PushEvent(&event));
        }
        CHECK(transfer(f) == 0);
        const uint64_t clicks = f->state.gameplay_left_clicks;
        bool released = false;
        while (SDL_GetTicks() < deadline) {
            leaf_mouse previous, after;
            CHECK(leaf_mouse_read(f, timestamp, &previous) == 0);
            const size_t events = f->state.mouse_events;
            const size_t unread = (previous.read.limit - previous.read.position) / 22;
            CHECK(leaf_frame(f) == 0 && leaf_mouse_read(f, timestamp, &after) == 0);
            released |= previous.release_unread && f->state.mouse_events - events >= unread;
            released |= after.input.current_mouse.button == 0 &&
                        !after.input.current_mouse.pressed &&
                        (uint64_t)after.input.current_mouse.nanos == timestamp;
            int32_t target;
            CHECK(tall_cell(f, z, &target) == 0);
            if (target == 0) {
                observer->armed = false;
                CHECK(f->state.gameplay_left_clicks > clicks &&
                      f->state.gameplay_mined_blocks == expected_mined && !observer->bad &&
                      leaf_focus(f) == 0);
                printf("TALL_SDK_INPUT z=%" PRId32 " attempt=%u mined=%" PRIu64
                       " actual-clicks=%" PRIu64 " release-consumed=%d focus=1\n",
                       z, attempt + 1, expected_mined, f->state.gameplay_left_clicks,
                       (int)released);
                return 0;
            }
            CHECK(target == 31);
            if (released)
                break;
        }
        observer->armed = false;
        CHECK(released);
    }
    fprintf(stderr, "TallGrass source tap was not consumed within the declared bound\n");
    return 1;
}

static int tall_inventory_scene(fixture *f, mining_work *work, int32_t seeds) {
    CHECK(tall_key(f, SDL_SCANCODE_E) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    mc_client_source_ue *screen = NULL;
    while (SDL_GetTicks() < deadline) {
        CHECK(leaf_frame(f) == 0);
        if (f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_INVENTORY) {
            screen = (mc_client_source_ue *)f->app->r;
            break;
        }
    }
    CHECK(screen != NULL);
    ++tall_inventory_opened;
    /* Every actual Slot inherits GP.e()I returning -1, so all 45 invoke BB,
     * including NULL Stacks. The two nonempty payloads are checked separately. */
    for (unsigned int i = 0; i < 3; ++i) {
        CHECK(leaf_frame(f) == 0 && f->app->r == &screen->container.base && !f->app->N &&
              f->state.inventory != NULL && f->state.inventory->returned &&
              f->state.inventory->receiver == screen && f->state.inventory->background_returned &&
              f->state.inventory->preview_returned && f->state.inventory->pose_restored &&
              f->state.inventory->foreground_returned && f->state.inventory->mouse_stored &&
              f->state.inventory->slots_drawn == 45 && f->state.inventory->item_draws == 45 &&
              !f->state.inventory->cursor_drawn &&
              screen->container.id.j == f->app->h->local_player->fields.player.d &&
              f->app->h->local_player->fields.player.e == screen->container.id.j);
        int32_t actual;
        CHECK(tall_inventory_seeds(f, &actual) == 0 && actual == seeds);
    }
    CHECK(mining_capture(f, work, "tall-grass-inventory.png") == 0 &&
          tall_key(f, SDL_SCANCODE_E) == 0);
    const uint64_t close_deadline = SDL_GetTicks() + 15000;
    while ((f->app->r != NULL || !f->app->N) && SDL_GetTicks() < close_deadline)
        CHECK(leaf_frame(f) == 0);
    return leaf_focus(f);
}

typedef struct tall_held_observation {
    bool inventory_read, renderer_read;
    mc_inventory_status current_status;
    mc_item_stack_status current_payload_status, cached_payload_status;
    mc_inventory_player_state inventory;
    mc_client_entity_renderer_state renderer;
    const mc_item_stack *current;
    mc_item_stack_fields current_payload, cached_payload;
} tall_held_observation;

static void tall_held_read(fixture *f, tall_held_observation *observation) {
    *observation = (tall_held_observation){.current_status = MC_INVENTORY_INVALID_ARGUMENT,
                                           .current_payload_status = MC_ITEM_STACK_INVALID_ARGUMENT,
                                           .cached_payload_status = MC_ITEM_STACK_INVALID_ARGUMENT,
                                           .inventory = {.selected = -1}};
    const mc_entity *player = f->app->h;
    if (player != NULL && player->local_player != NULL) {
        mc_inventory_player *inventory = player->local_player->fields.player.c;
        observation->inventory_read =
            mc_inventory_player_read_state(inventory, &observation->inventory);
        observation->current_status =
            mc_inventory_player_current(inventory, &observation->current, NULL);
        if (observation->current_status == MC_INVENTORY_OK && observation->current != NULL)
            observation->current_payload_status =
                mc_item_stack_fields_get(observation->current, &observation->current_payload);
    }
    observation->renderer_read =
        mc_client_entity_renderer_inspect(f->app->t, &observation->renderer);
    /* RA owns this separate Stack occurrence across completed child retirement.
     * The completed first-person snapshot deliberately censors draw->stack
     * before overlays; it is never used as a live Stack getter receiver here. */
    if (observation->renderer_read && observation->renderer.ra_b != NULL)
        observation->cached_payload_status =
            mc_item_stack_fields_get(observation->renderer.ra_b, &observation->cached_payload);
}

static void tall_held_failure(fixture *f, unsigned int held_frames) {
    tall_held_observation observation;
    tall_held_read(f, &observation);
    native_focus_receipt(f, "tall-grass-held-timeout");
    fprintf(stderr,
            "TALL_SDK_HELD_FAILURE frames=%u inventory-read=%d selected=%" PRId32
            " current-status=%d current=%" PRIuPTR " current-payload-status=%d"
            " current-payload=%" PRId32 ",%" PRId32 ",%" PRId32 " renderer-read=%d cache=%" PRIuPTR
            " cached-payload-status=%d"
            " cached-payload=%" PRId32 ",%" PRId32 ",%" PRId32 "\n",
            held_frames, (int)observation.inventory_read, observation.inventory.selected,
            (int)observation.current_status, (uintptr_t)observation.current,
            (int)observation.current_payload_status, observation.current_payload.id,
            observation.current_payload.count, observation.current_payload.damage,
            (int)observation.renderer_read, (uintptr_t)observation.renderer.ra_b,
            (int)observation.cached_payload_status, observation.cached_payload.id,
            observation.cached_payload.count, observation.cached_payload.damage);
    const mc_client_first_person_hand_state *hand =
        f->state.world_frame == NULL ? NULL : f->state.world_frame->hand;
    const mc_client_held_item_draw_state empty = {0};
    const mc_client_held_item_draw_state *draw =
        hand == NULL || hand->held_item == NULL ? &empty : hand->held_item;
    fprintf(stderr,
            "TALL_SDK_HELD_SNAPSHOT hand=%d hand-returned=%d held-returned=%d draw=%d"
            " draw-returned=%d censored-stack=%" PRIuPTR " stack-captured=%d empty-hand=%d"
            " flat=%d icon-returned=%d icon=%" PRId32 " geometry-returned=%d faces=%u"
            " vertices=%u hud=%d hud-returned=%d hotbar-returned=%d\n",
            hand != NULL ? 1 : 0, hand != NULL && hand->returned ? 1 : 0,
            hand != NULL && hand->held_returned ? 1 : 0,
            hand != NULL && hand->held_item != NULL ? 1 : 0, (int)draw->returned,
            (uintptr_t)draw->stack, (int)draw->stack_captured, (int)draw->empty_hand,
            (int)draw->flat_item, (int)draw->item_icon_returned, draw->item_icon,
            (int)draw->item_geometry_returned, draw->faces, draw->vertices,
            f->state.hud != NULL ? 1 : 0, f->state.hud != NULL && f->state.hud->returned ? 1 : 0,
            f->state.hud != NULL && f->state.hud->hotbar_returned ? 1 : 0);
    (void)fflush(stderr);
}

static int tall_run(fixture *f, mining_work *work, tall_observer *observer) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && leaf_choose(f) == 0 && tall_observer_start(f, observer) == 0 &&
          tall_shears(f) == 0 && mining_capture(f, work, "tall-grass-before.png") == 0);
    const mc_item_stack *held = NULL;
    CHECK(mc_inventory_player_current(f->app->h->local_player->fields.player.c, &held, NULL) ==
              MC_INVENTORY_OK &&
          held == NULL);
    mc_registry_statistic_view statistics;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &statistics, NULL) ==
              MC_REGISTRY_OK &&
          statistics.C != NULL && statistics.C[31] != NULL && statistics.E != NULL &&
          statistics.E[359] != NULL && statistics.F != NULL && statistics.F[359] != NULL);
    int32_t mined_before, used_before, broken_before;
    CHECK(mc_client_stats_writer_value(f->app->stats_writer, statistics.C[31], &mined_before,
                                       NULL) == MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.E[359], &used_before,
                                       NULL) == MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.F[359], &broken_before,
                                       NULL) == MC_CLIENT_STATS_OK);
    const double scene_z_start = f->app->h->base.z;
    int32_t initial_seeds;
    CHECK(tall_inventory_seeds(f, &initial_seeds) == 0 && initial_seeds == 0);
    uint64_t mined = 0;
    bool drawn = false;
    for (int32_t plant = 0; plant < TALL_PLANTS; ++plant) {
        if (plant == 1) {
            CHECK(tall_key(f, SDL_SCANCODE_2) == 0);
            const uint64_t selected_deadline = SDL_GetTicks() + 15000;
            do {
                CHECK(leaf_frame(f) == 0 &&
                      mc_inventory_player_current(f->app->h->local_player->fields.player.c, &held,
                                                  NULL) == MC_INVENTORY_OK);
            } while (held == NULL && SDL_GetTicks() < selected_deadline);
            mc_item_stack_fields selected;
            CHECK(held != NULL && mc_item_stack_fields_get(held, &selected) == MC_ITEM_STACK_OK &&
                  selected.id == 359 && selected.damage == 238);
        }
        const int32_t z = TALL_FIRST_Z + plant;
        CHECK(tall_walk_to_pick(f, work, z) == 0 && tall_click(f, observer, z, mined + 1) == 0);
        ++mined;
        CHECK(tall_shears(f) == 0);
        size_t items;
        mc_entity *item = NULL;
        CHECK(tall_item(f, observer, &items, &item) == 0);
        if (item != NULL && !drawn) {
            CHECK(item->item.c > 0 && item->item.c <= 10);
            CHECK(tall_draw(f, work, item) == 0 &&
                  mining_semantic(item, work, "tall-grass-item.nbt") == 0);
            drawn = true;
        }
        int32_t inventory;
        CHECK(tall_item(f, observer, &items, &item) == 0 &&
              tall_inventory_seeds(f, &inventory) == 0 &&
              (size_t)inventory + items == observer->count);
        if (mined >= 2 && observer->count != 0)
            break;
    }
    size_t live_items;
    mc_entity *last_item = NULL;
    int32_t already_picked;
    CHECK(mined >= 2 && mined <= TALL_PLANTS && drawn &&
          tall_item(f, observer, &live_items, &last_item) == 0 &&
          tall_inventory_seeds(f, &already_picked) == 0);
    const int32_t seeds = (int32_t)observer->count;
    CHECK(seeds >= 1 && seeds <= 2 && already_picked + (int32_t)live_items == seeds &&
          mining_capture(f, work, "tall-grass-dropped.png") == 0);
    const double before_z = f->app->h->base.z;
    CHECK(gameplay_key(f, SDL_SCANCODE_W, true) == 0 && transfer(f) == 0);
    work->forward_down = true;
    const uint64_t pickup_deadline = SDL_GetTicks() + 60000;
    int32_t picked = already_picked;
    while (SDL_GetTicks() < pickup_deadline &&
           (picked != seeds || tall_removed_count(observer) != observer->count)) {
        CHECK(leaf_frame(f) == 0 && tall_inventory_seeds(f, &picked) == 0);
    }
    CHECK(gameplay_key(f, SDL_SCANCODE_W, false) == 0 && transfer(f) == 0);
    work->forward_down = false;
    CHECK(picked == seeds && !observer->bad && tall_removed_count(observer) == observer->count &&
          (already_picked == seeds || f->app->h->base.z > before_z) &&
          tall_item(f, observer, &live_items, &last_item) == 0 && live_items == 0 &&
          tall_key(f, SDL_SCANCODE_1) == 0);
    const uint64_t held_deadline = SDL_GetTicks() + 15000;
    unsigned int held_frames = 0;
    while (held_frames < 3 && SDL_GetTicks() < held_deadline) {
        CHECK(leaf_frame(f) == 0);
        const mc_client_first_person_hand_state *hand =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->hand;
        const mc_client_held_item_draw_state *draw = hand == NULL ? NULL : hand->held_item;
        tall_held_observation observation;
        tall_held_read(f, &observation);
        if (hand != NULL && hand->returned && hand->held_returned && draw != NULL &&
            draw->returned && draw->stack_captured && !draw->empty_hand &&
            observation.inventory_read && observation.inventory.selected == 0 &&
            observation.current_status == MC_INVENTORY_OK && observation.current != NULL &&
            observation.renderer_read && observation.renderer.ra_b == observation.current &&
            observation.current_payload_status == MC_ITEM_STACK_OK &&
            observation.cached_payload_status == MC_ITEM_STACK_OK &&
            observation.current_payload.id == 295 && observation.current_payload.count == seeds &&
            observation.current_payload.damage == 0 && observation.cached_payload.id == 295 &&
            observation.cached_payload.count == seeds && observation.cached_payload.damage == 0) {
            CHECK(draw->stack == NULL && draw->flat_item && draw->item_icon_returned &&
                  draw->item_geometry_returned && draw->item_icon == 9 && draw->faces == 66 &&
                  draw->vertices == 264 && f->state.hud != NULL && f->state.hud->returned &&
                  f->state.hud->hotbar_returned);
            ++held_frames;
            printf(
                "TALL_SDK_HELD frame=%u selected=%" PRId32 " current=%" PRIuPTR " cache=%" PRIuPTR
                " item-id=%" PRId32 " count=%" PRId32 " damage=%" PRId32
                " hand-returned=%d held-returned=%d draw-returned=%d stack-captured=%d"
                " empty-hand=%d child-stack-null=%d flat=%d icon-returned=%d icon=%" PRId32
                " geometry-returned=%d faces=%u vertices=%u hud-returned=%d hotbar-returned=%d\n",
                held_frames, observation.inventory.selected, (uintptr_t)observation.current,
                (uintptr_t)observation.renderer.ra_b, observation.current_payload.id,
                observation.current_payload.count, observation.current_payload.damage,
                (int)hand->returned, (int)hand->held_returned, (int)draw->returned,
                (int)draw->stack_captured, (int)draw->empty_hand, draw->stack == NULL ? 1 : 0,
                (int)draw->flat_item, (int)draw->item_icon_returned, draw->item_icon,
                (int)draw->item_geometry_returned, draw->faces, draw->vertices,
                (int)f->state.hud->returned, (int)f->state.hud->hotbar_returned);
        }
    }
    if (held_frames != 3)
        tall_held_failure(f, held_frames);
    int32_t mined_after, used_after, broken_after;
    CHECK(held_frames == 3 && f->app->h->base.z > scene_z_start &&
          tall_inventory_scene(f, work, seeds) == 0 && tall_shears(f) == 0 &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.C[31], &mined_after,
                                       NULL) == MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.E[359], &used_after,
                                       NULL) == MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app->stats_writer, statistics.F[359], &broken_after,
                                       NULL) == MC_CLIENT_STATS_OK &&
          mc_java_i32_sub(mined_after, mined_before) == (int32_t)mined &&
          used_after == used_before && broken_after == broken_before &&
          mining_semantic(f->app->h, work, "tall-grass-player.nbt") == 0 &&
          mining_capture(f, work, "tall-grass-after.png") == 0);
    mc_render_global_auxiliary_observation auxiliary;
    CHECK(mc_render_global_access_auxiliary_observe(f->app->g, &auxiliary) &&
          !auxiliary.overflowed && auxiliary.event2001_entered == mined &&
          auxiliary.event2001_returned == mined &&
          auxiliary.destruction_constructed == mined * 64 &&
          auxiliary.destruction_appended == mined * 64 && auxiliary.last.block_id == 31 &&
          auxiliary.last.sound_before_destruction && auxiliary.last.source_returned &&
          auxiliary.last.native_cleanup_returned);
    printf("TALL_SDK_HARVEST plants=%" PRIu64 " empty-hand=1 shears-punches=%" PRIu64
           " shears-damage=238 used-stat=0 broken-stat=0 mined-stat=%" PRIu64 " seeds=%" PRId32
           " live-items=0 item-draw=1 sdk-movement=1 pickup=1 held-frames=3"
           " held-icon=9 held-vertices=264 inventory-draws=3 focus=1 event2001=%" PRIu64
           " destruction-particles=%" PRIu64 "\n",
           mined, mined - 1, mined, seeds, mined, mined * 64);
    CHECK(fflush(stdout) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    mining_work *work = calloc(1, sizeof(*work));
    CHECK(f != NULL && work != NULL);
    mining_fixture = f;
    work->directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    CHECK(mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f->app = mc_client_bootstrap_app(f->bootstrap);
    mc_client_frame_state host;
    CHECK(f->app != NULL &&
          mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
          host.frame_window != NULL && host.canvas_window != NULL);
    const uint32_t frame_id = SDL_GetWindowID(host.frame_window),
                   canvas_id = SDL_GetWindowID(host.canvas_window);
    tall_observer observer = {0};
    int result = tall_run(f, work, &observer);
    if (result != 0)
        tall_failure(f, work, "tall-grass-sdk-failure", -1, result);
    if (work->forward_down) {
        (void)gameplay_key(f, SDL_SCANCODE_W, false);
        (void)transfer(f);
    }
    if (tall_observer_remove(&observer) != 0)
        result = 1;
    if (leaf_stop(f, frame_id, "tall-grass-stopped") != 0)
        result = 1;
    if (!mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    const bool cleaned = closed == MC_CLIENT_BOOTSTRAP_OK && f->bootstrap == NULL &&
                         f->loop == NULL && SDL_GetWindowFromID(frame_id) == NULL &&
                         SDL_GetWindowFromID(canvas_id) == NULL;
    if (cleaned && reload_snapshot("tall-grass-after-native-close") != 0)
        result = 1;
    printf("TALL_SDK_FINISH cleanup=%d windows-absent=%d checks=%u\n", (int)cleaned, (int)cleaned,
           checks);
    if (cleaned) {
        free(work);
        free(f);
    }
    return cleaned ? result : 1;
}
