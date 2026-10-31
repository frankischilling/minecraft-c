#ifndef MC_CLIENT_SETTINGS_CONSTRUCTION_INTERNAL_H
#define MC_CLIENT_SETTINGS_CONSTRUCTION_INTERNAL_H

#include "../../platform/text/stdout.h"
#include "mc/client/settings/construction.h"
#include "mc/client/settings/game_option.h"

#include <threads.h>

struct mc_game_settings_classes {
    mtx_t mutex;
    mc_game_settings_class_phase phase;
    size_t holds, array_bytes, arrays;
    mc_game_settings_class_options options;
    mc_game_settings_profile output_profile;
    mc_java_files *files;
    mc_world_info_name **tables[4], **pending_array;
    size_t pending_length;
    mc_world_info_name *options_literal;
    unsigned int last_store_pc;
    mc_text_stdout stdout_stream;
    bool stdout_captured, stdout_borrowed, native_stdout_failed;
    mc_java_writer *stdout_writer;
    mc_game_settings_source_error failure, retirement;
};

mc_game_settings_status settings_source_error(mc_game_settings_source_error *,
                                              mc_game_settings_status, const char *, const char *,
                                              const char *, const char *, int32_t);
mc_game_settings_status settings_class_options_name(mc_game_settings_classes *,
                                                    mc_world_info_name **,
                                                    mc_game_settings_source_error *);
mc_game_settings_status settings_class_emit(mc_game_settings_classes *,
                                            const mc_game_settings_diagnostic *,
                                            mc_game_settings_source_error *);
mc_game_option_status settings_class_value_key(mc_game_settings_classes *, mc_game_option_id,
                                               int32_t, mc_game_settings_text *);

#endif
