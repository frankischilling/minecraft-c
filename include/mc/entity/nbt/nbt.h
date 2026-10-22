#ifndef MC_ENTITY_NBT_NBT_H
#define MC_ENTITY_NBT_NBT_H

#include "mc/entity/entity.h"
#include "mc/entity/nbt/errors.h"
#include "mc/storage/nbt/tree.h"

/* Actual sn/lq.e(NBT), followed by the actual hl/ez Item, ju/gl FallingSand
 * or yl/pr Minecart a(NBT) hook selected by this Entity's immutable same-owner
 * canonical Class.
 * Other concrete subclasses require real dispatch and fields before use.
 * No dead/name/factory decision, constructor, READY gate, source preservation,
 * World callback or Random draw is added. A balanced temporary Entity hold
 * precedes source work; native hold overflow preserves the Entity unchanged.
 *
 * Read only the reached immutable input views, with ordinary exact tag checks.
 * Compound children/names and reached List spans must be readable and immutable;
 * this is not a pointer-probing or whole-tree validation API. Compound names
 * are unique. Individual List children are checked in source order, permitting
 * explicit prepared heterogeneous/NULL-child controls without encoding them.
 * Missing Lists behave as empty Lists; missing scalar fields use source zero.
 * Input NULL fails at the first Pos getter. A wrong later tag preserves every
 * completed earlier store, including current/previous positions and angles.
 *
 * The existing bounds identity is mutated at base294. Native strict-FP and
 * nonnull-bounds admission occur there, after earlier read stores. Rotation306
 * uses float truncating remainder by360. Reached failure never rolls back,
 * resets constructor/class state or replaces the whole Entity snapshot.
 * Item Health/Age precede a newly allocated/constructed Stack; old Stack stays
 * owned until successful reference replacement40. No limits argument or
 * provenance copy is implicit; that one Stack is a separate ordinary native
 * owner, excluded from the Entity class/direct-allocation ledger.
 * Input may borrow the old Stack's immutable source through its last read.
 * FallingSand instead reads only Tile Byte at own4, masks with255 and stores
 * carried a at11. Missing Tile supplies0; a wrong tag preserves the complete
 * earlier base prefix. Its age b, Item fields and Stack remain untouched.
 * Minecart reads Type first. Type2 reads PushX/PushZ/Fuel in order; Type1
 * captures Items before replacing its actual h with a new27 array. A later
 * malformed record retains Type, that new array and every earlier slot store.
 * Detached old h and each occurrence hold remain live until all input reads
 * finish, including failure, so input may borrow any old Stack source. Other
 * Type values leave h/push/fuel untouched. No provenance is attached.
 * Caller retains exclusive live owners for the call. error may be NULL. */
mc_entity_nbt_status mc_entity_read_nbt(mc_entity *entity, const mc_nbt_tag *compound,
                                        mc_entity_nbt_error *error);

/* Independent-result form of actual sn/lq.d(NBT) plus the concrete b(NBT).
 * Supported classes emit Pos, Motion, Rotation, FallDistance, Fire, Air
 * and OnGround. Item adds Health, Age and Item (ten fields); FallingSand adds
 * only carried a narrowed int->byte as Tile (eight fields), without age/Stack.
 * Minecart emits Type and, for Type1, Items from the actual h length; Type2
 * emits PushX/PushZ/Fuel instead. Its supported totals are8/9/11 fields.
 * Minecart Stack records use actual fields in every live phase, with no
 * READY-only/provenance builder, ID validation, count clamp or inventory callback.
 * No id/dead/namespace/Boolean c wrapper or retained unknown tags are supplied.
 * Values come from the held actual Entity/Stack; saved Y is y+widen(y_size),
 * and Item Health narrows int->byte->short.
 *
 * No Entity READY check. Nullable Stack is reached only at Item37, after the
 * earlier base/Item values. A nonnull Stack is retained there and its actual
 * fields are accepted in RAW/READY/failed phases, with source signed narrowing.
 * No eager Stack phase/Registry/count/ID check or provenance overlay is added.
 * Strict binary32/binary64 admission occurs at saved-Y arithmetic25. No source
 * fields, Random, World binding, class phases or direct-byte ledger change.
 *
 * NULL limits selects NBT defaults. The complete constructed tree is checked
 * by mc_nbt_tree_copy_compound: output bytes/tags/elements/depth, validator peak
 * and exact new tree allocation. max_input_bytes is unused. Minecart's own
 * independently built field tree stays charged while copying the final
 * base+own result, and is freed on every outcome. No other heap scratch is
 * allocated. Borrowed caller-owned Entity/Stack/old trees, allocator
 * overhead and bounded C-stack views are excluded, as in the tree contract.
 * Those external owners must be separately charged by a combining caller.
 * This is not Java mutable-setter/temporary-allocation/OOME/HashMap-order or
 * original/native floating-remainder qualification. The tree preserves raw
 * representations; later normal codec writing applies its canonicalization.
 *
 * out is required and unchanged on every failure; a successful independent
 * tree is freed with mc_nbt_tree_free. Hold Entity/dependencies exclusively
 * through return. out/error/limits must be mutually disjoint and must not
 * overlap any owner, input view, borrowed source or output storage. Read's
 * error follows the same rule; its input may borrow held immutable provenance
 * as described above. Success clears the whole optional diagnostic; failure
 * retains complete independent source/child frames, with index SIZE_MAX
 * outside selected vector/List accesses. */
mc_entity_nbt_status mc_entity_build_nbt(mc_entity *entity, const mc_nbt_limits *limits,
                                         mc_nbt_tree **out, mc_entity_nbt_error *error);

#endif
