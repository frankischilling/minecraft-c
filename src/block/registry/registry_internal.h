#ifndef MC_BLOCK_REGISTRY_INTERNAL_H
#define MC_BLOCK_REGISTRY_INTERNAL_H

#include "../../item/recipes/recipes_internal.h"
#include "../../item/registry/registry_internal.h"
#include "../../resources/translation/translation_internal.h"
#include "../../statistics/keys/keys_internal.h"
#include "../../statistics/registry/registry_internal.h"
#include "../../world/biome/registry_internal.h"
#include "mc/block/registry/registry.h"
#include "redstone_set_internal.h"
#define MC_REGISTRY_BLOCK_SLOTS 256u

typedef enum mc_block_kind {
    MC_BLOCK_BASE,
    MC_BLOCK_STONE,
    MC_BLOCK_GRASS,
    MC_BLOCK_DIRT,
    MC_BLOCK_SAPLING,
    MC_BLOCK_FLOWING_FLUID,
    MC_BLOCK_STILL_FLUID,
    MC_BLOCK_SAND,
    MC_BLOCK_GRAVEL,
    MC_BLOCK_ORE,
    MC_BLOCK_LOG,
    MC_BLOCK_LEAVES,
    MC_BLOCK_SPONGE,
    MC_BLOCK_GLASS,
    MC_BLOCK_DISPENSER,
    MC_BLOCK_SANDSTONE,
    MC_BLOCK_NOTE_BLOCK,
    MC_BLOCK_BED,
    MC_BLOCK_RAIL,
    MC_BLOCK_DETECTOR_RAIL,
    MC_BLOCK_PISTON_BASE,
    MC_BLOCK_COBWEB,
    MC_BLOCK_TALL_GRASS,
    MC_BLOCK_DEAD_BUSH,
    MC_BLOCK_PISTON_HEAD,
    MC_BLOCK_WOOL,
    MC_BLOCK_MOVING_PISTON,
    MC_BLOCK_FLOWER,
    MC_BLOCK_MUSHROOM,
    MC_BLOCK_MINERAL_STORAGE,
    MC_BLOCK_SLAB,
    MC_BLOCK_TNT,
    MC_BLOCK_BOOKSHELF,
    MC_BLOCK_OBSIDIAN,
    MC_BLOCK_TORCH,
    MC_BLOCK_FIRE,
    MC_BLOCK_MOB_SPAWNER,
    MC_BLOCK_STAIRS,
    MC_BLOCK_CHEST,
    MC_BLOCK_REDSTONE,
    MC_BLOCK_CRAFTING_TABLE,
    MC_BLOCK_CROPS,
    MC_BLOCK_FARMLAND,
    MC_BLOCK_FURNACE,
    MC_BLOCK_SIGN,
    MC_BLOCK_DOOR,
    MC_BLOCK_LADDER,
    MC_BLOCK_LEVER,
    MC_BLOCK_PRESSURE_PLATE,
    MC_BLOCK_REDSTONE_ORE,
    MC_BLOCK_REDSTONE_TORCH,
    MC_BLOCK_BUTTON,
    MC_BLOCK_SNOW_LAYER,
    MC_BLOCK_ICE,
    MC_BLOCK_SNOW_BLOCK,
    MC_BLOCK_CACTUS,
    MC_BLOCK_CLAY,
    MC_BLOCK_SUGAR_CANE,
    MC_BLOCK_JUKEBOX,
    MC_BLOCK_FENCE,
    MC_BLOCK_PUMPKIN,
    MC_BLOCK_NETHERRACK,
    MC_BLOCK_SOUL_SAND,
    MC_BLOCK_GLOWSTONE,
    MC_BLOCK_PORTAL,
    MC_BLOCK_CAKE,
    MC_BLOCK_REPEATER,
    MC_BLOCK_LOCKED_CHEST,
    MC_BLOCK_TRAPDOOR,
    MC_BLOCK_KIND_COUNT
} mc_block_kind;

typedef struct mc_registry_allocation mc_registry_allocation;
typedef union mc_registry_managed_allocation mc_registry_managed_allocation;
typedef struct mc_registry_literal mc_registry_literal;

struct mc_registry_string {
    const uint16_t *units;
    size_t length;
};

struct mc_block_sound {
    mc_block_sound_fields fields;
    /* ct/bu=0, glass al/aa=1, sand aj/x=2. Constructor dispatch only. */
    unsigned int kind;
};

typedef struct mc_registry_list {
    void **elements;
    size_t count;
    size_t capacity;
} mc_registry_list;

typedef struct mc_pressure_category {
    const mc_registry_string *name;
    int32_t ordinal;
} mc_pressure_category;

typedef struct mc_tile_entity_class_identity {
    const char *client_name;
    const char *server_name;
} mc_tile_entity_class_identity;

typedef enum mc_block_constructor_phase {
    MC_BLOCK_RAW,
    MC_BLOCK_CONSTRUCTING,
    MC_BLOCK_CONSTRUCTED,
    MC_BLOCK_CONSTRUCTION_FAILED
} mc_block_constructor_phase;

struct mc_block {
    mc_registry_classes *classes;
    mc_block_kind kind;
    mc_block_constructor_phase phase;
    int32_t requested_id;
    const char *constructor_class;
    int32_t constructor_pc;
    mc_block_fields fields;
    /* nr/in.b and jp/gh.a belong to distinct declaring classes. */
    bool nr_b;
    bool jp_a;

    union {
        struct {
            int32_t a;
            bool *b;
            int32_t *c;
        } om;

        struct {
            int32_t c;
            int32_t *a;
        } bk;

        struct {
            mc_java_random *a;
        } xq;

        struct {
            bool a;
        } pc;

        struct {
            bool a, b;
        } jq;

        struct {
            int32_t a;
        } h;

        struct {
            bool b;
        } ys;

        struct {
            int32_t *a, *b;
        } yq;

        struct {
            mc_block *a;
        } ss;

        struct {
            mc_java_random *a;
        } e;

        struct {
            bool a;
            mc_registry_redstone_set *b;
        } sm;

        struct {
            mc_java_random *a;
            bool b;
        } tc;

        struct {
            const mc_tile_entity_class_identity *a;
            bool b;
        } uj;

        struct {
            const mc_pressure_category *a;
        } bv;

        struct {
            bool a;
        } bs;

        struct {
            bool a;
        } db;

        struct {
            bool a;
        } fc;

        struct {
            bool c;
        } wo;
    } subtype;
};

typedef struct mc_block_arguments {
    int32_t id;
    int32_t texture;
    mc_material *material;
    bool flag;
    mc_block *source_block;
    const mc_tile_entity_class_identity *tile_entity_class;
    const mc_pressure_category *pressure;
} mc_block_arguments;

typedef struct mc_registry_block_storage {
    mc_block_sound *d, *e, *f, *g, *h, *i, *j, *k, *l;
    mc_block **m;
    /* Native extent metadata for the actual retained m array. */
    size_t m_length;
    bool *n, *o, *p;
    int32_t *q;
    bool *r;
    int32_t *s;
    bool *t;
    /* One cell for every actual case-sensitive static u..bl, in declaration
     * order. A name selector does not turn these into block-ID aliases. */
    mc_block *named[96];
} mc_registry_block_storage;

/* Automatic frames follow the real synchronous C call chain. The parent is
 * restored on every return, including failure; diagnostics copy values before
 * unwind. There is no fabricated fixed recursion/initializer budget. */
typedef struct mc_registry_initializer {
    struct mc_registry_initializer *parent;
    const char *client_class, *server_class;
    int32_t *pc;
    mc_block *saved_block;
    mc_item *saved_item;
} mc_registry_initializer;

struct mc_registry_classes {
    mc_registry_options options;
    mc_registry_phase blocks_phase;
    int32_t block_initializer_pc;
    /* Permanent arena/external-owner bytes retain reached constructor
     * prefixes. Managed bytes are reclaimable; peak covers their joint total. */
    size_t allocation_bytes;
    size_t managed_current_bytes;
    size_t total_peak_bytes;
    size_t named_blocks;
    /* Native runtime lifetime count, separate from Java class fields. */
    size_t runtime_borrows;
    unsigned int entered;
    mc_registry_error last_error;
    mc_registry_error block_failure;
    mc_registry_initializer *initializer;
    mc_registry_allocation *allocations;
    mc_registry_managed_allocation *managed_allocations;
    mc_registry_redstone_set *redstone_sets;
    size_t redstone_snapshot_count;
    struct mc_registry_stack_allocation *item_stacks;
    mc_registry_literal *literals;
    mc_block *current_block;
    mc_item *current_item;
    mc_registry_block_storage blocks;
    mc_registry_class_state block_states[MC_BLOCK_KIND_COUNT];
    mc_registry_class_state nr_state, jp_state, rp_state, rw_state, pressure_state;
    /* Independent wj/od class, distinct from ve/nj's bed-facing table. Source
     * cells may be captured independently of these native arena provenance
     * handles. Cold state is zero until this actual initializer first runs. */
    mc_registry_class_state direction_state;
    int32_t *direction_a, *direction_b, *direction_c, *direction_d;
    int32_t *direction_storage[4];
    bool gk_a;
    int32_t **ve_a;
    const mc_registry_string **ys_a;
    bool tc_c;
    mc_registry_list *db_b;
    double *wo_a;
    int32_t *wo_b;
    mc_pressure_category *pressure_a, *pressure_b, *pressure_c;
    mc_pressure_category **pressure_values;
    /* Resolving a Class literal does not construct/initialize its TileEntity. */
    mc_tile_entity_class_identity sign_class;
    mc_registry_item_storage items;
    mc_registry_class_state item_states[MC_ITEM_KIND_COUNT];
    mc_registry_class_state item_dq_state, item_cp_state, item_tool_state;
    mc_registry_translation_storage translations;
    mc_registry_class_state translation_state, translation_wrapper_state;
    mc_registry_key_storage statistic_keys;
    mc_registry_class_state statistic_key_state;
    mc_registry_statistic_base_storage statistic_base;
    mc_registry_class_state statistic_base_state;
    mc_registry_statistic_storage statistics;
    mc_registry_class_state statistics_state;
    mc_registry_achievement_storage achievements;
    mc_registry_class_state achievements_state;
    mc_registry_recipe_storage recipes;
    mc_registry_class_state crafting_state, smelting_state;
    mc_registry_biome_storage biomes;
    mc_registry_class_state biome_states[MC_BIOME_KIND_COUNT];
    /* One native hold on the actual canonical Entity Class owner. Existing
     * logical Biome cells remain inline and are never moved or replaced. */
    mc_entity_classes *entities;
    /* Independent jj/ge horizontal tables. Source projections may be replaced
     * or NULL; arena provenance retains all original arrays, including the
     * unpublished matrix/rows of a failed initializer prefix. */
    mc_registry_class_state horizontal_state;
    int32_t *horizontal_a, *horizontal_b;
    int32_t **horizontal_c;
    int32_t *horizontal_storage[2];
    int32_t **horizontal_c_storage;
    int32_t *horizontal_rows[4];
};

void mc_registry_initializer_enter(mc_registry_classes *classes, mc_registry_initializer *frame,
                                   const char *client, const char *server, int32_t *pc);
void mc_registry_initializer_leave(mc_registry_classes *classes, mc_registry_initializer *frame);
void mc_registry_initializer_stage(mc_registry_classes *classes, int32_t pc);
mc_registry_status mc_registry_blocks_active_use(mc_registry_classes *classes);
mc_registry_status mc_registry_finish_blocks(mc_registry_classes *classes);
mc_registry_status mc_registry_string_ascii(mc_registry_classes *classes, const char *prefix,
                                            const char *suffix, const mc_registry_string **out);
mc_registry_status mc_registry_block_texture_side(mc_registry_classes *classes,
                                                  const mc_block *block, int32_t side,
                                                  int32_t *out);

mc_registry_status mc_registry_fail(mc_registry_classes *classes, mc_registry_status status,
                                    const char *message, const mc_block *block);
void *mc_registry_allocate(mc_registry_classes *classes, size_t bytes);
void mc_registry_block_stage(mc_block *block, const char *client_class, const char *server_class,
                             int32_t pc);
mc_registry_status mc_registry_literal_ascii(mc_registry_classes *classes, const char *text,
                                             const mc_registry_string **out);
mc_registry_status mc_registry_name(mc_registry_classes *classes, mc_block *block,
                                    const char *suffix);

/* RAW allocation/declaring-class initialization precede argument evaluation.
 * construct consumes already evaluated source arguments; helpers execute only
 * actual game constructors/mutators and retain reached publications. */
mc_registry_status mc_registry_block_allocate(mc_registry_classes *classes, mc_block_kind kind,
                                              mc_block **out);
mc_registry_status mc_registry_block_construct(mc_registry_classes *classes, mc_block *block,
                                               const mc_block_arguments *arguments);
mc_registry_status mc_registry_block_base(mc_registry_classes *classes, mc_block *block, int32_t id,
                                          mc_material *material);
mc_registry_status mc_registry_block_base_texture(mc_registry_classes *classes, mc_block *block,
                                                  int32_t id, int32_t texture,
                                                  mc_material *material);
bool mc_registry_block_opaque(const mc_block *block);
void mc_registry_block_bounds(mc_block *block, float x0, float y0, float z0, float x1, float y1,
                              float z1);
mc_registry_status mc_registry_block_tick(mc_registry_classes *classes, mc_block *block, bool tick);
mc_registry_status mc_registry_block_opacity(mc_registry_classes *classes, mc_block *block,
                                             int32_t opacity);
mc_registry_status mc_registry_block_emission(mc_registry_classes *classes, mc_block *block,
                                              float emission);
mc_registry_status mc_registry_block_metadata(mc_registry_classes *classes, mc_block *block);
void mc_registry_block_sound(mc_block *block, const mc_block_sound *sound);
void mc_registry_block_resistance(mc_block *block, float resistance);
void mc_registry_block_hardness(mc_block *block, float hardness);
void mc_registry_block_unbreakable(mc_block *block);
void mc_registry_block_no_stats(mc_block *block);
mc_registry_status mc_registry_block_random(mc_registry_classes *classes, mc_block *block,
                                            mc_java_random **destination);
mc_registry_status mc_registry_pressure_initialize(mc_registry_classes *classes);
const mc_tile_entity_class_identity *mc_registry_sign_class(mc_registry_classes *classes);
mc_registry_status mc_registry_block_prefix(mc_registry_classes *classes);
mc_registry_status mc_registry_block_class_initialize(mc_registry_classes *classes,
                                                      mc_block_kind kind);
mc_registry_status mc_registry_register_blocks(mc_registry_classes *classes);
const char *mc_registry_block_static_name(size_t index);

#endif
