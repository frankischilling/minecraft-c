#ifndef MC_ENTITY_NBT_ERRORS_H
#define MC_ENTITY_NBT_ERRORS_H

#include "mc/entity/arrow/construction.h"
#include "mc/entity/errors.h"
#include "mc/entity/minecart/inventory_errors.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/registry/registry.h"
#include "mc/inventory/runtime/nbt.h"
#include "mc/storage/nbt/nbt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum mc_entity_nbt_status {
    MC_ENTITY_NBT_OK = 0,
    MC_ENTITY_NBT_INVALID_ARGUMENT,
    MC_ENTITY_NBT_NULL_REFERENCE,
    MC_ENTITY_NBT_WRONG_TAG,
    MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS,
    MC_ENTITY_NBT_LIMIT,
    MC_ENTITY_NBT_ALLOCATION_FAILED,
    MC_ENTITY_NBT_ENTITY_FAILED,
    MC_ENTITY_NBT_STACK_FAILED,
    MC_ENTITY_NBT_NBT_FAILED,
    MC_ENTITY_NBT_UNSUPPORTED_FP,
    MC_ENTITY_NBT_UNSUPPORTED_TYPE,
    MC_ENTITY_NBT_REGISTRY_FAILED,
    MC_ENTITY_NBT_SLEEP_FAILED,
    MC_ENTITY_NBT_BUSY,
    MC_ENTITY_NBT_CLEANUP_FAILED
} mc_entity_nbt_status;

typedef struct mc_entity_nbt_sleep_work mc_entity_nbt_sleep_work;
typedef struct mc_player_sleep_error mc_player_sleep_error;
typedef struct mc_player_sleep_cleanup mc_player_sleep_cleanup;

typedef enum mc_entity_nbt_sleep_cleanup_child {
    MC_ENTITY_NBT_SLEEP_CLEANUP_NONE,
    MC_ENTITY_NBT_SLEEP_CLEANUP_WAKE,
    MC_ENTITY_NBT_SLEEP_CLEANUP_REGISTRY
} mc_entity_nbt_sleep_cleanup_child;

typedef struct mc_entity_nbt_sleep_cleanup {
    mc_entity_nbt_status status;
    const char *operation;
    mc_entity_nbt_sleep_cleanup_child child;
    const mc_player_sleep_cleanup *wake;
    mc_registry_error registry;
    mc_world_storage_error cause;
} mc_entity_nbt_sleep_cleanup;

typedef enum mc_entity_nbt_late_hook {
    MC_ENTITY_NBT_LATE_NONE = 0,
    MC_ENTITY_NBT_LATE_CART,
    MC_ENTITY_NBT_LATE_PLAYER,
    MC_ENTITY_NBT_LATE_MOB,
    MC_ENTITY_NBT_LATE_MOB_CALL,
    MC_ENTITY_NBT_LATE_ARROW,
    MC_ENTITY_NBT_LATE_ARROW_CALL
} mc_entity_nbt_late_hook;

typedef enum mc_entity_mob_nbt_call_kind {
    MC_ENTITY_MOB_NBT_HOOK = 0,
    MC_ENTITY_MOB_NBT_SLIME_SIZE
} mc_entity_mob_nbt_call_kind;

typedef struct mc_entity_mob_nbt_detail {
    unsigned int living_bytecode;
    mc_mob_site concrete, parent, child, watcher;
    mc_entity_mob_nbt_call_kind call_kind;
    mc_mob_status call_status;
    mc_player_status string_status;
    bool native_policy, source_exception;
    const char *exception_class;
} mc_entity_mob_nbt_detail;

/* Whole per-attempt Mob call metadata. These pointers are static diagnostics,
 * never borrowed Entity/class/error storage. */
typedef struct mc_entity_mob_call_context {
    mc_mob_status status;
    const char *operation;
    mc_mob_site site, child;
    bool native_policy, source_exception;
    const char *exception_class;
} mc_entity_mob_call_context;

typedef enum mc_entity_mob_call_payload {
    MC_ENTITY_MOB_CALL_CURRENT = 0,
    MC_ENTITY_MOB_CALL_FIRST_SHARED,
    MC_ENTITY_MOB_CALL_FIRST_PREVIOUS
} mc_entity_mob_call_payload;

/* A first initializer failure copies the current lower causes. A repeated
 * erroneous-class use has fresh current lower causes and the complete first
 * failure. Store that exact selected value once, with independent current
 * metadata. No caller/frame/owner pointer or heap allocation is retained. */
typedef struct mc_entity_mob_call_detail {
    mc_entity_mob_call_context current;
    mc_mob_class_cause cause;
    mc_entity_mob_call_payload payload;
} mc_entity_mob_call_detail;

typedef struct mc_entity_arrow_nbt_detail {
    mc_arrow_site site, child;
    bool returned;
} mc_entity_arrow_nbt_detail;

/* Passive per-attempt frames. Zero PC normally denotes native admission or
 * unreached work; initializer0 is also the genuine first namespace allocation.
 * Use status/operation and the whole Registry cause to distinguish that site.
 * nested source calls retain their enclosing and child PCs independently.
 * index is SIZE_MAX unless a vector/List occurrence is selected. field and
 * operation are static ASCII strings, never borrowed input-tree storage.
 * The construction, Stack, namespace and codec causes remain whole values.
 * name1 denotes the final class-name wrapper's active use; name7 is reached
 * only after namespace initialization and the actual Class Map lookup. */
typedef struct mc_entity_nbt_error {
    mc_entity_nbt_status status;
    mc_entity_nbt_late_hook late_hook;
    const char *operation, *field;
    size_t index;
    unsigned int wrapper_bytecode, name_bytecode, factory_bytecode;
    unsigned int base_bytecode, item_bytecode, falling_bytecode, stack_bytecode;
    /* Actual dc -> gs -> ls hook frames, independent of the base wrapper. */
    unsigned int local_player_bytecode, player_bytecode;
    unsigned int initializer_bytecode, registration_bytecode;

    union {
        struct {
            mc_entity_error construction;
            mc_item_stack_error stack;
            mc_nbt_error nbt;
            mc_entity_registry_error registry;

            /* Concrete late hook selections; index is the Items list occurrence,
             * while inventory.slot is meaningful only when slot_selected is true.
             * Complete selected lower errors reuse construction/stack above. */
            /* NONE exposes the legacy fresh Cart defaults. CART, PLAYER, MOB and ARROW expose
             * only their complete selected arm; check late_hook before reading it.
             * Common/base/namespace/Stack/codec fields above remain independent for
             * these selections. MOB_CALL/ARROW_CALL instead select the complete suffix
             * below; use its passive expansion before inspecting lower causes. */
            union {
                struct {
                    mc_minecart_site minecart_read, minecart_write;
                    mc_minecart_inventory_context inventory;
                };

                struct {
                    unsigned int living_bytecode;
                    mc_inventory_nbt_error player_inventory;
                };

                mc_entity_mob_nbt_detail mob;
                mc_entity_arrow_nbt_detail arrow;
            };
        };

        mc_entity_mob_call_detail mob_call;
        /* Value-only constructor receipt; no owner or replay rights. */
        mc_arrow_error arrow_call;
    };

    /* Independent of every late detail arm. An interrupted GS93 owns this
     * stable native work, not an input-tree or stack diagnostic pointer. */
    mc_entity_nbt_sleep_work *sleep_work;
    const mc_player_sleep_error *sleep;
    const mc_entity_nbt_sleep_cleanup *sleep_cleanup;
} mc_entity_nbt_error;

/* A nonnull sleep_work makes this result noncopyable. It transfers exactly
 * once to its containing caller and must be checked-retired before reuse,
 * initialization, assignment or graph retirement. Views borrow that same work.
 * Source status/operation/caller PCs survive retirement. The typed child views
 * end when their producer is consumed, including before a later parent Registry
 * refusal. Cleanup retries perform no source work or input-tree reads. */
bool mc_entity_nbt_error_retire(mc_entity_nbt_error *);

/* Passive in-place initialization; NULL is a no-op. Never discard owned work. */
void mc_entity_nbt_error_init(mc_entity_nbt_error *error);
/* Passive concrete-detail selection. Repeated selection preserves the active
 * child; changing arms initializes that entire selected payload. Common
 * diagnostics remain independent for NONE/CART/PLAYER/MOB/ARROW. Leaving MOB_CALL
 * restores its exact current Entity/Stack and fresh codec/EntityList values
 * before selecting the new arm. Leaving ARROW_CALL restores its current Entity
 * cause and fresh Stack/codec/EntityList values. NULL is a no-op. No source
 * method is entered. */
void mc_entity_nbt_select_cart_detail(mc_entity_nbt_error *error);
void mc_entity_nbt_select_player_detail(mc_entity_nbt_error *error);
void mc_entity_nbt_select_mob_detail(mc_entity_nbt_error *error);
void mc_entity_nbt_select_arrow_detail(mc_entity_nbt_error *error);
/* Capture a whole actual constructor call by value. The admitted forms are
 * no first failure, a first failure with identical current lower causes, or
 * an erroneous-class use with fresh current lower causes. False preserves
 * error for a NULL/unsupported input. Prefix diagnostics remain unchanged.
 * Constructor entry has fresh Stack/codec/EntityList causes; successful JC
 * lookup returns that fresh EntityList value before the constructor. All
 * inputs/outputs must be disjoint and live through these passive calls. */
bool mc_entity_nbt_select_mob_call_detail(mc_entity_nbt_error *, const mc_mob_error *);
/* Expand a constructor selection, or a selected Slime size failure, into the
 * complete original Mob error value. False preserves out. No owner, ambient
 * last-error, source call, allocation or string lifetime is consulted.
 * Constructor current Entity/Stack causes are in this returned value; its
 * codec/EntityList causes are the documented fresh values. Switching back to
 * another late arm restores those current common causes by value. */
bool mc_entity_nbt_mob_call_error(const mc_entity_nbt_error *, mc_mob_error *out);
/* Arrow constructor calls preserve their complete independent source, heading,
 * Math, position and Random receipt. NULL or live sleep work is rejected;
 * false leaves the destination unchanged. */
bool mc_entity_nbt_select_arrow_call_detail(mc_entity_nbt_error *, const mc_arrow_error *);
bool mc_entity_nbt_arrow_call_error(const mc_entity_nbt_error *, mc_arrow_error *out);
mc_entity_nbt_error mc_entity_nbt_empty_error(void);
const char *mc_entity_nbt_status_string(mc_entity_nbt_status status);

#endif
