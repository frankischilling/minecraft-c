#ifndef MC_WORLD_INFO_INFO_H
#define MC_WORLD_INFO_INFO_H

#include "mc/storage/nbt/nbt.h"

#include <stdbool.h>

typedef struct mc_world_info mc_world_info;
typedef struct mc_world_info_name mc_world_info_name;
typedef struct mc_world_info_player mc_world_info_player;
typedef struct mc_world_info_data mc_world_info_data;
typedef struct mc_world_info_record mc_world_info_record;

typedef enum mc_world_info_side {
    MC_WORLD_INFO_CLIENT = 0,
    MC_WORLD_INFO_SERVER
} mc_world_info_side;

typedef enum mc_world_info_phase {
    MC_WORLD_INFO_RAW = 0,
    MC_WORLD_INFO_READY,
    MC_WORLD_INFO_FAILED_CONSTRUCTION
} mc_world_info_phase;

typedef enum mc_world_info_status {
    MC_WORLD_INFO_OK = 0,
    MC_WORLD_INFO_INVALID_ARGUMENT,
    MC_WORLD_INFO_INVALID_PHASE,
    MC_WORLD_INFO_INVALID_FIELD,
    MC_WORLD_INFO_UNSUPPORTED_OPERATION,
    MC_WORLD_INFO_LIMIT,
    MC_WORLD_INFO_ALLOCATION_FAILED,
    MC_WORLD_INFO_NBT_ERROR,
    MC_WORLD_INFO_CLOCK_FAILURE
} mc_world_info_status;

typedef struct mc_world_info_error {
    mc_world_info_status status;
    const char *field; /* Static ASCII key or NULL. */
    mc_nbt_error nbt;
} mc_world_info_error;

typedef struct mc_world_info_scalars {
    int64_t seed;
    int32_t spawn_x, spawn_y, spawn_z;
    int64_t time, last_played, size_on_disk;
    int32_t player_dimension; /* Cached at NBT construction, not a live query. */
    int32_t version, rain_time, thunder_time;
    bool raining, thundering;
} mc_world_info_scalars;

typedef struct mc_world_info_fields {
    mc_world_info_scalars value;
    mc_world_info_name *name;
    mc_world_info_player *player;
} mc_world_info_fields;

typedef struct mc_world_info_state {
    mc_world_info_side side;
    mc_world_info_phase phase;
    unsigned int last_store_pc; /* Last completed original constructor field store. */
} mc_world_info_state;

/* Stable native access to the actual mutable cells used by World contexts.
 * The descriptor cannot be rebound. Keep the supplying Info held and exclusive
 * through every use; a copied Info has its own distinct cells. */
typedef struct mc_world_info_live {
    int64_t *time;
    int32_t *spawn_x, *spawn_y, *spawn_z;
} mc_world_info_live;

const char *mc_world_info_status_string(mc_world_info_status status);

/* Native UTF-16 ownership conversion, not a gameplay String constructor.
 * Copy all code units, including NUL and unpaired surrogates, into a distinct
 * immutable identity. Nonnull empty names differ from NULL. max_bytes includes
 * this exact name allocation. No interning or text normalization occurs. */
mc_world_info_status mc_world_info_name_copy(mc_nbt_string text, size_t max_bytes,
                                             mc_world_info_name **out);
mc_nbt_string mc_world_info_name_text(const mc_world_info_name *name);
mc_world_info_status mc_world_info_name_retain(mc_world_info_name *name);
void mc_world_info_name_release(mc_world_info_name *name);

/* Extra native retention of a compound, with no WorldInfo schema validation.
 * Deep-copy its immutable children through mc_nbt_tree. The handle owns a
 * separate mutable root-name overlay: serialization below renames that SAME
 * root identity to Player. Borrowed views must not be in use during mutation.
 * General mutable child/tag operations and live Player simulation are separate.
 * NULL limits selects NBT defaults; the budget includes the handle and tree.
 * No source pointers are retained and failure leaves out unchanged. */
mc_world_info_status mc_world_info_player_copy(const mc_nbt_tag *compound,
                                               const mc_nbt_limits *limits,
                                               mc_world_info_player **out,
                                               mc_world_info_error *error);
const mc_nbt_tag *mc_world_info_player_root(const mc_world_info_player *player);
/* Original base-tag name operation on this same compound. The setter accepts
 * NULL; the getter returns the shared nonnull empty literal for that state.
 * An explicitly supplied nonnull empty name retains its distinct identity. */
mc_world_info_status mc_world_info_player_name_set(mc_world_info_player *player,
                                                   mc_world_info_name *name);
mc_world_info_name *mc_world_info_player_name(const mc_world_info_player *player);
mc_world_info_status mc_world_info_player_retain(mc_world_info_player *player);
void mc_world_info_player_release(mc_world_info_player *player);

/* Extra native ownership conversion of the INNER Data compound, not a
 * root/Data envelope or a WorldInfo constructor. Preserve its original tree
 * without normal-writer reencoding. Create shared name/Player handles for the
 * two typed reference fields, without rejecting other wrong known-field types.
 * Multiple constructions using this same data owner share those handles.
 * Missing LevelName uses one retained nonnull empty literal identity. A source
 * copy does not preserve aliases to arbitrary external C trees or JVM objects.
 * The allocation budget includes all new wrappers/name/tree storage; codec
 * validation scratch and the retained tree occupy separate phases.
 * Failure preserves out. No I/O, clock or gameplay construction occurs. */
mc_world_info_status mc_world_info_data_copy(const mc_nbt_tag *data, const mc_nbt_limits *limits,
                                             mc_world_info_data **out, mc_world_info_error *error);
mc_world_info_status mc_world_info_data_retain(mc_world_info_data *data);
void mc_world_info_data_release(mc_world_info_data *data);
/* Borrowed typed handles; NULL data or a wrong-typed field returns NULL.
 * Missing name returns the shared empty identity; absent Player returns NULL. */
mc_world_info_name *mc_world_info_data_name(const mc_world_info_data *data);
mc_world_info_player *mc_world_info_data_player(const mc_world_info_data *data);
/* Actual typed reference assignments in the retained input Data model. These
 * replace the selected LevelName String / Player compound reference, rather
 * than importing equal text/trees as new identities. Both arguments must be
 * nonnull, as the original compound setters dereference/construct them. Player
 * assignment first renames that SAME compound to the shared Player key literal.
 * These setters replace wrong-typed/missing selected fields for later Info
 * constructions; existing Infos retain their old field references. Immutable
 * native source provenance is a separate historical view and never changes.
 * No allocation, clock, scalar assignment or generic NBT-map mutation occurs. */
mc_world_info_status mc_world_info_data_name_set(mc_world_info_data *data,
                                                 mc_world_info_name *name);
mc_world_info_status mc_world_info_data_player_set(mc_world_info_data *data,
                                                   mc_world_info_player *player);

/* Allocate one stable identity with one native hold, raw fields and NULL
 * references. Construct once in RAW. A reached attempt latches FAILED_CONSTRUCTION and
 * retains every completed field assignment; retry is invalid. No constructor
 * performs I/O, samples clocks, creates Randoms or changes shared storage.
 * Fresh stores only seed then the exact nullable name. NBT reads the Data
 * fields in original order, publishes Player before reading its Dimension,
 * and retains the data as additional immutable native provenance. Copy keeps
 * scalars independent and retains the exact name/Player/provenance handles.
 * Copy source must be READY and have the same side. A valid RAW destination
 * latches failure even for NULL input or an invalid copy source. A NULL or
 * non-RAW destination rejects without changing its existing state.
 * Outputs remain unchanged
 * on allocation failure. Native retain failures are explicit resource policy. */
mc_world_info_status mc_world_info_allocate(mc_world_info_side side, mc_world_info **out);
mc_world_info_status mc_world_info_construct(mc_world_info *raw, int64_t seed,
                                             mc_world_info_name *name);
mc_world_info_status mc_world_info_construct_nbt(mc_world_info *raw, mc_world_info_data *data,
                                                 mc_world_info_error *error);
mc_world_info_status mc_world_info_construct_copy(mc_world_info *raw, const mc_world_info *source);
mc_world_info_status mc_world_info_retain(mc_world_info *info);
void mc_world_info_release(mc_world_info *info);
mc_world_info_status mc_world_info_fields_get(const mc_world_info *info, mc_world_info_fields *out);
mc_world_info_status mc_world_info_state_get(const mc_world_info *info, mc_world_info_state *out);
/* READY only; NULL or any other phase returns NULL. No snapshot or hold. */
const mc_world_info_live *mc_world_info_live_get(mc_world_info *info);
const mc_nbt_tag *mc_world_info_source(const mc_world_info *info);

/* READY-only direct flag reads. Each reads only its named current scalar;
 * failure preserves out, and no allocation, clock or other flag read occurs. */
mc_world_info_status mc_world_info_raining_get(const mc_world_info *info, bool *out);
mc_world_info_status mc_world_info_thundering_get(const mc_world_info *info, bool *out);

/* READY only. Retain before replacing a reference, including self-assignment.
 * name accepts NULL. Player assignment is client-only and accepts NULL without
 * reading or changing cached player_dimension. No numeric normalization. */
mc_world_info_status mc_world_info_name_set(mc_world_info *info, mc_world_info_name *name);
mc_world_info_status mc_world_info_player_set(mc_world_info *info, mc_world_info_player *player);
mc_world_info_status mc_world_info_time_set(mc_world_info *info, int64_t value);
mc_world_info_status mc_world_info_size_set(mc_world_info *info, int64_t value);
mc_world_info_status mc_world_info_spawn_set(mc_world_info *info, int32_t x, int32_t y, int32_t z);
mc_world_info_status mc_world_info_version_set(mc_world_info *info, int32_t value);
mc_world_info_status mc_world_info_rain_time_set(mc_world_info *info, int32_t value);
mc_world_info_status mc_world_info_thunder_time_set(mc_world_info *info, int32_t value);
mc_world_info_status mc_world_info_raining_set(mc_world_info *info, bool value);
mc_world_info_status mc_world_info_thundering_set(mc_world_info *info, bool value);

/* Actual no-list WorldInfo serializer, READY only. Allocate a stable output
 * record, capture the existing Player reference, and populate the inner unnamed
 * Data compound. Time then SizeOnDisk precede one actual wall-clock sample for
 * output LastPlayed; live last_played never changes. Null name fails after that
 * sample, before version/weather/Player insertion. A nonnull empty name works.
 * Finally rename/insert the captured SAME Player root, without rereading its
 * Dimension or assigning info->player. Record holds keep references live after
 * Info/source retirement. No normal NBT encoding, file save or list-of-live-
 * Players route occurs. Fields use insertion order; Java HashMap byte order is
 * not reproduced by this borrowed view.
 *
 * Failure preserves out, but never rolls back reached clock/Player effects.
 * A subsequent codec failure likewise cannot undo the shared Player rename.
 * Output views are valid until release; do not copy/move or mutate them, and
 * keep Player children exclusive/immutable through codec use. */
mc_world_info_status mc_world_info_record_create(const mc_world_info *info,
                                                 mc_world_info_record **out,
                                                 mc_world_info_error *error);
const mc_nbt_tag *mc_world_info_record_root(const mc_world_info_record *record);
/* Borrowed actual String identity supplying the record's LevelName span. */
mc_world_info_name *mc_world_info_record_name(const mc_world_info_record *record);
void mc_world_info_record_release(mc_world_info_record *record);

/* All owners require exclusive access; no implicit synchronization. Retain
 * overflow leaves holds unchanged. release(NULL) is safe. Other NULL owners
 * reject unless a specific getter documents an empty view (name_text,
 * player_root, player_name, data_name, data_player, source, record_root and record_name return
 * empty/NULL). Snapshot references are borrowed; retain independently before their supplying owner
 * changes. Inputs, outputs/errors, limits and live owner storage must be disjoint, except input
 * trees may borrow an existing held provenance tree. */

#endif
