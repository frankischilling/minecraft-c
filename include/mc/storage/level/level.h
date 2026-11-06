#ifndef MC_STORAGE_LEVEL_LEVEL_H
#define MC_STORAGE_LEVEL_LEVEL_H

#include "mc/storage/nbt/nbt.h"

#include <stdbool.h>

/* Borrowed view of the original level.dat Data compound. Strings and Player
 * remain owned by the source NBT document. player_dimension is derived from
 * Player.Dimension; building writes the Player record, not this convenience
 * value. No entity or player simulation is implied by retaining that record. */
typedef struct mc_level_info {
    int64_t seed;
    int32_t spawn_x, spawn_y, spawn_z;
    int64_t time, last_played, size_on_disk;
    mc_nbt_string level_name;
    int32_t version, rain_time, thunder_time;
    bool raining, thundering;
    const mc_nbt_tag *player;
    int32_t player_dimension;
} mc_level_info;

typedef enum mc_level_status {
    MC_LEVEL_OK = 0,
    MC_LEVEL_INVALID_ARGUMENT,
    MC_LEVEL_INVALID_FIELD,
    MC_LEVEL_LIMIT,
    MC_LEVEL_ALLOCATION_FAILED,
    MC_LEVEL_NBT_ERROR
} mc_level_status;

typedef struct mc_level_error {
    mc_level_status status;
    const char *field; /* Static ASCII key, or NULL for a non-field failure. */
    mc_nbt_error nbt;
} mc_level_error;

const char *mc_level_status_string(mc_level_status status);

/* root must be an immutable valid NBT tree from a successful compound read.
 * Missing Data is an empty compound. Missing known fields use zero/empty
 * defaults; present fields require their exact original tag types. Every
 * nonzero Byte boolean is true. Neither version nor spawn is validated.
 * out is required; failure leaves it unchanged. No allocation, compression,
 * filesystem access, session lock, fallback or world construction occurs. */
mc_level_status mc_level_decode(const mc_nbt_tag *root, mc_level_info *out, mc_level_error *error);

/* Build an independent owned NBT document from a borrowed info view. The
 * explicit saved_at_ms becomes output LastPlayed without changing info.
 * Version is copied unchanged; the actual McRegion list-save route must
 * choose version 19132 itself. player == NULL explicitly omits Player.
 *
 * baseline may be NULL for a fresh unnamed root, or a valid immutable tree
 * from a successful compound read. Unknown root/Data children and the root
 * name are retained, with known fields replaced in their existing positions.
 * This preservation is stronger than the original fresh-root serializer and
 * prevents intermediate edits from deleting unsupported save data. Player is
 * retained as supplied, including opaque children. No subtype is interpreted.
 *
 * NULL limits selects NBT defaults. max_allocation_bytes bounds peak heap
 * storage created by this call (including temporary serialization and the
 * resulting document); preexisting borrowed trees are excluded. Other NBT
 * limits apply to the complete result. Building uses the normal NBT writer,
 * including canonical Java NaNs and Byte-typed empty lists. out is required
 * and unchanged on failure. Free success with mc_nbt_document_free.
 * Arguments/results must be mutually disjoint except that info and baseline
 * may borrow the same source tree. No source memory is mutated. */
mc_level_status mc_level_build(const mc_level_info *info, const mc_nbt_tag *baseline,
                               int64_t saved_at_ms, const mc_nbt_limits *limits,
                               mc_nbt_document **out, mc_level_error *error);

/* Package an actual immutable inner Data record into an independent owned
 * document. Require exactly the thirteen known scalar/String fields, each with
 * its original NBT type, plus optional Player of type Compound. All fields are
 * required except Player; extra physical entries are rejected. Do not read or
 * validate Player.Dimension, replace LastPlayed/version, sample a clock, or
 * mutate any name/Player. Known record nodes are borrowed directly until codec
 * use; record and its Player must remain live, exclusive and immutable.
 *
 * baseline has the same valid immutable-tree contract as mc_level_build.
 * Preserve its unknown root/Data children and root name, replace all fourteen
 * known fields, and remove old Player when record omits it. This is additional
 * native preservation, not the original fresh-root serializer's behavior.
 * Normal NBT writer/read ownership and all limits/peak-allocation accounting
 * match mc_level_build, including canonical NaNs and Byte-typed empty lists.
 * out is required and unchanged on failure; free success with
 * mc_nbt_document_free. All inputs/results must be disjoint except that record
 * and baseline may borrow the same valid source tree. */
mc_level_status mc_level_build_record(const mc_nbt_tag *record, const mc_nbt_tag *baseline,
                                      const mc_nbt_limits *limits, mc_nbt_document **out,
                                      mc_level_error *error);

#endif
