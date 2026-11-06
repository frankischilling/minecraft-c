#ifndef MC_WORLD_RUNTIME_MUTATION_PLANTS_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_PLANTS_INTERNAL_H

#include "../../../../block/registry/registry_internal.h"
#include "../mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

typedef struct mc_plant_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    const char *declaring_class, *method;
} mc_plant_frame;

mc_plant_frame mc_plant_frame_make(mc_world_runtime *, const mc_block *, mc_world_mutation_error *,
                                   const char *client_class, const char *server_class,
                                   const char *client_method, const char *server_method);
void mc_plant_at(const mc_plant_frame *, unsigned int pc);
bool mc_plant_done(const mc_plant_frame *);
bool mc_plant_fail(const mc_plant_frame *, unsigned int pc, const char *operation);
bool mc_plant_storage_fail(const mc_plant_frame *, unsigned int pc, mc_world_storage_error cause);
bool mc_plant_nested(const mc_plant_frame *, unsigned int pc, const mc_world_mutation_error *);
bool mc_plant_random(const mc_plant_frame *, unsigned int pc, mc_java_random *, int32_t bound,
                     int32_t *out);
bool mc_plant_block(const mc_plant_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                    int32_t *out);
bool mc_plant_metadata(const mc_plant_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                       int32_t *out);
bool mc_plant_air(const mc_plant_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                  bool *out);
bool mc_plant_survives(const mc_plant_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                       bool *out);
bool mc_plant_light(const mc_plant_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                    int32_t *out);
bool mc_plant_set(const mc_plant_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                  int32_t id);
bool mc_plant_set_metadata(const mc_plant_frame *, unsigned int pc, int32_t x, int32_t y, int32_t z,
                           int32_t metadata);
bool mc_plant_flower_survival(mc_world_runtime *, const mc_block *, int32_t x, int32_t y, int32_t z,
                              mc_world_mutation_error *);
bool mc_plant_upright_tick(mc_world_runtime *, const mc_block *, int32_t x, int32_t y, int32_t z,
                           mc_world_mutation_error *);
bool mc_plant_mushroom_tick(mc_world_runtime *, const mc_block *, int32_t x, int32_t y, int32_t z,
                            mc_java_random *, mc_world_mutation_error *);
bool mc_plant_crop_tick(mc_world_runtime *, const mc_block *, int32_t x, int32_t y, int32_t z,
                        mc_java_random *, mc_world_mutation_error *);
bool mc_plant_sapling_tick(mc_world_runtime *, const mc_block *, int32_t x, int32_t y, int32_t z,
                           mc_java_random *, mc_world_mutation_error *);
bool mc_plant_sapling_grow(mc_world_runtime *, const mc_block *, int32_t x, int32_t y, int32_t z,
                           mc_java_random *, mc_world_mutation_error *);
bool mc_plant_farmland_neighbor(mc_world_runtime *, const mc_block *, int32_t x, int32_t y,
                                int32_t z, int32_t source_id, mc_world_mutation_error *);
bool mc_plant_farmland_tick(mc_world_runtime *, const mc_block *, int32_t x, int32_t y, int32_t z,
                            mc_java_random *, mc_world_mutation_error *);
/* Separate vl/no.b(World,III,Entity)V step-on body. The a contact overload
 * inherits the empty base body and must not invoke this trampling operation. */
bool mc_plant_farmland_contact(mc_world_runtime *, const mc_block *, int32_t x, int32_t y,
                               int32_t z, mc_entity *, mc_world_mutation_error *);

#endif
