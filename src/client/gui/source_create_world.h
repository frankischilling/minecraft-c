#ifndef MC_CLIENT_GUI_SOURCE_CREATE_WORLD_H
#define MC_CLIENT_GUI_SOURCE_CREATE_WORLD_H

#include "mc/client/saves/save_format.h"
#include "source_screen.h"
#include "source_text_field.h"

typedef struct mc_client_source_fj mc_client_source_fj;

typedef struct mc_client_screen_fj_fields {
    mc_client_source_da *a;
    mc_client_source_ro *i, *j;
    mc_client_screen_text l;
    bool m;
} mc_client_screen_fj_fields;

typedef struct mc_client_create_world_state {
    mc_client_screen_phase phase;
    size_t holds, initialization_count, naming_calls, metadata_queries;
    mc_client_screen_fj_fields fields;
    const char *method;
    unsigned int last_pc;
    bool random_constructed, random_drawn, seed_parsed, seed_hashed;
    int64_t default_seed, selected_seed;
    const mc_client_screen_error *failure;
    const mc_client_save_error *save;
} mc_client_create_world_state;

/* Genuine FJ/DA identity. The exact parent is held through checked retirement;
 * initialization owns each reached RO/Button and preserves replaced prefixes.
 * The App, Font, resource and source class graphs remain live/exclusive through
 * all calls and cleanup. Native retirement never calls source h or restores
 * keyboard repeat. Normal callers use DA.a(App,II) and its actual FJ dispatch. */
mc_client_screen_status mc_client_screen_create_world_allocate(mc_client_screen_classes *,
                                                               const mc_client_screen_options *,
                                                               mc_client_source_fj **,
                                                               mc_client_screen_error *);
mc_client_screen_status mc_client_screen_create_world_construct(mc_client_source_fj *,
                                                                mc_client_source_da *parent,
                                                                mc_client_screen_error *);
mc_client_source_da *mc_client_screen_create_world_base(mc_client_source_fj *);
mc_client_screen_status mc_client_screen_create_world_initialize(mc_client_source_fj *,
                                                                 mc_client_screen_error *);
mc_client_screen_status mc_client_screen_create_world_inspect(mc_client_source_fj *,
                                                              mc_client_create_world_state *,
                                                              mc_client_screen_error *);
mc_client_screen_status mc_client_screen_create_world_release(mc_client_source_fj **,
                                                              mc_client_screen_error *);
mc_client_screen_status mc_client_screen_create_world_retire(mc_client_source_fj **,
                                                             mc_client_screen_error *);

#endif
