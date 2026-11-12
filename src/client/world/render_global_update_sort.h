#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_UPDATE_SORT_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_UPDATE_SORT_H

#include "mc/client/world/render_global.h"

/* JDK17 Arrays.sort's object TimSort decisions, including an inconsistent
 * comparator's observable calls/mutations and detected contract exception.
 * Array entries are borrowed canonical DK occurrences; no source owner is
 * created or retained. Keep array/context/occurrences exclusive through return.
 * Error is required, initialized once on entry, and may be the SAME diagnostic
 * that compare fills on failure. Comparator failure preserves it verbatim and
 * retains all completed array mutations, without merge completion/rollback.
 * Native temporary allocation/admission failures are distinct from Java OOME. */
bool mc_render_global_update_sort(mc_client_source_dk **array, size_t count, void *context,
                                  bool (*compare)(void *, mc_client_source_dk *,
                                                  mc_client_source_dk *, int *),
                                  mc_client_render_global_error *error);

#endif
