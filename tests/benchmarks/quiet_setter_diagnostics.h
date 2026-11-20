/* Reviewed partial leaf mask. No entire child struct/union is treated as data.
 * Complete padding classification is confined to the explicitly enumerated
 * Registry/Format/Storage/Entity/Minecart-site definitions below. All remaining
 * bytes stay UNKNOWN, including unsupported Entity-NBT union payloads. */
#ifndef MC_BENCHMARK_QUIET_SETTER_DIAGNOSTICS_H
#define MC_BENCHMARK_QUIET_SETTER_DIAGNOSTICS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UNIT_UNKNOWN = 0, UNIT_NAMED = 1, UNIT_PADDING = 2 };

typedef struct unit_layout {
    size_t size;
    unsigned char *mask;
    const char **context, **member;
    int valid;
} unit_layout;

static void unit_range(unit_layout *layout, size_t base, size_t bytes, unsigned char kind,
                       const char *context, const char *member) {
    if (base > layout->size || bytes > layout->size - base) {
        layout->valid = 0;
        return;
    }
    for (size_t i = base; i < base + bytes; ++i) {
        if (layout->mask[i] != UNIT_UNKNOWN && layout->mask[i] != kind)
            layout->valid = 0;
        layout->mask[i] = kind;
        layout->context[i] = context;
        layout->member[i] = member;
    }
}

static void unit_leaf(unit_layout *layout, size_t base, size_t bytes, const char *context,
                      const char *type, const char *member) {
    QN_LAYOUT_REPORT("DIAG_MEMBER context=%s type=%s member=%s offset=%zu bytes=%zu\n", context,
                     type, member, base, bytes);
    unit_range(layout, base, bytes, UNIT_NAMED, context, member);
}

/* Call ONLY after every named member of this exact known definition has been
 * enumerated, recursively including its known child types. Unknown child
 * spans never enter this function. The compiler supplies offsets and widths. */
static void unit_finish_known(unit_layout *layout, size_t base, size_t bytes, const char *context,
                              const char *type) {
    QN_LAYOUT_REPORT("DIAG_KNOWN_TYPE context=%s type=%s offset=%zu bytes=%zu\n", context, type,
                     base, bytes);
    if (base > layout->size || bytes > layout->size - base) {
        layout->valid = 0;
        return;
    }
    for (size_t i = base; i < base + bytes; ++i)
        if (layout->mask[i] == UNIT_UNKNOWN)
            unit_range(layout, i, 1, UNIT_PADDING, context, "padding");
}

#define UNIT_FIELD(type, member)                                                                   \
    unit_leaf(layout, base + offsetof(type, member), sizeof(((type *)0)->member), context, #type,  \
              #member)

static void unit_format(unit_layout *layout, size_t base, const char *context) {
    UNIT_FIELD(mc_java_format_error, status);
    UNIT_FIELD(mc_java_format_error, offset);
    UNIT_FIELD(mc_java_format_error, length);
    UNIT_FIELD(mc_java_format_error, value);
    UNIT_FIELD(mc_java_format_error, flags);
    UNIT_FIELD(mc_java_format_error, conversion);
    UNIT_FIELD(mc_java_format_error, date_time);
    unit_finish_known(layout, base, sizeof(mc_java_format_error), context, "mc_java_format_error");
}

static void unit_registry(unit_layout *layout, size_t base, const char *context) {
    UNIT_FIELD(mc_registry_error, status);
    UNIT_FIELD(mc_registry_error, message);
    UNIT_FIELD(mc_registry_error, initializer_pc);
    UNIT_FIELD(mc_registry_error, block_id);
    UNIT_FIELD(mc_registry_error, constructor_class);
    UNIT_FIELD(mc_registry_error, constructor_pc);
    UNIT_FIELD(mc_registry_error, profile_status);
    UNIT_FIELD(mc_registry_error, active_initializer_class);
    UNIT_FIELD(mc_registry_error, active_initializer_pc);
    UNIT_FIELD(mc_registry_error, item_argument);
    unit_format(layout, base + offsetof(mc_registry_error, format_error), context);
    unit_finish_known(layout, base, sizeof(mc_registry_error), context, "mc_registry_error");
}

static void unit_storage(unit_layout *layout, size_t base, const char *context) {
    UNIT_FIELD(mc_world_storage_error, status);
    UNIT_FIELD(mc_world_storage_error, operation);
    UNIT_FIELD(mc_world_storage_error, system_error);
    unit_finish_known(layout, base, sizeof(mc_world_storage_error), context,
                      "mc_world_storage_error");
}

static void unit_site(unit_layout *layout, size_t base, const char *context) {
    UNIT_FIELD(mc_minecart_site, selected);
    UNIT_FIELD(mc_minecart_site, entered);
    UNIT_FIELD(mc_minecart_site, declaring_class);
    UNIT_FIELD(mc_minecart_site, method);
    UNIT_FIELD(mc_minecart_site, bytecode);
    unit_finish_known(layout, base, sizeof(mc_minecart_site), context, "mc_minecart_site");
}

static void unit_entity(unit_layout *layout, size_t base, const char *context) {
    UNIT_FIELD(mc_entity_error, status);
    UNIT_FIELD(mc_entity_error, operation);
    UNIT_FIELD(mc_entity_error, bytecode);
    UNIT_FIELD(mc_entity_error, profile);
    UNIT_FIELD(mc_entity_error, item);
    UNIT_FIELD(mc_entity_error, world);
    unit_site(layout, base + offsetof(mc_entity_error, minecart_world_constructor), context);
    unit_site(layout, base + offsetof(mc_entity_error, minecart_constructor), context);
    unit_site(layout, base + offsetof(mc_entity_error, minecart_initializer), context);
    unit_site(layout, base + offsetof(mc_entity_error, minecart_initializer_cause), context);
    unit_finish_known(layout, base, sizeof(mc_entity_error), context, "mc_entity_error");
}

/* Only fields independent of all Entity-NBT union selections are covered. */
static void unit_nbt_prefix(unit_layout *layout, size_t base, const char *context) {
    UNIT_FIELD(mc_entity_nbt_error, status);
    UNIT_FIELD(mc_entity_nbt_error, late_hook);
    UNIT_FIELD(mc_entity_nbt_error, operation);
    UNIT_FIELD(mc_entity_nbt_error, field);
    UNIT_FIELD(mc_entity_nbt_error, index);
    UNIT_FIELD(mc_entity_nbt_error, wrapper_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, name_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, factory_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, base_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, item_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, falling_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, stack_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, local_player_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, player_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, initializer_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, registration_bytecode);
    UNIT_FIELD(mc_entity_nbt_error, sleep_work);
    UNIT_FIELD(mc_entity_nbt_error, sleep);
    UNIT_FIELD(mc_entity_nbt_error, sleep_cleanup);
    QN_LAYOUT_REPORT("DIAG_PARTIAL_TYPE context=%s type=mc_entity_nbt_error offset=%zu bytes=%zu "
                     "union_payload=unknown\n",
                     context, base, sizeof(mc_entity_nbt_error));
}

#undef UNIT_FIELD
#define ROOT_LEAF(path)                                                                            \
    unit_leaf(layout, offsetof(mc_world_mutation_error, path),                                     \
              sizeof(((mc_world_mutation_error *)0)->path), #path, "mc_world_mutation_error",      \
              #path)
#define ROOT_REGISTRY(path) unit_registry(layout, offsetof(mc_world_mutation_error, path), #path)
#define ROOT_STORAGE(path) unit_storage(layout, offsetof(mc_world_mutation_error, path), #path)
#define ROOT_ENTITY(path) unit_entity(layout, offsetof(mc_world_mutation_error, path), #path)
#define ROOT_NBT(path) unit_nbt_prefix(layout, offsetof(mc_world_mutation_error, path), #path)
#define ROOT_LOAD(path)                                                                            \
    do {                                                                                           \
        ROOT_LEAF(path.index);                                                                     \
        ROOT_NBT(path.factory);                                                                    \
        ROOT_NBT(path.provenance);                                                                 \
        ROOT_STORAGE(path.native);                                                                 \
    } while (0)
#define ROOT_LIVE(path)                                                                            \
    do {                                                                                           \
        ROOT_REGISTRY(path.registry);                                                              \
        ROOT_REGISTRY(path.tile_entity.registry);                                                  \
        ROOT_STORAGE(path.cause);                                                                  \
        ROOT_ENTITY(path.entity);                                                                  \
        ROOT_ENTITY(path.publication.entity);                                                      \
        ROOT_STORAGE(path.publication.cause);                                                      \
        ROOT_LEAF(path.batch.index);                                                               \
        ROOT_ENTITY(path.batch.publication.entity);                                                \
        ROOT_STORAGE(path.batch.publication.cause);                                                \
        ROOT_LOAD(path.load_entity);                                                               \
    } while (0)
#define ROOT_NEARBY(path)                                                                          \
    do {                                                                                           \
        ROOT_REGISTRY(path.query.registry);                                                        \
        ROOT_STORAGE(path.query.cause);                                                            \
        ROOT_ENTITY(path.entity);                                                                  \
        ROOT_LIVE(path.tile_entity);                                                               \
        ROOT_LEAF(path.batch.index);                                                               \
        ROOT_ENTITY(path.batch.publication.entity);                                                \
        ROOT_STORAGE(path.batch.publication.cause);                                                \
        ROOT_LOAD(path.load_entity);                                                               \
        ROOT_LEAF(path.slice);                                                                     \
        ROOT_LEAF(path.index);                                                                     \
    } while (0)
#define ROOT_REGION(path)                                                                          \
    QN_LAYOUT_REPORT(                                                                              \
        "DIAG_ROOT_REGION member=%s offset=%zu bytes=%zu coverage=partial_or_unknown\n", #path,    \
        offsetof(mc_world_mutation_error, path), sizeof(((mc_world_mutation_error *)0)->path))

static void unit_prepare_layout(unit_layout *layout) {
    ROOT_LEAF(status);
    ROOT_LEAF(operation);
    ROOT_LEAF(wrapper_bytecode);
    ROOT_LEAF(bytecode);
    ROOT_LEAF(chunk_bytecode);
    ROOT_LEAF(height_bytecode);
    ROOT_LEAF(column_bytecode);
    ROOT_LEAF(interval_bytecode);
    ROOT_LEAF(world_access_bytecode);
    ROOT_LEAF(neighbor_call_bytecode);
    ROOT_LEAF(neighbor_bytecode);
    ROOT_LEAF(hook_bytecode);
    ROOT_LEAF(presence_bytecode);
    ROOT_LEAF(provider_bytecode);
    ROOT_LEAF(hook_class);
    ROOT_LEAF(hook_method);
    ROOT_LEAF(drop_class);
    ROOT_LEAF(drop_method);
    ROOT_LEAF(drop_bytecode);
    ROOT_LEAF(drop_call_bytecode);
    ROOT_LEAF(drop_spawn_bytecode);
    ROOT_LEAF(item);
    ROOT_LEAF(activation_bytecode);
    ROOT_LEAF(activation_world_bytecode);
    ROOT_LEAF(load_bytecode);
    ROOT_LEAF(load_builder_bytecode);
    ROOT_ENTITY(entity);
    ROOT_ENTITY(publication.entity);
    ROOT_STORAGE(publication.cause);
    ROOT_LEAF(batch.index);
    ROOT_ENTITY(batch.publication.entity);
    ROOT_STORAGE(batch.publication.cause);
    ROOT_LIVE(tile_entity);
    ROOT_LOAD(load_entity);
    ROOT_REGISTRY(registry);
    ROOT_STORAGE(cause);
    ROOT_NEARBY(nearby);
    ROOT_REGISTRY(schedule.registry);
    ROOT_REGISTRY(plant.registry);
    ROOT_REGISTRY(plant.query.registry);
    ROOT_REGISTRY(plant.material.registry);
    ROOT_REGISTRY(plant.support.registry);
    ROOT_REGISTRY(power.registry);
    ROOT_STORAGE(power.cause);
    ROOT_REGISTRY(power.support.registry);
    ROOT_STORAGE(power.support.cause);
    ROOT_NEARBY(power.access);
    ROOT_NEARBY(rail_access.nearby);
    ROOT_REGISTRY(rail_support.registry);
    ROOT_STORAGE(rail_support.cause);
    ROOT_REGION(entity);
    ROOT_REGION(publication);
    ROOT_REGION(batch);
    ROOT_REGION(tile_entity);
    ROOT_REGION(load_entity);
    ROOT_REGION(nearby);
    ROOT_REGION(material);
    ROOT_REGION(query);
    ROOT_REGION(plant);
    ROOT_REGION(schedule);
    ROOT_REGION(registry);
    ROOT_REGION(cause);
    ROOT_REGION(power);
    ROOT_REGION(rail_access);
    ROOT_REGION(rail_support);
    /* No finish_known on the root, partial wrappers or unsupported unions. */
}

#undef ROOT_REGION
#undef ROOT_NEARBY
#undef ROOT_LIVE
#undef ROOT_LOAD
#undef ROOT_NBT
#undef ROOT_ENTITY
#undef ROOT_STORAGE
#undef ROOT_REGISTRY
#undef ROOT_LEAF
#endif
