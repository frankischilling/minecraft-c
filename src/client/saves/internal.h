#ifndef MC_CLIENT_SAVES_INTERNAL_H
#define MC_CLIENT_SAVES_INTERNAL_H
#include "mc/client/saves/save_format.h"

struct mc_client_save_formats {
    size_t holds;
    bool active, retiring;
    mc_java_files *files;
    mc_math_classes *math;
    mc_world_store_source_classes *handlers;
    mc_world_source_error handler_cleanup;
    mc_world_info_name *label, *current_name, *old_name;
    mc_client_save_options options;
};

struct mc_client_source_nl {
    mc_client_save_formats *classes;
    mc_client_save_format_state state;
    mc_world_source_error handler_failure, handler_cleanup;
};

struct mc_client_save_selection {
    size_t holds;
    bool active, retiring;
    mc_client_save_selection_fields fields;
};

struct mc_client_save_list {
    size_t holds, count, capacity, maximum, maximum_bytes;
    bool active, retiring;
    mc_client_save_selection **items;
};

mc_client_save_status save_result(mc_client_save_error *e, mc_client_save_status s, const char *op,
                                  const char *owner, const char *desc, int pc);
void save_push(mc_client_save_error *e, const char *owner, const char *desc, int pc);
mc_client_save_status save_file_error(mc_client_save_error *e, mc_java_file_status s,
                                      mc_java_file_error child, const char *owner, const char *desc,
                                      int pc);
mc_client_save_status save_ready(mc_client_save_format *f, mc_client_save_error *e);
bool save_pending(const mc_client_save_format *f);
mc_client_save_status save_clear(mc_client_save_format *f, mc_client_save_error *e);
mc_client_save_status save_handler_error(mc_client_save_format *, mc_world_source_status,
                                         const mc_world_source_error *, int,
                                         mc_client_save_error *);
mc_client_save_status save_literal(mc_client_save_formats *c, const char *ascii,
                                   mc_world_info_name **slot, mc_client_save_error *e);
mc_client_save_status save_clear_decode(mc_client_save_format *format, mc_client_save_error *error);
mc_client_save_status save_clear_read(mc_client_save_format *f, mc_client_save_error *e);
mc_client_save_status save_read_info(mc_client_save_format *f, mc_world_info_name *folder,
                                     mc_world_info **out, mc_client_save_error *e);
mc_client_save_status save_list_allocate(mc_client_save_format *f, mc_client_save_error *e);
mc_client_save_status save_list_append(mc_client_save_list *list, mc_client_save_selection *s,
                                       mc_client_save_error *e);
#endif
