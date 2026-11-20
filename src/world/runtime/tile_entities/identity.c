#include "identity_internal.h"

#include "tile_entities_internal.h"

static mc_tile_entity_status identity_hash(mc_java_identity_cell *cell,
                                           mc_java_identity_thread *current_thread, uint32_t *out) {
    if (cell->hash > UINT32_C(0x7fffffff))
        return MC_TILE_ENTITY_STATE_ERROR;
    *out = mc_java_object_identity_hash(current_thread, cell);
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_identity_hash(mc_tile_entity *tile,
                                                   mc_java_identity_thread *current_thread,
                                                   uint32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    if (tile == NULL) {
        *out = 0;
        return MC_TILE_ENTITY_OK;
    }
    if (current_thread == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    if (tile->holds == 0)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    if (tile->phase != MC_TILE_ENTITY_RAW && tile->phase != MC_TILE_ENTITY_READY &&
        tile->phase != MC_TILE_ENTITY_FAILED_CONSTRUCTION)
        return MC_TILE_ENTITY_INVALID_PHASE;
    const mc_tile_entity_class_literal *literal = NULL;
    mc_tile_entity_status status = mc_tile_entity_get_class(tile, &literal);
    mc_java_identity_cell *class_cell = NULL;
    if (status == MC_TILE_ENTITY_OK)
        status = mc_tile_entity_class_literal_identity_cell(literal, &class_cell);
    if (status != MC_TILE_ENTITY_OK)
        return status;
    return identity_hash(&tile->identity, current_thread, out);
}

mc_tile_entity_status
mc_tile_entity_class_literal_identity_hash(const mc_tile_entity_class_literal *literal,
                                           mc_java_identity_thread *current_thread, uint32_t *out) {
    if (current_thread == NULL || out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    mc_java_identity_cell *cell = NULL;
    const mc_tile_entity_status status = mc_tile_entity_class_literal_identity_cell(literal, &cell);
    return status == MC_TILE_ENTITY_OK ? identity_hash(cell, current_thread, out) : status;
}
