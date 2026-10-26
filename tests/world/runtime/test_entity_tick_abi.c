#include "mc/world/runtime/acquisition_error.h"
#include <stddef.h>
#include <stdio.h>

/* Exact pinned pre-move type, renamed solely for compile-time ABI comparison. */
typedef struct mc_world_runtime_acquisition_error_before {
    mc_chunk_acquire_status status;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_live_tile_entity_error tile_entity;
    mc_world_entity_batch_error batch;
    unsigned int activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
    mc_world_entity_load_error load_entity;
    mc_world_population_context population;
} mc_world_runtime_acquisition_error_before;

_Static_assert(sizeof(mc_world_runtime_acquisition_error) ==
                   sizeof(mc_world_runtime_acquisition_error_before),
               "acquisition sizeof");
_Static_assert(_Alignof(mc_world_runtime_acquisition_error) ==
                   _Alignof(mc_world_runtime_acquisition_error_before),
               "acquisition alignof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, status) ==
                   offsetof(mc_world_runtime_acquisition_error_before, status),
               "acquisition status offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, cause) ==
                   offsetof(mc_world_runtime_acquisition_error_before, cause),
               "acquisition cause offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, registry) ==
                   offsetof(mc_world_runtime_acquisition_error_before, registry),
               "acquisition registry offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, tile_entity) ==
                   offsetof(mc_world_runtime_acquisition_error_before, tile_entity),
               "acquisition tile_entity offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, batch) ==
                   offsetof(mc_world_runtime_acquisition_error_before, batch),
               "acquisition batch offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, activation_bytecode) ==
                   offsetof(mc_world_runtime_acquisition_error_before, activation_bytecode),
               "acquisition activation_bytecode offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, activation_world_bytecode) ==
                   offsetof(mc_world_runtime_acquisition_error_before, activation_world_bytecode),
               "acquisition activation_world_bytecode offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, load_bytecode) ==
                   offsetof(mc_world_runtime_acquisition_error_before, load_bytecode),
               "acquisition load_bytecode offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, load_builder_bytecode) ==
                   offsetof(mc_world_runtime_acquisition_error_before, load_builder_bytecode),
               "acquisition load_builder_bytecode offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, load_entity) ==
                   offsetof(mc_world_runtime_acquisition_error_before, load_entity),
               "acquisition load_entity offsetof");
_Static_assert(offsetof(mc_world_runtime_acquisition_error, population) ==
                   offsetof(mc_world_runtime_acquisition_error_before, population),
               "acquisition population offsetof");

int main(void) {
    return printf("World acquisition ABI: 13 checks\n") < 0 ? 1 : 0;
}
