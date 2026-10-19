#ifndef MC_WORLD_BIOME_REGISTRY_H
#define MC_WORLD_BIOME_REGISTRY_H

#include "mc/block/registry/registry.h"

typedef struct mc_biome mc_biome;
typedef struct mc_biome_spawn_list mc_biome_spawn_list;
typedef struct mc_biome_spawn mc_biome_spawn;
typedef struct mc_biome_entity_class mc_biome_entity_class;

typedef enum mc_biome_kind {
    MC_BIOME_KIND_NONE = -1,
    MC_BIOME_KIND_KD_GS = 0,
    MC_BIOME_KIND_FS_DW,
    MC_BIOME_KIND_TF_MA,
    MC_BIOME_KIND_T_M,
    MC_BIOME_KIND_RY_LE,
    MC_BIOME_KIND_YJ_PP,
    MC_BIOME_KIND_RB_KK,
    MC_BIOME_KIND_G_F,
    MC_BIOME_KIND_COUNT
} mc_biome_kind;

/* Readonly view of the actual ten mutable declaring-class fields. These
 * objects are shared by Worlds, not copies of numerical mc_biome_id values.
 * Names and all child identities borrow the shared Registry lifetime. */
typedef struct mc_biome_fields {
    const mc_registry_string *n;
    int32_t o;
    int8_t p, q;
    int32_t r;
    const mc_biome_spawn_list *s, *t, *u;
    bool v, w;
} mc_biome_fields;

typedef struct mc_biome_spawn_fields {
    const mc_biome_entity_class *a;
    int32_t b;
} mc_biome_spawn_fields;

typedef struct mc_biome_registry_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    const mc_biome *a, *b, *c, *d, *e, *f, *g, *h, *i, *j, *k, *l, *m;
    const mc_biome *const *x;
    size_t x_length; /* Native extent of the actual nullable reference array. */
} mc_biome_registry_view;

/* Shared kd/gs bootstrap on the Registry's existing class/arena lifetime.
 * Ordinary public reentry is BUSY. Failure retains actual publications and
 * nested first causes; an erroneous class is never retried. Inspection alone
 * performs no active use and is allowed during an entered initialization.
 * Calls, views and retirement require exclusive access. Outputs/error/input
 * storage must be disjoint from each other and all live owner storage.
 * Native failure preserves result outputs; error may receive its diagnostic.
 * Registry/profile/material/resource owners must remain live for every view.
 * No World, entity initializer/constructor, seed, clock or tree method is
 * supplied by this module. Source Block active uses remain real dependencies. */
mc_registry_status mc_registry_classes_initialize_biomes(mc_registry_classes *classes,
                                                         mc_registry_error *error);
mc_registry_status mc_biome_registry_inspect(const mc_registry_classes *classes,
                                             mc_biome_registry_view *out, mc_registry_error *error);
mc_registry_status mc_biome_static_get(mc_registry_classes *classes, char field,
                                       const mc_biome **out, mc_registry_error *error);

/* Actual source selectors, distinct from biome.h's numerical projection API.
 * No finite/unit-interval clamp is added. Float selection preserves Java NaN
 * branches; double lookup uses Java d2i/wrapped indexing and actual null/bounds
 * failures. The captured x86-64 strict FP environment is required at reached
 * arithmetic: nearest-even, gradual underflow, and masked traps. Checks do not
 * move ahead of the constructor's RAW/Block prefix. */
mc_registry_status mc_biome_registry_classify_float(mc_registry_classes *classes, float temperature,
                                                    float rainfall, const mc_biome **out,
                                                    mc_registry_error *error);
mc_registry_status mc_biome_registry_lookup(mc_registry_classes *classes, double temperature,
                                            double rainfall, const mc_biome **out,
                                            mc_registry_error *error);
/* A cold call first completes clinit's fill, then executes the requested body;
 * a warm call rewrites the same table and late h/j fields without allocation. */
mc_registry_status mc_biome_table_rebuild(mc_registry_classes *classes, mc_registry_error *error);

/* Pure borrowed diagnostics. NULL kind is NONE, NULL fields/entry/class is
 * NULL, NULL list size is zero, and an out-of-range list position is NULL.
 * These diagnostic conventions do not model Java List.get exceptions. */
mc_biome_kind mc_biome_kind_get(const mc_biome *biome);
const mc_biome_fields *mc_biome_fields_get(const mc_biome *biome);
size_t mc_biome_spawn_list_size(const mc_biome_spawn_list *list);
const mc_biome_spawn *mc_biome_spawn_list_at(const mc_biome_spawn_list *list, size_t index);
const mc_biome_spawn_fields *mc_biome_spawn_fields_get(const mc_biome_spawn *spawn);
const char *mc_biome_entity_class_name(const mc_biome_entity_class *entity_class,
                                       mc_registry_side side);

#endif
