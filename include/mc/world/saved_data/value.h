#ifndef MC_WORLD_SAVED_DATA_VALUE_H
#define MC_WORLD_SAVED_DATA_VALUE_H

#include "mc/storage/nbt/array.h"
#include "mc/world/info/info.h"
#include "mc/world/storage/status.h"

typedef struct mc_saved_data_value mc_saved_data_value;
typedef struct mc_saved_data_class mc_saved_data_class;
typedef struct mc_saved_data_record mc_saved_data_record;

typedef enum mc_saved_data_exception {
    MC_SAVED_DATA_NO_EXCEPTION = 0,
    MC_SAVED_DATA_NULL_REFERENCE,
    MC_SAVED_DATA_CLASS_CAST,
    MC_SAVED_DATA_ARRAY_BOUNDS
} mc_saved_data_exception;

typedef struct mc_saved_data_error {
    mc_world_storage_error cause;
    mc_saved_data_exception exception;
    unsigned int bytecode, getter_bytecode;
    const char *field;
    int32_t index;
    size_t length;
} mc_saved_data_error;

typedef struct mc_saved_data_map_fields {
    const mc_saved_data_class *type;
    mc_world_info_name *name;
    bool dirty;
    int32_t x_center, z_center, updates;
    int8_t dimension, scale;
    mc_nbt_byte_array *colors;
    const void *trackers, *decorations, *player_trackers;
    size_t tracker_count, decoration_count, player_tracker_count;
} mc_saved_data_map_fields;

typedef struct mc_saved_data_map_cells {
    int32_t *x_center, *z_center, *updates;
    int8_t *dimension, *scale;
} mc_saved_data_map_cells;

/* Canonical class selections for the paired MapData classes iu/fv. A NULL or
 * foreign selection is not a constructible class. These passive native views
 * perform no initialization, constructor, allocation or random operation. */
const mc_saved_data_class *mc_saved_data_map_class(mc_world_info_side side);
bool mc_saved_data_class_side(const mc_saved_data_class *type, mc_world_info_side *out);

/* Construct the actual base name/dirty and MapData fields. Retain the supplied
 * nullable String identity, create one zeroed byte[16384] and three distinct
 * actual collection descriptors. Dirty starts false. max_new_bytes bounds this
 * value allocation plus its new colors allocation; the borrowed String's
 * allocation is separate. Failure preserves out. Trackers, map item updates
 * and packet behavior are not performed by this constructor. */
mc_world_storage_status mc_saved_data_map_create(const mc_saved_data_class *type,
                                                 mc_world_info_name *name, size_t max_new_bytes,
                                                 mc_saved_data_value **out,
                                                 mc_saved_data_error *error);
mc_world_storage_status mc_saved_data_value_retain(mc_saved_data_value *value);
void mc_saved_data_value_release(mc_saved_data_value *value);
mc_world_info_name *mc_saved_data_value_name(const mc_saved_data_value *value);
const mc_saved_data_class *mc_saved_data_value_class(const mc_saved_data_value *value);
bool mc_saved_data_value_dirty(const mc_saved_data_value *value);
mc_world_storage_status mc_saved_data_value_dirty_set(mc_saved_data_value *value, bool dirty);
mc_world_storage_status mc_saved_data_value_mark_dirty(mc_saved_data_value *value);
mc_world_storage_status mc_saved_data_map_fields_get(const mc_saved_data_value *value,
                                                     mc_saved_data_map_fields *out);
const mc_saved_data_map_cells *mc_saved_data_map_live(mc_saved_data_value *value);
/* Retain the selected SAME array, including nullable source f. Replacing f does
 * not invalidate older records or input tags which retain their own hold. */
mc_world_storage_status mc_saved_data_map_colors_set(mc_saved_data_value *value,
                                                     mc_nbt_byte_array *colors);

/* Actual MapData read. Borrow a readable Compound/tag graph for this call.
 * The selected colors ByteArray tag must use colors' exact data/length, or
 * {NULL,0} with colors=NULL for the Java null-array state. Its supplying handle
 * is a native lifetime bridge, not a copied payload or a synthesized getter.
 * For decoded input obtain it with mc_nbt_document_byte_array. Absent colors
 * ignores that bridge and constructs a distinct empty array at the getter.
 *
 * Numeric types are exact, absent fields default to zero, and scale is clamped
 * as a signed byte. Standard 128x128 input retains the SAME array at any length.
 * Other dimensions publish a new zeroed 16384 array before the original linear
 * copy, preserving its impossible AND guards and source-load-before-destination
 * store order. A later type/null/bounds failure retains every earlier scalar,
 * colors replacement and pixel store. Dirty and tracker fields stay unchanged.
 * max_new_bytes bounds new array allocations of this call; retained prior/input
 * owners and their documented budgets are separate. */
mc_world_storage_status mc_saved_data_map_read(mc_saved_data_value *value,
                                               const mc_nbt_tag *compound,
                                               mc_nbt_byte_array *colors, size_t max_new_bytes,
                                               mc_saved_data_error *error);
/* Decoded-input route. Resolve the reached colors getter by exact tag identity
 * in this document, including distinct zero-length tags. Retain its actual
 * array lazily at that getter, after earlier numeric reads. compound is a
 * borrowed Compound from document, or the actual fresh empty Compound selected
 * by a missing parent data key. No immutable provenance copy supplies f. */
mc_world_storage_status mc_saved_data_map_read_document(mc_saved_data_value *value,
                                                        mc_nbt_document *document,
                                                        const mc_nbt_tag *compound,
                                                        size_t max_new_bytes,
                                                        mc_saved_data_error *error);

/* Actual seven-field write into an independent record, retaining the SAME f
 * backing. Scalars are captured now; later mutations to that array remain
 * visible through the record. No dirty change, clamping or forced array length
 * occurs. A NULL f is retained as a nullable ByteArray tag state; creation still
 * succeeds. record_root then returns NULL with its explicit late encoding cause
 * instead of passing {NULL,0} to a codec as a fabricated empty array.
 * The source handler decides when encoding is reached relative to file open.
 * max_new_bytes bounds this record allocation; input owners remain separate. */
mc_world_storage_status mc_saved_data_map_write(const mc_saved_data_value *value,
                                                size_t max_new_bytes, mc_saved_data_record **out,
                                                mc_saved_data_error *error);
const mc_nbt_tag *mc_saved_data_record_root(const mc_saved_data_record *record,
                                            mc_saved_data_error *error);
mc_nbt_byte_array *mc_saved_data_record_colors(const mc_saved_data_record *record);
void mc_saved_data_record_release(mc_saved_data_record *record);

/* All owners and borrowed views require exclusive access. Scalars/cells and
 * descriptors are actual retained state; keep their value alive through use.
 * NULL value getters return NULL/false and mutation rejects NULL. Array and
 * value handles have independent lifetimes; release(NULL) is harmless. Outputs
 * and errors must be disjoint from live owners and each other. */

#endif
