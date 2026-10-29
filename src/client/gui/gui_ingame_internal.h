#ifndef MC_CLIENT_GUI_GUI_INGAME_INTERNAL_H
#define MC_CLIENT_GUI_GUI_INGAME_INTERNAL_H

#include "mc/client/gui/gui_ingame.h"
#include <threads.h>

struct mc_client_gui_ingame_list {
    void **entries;
    size_t size, capacity;
    int32_t mod_count; /* inherited AbstractList default0, supplied by existing calloc */
};

struct mc_client_gui_ingame_classes {
    mtx_t mutex;
    size_t holds, static_item_bytes;
    mc_client_render_classes *models;
    mc_java_random_profile *profile; /* Borrowed, same profile retained by the model scope. */
    mc_client_gui_ingame_class_phase phase;
    bool ub_initialized, retiring, models_held, static_item_retired;
    unsigned int last_pc, last_store_pc;
    mc_client_item_renderer *d, *item;
    mc_client_gui_ingame_error failure;
};

struct mc_client_source_uq {
    mc_client_gui_ingame_classes *classes;
    mc_client_gui_ingame_phase phase;
    mc_client_gui_ingame_options options;
    mc_client_gui_ingame_fields fields;
    mc_java_random *pending_random;
    size_t holds, bytes, allocations, attempts;
    unsigned int last_pc, store_count;
    mc_client_gui_ingame_site stores[10];
    mc_client_gui_ingame_error failure;
};

#endif
