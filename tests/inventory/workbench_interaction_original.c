/* Authored partial-App ownership fixture with genuine Registry/World/Player/OO/DA bodies; no
 * whole-App constructor claim. */
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define main mc_workbench_unused_statistics_main
#include "../client/statistics/test_player_dispatch.c"
#undef main

#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_workbench_screen_internal.h"
#include "entity/player/container_interaction.h"
#include "inventory/internal.h"
#include "inventory/player/source_internal.h"
#include "item/stack/source_internal.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"
#include "world/storage/objects_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>

typedef struct object_ref {
    mc_item_stack *stack;
    char label[16];
} object_ref;

typedef struct container_snapshot {
    int main[36], armor[4], craft[9], result[1], cached[46], cursor;
    int32_t selected;
    bool changed, default_same, active_same, inventory_owner_same;
    int32_t stacks[128][4], statistic_ids[64], statistics[64];
    size_t stack_count, statistic_count;
} container_snapshot;

typedef struct container_probe {
    fixture base;
    mc_math_classes *math;
    mc_container *container;
    mc_player_container_call *call, *second_call;
    mc_client_screen_classes *screen_classes;
    mc_client_source_oo *screen;
    mc_chunk_state *chunk;
    mc_world_access *access;
    object_ref objects[128];
    size_t object_count, derived;
    mc_entity *drops[20];
    int drop_refs[20];
    size_t drop_count;
    unsigned control_action;
    size_t active_refusals;
} container_probe;

static bool setup_workbench(fixture *f, const char *resources, const char *directory, bool remote) {
    memset(f, 0, sizeof(*f));
    CHECK(mc_resource_files_create(resources, &f->app.files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->app.materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->app.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->app.profile, NULL, &f->app.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->app.entities, f->app.profile,
                                          &f->app.render_classes) == MC_CLIENT_MODEL_OK);
    const mc_registry_options ro = {MC_REGISTRY_CLIENT, f->app.profile, f->app.materials, 0,
                                    mc_resource_files_source(f->app.files)};
    CHECK(mc_registry_classes_allocate(&ro, &f->app.registry, NULL) == MC_REGISTRY_OK);
    mc_client_achievement_notice *notice = NULL;
    CHECK(mc_client_achievement_notice_allocate(f->app.render_classes, f->app.registry,
                                                f->app.profile, &notice,
                                                NULL) == MC_CLIENT_NOTICE_OK);
    CHECK(mc_client_achievement_notice_construct(notice, &f->app, NULL) == MC_CLIENT_NOTICE_OK);
    f->app.achievement_notice = notice; /* Actual u publication after real constructor. */
    f->notice_retained = notice;
    mc_client_statistics_owner_error owner;
    CHECK(mc_client_application_create_session(&f->app, NULL, NULL, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE);
    CHECK(mc_client_application_construct_water_effect(&f->app, NULL, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(f->app.water_effect != NULL && f->app.water_effect == f->app.water_effect_retained);
    CHECK(mc_client_application_statistics_active_use(&f->app, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(f->app.statistics_active_use_completed);
    static const uint16_t user[] = {'P', 'l', 'a', 'y', 'e', 'r', '1', '2', '3'}, token[] = {'-'};
    mc_player_string *username = NULL, *session_id = NULL;
    CHECK(mc_player_string_create(user, 9, &username) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(token, 1, &session_id) == MC_PLAYER_OK);
    CHECK(mc_client_application_create_session(&f->app, username, session_id, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    mc_player_string_release(username);
    mc_player_string_release(session_id);
    CHECK(mc_client_game_directory_create(directory, NULL, &f->app.diagnostic_game_directory, NULL,
                                          NULL) == MC_CLIENT_GAME_DIRECTORY_OK);
    {
        mc_client_stats_options options = mc_client_stats_default_options();
        options.charset = MC_JAVA_READER_UTF8;
        options.diagnostics.emit = quiet;
        CHECK(mc_client_application_construct_statistics(&f->app, &options, NULL, &owner) ==
              MC_CLIENT_STATISTICS_OWNER_OK);
        CHECK(f->app.stats_writer == f->app.stats_writer_retained && f->app.stats_writer != NULL);
        mc_client_stats_state state;
        CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &state));
        CHECK(state.workers_started == 1 && state.workers_joined == 0); /* No startup join. */
        CHECK(mc_client_stats_writer_join(f->app.stats_writer, NULL) ==
              MC_CLIENT_STATS_OK); /* Finite test observation. */
    }
    mc_registry_achievement_view av;
    mc_registry_statistic_view sv;
    CHECK(mc_registry_classes_inspect_achievements(f->app.registry, &av, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &sv, NULL) == MC_REGISTRY_OK);
    f->root = av.named[0];
    f->child = av.named[1];
    f->next = av.named[2];
    f->general = sv.named[0];
    CHECK(mc_statistic_achievement_fields_get(f->root)->c == NULL);
    CHECK(mc_statistic_achievement_fields_get(f->child)->c == f->root);
    CHECK(mc_statistic_achievement_fields_get(f->next)->c == f->child);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->app.profile, &trig,
                                          &f->app.world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->app.world_classes, f->app.registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->app.world_classes, f->app.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t blocks[256] = {0};
    mc_registry_block_view bv;
    CHECK(mc_registry_classes_inspect_blocks(f->app.registry, &bv, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        blocks[i] = bv.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options so = {.session_token = 6381,
                                       .block_map = blocks,
                                       .block_map_length = 256,
                                       .limits = mc_world_store_default_limits()};
    char world_path[128];
    CHECK(strlen(directory) < 80);
    CHECK(snprintf(world_path, sizeof(world_path), "%s/world", directory) > 0);
    CHECK(mc_world_store_open(world_path, f->regions, &so, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    static const uint16_t world_name[] = {'S', 't', 'a', 't', 's'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){world_name, 5}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->app.world_classes, &limits, &f->world,
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {.store = f->store,
                                                   .name = f->name,
                                                   .seed = MC_PLAYER_STATISTICS_WORLD_SEED,
                                                   .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    f->world->fields.B = remote; /* Declared before the real Player constructor, as original. */
    CHECK(mc_local_player_allocate(f->app.entities, &f->player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(f->player, f->world, &f->app, f->app.session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    CHECK(f->player->phase == MC_ENTITY_READY &&
          f->player->local_player->fields.minecraft == &f->app);
    return true;
}

static int identify(container_probe *p, mc_item_stack *stack, const char *label) {
    if (stack == NULL)
        return -1;
    for (size_t i = 0; i < p->object_count; ++i)
        if (p->objects[i].stack == stack)
            return (int)i;
    if (p->object_count == 128 || mc_item_stack_retain(stack) != MC_ITEM_STACK_OK)
        return -2;
    object_ref *object = &p->objects[p->object_count];
    object->stack = stack;
    const int n = label != NULL
                      ? snprintf(object->label, sizeof(object->label), "%s", label)
                      : snprintf(object->label, sizeof(object->label), "N%zu", ++p->derived);
    if (n <= 0 || (size_t)n >= sizeof(object->label)) {
        mc_item_stack_release(stack);
        return -2;
    }
    return (int)p->object_count++;
}

static mc_world_storage_status published(void *context, mc_world_runtime *world, mc_entity *item,
                                         mc_world_storage_error *error) {
    container_probe *p = context;
    if (world != p->base.world || item == NULL || p->drop_count == 20 ||
        !mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM)) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "Container probe publication bound", 0};
        return error->status;
    }
    const int ref = identify(p, item->stack, NULL);
    if (ref < 0) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "Container probe Stack identity hold", 0};
        return error->status;
    }
    p->drop_refs[p->drop_count] = ref;
    p->drops[p->drop_count++] = item;
    if (p->control_action != 0) {
        if (mc_player_container_retire(&p->call, NULL) != MC_ENTITY_BUSY || p->call == NULL ||
            mc_container_retire(&p->container, NULL) != MC_INVENTORY_BUSY || p->container == NULL) {
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                              "Workbench active owner refusal", 0};
            return error->status;
        }
        ++p->active_refusals;
        if (p->control_action == 1 && p->drop_count == 1)
            p->container->world = NULL;
        if ((p->control_action == 2 || p->control_action == 3) && p->drop_count == 1)
            world->fields.B = true;
        if (p->control_action == 4 && p->drop_count == 2)
            p->base.app.stats_writer = NULL;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool refs(container_probe *p, mc_item_stack *const *values, size_t count, int *out) {
    for (size_t i = 0; i < count; ++i) {
        out[i] = identify(p, values[i], NULL);
        CHECK(out[i] >= -1);
    }
    return true;
}

static bool snapshot(container_probe *p, container_snapshot *out) {
    *out = (container_snapshot){0};
    mc_inventory_player *inventory = p->base.player->local_player->fields.player.c;
    CHECK(p->container->snapshots != NULL && p->container->snapshots->size == 46);
    for (size_t i = 0; i < 46; ++i) {
        out->cached[i] = identify(p, p->container->snapshots->values[i], NULL);
        CHECK(out->cached[i] >= -1);
    }
    CHECK(refs(p, inventory->main_slots, 36, out->main) &&
          refs(p, inventory->armor_slots, 4, out->armor) &&
          refs(p, p->container->crafting->slots, 9, out->craft) &&
          refs(p, p->container->result->slots, 1, out->result));
    out->cursor = identify(p, inventory->cursor, NULL);
    CHECK(out->cursor >= -1);
    out->selected = inventory->selected;
    out->changed = inventory->changed;
    out->default_same = p->base.player->local_player->fields.player.d == p->container;
    out->active_same = p->base.player->local_player->fields.player.e == p->container;
    out->inventory_owner_same = inventory->player == p->base.player;
    out->stack_count = p->object_count;
    for (size_t i = 0; i < out->stack_count; ++i) {
        mc_item_stack_fields f;
        CHECK(mc_item_stack_fields_get(p->objects[i].stack, &f) == MC_ITEM_STACK_OK);
        out->stacks[i][0] = f.id;
        out->stacks[i][1] = f.count;
        out->stacks[i][2] = f.damage;
        out->stacks[i][3] = f.animation;
    }
    const mc_client_stats_map *total = p->base.app.stats_writer->total;
    CHECK(total != NULL && total->count <= 64);
    for (const stats_entry *entry = total->head; entry != NULL; entry = entry->next) {
        const size_t i = out->statistic_count++;
        CHECK(i < 64 && mc_statistic_fields_get(entry->key) != NULL);
        out->statistic_ids[i] = mc_statistic_fields_get(entry->key)->e;
        out->statistics[i] = entry->value;
    }
    CHECK(out->statistic_count == total->count);
    return true;
}

static void print_ref(const container_probe *p, int index) {
    if (index < 0)
        printf("null");
    else
        printf("\"%s\"", p->objects[index].label);
}

static void print_refs(const container_probe *p, const int *values, size_t count) {
    printf("[");
    for (size_t i = 0; i < count; ++i) {
        if (i != 0)
            printf(",");
        print_ref(p, values[i]);
    }
    printf("]");
}

static void print_snapshot(const container_probe *p, const container_snapshot *s) {
    printf("{\"main\":");
    print_refs(p, s->main, 36);
    printf(",\"armor\":");
    print_refs(p, s->armor, 4);
    printf(",\"craft\":");
    print_refs(p, s->craft, 9);
    printf(",\"result\":");
    print_refs(p, s->result, 1);
    printf(",\"cursor\":");
    print_ref(p, s->cursor);
    printf(",\"cached\":");
    print_refs(p, s->cached, 46);
    printf(",\"selected\":%d,\"changed\":%s,\"default-same\":%s,\"active-same\":%s,"
           "\"inventory-owner-same\":%s,\"stats\":{",
           s->selected, s->changed ? "true" : "false", s->default_same ? "true" : "false",
           s->active_same ? "true" : "false", s->inventory_owner_same ? "true" : "false");
    for (size_t i = 0; i < s->statistic_count; ++i)
        printf("%s\"%d\":%d", i == 0 ? "" : ",", s->statistic_ids[i], s->statistics[i]);
    printf("},\"stacks\":{");
    for (size_t i = 0; i < s->stack_count; ++i)
        printf("%s\"%s\":[%d,%d,%d,%d]", i == 0 ? "" : ",", p->objects[i].label, s->stacks[i][0],
               s->stacks[i][1], s->stacks[i][2], s->stacks[i][3]);
    printf("}}");
}

static uint64_t double_word(double value) {
    uint64_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool print_drops(container_probe *p) {
    mc_world_chunk *chunk = mc_world_store_chunk_object(p->base.store, p->chunk);
    mc_world_chunk_fields fields;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK);
    printf("[");
    for (size_t i = 0; i < p->drop_count; ++i) {
        mc_entity *item = p->drops[i];
        size_t memberships = 0;
        for (size_t slice_index = 0; slice_index < 8; ++slice_index) {
            const mc_world_chunk_entity_list *slice = NULL;
            size_t count;
            CHECK(mc_world_chunk_slice_at(fields.m, slice_index, &slice) == MC_WORLD_STORAGE_OK &&
                  mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK);
            for (size_t j = 0; j < count; ++j) {
                mc_entity *member = NULL;
                CHECK(mc_world_chunk_entity_at(slice, j, &member) == MC_WORLD_STORAGE_OK);
                memberships += member == item;
            }
        }
        bool member = false;
        for (size_t j = 0; j < p->base.world->entity_membership.count; ++j)
            member |= p->base.world->entity_membership.entries[j] == item;
        mc_item_stack_fields f;
        CHECK(mc_item_stack_fields_get(item->stack, &f) == MC_ITEM_STACK_OK &&
              item->random != NULL);
        mc_player_drop_state child = {0};
        mc_player_container_call *calls[] = {
            p->call, p->second_call,
            p->screen->container.closed_parent == NULL ? NULL
                                                       : p->screen->container.closed_parent->call,
            p->screen->container.closed_direct == NULL ? NULL
                                                       : p->screen->container.closed_direct->call};
        bool found = false;
        for (size_t c = 0; c < sizeof(calls) / sizeof(calls[0]) && !found; ++c) {
            mc_player_container_state state;
            if (!mc_player_container_inspect(calls[c], &state))
                continue;
            for (size_t n = 0; n < state.drop_calls && !found; ++n)
                if (mc_player_container_drop_inspect(calls[c], n, &child) && child.item == item)
                    found = true;
        }
        CHECK(found && child.item == item && child.returned && child.perturbation_spread_returned &&
              child.perturbation_cosine_returned && child.perturbation_sine_returned);
        uint32_t angle_word;
        memcpy(&angle_word, &child.perturbation_angle, sizeof(angle_word));
        printf("%s{\"ref\":", i == 0 ? "" : ",");
        print_ref(p, p->drop_refs[i]);
        printf(",\"value\":[%d,%d,%d,%d],\"pickup-delay\":%d,\"world-member\":%s,"
               "\"chunk-members\":%zu,\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\"],\"velocity\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\"],\"private-random\":\"%016" PRIx64
               "\",\"trig\":{\"angle\":\"%08" PRIx32 "\",\"cos\":\"%016" PRIx64
               "\",\"sin\":\"%016" PRIx64 "\"}}",
               f.id, f.count, f.damage, f.animation, item->item.c, member ? "true" : "false",
               memberships, double_word(item->base.x), double_word(item->base.y),
               double_word(item->base.z), double_word(item->base.velocity_x),
               double_word(item->base.velocity_y), double_word(item->base.velocity_z),
               item->random->state, angle_word, double_word(child.perturbation_cosine),
               double_word(child.perturbation_sine));
    }
    printf("]");
    return true;
}

static bool number(const char *text, int32_t *out) {
    errno = 0;
    char *end = NULL;
    const long long value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != 0 || value < INT32_MIN || value > INT32_MAX)
        return false;
    *out = (int32_t)value;
    return true;
}

static mc_item_stack *named(container_probe *p, const char *name) {
    for (size_t i = 0; i < p->object_count; ++i)
        if (strcmp(p->objects[i].label, name) == 0)
            return p->objects[i].stack;
    return NULL;
}

static bool stacks(container_probe *p, char *text) {
    if (strcmp(text, "-") == 0)
        return true;
    for (char *token = text; token != NULL;) {
        char *next = strchr(token, ';');
        if (next != NULL)
            *next++ = 0;
        char *fields[5] = {token};
        for (size_t i = 1; i < 5; ++i) {
            char *comma = strchr(fields[i - 1], ',');
            CHECK(comma != NULL);
            *comma = 0;
            fields[i] = comma + 1;
        }
        int32_t values[4];
        for (size_t i = 0; i < 4; ++i)
            CHECK(number(fields[i + 1], &values[i]));
        CHECK(strlen(fields[0]) < 16 &&
              strspn(fields[0], "ABCDEFGHIJKLMNOPQRSTUVWXYZ") == strlen(fields[0]));
        mc_item_stack *stack = NULL;
        CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, values[0], values[1], values[2]) == MC_ITEM_STACK_OK &&
              mc_item_stack_source_animation_set(stack, values[3]) == MC_ITEM_STACK_OK);
        const int ref = identify(p, stack, fields[0]);
        mc_item_stack_release(stack);
        CHECK(ref >= 0);
        token = next;
    }
    return true;
}

static bool locations(container_probe *p, char *text, unsigned kind) {
    if (strcmp(text, "-") == 0)
        return true;
    for (char *token = text; token != NULL;) {
        char *next = strchr(token, ',');
        if (next != NULL)
            *next++ = 0;
        char *equal = strchr(token, '=');
        CHECK(equal != NULL);
        *equal++ = 0;
        int32_t index;
        mc_item_stack *stack = named(p, equal);
        CHECK(number(token, &index) && stack != NULL);
        if (kind < 2)
            CHECK(mc_inventory_player_set(p->base.player->local_player->fields.player.c,
                                          kind == 1 ? mc_java_i32_add(index, 36) : index, stack,
                                          NULL) == MC_INVENTORY_OK);
        else
            CHECK(mc_inventory_crafting_set(p->container->crafting, index, stack, NULL) ==
                  MC_INVENTORY_OK);
        token = next;
    }
    return true;
}

static bool prepare_oo(container_probe *p, const char *resources, const char *directory,
                       bool remote) {
    CHECK(setup_workbench(&p->base, resources, directory, remote) &&
          mc_math_classes_create(SIZE_MAX, &p->math, NULL) == MC_MATH_CLASS_OK);
    p->base.app.h = p->base.player;
    p->base.app.active_world = p->base.world;
    p->base.app.source_math_classes = p->math;
    CHECK(mc_client_singleplayer_controller_allocate(p->base.app.entities, p->base.app.profile, 0,
                                                     &p->base.app.controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(p->base.app.controller, &p->base.app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *chunk = NULL;
    CHECK(blocks != NULL);
    blocks[(8u << 11) | (8u << 7) | 64u] = 58; /* Declared actual Crafting Table scene. */
    CHECK(mc_world_store_create_generated(p->base.store, p->base.world, 0, 0, 0, &blocks,
                                          MC_CHUNK_BLOCKS, &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    p->chunk = mc_world_chunk_state(chunk);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(p->base.world);
    CHECK(acquisition != NULL && p->chunk != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, p->chunk) &&
          mc_entity_set_position(p->base.player, 8.5, 64, 8.5) == MC_ENTITY_OK);
    p->base.player->base.yaw = p->base.player->base.aT = 0;
    mc_container *default_container = p->base.player->local_player->fields.player.d;
    CHECK(default_container != NULL &&
          p->base.player->local_player->fields.player.e == default_container);
    CHECK(mc_client_screen_classes_create(
              p->base.app.profile, mc_resource_files_source(p->base.app.files),
              p->base.app.registry, &p->screen_classes, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_workbench_allocate(p->screen_classes, p->base.app.render_classes, NULL,
                                              &p->screen, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_workbench_construct(
              p->screen, p->base.player->local_player->fields.player.c, p->base.world, 8, 64, 8,
              NULL) == MC_CLIENT_SCREEN_OK);
    mc_client_screen_init_error init_error;
    CHECK(mc_client_screen_initialize(mc_client_screen_workbench_base(p->screen), &p->base.app, 427,
                                      240, &init_error) == MC_CLIENT_SCREEN_OK);
    mc_client_workbench_screen_state screen;
    CHECK(mc_client_screen_workbench_inspect(p->screen, &screen, NULL) == MC_CLIENT_SCREEN_OK &&
          screen.phase == MC_CLIENT_SCREEN_READY && screen.id.j != default_container &&
          screen.id.j == p->base.player->local_player->fields.player.e &&
          p->base.player->local_player->fields.player.d == default_container);
    p->container = screen.id.j;
    CHECK(p->container != NULL && p->container->kind == MC_CONTAINER_WORKBENCH &&
          p->container->world == p->base.world && p->container->crafting->size == 9 &&
          p->container->slots->size == 46);
    return true;
}

static bool cleanup_oo(container_probe *p) {
    CHECK(mc_player_container_retire(&p->second_call, NULL) == MC_ENTITY_OK &&
          mc_player_container_retire(&p->call, NULL) == MC_ENTITY_OK);
    /* The selected invocation and snapshots are already complete. End the
     * still-live publication through genuine GS.r before native retirement. */
    if (p->base.player->local_player->fields.player.e == p->container) {
        mc_client_source_oo *same = p->screen;
        mc_client_workbench_screen_state screen_before, screen_after;
        mc_container_state container_before, container_after;
        mc_item_stack *grid_before[9];
        memcpy(grid_before, p->container->crafting->slots, sizeof(grid_before));
        mc_inventory_player *inventory = p->base.player->local_player->fields.player.c;
        mc_item_stack *cursor = inventory->cursor, *result = p->container->result->slots[0];
        const size_t bound = inventory->containers, player_holds = p->base.player->holds,
                     world_holds = p->base.world->persistent_bindings;
        CHECK(mc_client_screen_workbench_inspect(p->screen, &screen_before, NULL) ==
                  MC_CLIENT_SCREEN_OK &&
              mc_container_read_state(p->container, &container_before));
        mc_client_screen_error refusal;
        CHECK(mc_client_screen_workbench_retire(&p->screen, &refusal) == MC_CLIENT_SCREEN_BUSY &&
              p->screen == same && p->base.player->local_player->fields.player.e == p->container &&
              mc_client_screen_workbench_inspect(p->screen, &screen_after, NULL) ==
                  MC_CLIENT_SCREEN_OK &&
              screen_after.phase == MC_CLIENT_SCREEN_READY &&
              screen_after.phase == screen_before.phase &&
              screen_after.holds == screen_before.holds && screen_after.id.j == p->container &&
              mc_container_read_state(p->container, &container_after) &&
              container_after.slot_list == container_before.slot_list &&
              container_after.slots == 46 &&
              container_after.crafting == container_before.crafting &&
              container_after.result == container_before.result &&
              container_after.world == container_before.world && inventory->containers == bound &&
              p->base.player->holds == player_holds &&
              p->base.world->persistent_bindings == world_holds && inventory->cursor == cursor &&
              p->container->result->slots[0] == result &&
              memcmp(grid_before, p->container->crafting->slots, sizeof(grid_before)) == 0);
        mc_player_container_error restored;
        CHECK(mc_player_container_restore_default(p->base.player, &restored) == MC_ENTITY_OK &&
              restored.site.pc == 8 &&
              p->base.player->local_player->fields.player.e ==
                  p->base.player->local_player->fields.player.d);
    }
    CHECK(mc_client_screen_workbench_retire(&p->screen, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_classes_release(&p->screen_classes, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_singleplayer_controller_free(&p->base.app.controller, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    p->container = NULL;
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(p->base.world);
    CHECK(acquisition != NULL &&
          mc_world_runtime_retire_entity_containers(p->base.world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(p->base.store, p->chunk, NULL) == MC_WORLD_STORAGE_OK &&
          mc_math_classes_release(&p->math, NULL) == MC_MATH_CLASS_OK && teardown(&p->base));
    return true;
}

static bool terminal_guard_control(const char *resources) {
    container_probe *p = calloc(1, sizeof(*p));
    CHECK(p != NULL && prepare_oo(p, resources, "workbench-terminal-control", false));
    mc_client_source_oo *same = p->screen;
    mc_client_screen_error error;
    CHECK(mc_client_screen_workbench_retire(&p->screen, &error) == MC_CLIENT_SCREEN_BUSY &&
          p->screen == same && p->base.player->local_player->fields.player.e == p->container);
    mc_client_workbench_screen_state state;
    CHECK(mc_client_screen_workbench_inspect(p->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_READY && state.id.j == p->container &&
          state.container_owned);
    mc_player_container_error restored;
    CHECK(mc_player_container_restore_default(p->base.player, &restored) == MC_ENTITY_OK &&
          strcmp(restored.site.class_name, "gs") == 0 && restored.site.pc == 8 &&
          p->base.player->local_player->fields.player.e ==
              p->base.player->local_player->fields.player.d);
    CHECK(cleanup_oo(p));
    free(p);
    return true;
}

static bool adapter_controls(const char *resources) {
    static const char *const names[] = {"workbench-remote-prefix",
                                        "workbench-fresh-null-world",
                                        "workbench-fresh-remote-world",
                                        "workbench-captured-local-world",
                                        "workbench-failed-second-stat",
                                        "workbench-cursor-owner-null",
                                        "workbench-direct-with-null-current",
                                        "workbench-null-player",
                                        "workbench-null-receiver",
                                        "workbench-remote-null-grid",
                                        "workbench-local-null-grid"};
    for (size_t mode = 0; mode < sizeof(names) / sizeof(names[0]); ++mode) {
        container_probe *p = calloc(1, sizeof(*p));
        CHECK(p != NULL && prepare_oo(p, resources, names[mode], mode == 0 || mode == 9));
        char declaration[] = "A,3,7,0,5;B,17,2,0,5";
        CHECK(stacks(p, declaration));
        mc_item_stack *a = named(p, "A"), *b = named(p, "B");
        CHECK(a != NULL && b != NULL &&
              mc_inventory_crafting_set(p->container->crafting, 0, b, NULL) == MC_INVENTORY_OK &&
              mc_inventory_crafting_set(p->container->crafting, 4, a, NULL) == MC_INVENTORY_OK &&
              mc_inventory_crafting_set(p->container->crafting, 8, b, NULL) == MC_INVENTORY_OK);
        mc_inventory_player *inventory = p->base.player->local_player->fields.player.c;
        if (mode != 3) {
            mc_entity *notify = NULL;
            CHECK(mc_inventory_player_source_cursor_store(inventory, a, &notify, NULL) ==
                      MC_INVENTORY_OK &&
                  notify == p->base.player);
        }
        mc_item_stack **saved_grid = p->container->crafting->slots;
        mc_client_stats_writer *writer = p->base.app.stats_writer;
        if (mode == 1 || mode == 2 || mode == 3 || mode == 4)
            p->control_action = (unsigned)mode;
        if (mode == 5)
            inventory->player = NULL;
        if (mode == 6)
            p->base.player->local_player->fields.player.e = NULL;
        if (mode == 9 || mode == 10)
            p->container->crafting->slots = NULL;
        const mc_world_access_callbacks callbacks = {.entity_added = published};
        CHECK(mc_world_access_create(&callbacks, p, &p->access, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(p->base.world, p->access, NULL) ==
                  MC_WORLD_STORAGE_OK);
        mc_player_container_error error = {0};
        const mc_entity_status status = mc_player_container_direct_close_begin(
            p->base.app.registry, p->math, mode == 7 || mode == 8 ? NULL : p->base.player,
            mode == 8 ? NULL : p->container, &p->call, &error);
        const bool returned = mode == 0 || mode == 2 || mode == 3 || mode == 6 || mode == 9;
        CHECK(status == (returned ? MC_ENTITY_OK : MC_ENTITY_STATE_ERROR));
        mc_player_container_state state;
        CHECK(mc_player_container_inspect(p->call, &state) && state.direct_target && state.close &&
              !state.active && !state.active_container_restored && state.returned == returned);
        static const size_t drops[] = {1, 1, 1, 3, 2, 1, 4, 0, 0, 1, 1};
        CHECK(p->drop_count == drops[mode] && state.drop_calls == drops[mode]);
        CHECK(inventory->cursor == (mode == 7 || mode == 8 ? a : NULL));
        CHECK(saved_grid[0] == b && saved_grid[4] == a && saved_grid[8] == b);
        int32_t a_count, b_count;
        CHECK(mc_item_stack_source_count_get(a, &a_count) == MC_ITEM_STACK_OK && a_count == 7 &&
              mc_item_stack_source_count_get(b, &b_count) == MC_ITEM_STACK_OK && b_count == 2);
        CHECK(p->base.player->local_player->fields.player.e == (mode == 6 ? NULL : p->container));
        if (mode != 8) {
            CHECK(state.interaction != NULL && state.interaction->crafting_cells_cleared == 0);
            if (mode == 0 || mode == 2 || mode == 9)
                CHECK(state.interaction->workbench_remote_read &&
                      state.interaction->workbench_remote && error.site.pc == 15 &&
                      strcmp(error.site.class_name, "it") == 0);
            if (mode == 3 || mode == 4 || mode == 6 || mode == 10)
                CHECK(state.interaction->workbench_remote_read &&
                      !state.interaction->workbench_remote);
            if (mode == 1 || mode == 5 || mode == 7)
                CHECK(!state.interaction->workbench_remote_read);
        }
        if (!returned) {
            CHECK(error.source_exception && !error.native_policy &&
                  strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
            if (mode == 1)
                CHECK(strcmp(error.site.class_name, "it") == 0 && error.site.pc == 9);
            if (mode == 4)
                CHECK(strcmp(error.site.class_name, "it") == 0 && error.site.pc == 39 &&
                      error.drop.statistic != NULL && error.drop.statistic->site.pc == 90);
            if (mode == 5)
                CHECK(strcmp(error.site.class_name, "dw") == 0 && error.site.pc == 22 &&
                      strcmp(error.interaction.child.class_name, "ix") == 0 &&
                      error.interaction.child.pc == 10);
            if (mode == 7)
                CHECK(strcmp(error.site.class_name, "dw") == 0 && error.site.pc == 1);
            if (mode == 8)
                CHECK(error.site.class_name == NULL && state.container == NULL);
            if (mode == 10)
                CHECK(strcmp(error.site.class_name, "it") == 0 && error.site.pc == 29 &&
                      strcmp(error.interaction.dependency.class_name, "mq") == 0 &&
                      error.interaction.dependency.pc == 4);
        }
        if (p->control_action != 0)
            CHECK(p->active_refusals == p->drop_count);
        CHECK(mc_player_container_retire(&p->call, NULL) == MC_ENTITY_OK);
        /* Only after the completed source observations and checked retirement:
         * restore authored nullable links needed for native fixture cleanup. */
        p->container->world = p->base.world;
        p->container->crafting->slots = saved_grid;
        p->base.app.stats_writer = writer;
        inventory->player = p->base.player;
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(p->base.world, p->access, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed && mc_world_access_free(p->access, NULL) == MC_WORLD_STORAGE_OK);
        for (size_t i = 0; i < p->object_count; ++i)
            mc_item_stack_release(p->objects[i].stack);
        CHECK(cleanup_oo(p));
        free(p);
    }
    return true;
}

static bool run_case(const char *resources, char *line) {
    char *fields[11] = {line};
    for (size_t i = 1; i < 11; ++i) {
        char *tab = strchr(fields[i - 1], '\t');
        CHECK(tab != NULL);
        *tab = 0;
        fields[i] = tab + 1;
    }
    CHECK(strchr(fields[10], '\t') == NULL && strlen(fields[0]) < 100 &&
          strspn(fields[0], "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") ==
              strlen(fields[0]));
    int32_t window, slot, button;
    CHECK(number(fields[2], &window) && number(fields[3], &slot) && number(fields[4], &button));
    const char *operation = fields[1];
    const bool close = strcmp(operation, "close") == 0 || strcmp(operation, "close-remote") == 0;
    const bool direct =
        strcmp(operation, "direct-close") == 0 || strcmp(operation, "direct-close-remote") == 0;
    const bool repeat = strcmp(operation, "repeat-direct-close") == 0 ||
                        strcmp(operation, "repeat-direct-close-remote") == 0;
    const bool screen_close =
        strcmp(operation, "screen-close") == 0 || strcmp(operation, "screen-close-remote") == 0;
    CHECK(close || direct || repeat || screen_close || strcmp(operation, "click") == 0);
    const size_t operation_length = strlen(operation);
    const bool remote =
        operation_length >= 7 && strcmp(operation + operation_length - 7, "-remote") == 0;
    const bool shift = strcmp(fields[5], "true") == 0;
    CHECK(shift || strcmp(fields[5], "false") == 0);
    container_probe *p = calloc(1, sizeof(*p));
    CHECK(p != NULL && prepare_oo(p, resources, fields[0], remote));
    CHECK(stacks(p, fields[6]) && locations(p, fields[7], 0) && locations(p, fields[8], 1) &&
          locations(p, fields[9], 2));
    if (strcmp(fields[10], "-") != 0) {
        mc_item_stack *cursor = named(p, fields[10]);
        mc_entity *notify = NULL;
        CHECK(cursor != NULL &&
              mc_inventory_player_source_cursor_store(p->base.player->local_player->fields.player.c,
                                                      cursor, &notify, NULL) == MC_INVENTORY_OK &&
              notify == p->base.player);
    }
    const mc_world_access_callbacks callbacks = {.entity_added = published};
    CHECK(mc_world_access_create(&callbacks, p, &p->access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(p->base.world, p->access, NULL) == MC_WORLD_STORAGE_OK);
    container_snapshot before, after;
    CHECK(snapshot(p, &before));
    const uint64_t player_before = p->base.player->random->state,
                   world_before = p->base.world->fields.r->state;
    mc_player_container_error error = {0};
    mc_entity_status status;
    if (screen_close) {
        mc_client_screen_error screen_error;
        CHECK(mc_client_screen_closed(mc_client_screen_workbench_base(p->screen), &screen_error) ==
              MC_CLIENT_SCREEN_OK);
        mc_client_workbench_screen_state state;
        CHECK(mc_client_screen_workbench_inspect(p->screen, &state, NULL) == MC_CLIENT_SCREEN_OK &&
              state.inherited_closed_returned && state.direct_closed_returned &&
              state.last_pc == 18);
        status = MC_ENTITY_OK;
    } else if (direct || repeat) {
        status = mc_player_container_direct_close_begin(
            p->base.app.registry, p->math, p->base.player, p->container, &p->call, &error);
        if (status == MC_ENTITY_OK && repeat)
            status = mc_player_container_direct_close_begin(p->base.app.registry, p->math,
                                                            p->base.player, p->container,
                                                            &p->second_call, &error);
    } else if (close)
        status = mc_player_container_close_begin(p->base.app.registry, p->math, p->base.player,
                                                 window, &p->call, &error);
    else
        status = mc_player_container_click_begin(p->base.app.registry, p->math, p->base.player,
                                                 window, slot, button, shift, &p->call, &error);
    const char *exception = "none";
    if (status != MC_ENTITY_OK) {
        if (error.source_exception && error.exception_class != NULL &&
            strcmp(error.exception_class, "java/lang/IndexOutOfBoundsException") == 0)
            exception = "java.lang.IndexOutOfBoundsException";
        else {
            fprintf(stderr, "Container %s status=%d %s at %s:%u child=%s:%u kernel=%s:%d/%s:%d\n",
                    fields[0], (int)status, error.operation == NULL ? "" : error.operation,
                    error.site.class_name, error.site.pc, error.child.class_name, error.child.pc,
                    error.interaction.site.class_name, error.interaction.site.pc,
                    error.interaction.dependency.class_name, error.interaction.dependency.pc);
            return false;
        }
    }
    mc_item_stack *returned = NULL;
    if (status == MC_ENTITY_OK && p->call != NULL)
        CHECK(mc_player_container_take_result(p->call, &returned) == MC_ENTITY_OK);
    const int returned_ref = identify(p, returned, NULL);
    mc_item_stack_release(returned);
    CHECK(returned_ref >= -1 && snapshot(p, &after));
    printf("WORKBENCH_INTERACTION "
           "{\"profile\":\"%s\",\"operation\":\"%s\",\"exception\":\"%s\",\"before\":",
           fields[0], fields[1], exception);
    print_snapshot(p, &before);
    printf(",\"after\":");
    print_snapshot(p, &after);
    printf(",\"return\":");
    print_ref(p, returned_ref);
    printf(",\"events\":[");
    for (size_t i = 0; i < p->drop_count; ++i) {
        printf("%s{\"event\":\"drop\",\"index\":%zu,\"ref\":", i == 0 ? "" : ",", i);
        print_ref(p, p->drop_refs[i]);
        printf("}");
    }
    printf("],\"drops\":");
    CHECK(print_drops(p));
    printf(",\"player-random-before\":\"%016" PRIx64 "\",\"player-random-after\":\"%016" PRIx64
           "\",\"world-random-before\":\"%016" PRIx64 "\",\"world-random-after\":\"%016" PRIx64
           "\"}\n",
           player_before, p->base.player->random->state, world_before,
           p->base.world->fields.r->state);
    CHECK(fflush(stdout) == 0);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(p->base.world, p->access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(p->access, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < p->object_count; ++i)
        mc_item_stack_release(p->objects[i].stack);
    CHECK(cleanup_oo(p));
    free(p);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    if (!terminal_guard_control(argv[1]) || !adapter_controls(argv[1]))
        return 1;
    FILE *input = fopen(argv[3], "rb");
    if (input == NULL)
        return 2;
    char line[8192];
    size_t rows = 0;
    while (fgets(line, sizeof(line), input) != NULL) {
        char *end = strchr(line, '\n');
        if (end == NULL || ++rows > 1000) {
            (void)fclose(input);
            return 2;
        }
        *end = 0;
        if (end != line && end[-1] == '\r')
            end[-1] = 0;
        if (!run_case(argv[1], line)) {
            (void)fclose(input);
            return 1;
        }
    }
    const bool complete = !ferror(input) && rows == 116;
    if (fclose(input) != 0 || !complete)
        return 2;
    printf("PASS: %zu Workbench original checks; %zu genuine calls\n", checks, rows);
    return 0;
}
