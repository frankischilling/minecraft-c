#ifndef MC_WORLD_RUNTIME_LIGHTING_H
#define MC_WORLD_RUNTIME_LIGHTING_H

#include "mc/world/lighting/world.h"
#include "mc/world/runtime/population/context.h"
#include "mc/world/runtime/runtime.h"

typedef struct mc_world_light_type mc_world_light_type;
typedef struct mc_world_light_region mc_world_light_region;

typedef struct mc_world_light_type_fields {
    mc_light_kind kind;
    int32_t c;
} mc_world_light_type_fields;

typedef struct mc_world_light_region_fields {
    const mc_world_light_type *a;
    mc_light_bounds bounds;
} mc_world_light_region_fields;

typedef struct mc_world_light_list_view {
    const mc_world_reference_list *identity;
    size_t count, capacity;
} mc_world_light_list_view;

typedef struct mc_world_light_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, provider_bytecode;
    mc_chunk_acquire_status acquisition;
    mc_world_storage_error cause;
    mc_world_population_context population;
} mc_world_light_error;

/* Actual GAME World.d(II) height method. Native admission requires a live GAME
 * owner other than RETIRED; RAW/constructing/failed owners retain their reached
 * field behavior. The asymmetric X/Z border returns zero before provider use.
 * Inside the border, read the current provider for presence first. Absence
 * returns zero without invoking the chunk getter or generating/loading data.
 * A present entry reaches the separate live getter, then its unsigned height
 * byte. Normal packed-key aliases and actual blank height zero are retained.
 *
 * Hold the World through the complete call. Native owner/context failures keep
 * their explicit storage status; error records World bytecode33/48/61 and the
 * nested provider call6 or acquisition result when reached. Admission is0.
 * acquisition is OK when no getter acquisition was attempted. Native admission
 * changes only error; reached source failure retains cause in World.last_error.
 * The result is unchanged on failure. Success does not erase prior World
 * diagnostics; entered getter effects retain their own ordinary behavior.
 * Keep output/error/owner storage disjoint and all owners live and exclusive.
 * This operation does not implement Chunk finishing or light propagation. */
mc_world_storage_status mc_world_runtime_height(mc_world_runtime *runtime, int32_t x, int32_t z,
                                                int32_t *out, mc_world_light_error *error);

/* Two shared immutable source Light identities on this class context, with
 * observed c=15/0. First active use constructs both handles; no World instance,
 * Registry/resource use or Random draw occurs. This finite native interface
 * does not implement generic Java Enum name/values/valueOf operations. Keep
 * the class context live through every returned view. Failure preserves out. */
mc_world_storage_status mc_world_runtime_light_type_get(mc_world_runtime_classes *classes,
                                                        mc_light_kind kind,
                                                        const mc_world_light_type **out,
                                                        mc_world_storage_error *error);
const mc_world_light_type_fields *
mc_world_runtime_light_type_fields_get(const mc_world_light_type *type);
const mc_world_light_region_fields *
mc_world_runtime_light_region_fields_get(const mc_world_light_region *region);

/* Passive actual C storage views, without class use or provider calls. A RAW
 * null C is reported as null identity/count0/capacity0. at requires a current
 * live index and preserves out on failure. Region pointers keep their identity
 * through list growth and in-place merging; removal by drain, clear, or final
 * free retires those borrowed pointers. Keep the World and its class live and grant exclusive
 * access. */
mc_world_storage_status mc_world_runtime_light_list_get(const mc_world_runtime *runtime,
                                                        mc_world_light_list_view *out,
                                                        mc_world_storage_error *error);
mc_world_storage_status mc_world_runtime_light_at(const mc_world_runtime *runtime, size_t index,
                                                  const mc_world_light_region **out,
                                                  mc_world_storage_error *error);

/* Actual full GAME region-submit body. type is an actual same-class handle or
 * NULL (a real nullable source reference). Native bounds are not normalized.
 * Read current Dimension.e before incrementing the same class-owned A. Then
 * use wrapped midpoint sums/truncating division, presence without an X/Z border
 * gate, the separate getter/blank predicate, and the last five actual C records
 * newest first. Same-type merges preserve the record identity. Otherwise
 * allocate/construct a distinct record219/234, then add237 to the same C list.
 *
 * Record and pointer-array requests use the World's direct native budget,
 * including old+new pointer-array peak during growth. An unpublished failed
 * record is retired; earlier shared effects remain. After append, a size over
 * one million reaches the actual diagnostic289 before clearing the same list296.
 * A failed native output operation preserves the appended list. Every guarded
 * exit decrements current A with Java wrapping, including failures; no-sky Sky
 * returns before increment. No propagation/drain or immediate feature callback
 * is substituted for the queue operation.
 *
 * Native admission0 changes only error. Source failures retain World.last_error
 * and error frames; ordinary out is unchanged. Success preserves prior World
 * diagnostics except reached getter effects. Hold the World for the full call.
 * As with height, reject COMPOSITION/RETIRED; reached RAW/failed fields are real.
 * Retired Worlds keep C storage until final free, which retires all records.
 * Keep arguments/out/error/owner storage disjoint and dependencies exclusive. */
mc_world_storage_status mc_world_runtime_light_submit(mc_world_runtime *runtime,
                                                      const mc_world_light_type *type,
                                                      const mc_light_bounds *bounds, bool coalesce,
                                                      mc_light_submit_result *out,
                                                      mc_world_light_error *error);

#endif
