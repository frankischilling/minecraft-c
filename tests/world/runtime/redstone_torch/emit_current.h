#ifndef DB_EMIT_CURRENT_H
#define DB_EMIT_CURRENT_H

#include "current_caller.h"

/* Read only live fixture owners and retained, heap-owned snapshot copies.
 * CURRENT physical identities remain native observations, not JDK topology. */
bool db_emit_worlds(FILE *, const db_fixture *);
bool db_emit_row(FILE *, const char *cohort, size_t index, const db_profile *, const db_fixture *,
                 const db_snapshot *before, const db_snapshot *after, const db_outcome *,
                 bool invoke_ok);
bool db_emit_finish(FILE *, const char *cohort, size_t rows, const db_fixture *);
/* Three known utility vectors; no game/class/RNG operation or output. */
bool db_emit_utility_controls(void);

#endif
