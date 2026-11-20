#ifndef MC_ENTITY_ARROW_REFERENCES_INTERNAL_H
#define MC_ENTITY_ARROW_REFERENCES_INTERNAL_H

#include "mc/entity/entity.h"

/* Actual nullable SL.c:Lls; and its separate native owning occurrence. Valid
 * in every phase; no health, READY or World-equality gate is inserted. Reads
 * prove source/hold provenance before borrowing. Stores retain the genuine
 * same-namespace Living replacement before publishing and releasing the old
 * occurrence. Same-value stores remain no-ops after provenance validation.
 * The caller keeps owners/arguments/errors live, exclusive and disjoint. */
mc_entity_status mc_arrow_shooter_get(mc_entity *, mc_entity **, mc_entity_error *);
mc_entity_status mc_arrow_shooter_store(mc_entity *, mc_entity *, mc_entity_error *);
/* Infallible final destruction or previously validated joint terminal commit
 * only. No source call/error cleanup may clear a surviving owner's source c. */
void mc_arrow_references_dispose(mc_entity *);

#endif
