#ifndef MC_CLIENT_GUI_SOURCE_TEXT_FIELD_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_TEXT_FIELD_INTERNAL_H

#include "source_screen_internal.h"
#include "source_text_field.h"

struct mc_client_source_ro {
    mc_client_source_da *native_owner;
    mc_client_text_field_fields fields;
    mc_player_string *text_hold;
    mc_client_screen_phase phase;
    bool active, retired;
    mc_client_screen_site site;
    mc_client_screen_error failure;
    mc_client_font_mapping_error mapping;
    size_t tab_calls, clipboard_calls, text_stores;
};

#endif
