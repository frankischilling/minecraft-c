/* Authored native ownership controls, not an original YC/WorldAccess packet.
 * Reuse the genuine loaded client World fixture. The source receiver below
 * removes its own actual registration and owns each immutable failure receipt;
 * no YC, Tile, World or receipt hold/phase field is repaired by the test. */
int mc_sign_dirty_unused_publication_main(int, char **);
#define main mc_sign_dirty_unused_publication_main
#include "../../world/runtime/test_entity_publication.c"
#undef main

#include "client/gui/display_screen.h"
#include "client/gui/source_sign_screen.h"
#include "world/runtime/world_access_internal.h"

typedef struct ycd_font {
    size_t cursor;
    mc_registry_resources api;
} ycd_font;

static const uint8_t ycd_font_bytes[] = "# authored YC ownership fixture\nA \xc3\xa9~\n";

static mc_java_properties_status ycd_read(void *context, uint8_t *out, size_t capacity,
                                          size_t *count) {
    ycd_font *font = context;
    size_t n = sizeof(ycd_font_bytes) - 1 - font->cursor;
    if (n > capacity)
        n = capacity;
    memcpy(out, ycd_font_bytes + font->cursor, n);
    font->cursor += n;
    *count = n;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status ycd_open(void *context, const char *path, mc_java_properties_input *out) {
    ycd_font *font = context;
    if (strcmp(path, "/font.txt") != 0)
        return MC_REGISTRY_NULL_REFERENCE;
    font->cursor = 0;
    *out = (mc_java_properties_input){font, ycd_read};
    return MC_REGISTRY_OK;
}

static mc_registry_status ycd_close(void *context, const mc_java_properties_input *input) {
    return input->context == context ? MC_REGISTRY_OK : MC_REGISTRY_INVALID_ARGUMENT;
}

static bool ycd_ready(void *context, const mc_java_properties_input *input) {
    ycd_font *font = context;
    return input->context == context && font->cursor < sizeof(ycd_font_bytes) - 1;
}

typedef struct ycd_observer {
    mc_world_access *self;
    mc_world_access_receipt *receipts[3];
    mc_world_runtime *world;
    mc_tile_entity *tile;
    mc_chunk_state *chunk;
    size_t calls;
    bool observations_ok, source_exception;
} ycd_observer;

static mc_world_storage_status ycd_dirty(void *context, mc_world_runtime *world, int32_t x,
                                         int32_t y, int32_t z, mc_tile_entity *tile,
                                         mc_world_storage_error *cause,
                                         mc_world_access_failure *failure) {
    ycd_observer *observer = context;
    const size_t index = observer->calls++;
    observer->observations_ok &= world == observer->world && tile == observer->tile && x == 0 &&
                                 y == 64 && z == 0 && observer->chunk->dirty &&
                                 index < COUNT(observer->receipts);
    if (index >= COUNT(observer->receipts)) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "YC authored observer call bound", 92};
        return cause->status;
    }
    bool removed = false;
    const mc_world_storage_status removed_status =
        mc_world_runtime_remove_world_access(world, observer->self, &removed, cause);
    observer->observations_ok &=
        removed_status == MC_WORLD_STORAGE_OK && removed &&
        mc_world_access_free(observer->self, NULL) == MC_WORLD_STORAGE_BUSY;
    if (removed_status != MC_WORLD_STORAGE_OK || !removed)
        return removed_status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_INVALID_DATA
                                                     : removed_status;
    const mc_world_storage_status receipt_status = mc_world_access_receipt_create(
        observer->self, NULL, &observer->receipts[index], failure, cause);
    if (receipt_status != MC_WORLD_STORAGE_OK)
        return receipt_status;
    /* This authored producer chooses its exact Throwable class; YC must use
     * this invocation receipt, rather than map a native status to a class. */
    observer->receipts[index]->exception_class =
        observer->source_exception ? "java/lang/IllegalStateException" : NULL;
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "YC authored dirty receiver failure", 91};
    return cause->status;
}

int main(int argc, char **argv) {
    bool ok = false;
    char root[8192];
    fixture *f = NULL;
    mc_client_screen_classes *screens = NULL;
    mc_client_font_mapping *mapping = NULL;
    mc_player_string *empty = NULL;
    mc_tile_entity *tile = NULL;
    mc_client_source_yc *yc = NULL;
    mc_client_source_da *external = NULL;
    mc_client_source_ke *button = NULL;
    mc_client_display_screen_call *display = NULL;
    mc_client_screen_error error = {0};
    mc_client_sign_screen_state state = {0};
    ycd_font font = {0};
    ycd_observer observer = {.observations_ok = true, .source_exception = true};
    const uint16_t title[] = {'Y', 'C', ' ', 'd', 'i', 'r', 't', 'y'};
    if (argc != 3 || strcmp(argv[1], "world-sign-screen-dirty") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = (stream){.bytes = language};
    r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options registry = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&registry, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          open_store(f, 0, root) && create_world(f, 0));
    CHECK(!f->states[0]->dirty &&
          mc_tile_entity_allocate(f->worlds[0], MC_TILE_ENTITY_SIGN, &tile) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_bind_world(tile, f->worlds[0]) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_set_coordinates(tile, 0, 64, 0) == MC_TILE_ENTITY_OK);
    font.api = (mc_registry_resources){&font, ycd_open, ycd_close, ycd_ready};
    CHECK(mc_client_screen_classes_create(f->profile, &r->api, f->registry[0], &screens, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          mc_player_string_create(NULL, 0, &empty) == MC_PLAYER_OK &&
          mc_client_font_mapping_create(&font.api, empty, SIZE_MAX, SIZE_MAX, &mapping) ==
              MC_CLIENT_FONT_MAPPING_OK);
    const mc_client_sign_screen_dependencies dependencies = {.mapping = mapping,
                                                             .literal_classes = f->classes[0]};
    CHECK(mc_client_screen_sign_allocate(screens, &dependencies, NULL, &yc, NULL) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_construct(yc, tile, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_button_allocate(screens, &button, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_button_construct_default(
              button, 0, 0, 0,
              (mc_client_screen_text){.kind = MC_CLIENT_SCREEN_TEXT_PLAYER, .player = empty},
              NULL) == MC_CLIENT_SCREEN_OK);
    observer.world = f->worlds[0];
    observer.tile = tile;
    observer.chunk = f->states[0];
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = ycd_dirty};
    CHECK(mc_world_access_source_create(&callbacks, &observer, &observer.self, NULL) ==
          MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 3; ++i) {
        observer.source_exception = i != 1;
        CHECK(mc_world_runtime_add_world_access(f->worlds[0], observer.self, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_client_screen_sign_button(yc, button, &display, &error) ==
                  MC_CLIENT_SCREEN_APP_FAILED &&
              display == NULL && error.sign != NULL && error.site.pc == 19 &&
              strcmp(error.site.class_name, "yc") == 0 &&
              error.source_exception == observer.source_exception &&
              error.native_policy != observer.source_exception && !error.source_error &&
              error.sign->dirty.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              error.sign->dirty.bytecode == 55 && error.sign->dirty.world_access_bytecode == 55 &&
              error.sign->dirty.tile_entity_bytecode == 24 &&
              error.sign->dirty.cause.system_error == 91 &&
              error.sign->dirty.world_access_failure.receipt == observer.receipts[i] &&
              !error.sign->dirty_returned && !error.sign->display_entered &&
              observer.calls == i + 1 && observer.observations_ok && f->states[0]->dirty);
        if (observer.source_exception)
            CHECK(error.exception_class != NULL &&
                  strcmp(error.exception_class, "java/lang/IllegalStateException") == 0);
        else
            CHECK(error.exception_class == NULL);
        for (size_t j = 0; j <= i; ++j)
            CHECK(observer.receipts[j]->holds == 2 &&
                  mc_world_access_receipt_retire(&observer.receipts[j], NULL) ==
                      MC_WORLD_STORAGE_BUSY);
        if (i == 0) {
            /* A later dirty return overwrites the public detail, but cannot
             * release the earlier invocation's retained producer. No App is
             * supplied, so this genuine successful dirty stops at YC27. */
            CHECK(mc_client_screen_sign_button(yc, button, &display, &error) ==
                      MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
                  display == NULL && error.site.pc == 27 && error.source_exception &&
                  !error.native_policy && error.sign->dirty_returned &&
                  error.sign->dirty.world_access_failure.receipt == NULL && observer.calls == 1 &&
                  observer.receipts[0]->holds == 2 &&
                  mc_world_access_receipt_retire(&observer.receipts[0], NULL) ==
                      MC_WORLD_STORAGE_BUSY);
        }
    }
    external = mc_client_screen_sign_base(yc);
    CHECK(mc_client_screen_retain(external, NULL) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_terminal_prepare(yc, &error) == MC_CLIENT_SCREEN_BUSY &&
          mc_client_screen_sign_inspect(yc, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.detail->dirty.world_access_failure.receipt == observer.receipts[2] &&
          state.failure->site.pc == 19 && state.failure->source_exception &&
          !state.failure->native_policy);
    for (size_t i = 0; i < COUNT(observer.receipts); ++i)
        CHECK(observer.receipts[i]->holds == 2 &&
              mc_world_access_receipt_retire(&observer.receipts[i], NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_client_screen_release(&external, NULL) == MC_CLIENT_SCREEN_OK && external == NULL &&
          mc_client_screen_sign_terminal_prepare(yc, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_inspect(yc, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.terminal_prepared && !state.tile_held && state.tile == NULL &&
          state.detail->dirty.world_access_failure.receiver == NULL &&
          state.detail->dirty.world_access_failure.render_global == NULL &&
          state.detail->dirty.world_access_failure.receipt == NULL &&
          state.detail->dirty.bytecode == 55 && state.detail->dirty.tile_entity_bytecode == 24 &&
          state.detail->dirty.cause.system_error == 91 && state.failure->site.pc == 19 &&
          state.failure->source_exception && !state.failure->native_policy &&
          strcmp(state.failure->exception_class, "java/lang/IllegalStateException") == 0);
    for (size_t i = 0; i < COUNT(observer.receipts); ++i)
        CHECK(observer.receipts[i]->holds == 1 &&
              mc_world_access_receipt_retire(&observer.receipts[i], NULL) == MC_WORLD_STORAGE_OK &&
              observer.receipts[i] == NULL);
    CHECK(mc_world_access_free(observer.self, NULL) == MC_WORLD_STORAGE_OK);
    observer.self = NULL;
    ok = true;
cleanup:
    if (external != NULL && mc_client_screen_release(&external, NULL) != MC_CLIENT_SCREEN_OK)
        return 1;
    if (display != NULL && mc_client_display_screen_retire(&display) != MC_CLIENT_DISPLAY_SCREEN_OK)
        return 1;
    if (yc != NULL && mc_client_screen_sign_retire(&yc, NULL) != MC_CLIENT_SCREEN_OK)
        return 1;
    if (button != NULL && mc_client_screen_button_release(&button, NULL) != MC_CLIENT_SCREEN_OK)
        return 1;
    mc_tile_entity_release(tile);
    for (size_t i = 0; i < COUNT(observer.receipts); ++i)
        if (mc_world_access_receipt_retire(&observer.receipts[i], NULL) != MC_WORLD_STORAGE_OK)
            return 1;
    if (observer.self != NULL) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(observer.world, observer.self, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            mc_world_access_free(observer.self, NULL) != MC_WORLD_STORAGE_OK)
            return 1;
    }
    if (mc_client_screen_classes_release(&screens, NULL) != MC_CLIENT_SCREEN_OK ||
        mc_client_font_mapping_release(mapping) != MC_CLIENT_FONT_MAPPING_OK)
        return 1;
    mc_player_string_release(empty);
    if (!cleanup_fixture(f))
        return 1;
    if (!ok)
        return 1;
    printf("YC dirty ownership passed: %u checks\n", checks);
    return 0;
}
