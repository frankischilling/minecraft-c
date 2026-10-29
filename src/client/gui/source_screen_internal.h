#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_INTERNAL_H
#include "source_screen.h"
#include <threads.h>

typedef struct mc_screen_block {
    struct mc_screen_block *next;
    void *memory;
} mc_screen_block;

typedef struct mc_screen_memory {
    mc_client_screen_options options;
    size_t bytes, allocations, attempts;
    mc_client_screen_status allocation_status;
    mc_screen_block *blocks;
} mc_screen_memory;

typedef enum mc_screen_list_kind {
    MC_SCREEN_LIST_BUTTONS,
    MC_SCREEN_LIST_PARTICLES,
    MC_SCREEN_LIST_STRINGS
} mc_screen_list_kind;

struct mc_client_screen_list {
    void **entries;
    size_t size, capacity;
    int32_t mod_count;
    mc_screen_list_kind kind;
    bool native_live;
};

typedef struct mc_screen_literal {
    struct mc_screen_literal *next;
    const char *ascii;
    mc_player_string *value;
} mc_screen_literal;

struct mc_client_screen_classes {
    mtx_t mutex;
    size_t holds;
    mc_java_random_profile *profile;
    const mc_registry_resources *resources;
    mc_registry_classes *registry;
    bool retiring, registry_held;
    mc_client_screen_class_phase phase;
    mc_java_random *a, *pending_random;
    mc_player_string *missingno, *splash_path;
    mc_screen_literal *literals;
    unsigned int last_pc, last_store_pc;
    mc_client_screen_error failure;
};

struct mc_client_source_da {
    mc_client_screen_classes *classes;
    mc_client_screen_phase phase;
    size_t holds;
    bool is_menu;
    mc_screen_memory memory;
    mc_client_screen_da_fields fields;
    unsigned int last_pc, store_count;
    mc_client_screen_site stores[16];
    mc_client_screen_error failure;
    mc_client_screen_initialization *initializations, *last_initialization;
    size_t initialization_count;
    bool native_field_occurrences_retired;
    mc_client_source_ke *selected_cleanup; /* Retained replaced/released a occurrence. */
};

typedef struct mc_screen_string_hold {
    struct mc_screen_string_hold *next;
    mc_player_string *value;
} mc_screen_string_hold;

typedef struct mc_screen_input_reader {
    mc_client_image_io_input bytes;
    bool constructed;
} mc_screen_input_reader;

typedef struct mc_screen_buffered_reader {
    mc_java_reader *reader;
    bool constructed;
} mc_screen_buffered_reader;

struct mc_client_source_fu {
    mc_client_source_da base;
    mc_client_screen_fu_fields fields;
    mc_client_screen_list *splashes;
    mc_screen_input_reader *input_reader;
    mc_screen_buffered_reader *buffered_reader;
    mc_client_class_stream *stream;
    mc_screen_string_hold *strings;
    mc_player_string *last_line, *last_trimmed;
    mc_client_screen_exception caught;
    unsigned int caught_pc;
    mc_client_screen_error caught_cause;
    size_t lines_read;
    mc_client_image_io_status input_status;
    mc_client_class_stream_status ready_status;
    mc_client_class_stream_error ready_error;
    mc_client_image_io_error input_error;
};

struct mc_client_source_du {
    mc_client_screen_classes *classes;
    mc_client_screen_phase phase;
    size_t holds;
    mc_screen_memory memory;
    float k;
    mc_client_screen_list *a;
    client_application *b;
    bool app_held, app_counted;
    unsigned int last_pc;
    mc_client_screen_error failure;
};

struct mc_client_source_ke {
    mc_client_screen_classes *classes;
    mc_client_screen_phase phase;
    size_t holds;
    mc_client_screen_ke_fields fields;
    unsigned int last_pc, store_count, stores[11];
    mc_client_screen_error failure;
};

mc_client_screen_status mc_screen_publish(const mc_client_screen_error *, mc_client_screen_error *);
mc_client_screen_status mc_screen_fail(mc_client_screen_error *, mc_client_screen_status,
                                       const char *, const char *, const char *, const char *,
                                       unsigned int);
bool mc_screen_lock(mc_client_screen_classes *, mc_client_screen_error *);
bool mc_screen_active_locked(mc_client_screen_classes *, mc_client_screen_error *);
bool mc_screen_class_final_locked(mc_client_screen_classes *, mc_client_screen_error *);
void mc_screen_class_drop_locked(mc_client_screen_classes *);
bool mc_screen_class_hold_locked(mc_client_screen_classes *, mc_client_screen_error *);
void mc_screen_memory_init(mc_screen_memory *, const mc_client_screen_options *, size_t);
void *mc_screen_allocate(mc_screen_memory *, size_t);
void mc_screen_memory_free(mc_screen_memory *);
void mc_screen_store(mc_client_source_da *, const char *, unsigned int);
bool mc_screen_base_construct_locked(mc_client_source_da *, mc_client_screen_error *);
bool mc_screen_menu_construct_locked(mc_client_source_fu *, mc_client_screen_error *);
bool mc_screen_menu_cleanup_locked(mc_client_source_fu *, mc_client_screen_error *);
void mc_screen_menu_strings_free(mc_client_source_fu *);

mc_client_source_du *mc_screen_particles_allocate_locked(mc_client_screen_classes *,
                                                         const mc_client_screen_options *,
                                                         mc_client_screen_error *);
bool mc_screen_particles_construct_locked(mc_client_source_du *, client_application *,
                                          mc_client_screen_error *);
bool mc_screen_particles_release_locked(mc_client_source_du **, mc_client_screen_error *);
mc_client_source_ke *mc_screen_button_allocate_locked(mc_client_screen_classes *,
                                                      mc_client_screen_error *);
bool mc_screen_button_construct_locked(mc_client_source_ke *, int32_t, int32_t, int32_t, int32_t,
                                       int32_t, mc_client_screen_text, mc_client_screen_error *);
bool mc_screen_button_hold_locked(mc_client_source_ke *, mc_client_screen_error *);
bool mc_screen_button_release_locked(mc_client_source_ke **, mc_client_screen_error *);
bool mc_screen_initializations_prepare_locked(mc_client_source_da *, mc_client_screen_error *);
bool mc_screen_initializations_release_apps_locked(mc_client_source_da *, mc_client_screen_error *);
bool mc_screen_literal_locked(mc_client_screen_classes *, const char *, mc_player_string **,
                              mc_client_screen_error *);
#endif
