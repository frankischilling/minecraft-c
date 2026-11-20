#ifndef MC_BLOCK_MATERIAL_MATERIAL_H
#define MC_BLOCK_MATERIAL_MATERIAL_H

#include <stdbool.h>
#include <stdint.h>

/* These are case-sensitive original
 * ln/hj static field selectors, not block IDs or interchangeable properties. */
typedef enum mc_material_id {
    MC_MATERIAL_a,
    MC_MATERIAL_b,
    MC_MATERIAL_c,
    MC_MATERIAL_d,
    MC_MATERIAL_e,
    MC_MATERIAL_f,
    MC_MATERIAL_g,
    MC_MATERIAL_h,
    MC_MATERIAL_i,
    MC_MATERIAL_j,
    MC_MATERIAL_k,
    MC_MATERIAL_l,
    MC_MATERIAL_m,
    MC_MATERIAL_n,
    MC_MATERIAL_o,
    MC_MATERIAL_p,
    MC_MATERIAL_q,
    MC_MATERIAL_r,
    MC_MATERIAL_s,
    MC_MATERIAL_t,
    MC_MATERIAL_u,
    MC_MATERIAL_v,
    MC_MATERIAL_w,
    MC_MATERIAL_x,
    MC_MATERIAL_y,
    MC_MATERIAL_z,
    MC_MATERIAL_A,
    MC_MATERIAL_B,
    MC_MATERIAL_COUNT
} mc_material_id;

/* dx/cm field b has index 0, through field o at index 13. */
typedef enum mc_material_palette_id {
    MC_PALETTE_b,
    MC_PALETTE_c,
    MC_PALETTE_d,
    MC_PALETTE_e,
    MC_PALETTE_f,
    MC_PALETTE_g,
    MC_PALETTE_h,
    MC_PALETTE_i,
    MC_PALETTE_j,
    MC_PALETTE_k,
    MC_PALETTE_l,
    MC_PALETTE_m,
    MC_PALETTE_n,
    MC_PALETTE_o,
    MC_PALETTE_COUNT
} mc_material_palette_id;

#define MC_MATERIAL_PALETTE_SLOTS 16u

typedef struct mc_materials mc_materials;
typedef struct mc_material mc_material;
typedef struct mc_material_palette mc_material_palette;

/* One owner represents one shared process/class-profile lifetime. All worlds
 * in that profile must share this same owner and its actual object addresses;
 * never create/reset a material set per world or replace it while views live.
 * There is no reinitialization or owning-copy operation.
 *
 * create makes one private native allocation. False (including out == NULL)
 * leaves the caller's output unchanged; success publishes only a fully built
 * owner. The output slot must not own another live owner. This native boundary
 * does not reproduce Java allocation identity, exceptions or partial class
 * initialization after a VM allocation failure. Constructors here have no game
 * callbacks, entropy source, world access or block/bootstrap dependencies.
 * destroy(NULL) is allowed. Otherwise destroy only after every sharing world,
 * material/palette view and table view has ceased using the owner. */
bool mc_materials_create(mc_materials **out);
void mc_materials_destroy(mc_materials *owner);

/* All other functions require live, nonnull owner/receiver pointers from this
 * API. Selectors must be in their declared ranges; table indices are 0..15.
 * Returned pointers are borrowed, stable until owner destruction, and must not
 * be freed or copied into an independently owned set. All 28 material objects
 * and all 14 palette objects are distinct even when properties/RGB coincide.
 * Slots 14 and 15 of the actual 16-slot palette view are null. */
mc_material *mc_materials_get(mc_materials *owner, mc_material_id id);
const mc_material_palette *mc_materials_palette(const mc_materials *owner,
                                                mc_material_palette_id id);
const mc_material_palette *const *mc_materials_palette_table(const mc_materials *owner);
const mc_material_palette *mc_materials_palette_at(const mc_materials *owner, int32_t index);

/* Original final C palette identity; palette q is index and p is RGB. */
const mc_material_palette *mc_material_palette_of(const mc_material *material);
int32_t mc_material_palette_index(const mc_material_palette *palette);
int32_t mc_material_palette_rgb(const mc_material_palette *palette);

/* Exact original method results, with separate entry points. They perform no
 * acquiring world queries. f() alone mutates the receiver's E flag, returning
 * the same identity. Its change is visible to every world sharing this owner.
 * j() is the original int result; f() is not a boolean predicate. Serialize
 * mutation/lifetime access: this module provides no thread arbitration.
 * These pure receiver methods assume valid pointers, not nullable Java calls. */
bool mc_material_a(const mc_material *material);
bool mc_material_b(const mc_material *material);
bool mc_material_c(const mc_material *material);
bool mc_material_d(const mc_material *material);
bool mc_material_e(const mc_material *material);
mc_material *mc_material_f(mc_material *material);
bool mc_material_g(const mc_material *material);
bool mc_material_h(const mc_material *material);
bool mc_material_i(const mc_material *material);
int32_t mc_material_j(const mc_material *material);

#endif
