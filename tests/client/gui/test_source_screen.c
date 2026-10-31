/* Authored constructor/resource controls; original menu parity is separate. */
#include "client/application_state.h"
#include "client/gui/source_screen_internal.h"
#include "mc/resources/files/files.h"
#include "random_profile_internal.h"

#include <errno.h>
#include <fenv.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static unsigned int checks;
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "screen check%u line%d: %s\n", checks, __LINE__, #expression);         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
#define UNIQUE_MULTIPLIER UINT64_C(1181783497276652981)

static uint64_t unique_value(mc_java_random_profile *p) {
    return atomic_load_explicit(&p->uniquifier, memory_order_seq_cst);
}

static uint32_t float_bits(float f) {
    uint32_t v;
    memcpy(&v, &f, sizeof(v));
    return v;
}

static int constructors(void) {
    mc_java_random_profile *profile = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_screen_error e = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const uint64_t before = unique_value(profile);
    CHECK(mc_client_screen_classes_create(profile, NULL, NULL, &classes, &e) ==
          MC_CLIENT_SCREEN_OK);
    mc_client_screen_class_state cs;
    CHECK(mc_client_screen_classes_inspect(classes, &cs, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(cs.phase == MC_CLIENT_SCREEN_CLASS_COLD && cs.a == NULL && cs.pending_random == NULL &&
          unique_value(profile) == before);
    mc_client_source_da *base = NULL;
    CHECK(mc_client_screen_allocate(classes, NULL, &base, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_construct(base, &e) == MC_CLIENT_SCREEN_OK && e.source_returned);
    mc_client_screen_state state;
    size_t n = SIZE_MAX;
    CHECK(mc_client_screen_inspect(base, &state, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(!state.is_menu && state.phase == MC_CLIENT_SCREEN_READY && state.last_pc == 25 &&
          state.store_count == 4);
    CHECK(float_bits(state.da.k) == 0 && state.da.b == NULL && state.da.c == 0 && state.da.d == 0 &&
          !state.da.f && state.da.a == NULL && state.da.g == NULL && state.da.h == NULL);
    CHECK(mc_client_screen_list_size(state.da.e, &n) && n == 0 && unique_value(profile) == before);
    const unsigned int da_stores[] = {6, 12, 17, 22};
    for (size_t i = 0; i < sizeof(da_stores) / sizeof(da_stores[0]); ++i)
        CHECK(state.stores[i].pc == da_stores[i]);
    CHECK(mc_client_screen_construct(base, &e) == MC_CLIENT_SCREEN_INVALID_PHASE);
    mc_client_source_da *held = base;
    CHECK(mc_client_screen_retain(held, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_retire(&base, &e) == MC_CLIENT_SCREEN_BUSY && base == held);
    CHECK(mc_client_screen_inspect(base, &state, &e) == MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_READY && state.holds == 2);
    CHECK(mc_client_screen_release(&held, &e) == MC_CLIENT_SCREEN_OK && held == NULL);
    CHECK(mc_client_screen_retire(&base, &e) == MC_CLIENT_SCREEN_OK && base == NULL);

    client_application raw = {0};
    mc_client_source_du *particles = NULL;
    CHECK(mc_client_screen_particles_allocate(classes, NULL, &particles, &e) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_particles_construct(particles, &raw, &e) == MC_CLIENT_SCREEN_OK &&
          e.source_returned);
    mc_client_screen_du_state ds;
    CHECK(mc_client_screen_particles_inspect(particles, &ds, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(ds.phase == MC_CLIENT_SCREEN_READY && ds.b == &raw && float_bits(ds.k) == 0 &&
          ds.last_pc == 20 && mc_client_screen_list_size(ds.a, &n) && n == 0);
    mc_client_source_du *du_held = particles;
    CHECK(mc_client_screen_particles_retain(du_held, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_particles_retire(&particles, &e) == MC_CLIENT_SCREEN_BUSY &&
          particles == du_held);
    CHECK(mc_client_screen_particles_release(&du_held, &e) == MC_CLIENT_SCREEN_OK &&
          du_held == NULL);
    CHECK(mc_client_screen_particles_retire(&particles, &e) == MC_CLIENT_SCREEN_OK &&
          particles == NULL);
    raw.constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_screen_particles_allocate(classes, NULL, &particles, &e) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_particles_construct(particles, &raw, &e) ==
              MC_CLIENT_SCREEN_INVALID_PHASE &&
          e.site.pc == 17);
    CHECK(mc_client_screen_particles_inspect(particles, &ds, &e) == MC_CLIENT_SCREEN_OK &&
          ds.a != NULL && ds.b == NULL && ds.phase == MC_CLIENT_SCREEN_FAILED);
    CHECK(mc_client_screen_particles_retire(&particles, &e) == MC_CLIENT_SCREEN_OK);

    const uint16_t units[] = {'A', 0, 0xd800u};
    mc_player_string *label = NULL;
    CHECK(mc_player_string_create(units, 3, &label) == MC_PLAYER_OK);
    mc_client_source_ke *button = NULL;
    CHECK(mc_client_screen_button_allocate(classes, &button, &e) == MC_CLIENT_SCREEN_OK);
    const mc_client_screen_text text = {MC_CLIENT_SCREEN_TEXT_PLAYER, label, NULL};
    CHECK(mc_client_screen_button_construct(button, -7, INT32_MIN, INT32_MAX, -2, 0, text, &e) ==
              MC_CLIENT_SCREEN_OK &&
          e.source_returned);
    mc_player_string_release(label); /* The actual label occurrence is retained. */
    mc_client_screen_ke_state bs;
    CHECK(mc_client_screen_button_inspect(button, &bs, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(bs.phase == MC_CLIENT_SCREEN_READY && bs.last_pc == 60 && bs.store_count == 11 &&
          bs.fields.a == -2 && bs.fields.b == 0 && bs.fields.c == INT32_MIN &&
          bs.fields.d == INT32_MAX && bs.fields.f == -7 && bs.fields.g && bs.fields.h &&
          float_bits(bs.fields.k) == 0 && bs.fields.e.player == label);
    mc_player_string_view view;
    CHECK(mc_client_screen_text_read(bs.fields.e, &view) && view.length == 3 &&
          memcmp(view.units, units, sizeof(units)) == 0);
    const unsigned int button_stores[] = {6, 8, 14, 19, 24, 29, 34, 39, 45, 51, 57};
    for (size_t i = 0; i < sizeof(button_stores) / sizeof(button_stores[0]); ++i)
        CHECK(bs.stores[i] == button_stores[i]);
    mc_client_source_ke *button_held = button;
    CHECK(mc_client_screen_button_retain(button_held, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_button_retire(&button, &e) == MC_CLIENT_SCREEN_BUSY &&
          button == button_held);
    CHECK(mc_client_screen_button_release(&button_held, &e) == MC_CLIENT_SCREEN_OK &&
          button_held == NULL);
    CHECK(mc_client_screen_button_retire(&button, &e) == MC_CLIENT_SCREEN_OK && button == NULL);
    CHECK(mc_client_screen_button_allocate(classes, &button, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_button_construct_default(button, 1, 2, 3, (mc_client_screen_text){0},
                                                    &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_button_inspect(button, &bs, &e) == MC_CLIENT_SCREEN_OK &&
          bs.fields.a == 200 && bs.fields.b == 20 &&
          bs.fields.e.kind == MC_CLIENT_SCREEN_TEXT_NULL);
    CHECK(mc_client_screen_button_retire(&button, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(unique_value(profile) == before);
    CHECK(mc_client_screen_classes_release(&classes, &e) == MC_CLIENT_SCREEN_OK && classes == NULL);
    mc_java_random_profile_free(profile);
    return 0;
}

static int class_failure(void) {
    mc_java_random_profile *profile = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_screen_error e = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_client_screen_classes_create(profile, NULL, NULL, &classes, &e) ==
          MC_CLIENT_SCREEN_OK);
    const uint64_t before = unique_value(profile);
    const int rounding = fegetround();
    CHECK(rounding != -1 && fesetround(FE_DOWNWARD) == 0);
    const mc_client_screen_status first = mc_client_screen_menu_active_use(classes, &e);
    CHECK(fesetround(rounding) == 0); /* Restore before interpreting the failure. */
    CHECK(first == MC_CLIENT_SCREEN_RANDOM_FAILED &&
          e.random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP && e.site.pc == 4);
    CHECK(unique_value(profile) == before);
    mc_client_screen_class_state cs;
    CHECK(mc_client_screen_classes_inspect(classes, &cs, &e) == MC_CLIENT_SCREEN_OK &&
          cs.phase == MC_CLIENT_SCREEN_CLASS_FAILED && cs.a == NULL && cs.pending_random != NULL);
    CHECK(mc_client_screen_menu_active_use(classes, &e) == MC_CLIENT_SCREEN_CLASS_ERRONEOUS &&
          e.first_class_status == MC_CLIENT_SCREEN_RANDOM_FAILED && e.first_class_site.pc == 4 &&
          e.first_class_random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP &&
          unique_value(profile) == before);
    CHECK(mc_client_screen_classes_release(&classes, &e) == MC_CLIENT_SCREEN_OK);
    mc_java_random_profile_free(profile);
    return 0;
}

typedef struct forwarding {
    const mc_registry_resources *real;
    mc_registry_resources source;
    mc_client_screen_classes *classes;
    mc_client_source_fu **menu_slot;
    mc_client_class_stream **stream_slot;
    bool preclose, fail_close_once, saw_busy, ready_reentry;
    unsigned int opens, closes, available_calls;
} forwarding;

static mc_registry_status forward_open(void *context, const char *name,
                                       mc_java_properties_input *out) {
    forwarding *f = context;
    ++f->opens;
    const mc_registry_status s = f->real->open(f->real->context, name, out);
    if (s == MC_REGISTRY_OK && f->preclose)
        return f->real->close(f->real->context, out);
    return s;
}

static mc_registry_status forward_close(void *context, const mc_java_properties_input *input) {
    forwarding *f = context;
    ++f->closes;
    if (f->menu_slot != NULL && *f->menu_slot != NULL) {
        mc_client_source_fu *saved = *f->menu_slot;
        mc_client_screen_error e = {0};
        f->saw_busy = mc_client_screen_menu_retire(f->menu_slot, &e) == MC_CLIENT_SCREEN_BUSY &&
                      *f->menu_slot == saved;
    }
    const mc_registry_status s = f->real->close(f->real->context, input);
    if (s == MC_REGISTRY_OK && f->fail_close_once) {
        f->fail_close_once = false;
        return MC_REGISTRY_IO_FAILED;
    }
    return s;
}

static bool forward_ready(void *context, const mc_java_properties_input *input) {
    forwarding *f = context;
    ++f->available_calls;
    if (f->stream_slot != NULL && *f->stream_slot != NULL) {
        mc_client_class_stream *saved = *f->stream_slot;
        mc_client_class_stream_error e = {0};
        bool unchanged = true;
        const mc_client_class_stream_status s = mc_client_class_stream_ready(saved, &unchanged, &e);
        const mc_client_class_stream_status close =
            mc_client_class_stream_retire(f->stream_slot, &e);
        f->ready_reentry = s == MC_CLIENT_CLASS_STREAM_BUSY && unchanged &&
                           close == MC_CLIENT_CLASS_STREAM_BUSY && *f->stream_slot == saved;
    }
    return f->real->ready != NULL && f->real->ready(f->real->context, input);
}

static int write_splashes(const char *root, const unsigned char *bytes, size_t size) {
    char directory[4096], path[4096];
    int n = snprintf(directory, sizeof(directory), "%s/title", root);
    CHECK(n > 0 && (size_t)n < sizeof(directory));
#ifdef _WIN32
    const int made = _mkdir(directory);
#else
    const int made = mkdir(directory, 0700);
#endif
    CHECK(made == 0 || errno == EEXIST);
    n = snprintf(path, sizeof(path), "%s/splashes.txt", directory);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    FILE *file = NULL;
#ifdef _WIN32
    CHECK(fopen_s(&file, path, "wb") == 0 && file != NULL);
#else
    file = fopen(path, "wb");
    CHECK(file != NULL);
#endif
    const size_t written = fwrite(bytes, 1, size, file);
    const int closed = fclose(file);
    CHECK(written == size && closed == 0);
    return 0;
}

static int resource_constructors(const char *root) {
    mc_java_random_profile *profile = NULL;
    mc_resource_files *files = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_screen_error e = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_resource_files_create(root, &files) == MC_REGISTRY_OK);
    forwarding f = {0};
    f.real = mc_resource_files_source(files);
    f.source = (mc_registry_resources){&f, forward_open, forward_close, forward_ready};
    CHECK(mc_client_screen_classes_create(profile, &f.source, NULL, &classes, &e) ==
          MC_CLIENT_SCREEN_OK);
    f.classes = classes;
    const uint64_t before = unique_value(profile);
    mc_client_source_fu *menu = NULL;
    CHECK(mc_client_screen_menu_allocate(classes, NULL, &menu, &e) == MC_CLIENT_SCREEN_OK &&
          unique_value(profile) == before * UNIQUE_MULTIPLIER);
    mc_client_screen_class_state ready_class;
    CHECK(mc_client_screen_classes_inspect(classes, &ready_class, &e) == MC_CLIENT_SCREEN_OK &&
          ready_class.phase == MC_CLIENT_SCREEN_CLASS_READY && ready_class.a != NULL &&
          ready_class.pending_random == NULL);
    CHECK(mc_client_screen_menu_construct(menu, &e) == MC_CLIENT_SCREEN_OK && e.source_returned);
    mc_client_screen_state state;
    CHECK(mc_client_screen_inspect(mc_client_screen_menu_base(menu), &state, &e) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(state.is_menu && state.phase == MC_CLIENT_SCREEN_READY &&
          state.caught_exception == MC_CLIENT_SCREEN_NULL_STREAM_EXCEPTION &&
          state.caught_pc == 43 && state.open_stream == NULL && !state.input_reader_constructed &&
          !state.buffered_reader_constructed && state.store_count == 6);
    mc_player_string_view view;
    CHECK(mc_player_string_read(state.fu.j, &view) && view.length == 9 && view.units[0] == 'm' &&
          float_bits(state.fu.i) == 0 && state.fu.l == NULL);
    const mc_player_string *missingno = state.fu.j;
    CHECK(mc_client_screen_menu_retire(&menu, &e) == MC_CLIENT_SCREEN_OK && menu == NULL &&
          f.closes == 0);
    static const unsigned char normal[] = "  alpha \t\r\n\r\n \t\n beta \xf0\x9f\x98\x80\ngamma\n";
    CHECK(write_splashes(root, normal, sizeof(normal) - 1u) == 0);
    CHECK(mc_client_screen_menu_allocate(classes, NULL, &menu, &e) == MC_CLIENT_SCREEN_OK &&
          unique_value(profile) == before * UNIQUE_MULTIPLIER);
    mc_java_random expected_random = *ready_class.a;
    int32_t expected_index = -1;
    CHECK(mc_java_random_next_bounded(&expected_random, 3, &expected_index));
    CHECK(mc_client_screen_menu_construct(menu, &e) == MC_CLIENT_SCREEN_OK && e.source_returned);
    CHECK(mc_client_screen_inspect(mc_client_screen_menu_base(menu), &state, &e) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(state.caught_exception == MC_CLIENT_SCREEN_NO_EXCEPTION && state.splash_count == 3 &&
          state.open_stream != NULL && state.input_reader_constructed &&
          state.buffered_reader_constructed && state.store_count == 7 &&
          state.stores[6].pc == 107 && state.fu.j != missingno && f.closes == 0 &&
          f.available_calls != 0);
    CHECK(state.fu.j == menu->splashes->entries[(size_t)expected_index] &&
          ready_class.a->state == expected_random.state);
    CHECK(mc_player_string_read(state.fu.j, &view) && view.length != 0);
    size_t gamma_identities = 0;
    for (mc_screen_string_hold *h = menu->strings; h != NULL; h = h->next) {
        mc_player_string_view value;
        if (mc_player_string_read(h->value, &value) && value.length == 5 && value.units[0] == 'g') {
            ++gamma_identities;
            CHECK(h->value == menu->splashes->entries[2]);
        }
    }
    CHECK(gamma_identities == 1 && state.last_trimmed == menu->splashes->entries[2]);
    f.menu_slot = &menu;
    f.fail_close_once = true;
    mc_client_source_fu *saved = menu;
    CHECK(mc_client_screen_menu_retire(&menu, &e) == MC_CLIENT_SCREEN_STREAM_FAILED &&
          menu == saved && f.saw_busy && e.stream.close == MC_REGISTRY_IO_FAILED);
    CHECK(mc_client_screen_inspect(mc_client_screen_menu_base(menu), &state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          state.phase == MC_CLIENT_SCREEN_RETIRING && state.fu.j != NULL);
    CHECK(mc_client_screen_menu_retire(&menu, &e) == MC_CLIENT_SCREEN_OK && menu == NULL &&
          f.closes == 2 && f.saw_busy);
    f.menu_slot = NULL;
    static const unsigned char blank[] = "\r\n \t\n";
    CHECK(write_splashes(root, blank, sizeof(blank) - 1u) == 0);
    mc_client_screen_class_state cs;
    CHECK(mc_client_screen_classes_inspect(classes, &cs, &e) == MC_CLIENT_SCREEN_OK);
    const mc_java_random before_empty = *cs.a;
    CHECK(mc_client_screen_menu_allocate(classes, NULL, &menu, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_menu_construct(menu, &e) == MC_CLIENT_SCREEN_OK && e.source_returned);
    CHECK(mc_client_screen_inspect(mc_client_screen_menu_base(menu), &state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          state.caught_exception == MC_CLIENT_SCREEN_EMPTY_SPLASH_EXCEPTION &&
          state.caught_pc == 96 && state.fu.j == missingno && state.splash_count == 0 &&
          cs.a->state == before_empty.state);
    CHECK(mc_client_screen_menu_retire(&menu, &e) == MC_CLIENT_SCREEN_OK);
    f.preclose = true;
    CHECK(mc_client_screen_menu_allocate(classes, NULL, &menu, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_menu_construct(menu, &e) == MC_CLIENT_SCREEN_OK && e.source_returned);
    CHECK(mc_client_screen_inspect(mc_client_screen_menu_base(menu), &state, &e) ==
              MC_CLIENT_SCREEN_OK &&
          state.caught_exception == MC_CLIENT_SCREEN_READ_IO_EXCEPTION && state.caught_pc == 54 &&
          state.caught_cause.input.status == MC_CLIENT_IMAGE_IO_SOURCE_IO &&
          state.fu.j == missingno);
    CHECK(mc_client_screen_menu_retire(&menu, &e) == MC_CLIENT_SCREEN_OK);
    f.preclose = false;

    mc_client_class_stream *stream = NULL;
    CHECK(mc_client_class_stream_open(&f.source, cs.splash_path, 512, 1024 * 1024, &stream, NULL) ==
              MC_CLIENT_CLASS_STREAM_OK &&
          stream != NULL);
    f.stream_slot = &stream;
    bool ready = false;
    CHECK(mc_client_class_stream_ready(stream, &ready, NULL) == MC_CLIENT_CLASS_STREAM_OK &&
          ready && f.ready_reentry);
    f.stream_slot = NULL;
    CHECK(mc_client_class_stream_retire(&stream, NULL) == MC_CLIENT_CLASS_STREAM_OK &&
          stream == NULL);
    bool unchanged = true;
    CHECK(mc_client_class_stream_ready(NULL, &unchanged, NULL) ==
              MC_CLIENT_CLASS_STREAM_INVALID_ARGUMENT &&
          unchanged);
    for (size_t quota = 0; quota < 16; ++quota) {
        const mc_client_screen_options options = {1024 * 1024, quota, 1024 * 1024, 4096, 512};
        CHECK(mc_client_screen_menu_allocate(classes, &options, &menu, &e) == MC_CLIENT_SCREEN_OK);
        const mc_client_screen_status result = mc_client_screen_menu_construct(menu, &e);
        CHECK(result == MC_CLIENT_SCREEN_OK || result == MC_CLIENT_SCREEN_LIMIT ||
              result == MC_CLIENT_SCREEN_READER_FAILED);
        CHECK(mc_client_screen_inspect(mc_client_screen_menu_base(menu), &state, &e) ==
              MC_CLIENT_SCREEN_OK);
        CHECK((result == MC_CLIENT_SCREEN_OK) == (state.phase == MC_CLIENT_SCREEN_READY));
        CHECK(mc_client_screen_menu_construct(menu, &e) == MC_CLIENT_SCREEN_INVALID_PHASE);
        CHECK(mc_client_screen_menu_retire(&menu, &e) == MC_CLIENT_SCREEN_OK && menu == NULL);
    }
    CHECK(unique_value(profile) == before * UNIQUE_MULTIPLIER);
    CHECK(mc_client_screen_classes_release(&classes, &e) == MC_CLIENT_SCREEN_OK && classes == NULL);
    CHECK(mc_resource_files_free(files) == MC_REGISTRY_OK);
    mc_java_random_profile_free(profile);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        if (constructors() != 0 || class_failure() != 0)
            return 1;
    } else if (argc == 3 && strcmp(argv[1], "resource") == 0) {
        /* Caller supplies a fresh owned empty root, never imported user files. */
        if (resource_constructors(argv[2]) != 0)
            return 1;
    } else
        return 2;
    printf("Source screen controls passed (%u checks)\n", checks);
    return 0;
}
