#ifndef MC_CLIENT_INPUT_MOVEMENT_INPUT_H
#define MC_CLIENT_INPUT_MOVEMENT_INPUT_H

#include "mc/client/settings/game_settings.h"

struct mc_entity;
typedef struct mc_movement_input mc_movement_input;

#define MC_MOVEMENT_INPUT_KEYS 10u

typedef enum mc_movement_input_kind {
    MC_MOVEMENT_INPUT_BASE,
    MC_MOVEMENT_INPUT_FROM_OPTIONS
} mc_movement_input_kind;

typedef enum mc_movement_input_status {
    MC_MOVEMENT_INPUT_OK,
    MC_MOVEMENT_INPUT_INVALID_ARGUMENT,
    MC_MOVEMENT_INPUT_ALLOCATION_FAILED,
    MC_MOVEMENT_INPUT_SOURCE_EXCEPTION
} mc_movement_input_status;

typedef enum mc_movement_input_exception {
    MC_MOVEMENT_INPUT_EXCEPTION_NONE,
    MC_MOVEMENT_INPUT_NULL_SETTINGS,
    MC_MOVEMENT_INPUT_NULL_BINDING
} mc_movement_input_exception;

typedef struct mc_movement_input_error {
    mc_movement_input_status status;
    mc_movement_input_exception exception;
    const char *operation;
    unsigned int bytecode;
    /* Current GameSettings named-field index, or -1 outside a binding read. */
    int32_t named_index;
} mc_movement_input_error;

/* Actual public uo.a/b/c/d/e fields. source_c has no established gameplay
 * meaning and is not changed by either class's three methods. */
typedef struct mc_movement_input_fields {
    float strafe, forward;
    bool source_c, jump, sneak;
} mc_movement_input_fields;

typedef struct mc_movement_input_state {
    mc_movement_input_kind kind;
    mc_movement_input_fields fields;
    const mc_game_settings *settings; /* Borrowed lr.g; NULL for base or null g. */
    const void *keys_identity;        /* Actual private ten-cell array, else NULL. */
    size_t key_count;
    bool keys[MC_MOVEMENT_INPUT_KEYS]; /* Snapshot, not writable source storage. */
} mc_movement_input_state;

/* Constructors reproduce uo() or lr(GameSettings), including inherited +0/false
 * fields and the derived class's separately allocated ten false cells. The
 * options owner borrows the supplied settings identity, including NULL; it
 * never copies bindings/codes or performs settings I/O. Retire input BEFORE
 * retiring nonnull settings. Settings must remain live/exclusive for every
 * input operation; its public mutations between calls remain visible.
 *
 * Fixed native allocation failure preserves out. No Entity, Player, World,
 * platform keyboard, clock, Random, renderer or audio operation occurs. */
mc_movement_input_status mc_movement_input_create(mc_movement_input **out,
                                                  mc_movement_input_error *error);
mc_movement_input_status mc_movement_input_create_from_options(mc_game_settings *settings,
                                                               mc_movement_input **out,
                                                               mc_movement_input_error *error);

bool mc_movement_input_read_state(const mc_movement_input *input, mc_movement_input_state *out);
/* Direct assignment of the five source public fields, not a source update.
 * Every float bit pattern is accepted; no normalization or key-cell change. */
mc_movement_input_status mc_movement_input_set_fields(mc_movement_input *input,
                                                      const mc_movement_input_fields *fields,
                                                      mc_movement_input_error *error);

/* Dynamic uo/lr.a(IZ)V. Base genuinely returns without work. Options reads
 * current named m,o,n,p,q,v references and each actual live code, in that
 * order; only the LAST matching cell is changed. The current w array is unused.
 * A null settings/named binding fails at its source getfield; even an earlier
 * match has not yet changed a key cell. All signed int32 codes are admitted.
 * This does not update the five output fields or save settings. */
mc_movement_input_status mc_movement_input_key(mc_movement_input *input, int32_t code, bool pressed,
                                               mc_movement_input_error *error);
/* Dynamic a()V: base returns; options clears all ten cells. The public fields
 * are unchanged until the caller reaches a later update. */
mc_movement_input_status mc_movement_input_reset(mc_movement_input *input,
                                                 mc_movement_input_error *error);
/* Dynamic a(Player)V. Both original bodies leave the argument unread; NULL is
 * supported without constructing/substituting a Player. Options resets axes,
 * applies the ordered key contributions, copies jump/sneak, and scales axes
 * through double 0.3 then float when sneaking. source_c is preserved. No
 * settings read occurs here, including when the stored settings is NULL.
 *
 * Requires IEEE binary32/binary64, nearest-even, gradual underflow and strict
 * evaluation without reassociation, excess precision or fused operations. */
mc_movement_input_status mc_movement_input_update(mc_movement_input *input,
                                                  struct mc_entity *player,
                                                  mc_movement_input_error *error);

/* Native retirement only: frees its array/owner, clears the caller's pointer,
 * and never resets fields/keys through source methods or retires settings.
 * NULL *input succeeds. All calls require exclusive live owners; arguments,
 * output/error storage and borrowed views must be disjoint from owner storage.
 * No stale-pointer detection or automatic settings-retirement guard is added. */
mc_movement_input_status mc_movement_input_retire(mc_movement_input **input,
                                                  mc_movement_input_error *error);

#endif
