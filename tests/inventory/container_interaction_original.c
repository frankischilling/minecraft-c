/* Genuine production App/World/Player construction; its earlier entry is not run. */
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define main mc_container_unused_statistics_main
#include "../client/statistics/test_player_dispatch.c"
#undef main

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
    int main[36], armor[4], craft[4], result[1], cached[45], cursor;
    int32_t selected;
    bool changed, default_same, active_same, inventory_owner_same;
    int32_t stacks[128][4], statistic_ids[64], statistics[64];
    size_t stack_count, statistic_count;
} container_snapshot;

typedef struct container_probe {
    fixture base;
    mc_math_classes *math;
    mc_container *container;
    mc_player_container_call *call;
    mc_chunk_state *chunk;
    mc_world_access *access;
    object_ref objects[128];
    size_t object_count, derived;
    mc_entity *drops[8];
    int drop_refs[8];
    size_t drop_count;
} container_probe;

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
    if (world != p->base.world || item == NULL || p->drop_count == 8 ||
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
    CHECK(p->container->snapshots != NULL && p->container->snapshots->size == 45);
    for (size_t i = 0; i < 45; ++i) {
        out->cached[i] = identify(p, p->container->snapshots->values[i], NULL);
        CHECK(out->cached[i] >= -1);
    }
    CHECK(refs(p, inventory->main_slots, 36, out->main) &&
          refs(p, inventory->armor_slots, 4, out->armor) &&
          refs(p, p->container->crafting->slots, 4, out->craft) &&
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
    print_refs(p, s->craft, 4);
    printf(",\"result\":");
    print_refs(p, s->result, 1);
    printf(",\"cursor\":");
    print_ref(p, s->cursor);
    printf(",\"cached\":");
    print_refs(p, s->cached, 45);
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
        mc_player_drop_state child;
        CHECK(mc_player_container_drop_inspect(p->call, i, &child) && child.item == item &&
              child.returned && child.perturbation_spread_returned &&
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
    const bool close = strcmp(fields[1], "close") == 0;
    CHECK(close || strcmp(fields[1], "click") == 0);
    const bool shift = strcmp(fields[5], "true") == 0;
    CHECK(shift || strcmp(fields[5], "false") == 0);
    container_probe *p = calloc(1, sizeof(*p));
    CHECK(p != NULL && setup(&p->base, resources, fields[0], true) &&
          mc_math_classes_create(SIZE_MAX, &p->math, NULL) == MC_MATH_CLASS_OK &&
          mc_player_container_default_get(p->base.player, &p->container) == MC_ENTITY_OK);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *chunk = NULL;
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(p->base.store, p->base.world, 0, 0, 0, &blocks,
                                          MC_CHUNK_BLOCKS, &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    p->chunk = mc_world_chunk_state(chunk);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(p->base.world);
    CHECK(acquisition != NULL && p->chunk != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, p->chunk) &&
          mc_entity_set_position(p->base.player, 8.5, 64.0, 8.5) == MC_ENTITY_OK);
    p->base.player->base.yaw = p->base.player->base.aT = 0.0f;
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
    mc_player_container_error error;
    const mc_entity_status status =
        close ? mc_player_container_close_begin(p->base.app.registry, p->math, p->base.player,
                                                window, &p->call, &error)
              : mc_player_container_click_begin(p->base.app.registry, p->math, p->base.player,
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
    if (status == MC_ENTITY_OK)
        CHECK(mc_player_container_take_result(p->call, &returned) == MC_ENTITY_OK);
    const int returned_ref = identify(p, returned, NULL);
    mc_item_stack_release(returned);
    CHECK(returned_ref >= -1 && snapshot(p, &after));
    printf("CONTAINER_INTERACTION "
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
    CHECK(fflush(stdout) == 0 && mc_player_container_retire(&p->call, NULL) == MC_ENTITY_OK);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(p->base.world, p->access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(p->access, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < p->object_count; ++i)
        mc_item_stack_release(p->objects[i].stack);
    CHECK(mc_world_runtime_retire_entity_containers(p->base.world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(p->base.store, p->chunk, NULL) == MC_WORLD_STORAGE_OK &&
          mc_math_classes_release(&p->math, NULL) == MC_MATH_CLASS_OK && teardown(&p->base));
    free(p);
    return true;
}

static bool map_frontier_controls(const char *resources) {
    fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && setup(f, resources, "map-frontier", true));
    mc_container *container = f->player->local_player->fields.player.d;
    mc_item_stack *map = NULL;
    CHECK(mc_item_stack_allocate(&map) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(map, 358, 1, 0) == MC_ITEM_STACK_OK &&
          mc_inventory_result_set(container->result, 0, map, NULL) == MC_INVENTORY_OK);
    const mc_statistic *crafted_stat = f->app.registry->statistics.D[358];
    int32_t before, after;
    CHECK(crafted_stat != NULL &&
          mc_client_stats_writer_value(f->app.stats_writer, crafted_stat, &before, NULL) ==
              MC_CLIENT_STATS_OK);
    mc_player_container_call *call = NULL;
    mc_player_container_error error;
    f->player->base.world = NULL; /* Declared malformed source argument, before invocation. */
    CHECK(mc_player_container_click_begin(f->app.registry, NULL, f->player, 7, 0, 0, false, &call,
                                          &error) == MC_ENTITY_STATE_ERROR &&
          error.source_exception && !error.native_policy && error.child.pc == 4 &&
          strcmp(error.child.class_name, "wr") == 0 &&
          strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
    CHECK(container->result->slots[0] == NULL &&
          f->player->local_player->fields.player.c->cursor == map &&
          mc_client_stats_writer_value(f->app.stats_writer, crafted_stat, &after, NULL) ==
              MC_CLIENT_STATS_OK &&
          after == mc_java_i32_add(before, 1));
    CHECK(mc_player_container_retire(&call, NULL) == MC_ENTITY_OK);
    f->player->base.world =
        f->world; /* Restore only the authored fixture link after observation/retirement. */
    mc_item_stack_release(map);
    CHECK(teardown(f));
    free(f);
    return true;
}

static bool failure_prefix_controls(const char *resources) {
    static const char *const names[] = {
        "prefix-D-null",           "prefix-D-element-null",    "prefix-writer-before-items",
        "prefix-items-after-stat", "prefix-cursor-owner-null", "prefix-grid-array-null"};
    for (size_t mode = 0; mode < sizeof(names) / sizeof(names[0]); ++mode) {
        fixture *f = calloc(1, sizeof(*f));
        CHECK(f != NULL && setup(f, resources, names[mode], true));
        mc_container *container = f->player->local_player->fields.player.d;
        mc_inventory_player *inventory = f->player->local_player->fields.player.c;
        mc_item_stack *input = NULL;
        CHECK(mc_item_stack_allocate(&input) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(input, 17, 2, 0) == MC_ITEM_STACK_OK &&
              mc_inventory_crafting_set(container->crafting, 0, input, NULL) == MC_INVENTORY_OK);
        mc_item_stack *output = container->result->slots[0];
        CHECK(output != NULL && mc_item_stack_retain(output) == MC_ITEM_STACK_OK);
        mc_statistic **captured_D = f->app.registry->statistics.D;
        mc_statistic *crafted_stat = captured_D[5];
        mc_registry_item_array *captured_items = f->app.registry->items.c;
        mc_client_stats_writer *writer = f->app.stats_writer;
        mc_item_stack **grid_slots = container->crafting->slots;
        int32_t before, after;
        CHECK(crafted_stat != NULL && mc_client_stats_writer_value(writer, crafted_stat, &before,
                                                                   NULL) == MC_CLIENT_STATS_OK);
        /* All malformed operands are declared before entering the real call. */
        if (mode == 0)
            f->app.registry->statistics.D = NULL;
        if (mode == 1) {
            captured_D[5] = NULL;
            f->app.stats_writer = NULL;
        }
        if (mode == 2) {
            f->app.stats_writer = NULL;
            f->app.registry->items.c = NULL;
        }
        if (mode == 3)
            f->app.registry->items.c = NULL;
        if (mode == 4)
            inventory->player = NULL;
        if (mode == 5)
            container->crafting->slots = NULL;
        mc_player_container_call *call = NULL;
        mc_player_container_error error;
        const mc_entity_status status = mc_player_container_click_begin(
            f->app.registry, NULL, f->player, 7, 0, 0, false, &call, &error);
        CHECK(status == (mode == 1 ? MC_ENTITY_OK : MC_ENTITY_STATE_ERROR));
        CHECK(inventory->cursor == output && !inventory->changed &&
              mc_client_stats_writer_value(writer, crafted_stat, &after, NULL) ==
                  MC_CLIENT_STATS_OK);
        int32_t remaining;
        CHECK(mc_item_stack_source_count_get(input, &remaining) == MC_ITEM_STACK_OK &&
              remaining == (mode == 1 ? 1 : 2));
        CHECK(after == (mode == 3 || mode == 5 ? mc_java_i32_add(before, 4) : before));
        if (mode == 1) {
            mc_player_container_state state;
            CHECK(mc_player_container_inspect(call, &state) && state.returned &&
                  container->result->slots[0] != NULL && container->result->slots[0] != output);
        } else {
            CHECK(error.source_exception && !error.native_policy &&
                  strcmp(error.exception_class, "java/lang/NullPointerException") == 0 &&
                  container->result->slots[0] == NULL);
            if (mode == 0 || mode == 3)
                CHECK(strcmp(error.child.class_name, "iz") == 0 &&
                      strcmp(error.child.method, "b") == 0 &&
                      error.child.pc == (mode == 0 ? 8u : 23u));
            if (mode == 2)
                CHECK(strcmp(error.child.class_name, "dc") == 0 && error.child.pc == 90);
            if (mode == 4)
                CHECK(strcmp(error.interaction.child.class_name, "ix") == 0 &&
                      error.interaction.child.pc == 10 && error.interaction.site.pc == 361);
            if (mode == 5)
                CHECK(strcmp(error.interaction.child.class_name, "yv") == 0 &&
                      error.interaction.child.pc == 235 &&
                      strcmp(error.interaction.dependency.class_name, "mq") == 0 &&
                      error.interaction.dependency.pc == 4);
        }
        CHECK(mc_player_container_retire(&call, NULL) == MC_ENTITY_OK);
        /* Restore only authored fixture links, after all invocation observations
         * and checked retirement; no source effect is undone by cleanup. */
        f->app.registry->statistics.D = captured_D;
        captured_D[5] = crafted_stat;
        f->app.registry->items.c = captured_items;
        f->app.stats_writer = writer;
        inventory->player = f->player;
        container->crafting->slots = grid_slots;
        mc_item_stack_release(input);
        mc_item_stack_release(output);
        CHECK(teardown(f));
        free(f);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    if (!map_frontier_controls(argv[1]))
        return 1;
    if (!failure_prefix_controls(argv[1]))
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
    const bool complete = !ferror(input) && rows == 73;
    if (fclose(input) != 0 || !complete)
        return 2;
    printf("PASS: %zu Container original checks; %zu genuine calls\n", checks, rows);
    return 0;
}
