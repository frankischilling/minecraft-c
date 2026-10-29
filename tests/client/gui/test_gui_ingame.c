/* Authored CPU controls; original constructor/Random correspondence is separate. */
#include "client/application_state.h"
#include "client/gui/gui_ingame_internal.h"
#include "mc/entity/player/player.h"
#include "random_profile_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "GuiIngame check%u line%d: %s\n", checks, __LINE__, #condition);       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#define UNIQUE_MULTIPLIER UINT64_C(1181783497276652981)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_client_render_classes *models;
} fixture;

static uint64_t uniquifier(const fixture *f) {
    return atomic_load_explicit(&f->profile->uniquifier, memory_order_seq_cst);
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static int open_fixture(fixture *f) {
    *f = (fixture){0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &f->models) ==
          MC_CLIENT_MODEL_OK);
    return 0;
}

static int close_fixture(fixture *f) {
    CHECK(mc_client_render_classes_free(f->models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    return 0;
}

static int fields_and_holds(void) {
    fixture f;
    CHECK(open_fixture(&f) == 0);
    mc_client_gui_ingame_classes *classes = NULL;
    mc_client_gui_ingame_error e = {0};
    const uint64_t initial = uniquifier(&f);
    CHECK(mc_client_gui_ingame_classes_create(f.models, f.profile, 0, &classes, &e) ==
          MC_CLIENT_GUI_INGAME_OK);
    mc_client_gui_ingame_class_state cs;
    CHECK(mc_client_gui_ingame_classes_inspect(classes, &cs, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(cs.phase == MC_CLIENT_GUI_INGAME_CLASS_COLD && cs.holds == 1 && cs.d == NULL &&
          cs.pending_item == NULL && !cs.ub_initialized && uniquifier(&f) == initial);
    mc_client_source_uq *first = NULL, *second = NULL;
    CHECK(mc_client_gui_ingame_allocate(classes, NULL, &first, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(uniquifier(&f) == initial * UNIQUE_MULTIPLIER);
    CHECK(mc_client_gui_ingame_classes_inspect(classes, &cs, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(cs.phase == MC_CLIENT_GUI_INGAME_CLASS_READY && cs.d != NULL && cs.pending_item == NULL &&
          cs.ub_initialized && cs.static_item_live && cs.last_store_pc == 7 && cs.last_pc == 10 &&
          mc_client_item_renderer_phase(cs.d) == MC_CLIENT_MODEL_READY);
    const mc_client_item_renderer *static_item = cs.d;
    mc_client_gui_ingame_state state;
    CHECK(mc_client_gui_ingame_inspect(first, &state, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(state.phase == MC_CLIENT_GUI_INGAME_RAW && state.fields.e == NULL &&
          state.fields.f == NULL && state.fields.g == NULL && state.fields.a == NULL &&
          state.fields.i == NULL && bits(state.fields.k) == 0 && bits(state.fields.b) == 0 &&
          bits(state.fields.c) == 0 && state.store_count == 0);
    CHECK(mc_client_gui_ingame_construct(first, NULL, &e) == MC_CLIENT_GUI_INGAME_OK &&
          e.source_returned);
    CHECK(uniquifier(&f) == initial * UNIQUE_MULTIPLIER * UNIQUE_MULTIPLIER);
    CHECK(mc_client_gui_ingame_inspect(first, &state, &e) == MC_CLIENT_GUI_INGAME_OK);
    size_t size = SIZE_MAX;
    CHECK(state.phase == MC_CLIENT_GUI_INGAME_READY && state.last_pc == 62 &&
          state.store_count == 10 && state.fields.e != NULL &&
          mc_client_gui_ingame_list_size(state.fields.e, &size) && size == 0 &&
          state.fields.f != NULL && !state.fields.f->have_next_next_gaussian &&
          state.fields.f->next_next_gaussian == 0.0 && state.pending_random == NULL &&
          state.fields.g == NULL && state.fields.a == NULL && state.fields.h == 0 &&
          state.fields.j == 0 && !state.fields.l && bits(state.fields.k) == 0 &&
          bits(state.fields.b) == 0 && bits(state.fields.c) == UINT32_C(0x3f800000) &&
          state.fields.i == mc_player_empty_literal());
    const unsigned int stores[] = {6, 12, 23, 28, 33, 39, 44, 49, 54, 59};
    for (size_t i = 0; i < sizeof(stores) / sizeof(stores[0]); ++i) {
        CHECK(state.stores[i].pc == stores[i]);
        CHECK(strcmp(state.stores[i].class_name, i == 0 ? "ub" : "uq") == 0);
    }
    const uint64_t before_second = uniquifier(&f);
    CHECK(mc_client_gui_ingame_allocate(classes, NULL, &second, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(uniquifier(&f) == before_second); /* Same static d, no second class Random. */
    client_application app = {
        0}; /* Genuine RAW complete App fixture; no source field is mirrored. */
    CHECK(mc_client_gui_ingame_construct(second, &app, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(uniquifier(&f) == before_second * UNIQUE_MULTIPLIER);
    mc_client_gui_ingame_state second_state;
    CHECK(mc_client_gui_ingame_inspect(second, &second_state, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(second_state.fields.g == &app && second_state.fields.e != state.fields.e &&
          second_state.fields.f != state.fields.f && second_state.fields.i == state.fields.i);
    CHECK(mc_client_gui_ingame_classes_inspect(classes, &cs, &e) == MC_CLIENT_GUI_INGAME_OK &&
          cs.d == static_item);
    /* A fresh canonical bb is distinct, including its real inherited children.
     * uq never adopts a renderer made for another GUI/manager occurrence. */
    mc_client_item_renderer *other_item = NULL;
    CHECK(mc_client_item_renderer_allocate(f.models, 0, &other_item) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_item_renderer_construct(other_item, NULL) == MC_CLIENT_MODEL_OK);
    CHECK(other_item != static_item &&
          mc_client_item_renderer_fields_get(other_item)->base_model !=
              mc_client_item_renderer_fields_get(static_item)->base_model);
    CHECK(mc_client_item_renderer_free(&other_item) == MC_CLIENT_MODEL_OK);
    mc_client_source_uq *external = second;
    CHECK(mc_client_gui_ingame_retain(external, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_retire(&second, &e) == MC_CLIENT_GUI_INGAME_BUSY &&
          second == external);
    CHECK(mc_client_gui_ingame_inspect(second, &second_state, &e) == MC_CLIENT_GUI_INGAME_OK &&
          second_state.phase == MC_CLIENT_GUI_INGAME_READY && second_state.holds == 2 &&
          second_state.fields.g == &app); /* Caller must keep App live on BUSY. */
    CHECK(mc_client_gui_ingame_release(&external, &e) == MC_CLIENT_GUI_INGAME_OK &&
          external == NULL);
    CHECK(mc_client_gui_ingame_retire(&second, &e) == MC_CLIENT_GUI_INGAME_OK && second == NULL);
    CHECK(mc_client_gui_ingame_classes_release(&classes, &e) == MC_CLIENT_GUI_INGAME_OK &&
          classes == NULL);
    /* first owns the LAST namespace occurrence; its strict retirement also
     * retires the actual class renderer, not a leaked external namespace. */
    CHECK(mc_client_gui_ingame_retire(&first, &e) == MC_CLIENT_GUI_INGAME_OK && first == NULL);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

static int class_failures(void) {
    for (unsigned int profile = 0; profile < 2; ++profile) {
        fixture f;
        CHECK(open_fixture(&f) == 0);
        const uint64_t initial = uniquifier(&f);
        if (profile == 1)
            CHECK(mc_java_random_profile_bind_constructor_inputs(f.profile, NULL, 0) ==
                  MC_JAVA_RANDOM_PROFILE_OK);
        mc_client_gui_ingame_classes *classes = NULL;
        mc_client_gui_ingame_error e = {0};
        CHECK(mc_client_gui_ingame_classes_create(f.models, f.profile, profile == 0 ? 1 : 0,
                                                  &classes, &e) == MC_CLIENT_GUI_INGAME_OK);
        CHECK(mc_client_gui_ingame_classes_active_use(classes, &e) ==
              MC_CLIENT_GUI_INGAME_MODEL_FAILED);
        mc_client_gui_ingame_error first = e;
        mc_client_gui_ingame_class_state state;
        CHECK(mc_client_gui_ingame_classes_inspect(classes, &state, &e) == MC_CLIENT_GUI_INGAME_OK);
        CHECK(state.phase == MC_CLIENT_GUI_INGAME_CLASS_FAILED && state.ub_initialized &&
              state.d == NULL && state.last_store_pc == 0 && !state.static_item_live &&
              first.site.pc == (profile == 0 ? 0u : 4u));
        CHECK((profile == 0 && state.pending_item == NULL &&
               first.model.status == MC_CLIENT_MODEL_LIMIT && uniquifier(&f) == initial) ||
              (profile == 1 && state.pending_item != NULL &&
               first.model.status == MC_CLIENT_MODEL_RANDOM_FAILED &&
               first.model.random == MC_JAVA_RANDOM_PROFILE_RANGE &&
               uniquifier(&f) == initial * UNIQUE_MULTIPLIER));
        const uint64_t committed = uniquifier(&f);
        CHECK(mc_client_gui_ingame_classes_active_use(classes, &e) ==
                  MC_CLIENT_GUI_INGAME_CLASS_ERRONEOUS &&
              e.first_class_failure.status == first.status &&
              e.first_class_failure.site.pc == first.site.pc &&
              e.first_class_failure.model.status == first.model.status &&
              e.first_class_failure.model.random == first.model.random &&
              uniquifier(&f) == committed);
        mc_client_source_uq *unchanged = NULL;
        CHECK(mc_client_gui_ingame_allocate(classes, NULL, &unchanged, &e) ==
                  MC_CLIENT_GUI_INGAME_CLASS_ERRONEOUS &&
              unchanged == NULL && uniquifier(&f) == committed);
        mc_client_gui_ingame_classes *external = classes;
        CHECK(mc_client_gui_ingame_classes_retain(external, &e) == MC_CLIENT_GUI_INGAME_OK);
        CHECK(mc_client_gui_ingame_classes_release(&classes, &e) == MC_CLIENT_GUI_INGAME_OK &&
              classes == NULL);
        CHECK(mc_client_gui_ingame_classes_release(&external, &e) == MC_CLIENT_GUI_INGAME_OK &&
              external == NULL);
        CHECK(close_fixture(&f) == 0);
    }
    return 0;
}

static int instance_prefixes(void) {
    fixture f;
    CHECK(open_fixture(&f) == 0);
    mc_client_gui_ingame_classes *classes = NULL;
    mc_client_gui_ingame_error e = {0};
    CHECK(mc_client_gui_ingame_classes_create(f.models, f.profile, 0, &classes, &e) ==
          MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_classes_active_use(classes, &e) == MC_CLIENT_GUI_INGAME_OK);
    for (size_t quota = 0; quota < 3; ++quota) {
        mc_client_source_uq *o = NULL;
        const mc_client_gui_ingame_options options = {1024u * 1024u, quota};
        CHECK(mc_client_gui_ingame_allocate(classes, &options, &o, &e) == MC_CLIENT_GUI_INGAME_OK);
        const uint64_t before = uniquifier(&f);
        const mc_client_gui_ingame_status status = mc_client_gui_ingame_construct(o, NULL, &e);
        mc_client_gui_ingame_state state;
        CHECK(mc_client_gui_ingame_inspect(o, &state, NULL) == MC_CLIENT_GUI_INGAME_OK);
        if (quota == 0)
            CHECK(status == MC_CLIENT_GUI_INGAME_LIMIT && e.site.pc == 5 &&
                  state.store_count == 1 && state.fields.e == NULL && bits(state.fields.k) == 0 &&
                  uniquifier(&f) == before);
        else if (quota == 1)
            CHECK(status == MC_CLIENT_GUI_INGAME_LIMIT && e.site.pc == 16 &&
                  state.store_count == 2 && state.fields.e != NULL && state.fields.f == NULL &&
                  state.pending_random == NULL && state.fields.i == NULL &&
                  uniquifier(&f) == before);
        else
            CHECK(status == MC_CLIENT_GUI_INGAME_OK && state.store_count == 10 &&
                  state.allocations == 2 && uniquifier(&f) == before * UNIQUE_MULTIPLIER);
        const uint64_t stopped = uniquifier(&f);
        CHECK(mc_client_gui_ingame_construct(o, NULL, &e) == MC_CLIENT_GUI_INGAME_INVALID_PHASE &&
              uniquifier(&f) == stopped);
        CHECK(mc_client_gui_ingame_retire(&o, &e) == MC_CLIENT_GUI_INGAME_OK && o == NULL);
    }
    /* Byte quota stops before the first List allocation, after actual ub.k6. */
    const mc_client_gui_ingame_options tight = {sizeof(mc_client_source_uq), SIZE_MAX};
    mc_client_source_uq *tight_owner = NULL;
    CHECK(mc_client_gui_ingame_allocate(classes, &tight, &tight_owner, &e) ==
          MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_construct(tight_owner, NULL, &e) == MC_CLIENT_GUI_INGAME_LIMIT &&
          e.site.pc == 5);
    CHECK(mc_client_gui_ingame_retire(&tight_owner, &e) == MC_CLIENT_GUI_INGAME_OK);
    client_application app = {0};
    app.constructor_phase = MC_CLIENT_APP_RETIRING;
    mc_client_source_uq *late = NULL;
    CHECK(mc_client_gui_ingame_allocate(classes, NULL, &late, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_construct(late, &app, &e) == MC_CLIENT_GUI_INGAME_INVALID_PHASE &&
          e.site.pc == 59);
    mc_client_gui_ingame_state state;
    CHECK(mc_client_gui_ingame_inspect(late, &state, &e) == MC_CLIENT_GUI_INGAME_OK &&
          state.phase == MC_CLIENT_GUI_INGAME_FAILED && state.store_count == 9 &&
          state.fields.f != NULL && state.fields.i == mc_player_empty_literal() &&
          state.fields.c == 1.0f && state.fields.g == NULL);
    CHECK(mc_client_gui_ingame_retire(&late, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_classes_release(&classes, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

static int instance_random_failure(void) {
    fixture f;
    CHECK(open_fixture(&f) == 0);
    /* A declared private constructor boundary, NOT an original observation or
     * a production seed. Only bb's first default constructor has an input;
     * uq's own second constructor commits its real U then rejects exhaustion. */
    const mc_java_random_constructor_input input = {uniquifier(&f) * UNIQUE_MULTIPLIER,
                                                    INT64_C(123)};
    CHECK(mc_java_random_profile_bind_constructor_inputs(f.profile, &input, 1) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    mc_client_gui_ingame_classes *classes = NULL;
    mc_client_gui_ingame_error e = {0};
    CHECK(mc_client_gui_ingame_classes_create(f.models, f.profile, 0, &classes, &e) ==
          MC_CLIENT_GUI_INGAME_OK);
    mc_client_source_uq *o = NULL;
    CHECK(mc_client_gui_ingame_allocate(classes, NULL, &o, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(uniquifier(&f) == input.committed_uniquifier);
    CHECK(mc_client_gui_ingame_construct(o, NULL, &e) == MC_CLIENT_GUI_INGAME_RANDOM_FAILED &&
          e.site.pc == 20 && e.random == MC_JAVA_RANDOM_PROFILE_RANGE);
    CHECK(uniquifier(&f) == input.committed_uniquifier * UNIQUE_MULTIPLIER);
    mc_client_gui_ingame_state state;
    CHECK(mc_client_gui_ingame_inspect(o, &state, &e) == MC_CLIENT_GUI_INGAME_OK &&
          state.phase == MC_CLIENT_GUI_INGAME_FAILED && state.store_count == 2 &&
          state.fields.e != NULL && state.fields.f == NULL && state.pending_random != NULL &&
          state.fields.i == NULL && state.fields.g == NULL);
    const uint64_t committed = uniquifier(&f);
    CHECK(mc_client_gui_ingame_construct(o, NULL, &e) == MC_CLIENT_GUI_INGAME_INVALID_PHASE &&
          uniquifier(&f) == committed);
    CHECK(mc_client_gui_ingame_classes_active_use(classes, &e) == MC_CLIENT_GUI_INGAME_OK &&
          uniquifier(&f) == committed);
    CHECK(mc_client_gui_ingame_retire(&o, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_classes_release(&classes, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

static int clear_contracts(void) {
    fixture f;
    CHECK(open_fixture(&f) == 0);
    mc_client_gui_ingame_classes *classes = NULL;
    mc_client_gui_ingame_error e = {0};
    CHECK(mc_client_gui_ingame_classes_create(f.models, f.profile, 0, &classes, &e) ==
          MC_CLIENT_GUI_INGAME_OK);
    mc_client_source_uq *o = NULL;
    CHECK(mc_client_gui_ingame_allocate(classes, NULL, &o, &e) == MC_CLIENT_GUI_INGAME_OK);
    const uint64_t raw_random = uniquifier(&f);
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) ==
              MC_CLIENT_GUI_INGAME_SOURCE_NULL_REFERENCE &&
          !e.native_policy && !e.source_returned && e.site.pc == 4 &&
          strcmp(e.site.class_name, "uq") == 0 && strcmp(e.site.method, "b") == 0 &&
          strcmp(e.site.descriptor, "()V") == 0);
    mc_client_gui_ingame_state state;
    CHECK(mc_client_gui_ingame_inspect(o, &state, NULL) == MC_CLIENT_GUI_INGAME_OK &&
          state.phase == MC_CLIENT_GUI_INGAME_RAW && state.fields.e == NULL &&
          state.store_count == 0 && state.last_pc == 0 && uniquifier(&f) == raw_random);
    CHECK(mc_client_gui_ingame_construct(o, NULL, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_inspect(o, &state, NULL) == MC_CLIENT_GUI_INGAME_OK);
    mc_client_gui_ingame_list *same = state.fields.e;
    const unsigned int constructor_pc = state.last_pc, constructor_stores = state.store_count;
    const mc_client_gui_ingame_error constructor_result = state.failure;
    const uint64_t after_construct = uniquifier(&f);
    const uint64_t instance_random = state.fields.f->state;
    CHECK(same != NULL && same->size == 0 && same->capacity == 0 && same->entries == NULL &&
          same->mod_count == 0);
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_OK &&
          e.source_returned && !e.native_policy && e.site.pc == 9 &&
          strcmp(e.site.method, "b") == 0 && strcmp(e.site.descriptor, "()V") == 0);
    CHECK(same->size == 0 && same->mod_count == 1 && same->capacity == 0 && same->entries == NULL);
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_OK &&
          same->mod_count == 2);
    same->mod_count = INT32_MAX; /* Explicit native arrangement of the actual counter. */
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_OK &&
          same->mod_count == INT32_MIN);
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_OK &&
          same->mod_count == INT32_MIN + 1);

    /* Native arrangement ONLY, not source-added ChatLine/READY construction.
     * These are actual live canonical String owners; clear removes references
     * without releasing, closing or pretending to execute a referent method. */
    const uint16_t a[] = {'o', 'n', 'e'}, b[] = {'t', 'w', 'o'};
    mc_player_string *first = NULL, *second = NULL;
    CHECK(mc_player_string_create(a, 3, &first) == MC_PLAYER_OK &&
          mc_player_string_create(b, 3, &second) == MC_PLAYER_OK);
    same->entries = calloc(5, sizeof(*same->entries));
    CHECK(same->entries != NULL);
    same->capacity = 5;
    same->size = 3;
    same->entries[0] = first;
    same->entries[1] = second;
    same->entries[2] = first;
    void **array = same->entries;
    const int32_t before_clear = same->mod_count;
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_OK &&
          same->size == 0 && same->entries == array && same->capacity == 5 &&
          same->mod_count == mc_java_i32_add(before_clear, 1));
    for (size_t i = 0; i < 5; ++i)
        CHECK(array[i] == NULL);
    mc_player_string_view view;
    CHECK(mc_player_string_read(first, &view) && view.length == 3 && view.units[0] == 'o');
    CHECK(mc_player_string_read(second, &view) && view.length == 3 && view.units[0] == 't');
    CHECK(mc_client_gui_ingame_inspect(o, &state, NULL) == MC_CLIENT_GUI_INGAME_OK &&
          state.fields.e == same && state.last_pc == constructor_pc &&
          state.store_count == constructor_stores &&
          state.failure.site.pc == constructor_result.site.pc &&
          state.failure.source_returned == constructor_result.source_returned &&
          state.fields.f->state == instance_random && uniquifier(&f) == after_construct);

    CHECK(mtx_lock(&classes->mutex) == thrd_success);
    const mc_client_gui_ingame_status contended = mc_client_gui_ingame_clear_messages(o, &e);
    CHECK(mtx_unlock(&classes->mutex) == thrd_success);
    CHECK(contended == MC_CLIENT_GUI_INGAME_BUSY && e.native_policy &&
          strcmp(e.site.method, "b") == 0 && strcmp(e.site.descriptor, "()V") == 0 &&
          same->mod_count == mc_java_i32_add(before_clear, 1));
    const mc_client_gui_ingame_phase phase = o->phase;
    o->phase = MC_CLIENT_GUI_INGAME_CONSTRUCTING;
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_BUSY &&
          e.native_policy);
    o->phase = MC_CLIENT_GUI_INGAME_RETIRING;
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_BUSY &&
          e.native_policy);
    o->phase = phase;
    const mc_client_gui_ingame_class_phase class_phase = classes->phase;
    classes->phase = MC_CLIENT_GUI_INGAME_CLASS_INITIALIZING;
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_BUSY &&
          e.native_policy);
    classes->phase = class_phase;
    classes->retiring = true;
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_BUSY &&
          e.native_policy);
    classes->retiring = false;
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_OK);
    /* Array retirement is independent from source clear. Retire with live
     * arranged references; caller-owned referents must remain usable. */
    same->size = 1;
    array[0] = first;
    mc_client_source_uq *held = o;
    CHECK(mc_client_gui_ingame_retain(held, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_retire(&o, &e) == MC_CLIENT_GUI_INGAME_BUSY && o == held &&
          same->entries == array && array[0] == first);
    CHECK(mc_client_gui_ingame_release(&held, &e) == MC_CLIENT_GUI_INGAME_OK && held == NULL);
    CHECK(mc_client_gui_ingame_retire(&o, &e) == MC_CLIENT_GUI_INGAME_OK && o == NULL);
    CHECK(mc_player_string_read(first, &view) && view.length == 3 && view.units[0] == 'o');
    mc_player_string_release(first);
    mc_player_string_release(second);

    const mc_client_gui_ingame_options no_list = {1024u * 1024u, 0};
    CHECK(mc_client_gui_ingame_allocate(classes, &no_list, &o, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_construct(o, NULL, &e) == MC_CLIENT_GUI_INGAME_LIMIT &&
          e.site.pc == 5);
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) ==
              MC_CLIENT_GUI_INGAME_SOURCE_NULL_REFERENCE &&
          e.site.pc == 4 && !e.native_policy);
    CHECK(mc_client_gui_ingame_inspect(o, &state, NULL) == MC_CLIENT_GUI_INGAME_OK &&
          state.phase == MC_CLIENT_GUI_INGAME_FAILED && state.last_pc == 5 &&
          state.failure.status == MC_CLIENT_GUI_INGAME_LIMIT && state.failure.site.pc == 5 &&
          strcmp(state.failure.site.method, "<init>") == 0);
    CHECK(mc_client_gui_ingame_retire(&o, &e) == MC_CLIENT_GUI_INGAME_OK);
    const mc_client_gui_ingame_options no_random = {1024u * 1024u, 1};
    CHECK(mc_client_gui_ingame_allocate(classes, &no_random, &o, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_construct(o, NULL, &e) == MC_CLIENT_GUI_INGAME_LIMIT &&
          e.site.pc == 16);
    CHECK(mc_client_gui_ingame_clear_messages(o, &e) == MC_CLIENT_GUI_INGAME_OK &&
          e.source_returned);
    CHECK(mc_client_gui_ingame_inspect(o, &state, NULL) == MC_CLIENT_GUI_INGAME_OK &&
          state.phase == MC_CLIENT_GUI_INGAME_FAILED && state.failure.site.pc == 16 &&
          state.fields.e->mod_count == 1 && state.last_pc == 16);
    CHECK(mc_client_gui_ingame_retire(&o, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_classes_release(&classes, &e) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

int main(void) {
    CHECK(fields_and_holds() == 0);
    CHECK(class_failures() == 0);
    CHECK(instance_prefixes() == 0);
    CHECK(instance_random_failure() == 0);
    CHECK(clear_contracts() == 0);
    printf("GuiIngame constructor contracts: %u checks\n", checks);
    return 0;
}
