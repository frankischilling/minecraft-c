#include "../../entity/classes/classes_internal.h"
#include "../../item/registry/stacks_internal.h"
#include "managed_internal.h"
#include "registry_internal.h"

#include <stdlib.h>
#include <string.h>

#define MC_REGISTRY_DEFAULT_BYTES (4u * 1024u * 1024u)

struct mc_registry_allocation {
    void *data;
    size_t bytes;
    mc_registry_allocation *next;
};

struct mc_registry_literal {
    const mc_registry_string *string;
    mc_registry_literal *next;
};

static mc_registry_error native_error(mc_registry_status status, const char *message) {
    return (mc_registry_error){.status = status,
                               .message = message,
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_class = NULL,
                               .constructor_pc = -1,
                               .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                               .active_initializer_class = NULL,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_registry_status result(mc_registry_error *error, mc_registry_status status,
                                 const char *message) {
    if (error != NULL)
        *error = native_error(status, message);
    return status;
}

mc_registry_status mc_registry_fail(mc_registry_classes *classes, mc_registry_status status,
                                    const char *message, const mc_block *block) {
    classes->last_error = native_error(status, message);
    classes->last_error.initializer_pc = classes->block_initializer_pc;
    if (classes->initializer != NULL) {
        const mc_registry_initializer *frame = classes->initializer;
        classes->last_error.active_initializer_class =
            classes->options.side == MC_REGISTRY_CLIENT ? frame->client_class : frame->server_class;
        if (frame->pc != NULL)
            classes->last_error.active_initializer_pc = *frame->pc;
    }
    if (block == NULL)
        block = classes->current_block;
    if (block != NULL) {
        classes->last_error.block_id = block->requested_id;
        classes->last_error.constructor_class = block->constructor_class;
        classes->last_error.constructor_pc = block->constructor_pc;
    }
    if (classes->current_item != NULL) {
        const mc_item *item = classes->current_item;
        classes->last_error.item_argument = item->requested_argument;
        classes->last_error.constructor_class = item->constructor_class;
        classes->last_error.constructor_pc = item->constructor_pc;
    }
    return status;
}

void mc_registry_initializer_enter(mc_registry_classes *classes, mc_registry_initializer *frame,
                                   const char *client, const char *server, int32_t *pc) {
    *frame = (mc_registry_initializer){.parent = classes->initializer,
                                       .client_class = client,
                                       .server_class = server,
                                       .pc = pc,
                                       .saved_block = classes->current_block,
                                       .saved_item = classes->current_item};
    classes->initializer = frame;
    classes->current_block = NULL;
    classes->current_item = NULL;
}

void mc_registry_initializer_leave(mc_registry_classes *classes, mc_registry_initializer *frame) {
    classes->current_block = frame->saved_block;
    classes->current_item = frame->saved_item;
    classes->initializer = frame->parent;
}

void mc_registry_initializer_stage(mc_registry_classes *classes, int32_t pc) {
    if (classes->initializer != NULL && classes->initializer->pc != NULL)
        *classes->initializer->pc = pc;
}

void mc_registry_block_stage(mc_block *block, const char *client_class, const char *server_class,
                             int32_t pc) {
    block->constructor_class =
        block->classes->options.side == MC_REGISTRY_CLIENT ? client_class : server_class;
    block->constructor_pc = pc;
}

void *mc_registry_allocate(mc_registry_classes *classes, size_t bytes) {
    if (bytes == 0) {
        (void)mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "empty native request", NULL);
        return NULL;
    }
    const size_t remaining = mc_registry_managed_remaining(classes);
    if (!mc_registry_managed_fits(classes, 0) || remaining < sizeof(mc_registry_allocation) ||
        bytes > remaining - sizeof(mc_registry_allocation)) {
        (void)mc_registry_fail(classes, MC_REGISTRY_LIMIT, "registry allocation bound", NULL);
        return NULL;
    }
    mc_registry_allocation *record = malloc(sizeof(*record));
    if (record == NULL) {
        (void)mc_registry_fail(classes, MC_REGISTRY_ALLOCATION_FAILED, "registry allocation record",
                               NULL);
        return NULL;
    }
    /* This unpublished record is reclaimable until its payload succeeds.
     * Include its real acquisition in the peak even if calloc fails. There
     * are no callbacks between this temporary charge and its retirement. */
    classes->managed_current_bytes += sizeof(*record);
    mc_registry_managed_peak_update(classes);
    void *data = calloc(1, bytes);
    if (data == NULL) {
        classes->managed_current_bytes -= sizeof(*record);
        free(record);
        (void)mc_registry_fail(classes, MC_REGISTRY_ALLOCATION_FAILED, "registry allocation", NULL);
        return NULL;
    }
    record->data = data;
    record->bytes = bytes;
    record->next = classes->allocations;
    classes->allocations = record;
    classes->managed_current_bytes -= sizeof(*record);
    classes->allocation_bytes += sizeof(*record) + bytes;
    mc_registry_managed_peak_update(classes);
    return data;
}

mc_registry_status mc_registry_string_ascii(mc_registry_classes *classes, const char *prefix,
                                            const char *suffix, const mc_registry_string **out) {
    const size_t prefix_length = strlen(prefix);
    const size_t suffix_length = strlen(suffix);
    if (suffix_length > SIZE_MAX - prefix_length ||
        prefix_length + suffix_length > SIZE_MAX / sizeof(uint16_t))
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "registry string length", NULL);
    mc_registry_string *string = mc_registry_allocate(classes, sizeof(*string));
    if (string == NULL)
        return classes->last_error.status;
    const size_t length = prefix_length + suffix_length;
    uint16_t *units = NULL;
    if (length != 0) {
        units = mc_registry_allocate(classes, length * sizeof(*units));
        if (units == NULL)
            return classes->last_error.status;
        for (size_t i = 0; i < prefix_length; ++i)
            units[i] = (unsigned char)prefix[i];
        for (size_t i = 0; i < suffix_length; ++i)
            units[prefix_length + i] = (unsigned char)suffix[i];
    }
    string->units = units;
    string->length = length;
    *out = string;
    return MC_REGISTRY_OK;
}

static bool literal_equal(const mc_registry_string *string, const char *text) {
    const size_t length = strlen(text);
    if (length != string->length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (string->units[i] != (unsigned char)text[i])
            return false;
    return true;
}

mc_registry_status mc_registry_literal_ascii(mc_registry_classes *classes, const char *text,
                                             const mc_registry_string **out) {
    for (mc_registry_literal *literal = classes->literals; literal != NULL; literal = literal->next)
        if (literal_equal(literal->string, text)) {
            *out = literal->string;
            return MC_REGISTRY_OK;
        }
    const mc_registry_string *string = NULL;
    mc_registry_status status = mc_registry_string_ascii(classes, "", text, &string);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_literal *literal = mc_registry_allocate(classes, sizeof(*literal));
    if (literal == NULL)
        return classes->last_error.status;
    literal->string = string;
    literal->next = classes->literals;
    classes->literals = literal;
    *out = string;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_name(mc_registry_classes *classes, mc_block *block,
                                    const char *suffix) {
    /* Each builder result is a distinct String identity. These constructor
     * call sites use ASCII literals; this is not a resource/UTF-8 parser.
     * Native string admission precedes any assignment to the Block field. */
    const mc_registry_string *name = NULL;
    mc_registry_status status =
        mc_registry_string_ascii(classes, "tile.", suffix == NULL ? "null" : suffix, &name);
    if (status != MC_REGISTRY_OK)
        return status;
    block->fields.a = name;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_classes_allocate(const mc_registry_options *options,
                                                mc_registry_classes **out,
                                                mc_registry_error *error) {
    if (options == NULL || out == NULL || options->random_profile == NULL ||
        options->materials == NULL ||
        (options->resources != NULL && options->resources->open == NULL) ||
        (options->side != MC_REGISTRY_CLIENT && options->side != MC_REGISTRY_SERVER))
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "registry options");
    const size_t maximum = options->max_allocation_bytes == 0 ? MC_REGISTRY_DEFAULT_BYTES
                                                              : options->max_allocation_bytes;
    if (maximum < sizeof(mc_registry_classes))
        return result(error, MC_REGISTRY_LIMIT, "registry metadata bound");
    mc_registry_classes *classes = calloc(1, sizeof(*classes));
    if (classes == NULL)
        return result(error, MC_REGISTRY_ALLOCATION_FAILED, "registry metadata");
    classes->options = *options;
    classes->options.max_allocation_bytes = maximum;
    classes->allocation_bytes = sizeof(*classes);
    classes->total_peak_bytes = sizeof(*classes);
    classes->blocks_phase = MC_REGISTRY_COLD;
    classes->block_initializer_pc = -1;
    classes->last_error = native_error(MC_REGISTRY_OK, NULL);
    classes->block_failure = native_error(MC_REGISTRY_OK, NULL);
    for (size_t i = 0; i < MC_BLOCK_KIND_COUNT; ++i) {
        classes->block_states[i].pc = -1;
        classes->block_states[i].failure = native_error(MC_REGISTRY_OK, NULL);
    }
    classes->nr_state.pc = classes->jp_state.pc = classes->rp_state.pc = -1;
    classes->rw_state.pc = classes->pressure_state.pc = -1;
    for (size_t i = 0; i < MC_ITEM_KIND_COUNT; ++i) {
        classes->item_states[i].pc = -1;
        classes->item_states[i].failure = native_error(MC_REGISTRY_OK, NULL);
    }
    classes->item_dq_state.pc = -1;
    classes->item_cp_state.pc = -1;
    classes->item_tool_state.pc = -1;
    classes->translation_state.pc = -1;
    classes->translation_wrapper_state.pc = -1;
    classes->translation_state.failure = native_error(MC_REGISTRY_OK, NULL);
    classes->translation_wrapper_state.failure = native_error(MC_REGISTRY_OK, NULL);
    classes->statistic_key_state.pc = -1;
    classes->statistic_key_state.failure = native_error(MC_REGISTRY_OK, NULL);
    classes->statistic_base_state.pc = -1;
    classes->statistic_base_state.failure = native_error(MC_REGISTRY_OK, NULL);
    classes->statistics_state.pc = -1;
    classes->statistics_state.failure = native_error(MC_REGISTRY_OK, NULL);
    classes->achievements_state.pc = -1;
    classes->achievements_state.failure = native_error(MC_REGISTRY_OK, NULL);
    classes->crafting_state.pc = classes->smelting_state.pc = -1;
    classes->crafting_state.failure = native_error(MC_REGISTRY_OK, NULL);
    classes->smelting_state.failure = native_error(MC_REGISTRY_OK, NULL);
    for (size_t i = 0; i < MC_BIOME_KIND_COUNT; ++i) {
        classes->biome_states[i].pc = -1;
        classes->biome_states[i].failure = native_error(MC_REGISTRY_OK, NULL);
    }
    *out = classes;
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_initialize_blocks(mc_registry_classes *classes,
                                                         mc_registry_error *error) {
    if (classes == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "registry owner");
    if (classes->entered != 0)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    if (classes->blocks_phase == MC_REGISTRY_READY)
        return result(error, MC_REGISTRY_OK, NULL);
    if (classes->blocks_phase == MC_REGISTRY_INITIALIZING)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    if (classes->blocks_phase == MC_REGISTRY_FAILED) {
        if (error != NULL) {
            *error = classes->block_failure;
            error->status = MC_REGISTRY_FAILED_INITIALIZATION;
            error->message = "Block initialization previously failed";
        }
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    ++classes->entered;
    mc_registry_status status = mc_registry_blocks_active_use(classes);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return result(error, status, NULL);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_blocks_active_use(mc_registry_classes *classes) {
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Block active use requires an admitted initialization flow", NULL);
    if (classes->blocks_phase == MC_REGISTRY_READY ||
        classes->blocks_phase == MC_REGISTRY_INITIALIZING)
        return MC_REGISTRY_OK;
    if (classes->blocks_phase == MC_REGISTRY_FAILED) {
        classes->last_error = classes->block_failure;
        classes->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    classes->blocks_phase = MC_REGISTRY_INITIALIZING;
    classes->block_states[MC_BLOCK_BASE].phase = MC_REGISTRY_INITIALIZING;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "uu", "na", &classes->block_initializer_pc);
    mc_registry_status status = mc_registry_block_prefix(classes);
    if (status == MC_REGISTRY_OK)
        status = mc_registry_register_blocks(classes);
    if (status == MC_REGISTRY_OK) {
        classes->current_block = NULL;
        status = mc_registry_finish_blocks(classes);
    }
    classes->blocks_phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    classes->block_states[MC_BLOCK_BASE].phase = classes->blocks_phase;
    classes->block_states[MC_BLOCK_BASE].pc = classes->block_initializer_pc;
    if (status != MC_REGISTRY_OK) {
        classes->block_failure = classes->last_error;
        classes->block_states[MC_BLOCK_BASE].failure = classes->last_error;
    }
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_classes_inspect_blocks(const mc_registry_classes *classes,
                                                      mc_registry_block_view *out,
                                                      mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "registry view arguments");
    if (classes->managed_current_bytes > SIZE_MAX - classes->allocation_bytes)
        return result(error, MC_REGISTRY_LIMIT, "registry view allocation total");
    *out = (mc_registry_block_view){.phase = classes->blocks_phase,
                                    .initializer_pc = classes->block_initializer_pc,
                                    .allocation_bytes =
                                        classes->allocation_bytes + classes->managed_current_bytes,
                                    .named_blocks = classes->named_blocks,
                                    .d = classes->blocks.d,
                                    .e = classes->blocks.e,
                                    .f = classes->blocks.f,
                                    .g = classes->blocks.g,
                                    .h = classes->blocks.h,
                                    .i = classes->blocks.i,
                                    .j = classes->blocks.j,
                                    .k = classes->blocks.k,
                                    .l = classes->blocks.l,
                                    .m = classes->blocks.m,
                                    .n = classes->blocks.n,
                                    .o = classes->blocks.o,
                                    .p = classes->blocks.p,
                                    .q = classes->blocks.q,
                                    .r = classes->blocks.r,
                                    .s = classes->blocks.s,
                                    .t = classes->blocks.t};
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_inspect_block_static(const mc_registry_classes *classes,
                                                            const char *name, const mc_block **out,
                                                            mc_registry_error *error) {
    if (classes == NULL || name == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block static view arguments");
    for (size_t i = 0; i < 96; ++i)
        if (strcmp(mc_registry_block_static_name(i), name) == 0) {
            *out = classes->blocks.named[i];
            return result(error, MC_REGISTRY_OK, NULL);
        }
    return result(error, MC_REGISTRY_INVALID_ARGUMENT, "unknown Block static name");
}

const mc_block_fields *mc_block_fields_get(const mc_block *block) {
    return block == NULL ? NULL : &block->fields;
}

const mc_block_sound_fields *mc_block_sound_fields_get(const mc_block_sound *sound) {
    return sound == NULL ? NULL : &sound->fields;
}

bool mc_registry_string_read(const mc_registry_string *string, mc_registry_string_view *out) {
    if (string == NULL || out == NULL)
        return false;
    *out = (mc_registry_string_view){.units = string->units, .length = string->length};
    return true;
}

mc_registry_status mc_registry_classes_bind_entities(mc_registry_classes *classes,
                                                     mc_entity_classes *entities,
                                                     mc_registry_error *error) {
    if (classes == NULL || entities == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Registry Entity binding arguments");
    if (classes->entered != 0)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    if (classes->entities != NULL) {
        if (classes->entities != entities)
            return result(error, MC_REGISTRY_BUSY, "Registry Entity owner already bound");
        if (mc_entity_classes_runtime_release_admit(entities) != MC_ENTITY_OK)
            return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Registry Entity binding ownership");
        return result(error, MC_REGISTRY_OK, NULL);
    }
    const mc_entity_status held = mc_entity_classes_retain(
        entities, classes->options.side == MC_REGISTRY_CLIENT ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER,
        classes->options.random_profile);
    if (held != MC_ENTITY_OK)
        return result(error,
                      held == MC_ENTITY_LIMIT ? MC_REGISTRY_LIMIT : MC_REGISTRY_INVALID_ARGUMENT,
                      "Registry Entity binding dependencies");
    classes->entities = entities;
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_free(mc_registry_classes *classes,
                                            mc_registry_error *error) {
    if (classes == NULL)
        return result(error, MC_REGISTRY_OK, NULL);
    if (classes->entered != 0)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    if (classes->runtime_borrows != 0)
        return result(error, MC_REGISTRY_BUSY, "registry runtime bindings remain");
    if (classes->entities != NULL &&
        mc_entity_classes_runtime_release_admit(classes->entities) != MC_ENTITY_OK)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Registry Entity binding release");
    if (classes->redstone_snapshot_count != 0)
        return result(error, MC_REGISTRY_BUSY, "Registry redstone snapshots remain");
    mc_registry_redstone_sets_retire(classes);
    mc_registry_managed_retire(classes);
    mc_registry_stacks_retire(classes);
    mc_registry_allocation *record = classes->allocations;
    while (record != NULL) {
        mc_registry_allocation *next = record->next;
        free(record->data);
        free(record);
        record = next;
    }
    /* Complete release admission above; native arena retirement cannot change
     * this exclusive Entity binding or invoke source callbacks. */
    if (classes->entities != NULL) {
        (void)mc_entity_classes_release(classes->entities);
        classes->entities = NULL;
    }
    free(classes);
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_block_allocate(mc_registry_classes *classes, mc_block_kind kind,
                                              mc_block **out) {
    if ((unsigned int)kind >= (unsigned int)MC_BLOCK_KIND_COUNT || out == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Block allocation kind",
                                NULL);
    classes->current_block = NULL;
    mc_registry_status status = mc_registry_block_class_initialize(classes, kind);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_block *block = mc_registry_allocate(classes, sizeof(*block));
    if (block == NULL)
        return classes->last_error.status;
    block->classes = classes;
    block->kind = kind;
    block->phase = MC_BLOCK_RAW;
    block->requested_id = -1;
    block->constructor_pc = -1;
    classes->current_block = block;
    *out = block;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_block_base(mc_registry_classes *classes, mc_block *block, int32_t id,
                                          mc_material *material) {
    mc_registry_block_stage(block, "uu", "na", 6);
    block->fields.bq = true;
    mc_registry_block_stage(block, "uu", "na", 11);
    block->fields.br = true;
    const mc_block_sound *sound = classes->blocks.d;
    mc_registry_block_stage(block, "uu", "na", 18);
    block->fields.by = sound;
    mc_registry_block_stage(block, "uu", "na", 23);
    block->fields.bz = 1.0f;
    mc_registry_block_stage(block, "uu", "na", 29);
    block->fields.bB = 0.6f;
    mc_block **slots = classes->blocks.m;
    mc_registry_block_stage(block, "uu", "na", 36);
    if (slots == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block registry array", block);
    if (id < 0 || id >= (int32_t)MC_REGISTRY_BLOCK_SLOTS)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE, "Block registry index",
                                block);
    if (slots[id] != NULL)
        return mc_registry_fail(classes, MC_REGISTRY_DUPLICATE_BLOCK,
                                "Block registry slot already occupied", block);
    mc_registry_block_stage(block, "uu", "na", 91);
    block->fields.bA = material;
    slots = classes->blocks.m;
    mc_registry_block_stage(block, "uu", "na", 99);
    slots[id] = block;
    mc_registry_block_stage(block, "uu", "na", 102);
    block->fields.bn = id;
    mc_registry_block_stage(block, "uu", "na", 112);
    mc_registry_block_bounds(block, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bool *opaque = classes->blocks.o;
    mc_registry_block_stage(block, "uu", "na", 120);
    const bool opaque_value = mc_registry_block_opaque(block);
    mc_registry_block_stage(block, "uu", "na", 123);
    if (opaque == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block opacity array", block);
    opaque[id] = opaque_value;
    int32_t *opacity = classes->blocks.q;
    mc_registry_block_stage(block, "uu", "na", 129);
    const bool second_opaque = mc_registry_block_opaque(block);
    mc_registry_block_stage(block, "uu", "na", 142);
    if (opacity == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block light-opacity array",
                                block);
    opacity[id] = second_opaque ? 255 : 0;
    bool *unsupported = classes->blocks.r;
    mc_registry_block_stage(block, "uu", "na", 148);
    if (material == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block material", block);
    const bool material_value = mc_material_b(material);
    mc_registry_block_stage(block, "uu", "na", 159);
    if (unsupported == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block material array", block);
    unsupported[id] = !material_value;
    bool *tile_entities = classes->blocks.p;
    mc_registry_block_stage(block, "uu", "na", 165);
    if (tile_entities == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block TileEntity array",
                                block);
    tile_entities[id] = false;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_block_base_texture(mc_registry_classes *classes, mc_block *block,
                                                  int32_t id, int32_t texture,
                                                  mc_material *material) {
    mc_registry_status status = mc_registry_block_base(classes, block, id, material);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_block_stage(block, "uu", "na", 8);
    block->fields.bm = texture;
    return MC_REGISTRY_OK;
}

void mc_registry_block_bounds(mc_block *block, float x0, float y0, float z0, float x1, float y1,
                              float z1) {
    block->fields.bs = (double)x0;
    block->fields.bt = (double)y0;
    block->fields.bu = (double)z0;
    block->fields.bv = (double)x1;
    block->fields.bw = (double)y1;
    block->fields.bx = (double)z1;
}

static mc_registry_status array_index(mc_registry_classes *classes, const mc_block *block,
                                      const void *array, const char *message) {
    if (array == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, message, block);
    if (block->fields.bn < 0 || block->fields.bn >= (int32_t)MC_REGISTRY_BLOCK_SLOTS)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE, "Block property index",
                                block);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_block_tick(mc_registry_classes *classes, mc_block *block,
                                          bool tick) {
    bool *array = classes->blocks.n;
    mc_registry_status status = array_index(classes, block, array, "Block tick array");
    if (status == MC_REGISTRY_OK)
        array[block->fields.bn] = tick;
    return status;
}

mc_registry_status mc_registry_block_opacity(mc_registry_classes *classes, mc_block *block,
                                             int32_t opacity) {
    int32_t *array = classes->blocks.q;
    mc_registry_status status = array_index(classes, block, array, "Block light-opacity array");
    if (status == MC_REGISTRY_OK)
        array[block->fields.bn] = opacity;
    return status;
}

mc_registry_status mc_registry_block_emission(mc_registry_classes *classes, mc_block *block,
                                              float emission) {
    int32_t *array = classes->blocks.s;
    mc_registry_status status = array_index(classes, block, array, "Block emission array");
    if (status == MC_REGISTRY_OK) {
        const float scaled = 15.0f * emission;
        array[block->fields.bn] = mc_java_float_to_i32(scaled);
    }
    return status;
}

mc_registry_status mc_registry_block_metadata(mc_registry_classes *classes, mc_block *block) {
    bool *array = classes->blocks.t;
    mc_registry_status status = array_index(classes, block, array, "Block metadata array");
    if (status == MC_REGISTRY_OK)
        array[block->fields.bn] = true;
    return status;
}

void mc_registry_block_sound(mc_block *block, const mc_block_sound *sound) {
    block->fields.by = sound;
}

void mc_registry_block_resistance(mc_block *block, float resistance) {
    block->fields.bp = resistance * 3.0f;
}

void mc_registry_block_hardness(mc_block *block, float hardness) {
    block->fields.bo = hardness;
    const float candidate = hardness * 5.0f;
    if (block->fields.bp < candidate)
        block->fields.bp = hardness * 5.0f;
}

void mc_registry_block_unbreakable(mc_block *block) {
    mc_registry_block_hardness(block, -1.0f);
}

void mc_registry_block_no_stats(mc_block *block) {
    block->fields.br = false;
}

mc_registry_status mc_registry_block_random(mc_registry_classes *classes, mc_block *block,
                                            mc_java_random **destination) {
    block->constructor_pc = 9;
    mc_java_random *random = mc_registry_allocate(classes, sizeof(*random));
    if (random == NULL)
        return classes->last_error.status;
    block->constructor_pc = 13;
    mc_java_random_profile_status profile =
        mc_java_random_construct_default(classes->options.random_profile, random);
    if (profile != MC_JAVA_RANDOM_PROFILE_OK) {
        (void)mc_registry_fail(classes, MC_REGISTRY_PROFILE_FAILED, "Block default Random", block);
        classes->last_error.profile_status = profile;
        return MC_REGISTRY_PROFILE_FAILED;
    }
    block->constructor_pc = 16;
    *destination = random;
    return MC_REGISTRY_OK;
}

const mc_tile_entity_class_identity *mc_registry_sign_class(mc_registry_classes *classes) {
    classes->sign_class.client_name = "yk";
    classes->sign_class.server_name = "pq";
    return &classes->sign_class;
}
