#include "client/startup/application_construction_internal.h"
/* Reuse the existing genuine constructor fixture and cleanup, not a second App
 * representation or constructor. The earlier main remains separately runnable. */
#define main mc_kh_previous_constructor_main
#include "test_application_construction.c"
#undef main
#include "block/registry/registry_internal.h"
#include "client/achievements/description_registry.h"

static size_t callback_holds(const client_application *app) {
    mc_client_application_volatile_state state;
    if (!mc_client_application_volatile_inspect(app, &state))
        return SIZE_MAX;
    return state.native_callback_holds;
}

static int raw_contracts(const char *root) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    client_application app = {0}, borrowed = {0};
    app.registry = f.services.registry;
    CHECK(mc_client_application_lifetime_register(&app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(callback_holds(&app) == 0);
    mc_registry_error cause;
    const mc_achievement_description_callback *legacy = NULL, *legacy2 = NULL;
    const size_t before = f.services.registry->allocation_bytes;
    CHECK(mc_client_inventory_callback_create(f.services.registry, &borrowed, &legacy, &cause) ==
          MC_REGISTRY_OK);
    const size_t legacy_charge = f.services.registry->allocation_bytes - before;
    CHECK(legacy_charge > 2u * sizeof(void *));
    CHECK(mc_client_inventory_callback_create(f.services.registry, &borrowed, &legacy2, &cause) ==
          MC_REGISTRY_OK);
    CHECK(f.services.registry->allocation_bytes - before == 2u * legacy_charge);
    CHECK(!mc_client_inventory_callback_is_owned(f.services.registry, legacy));
    client_application *read_app = NULL;
    CHECK(mc_client_inventory_callback_client(f.services.registry, legacy, &read_app, &cause) ==
              MC_REGISTRY_OK &&
          read_app == &borrowed);
    CHECK(mc_registry_classes_initialize_achievements(f.services.registry, &cause) ==
          MC_REGISTRY_OK);
    mc_registry_achievement_view achievements;
    CHECK(mc_registry_classes_inspect_achievements(f.services.registry, &achievements, &cause) ==
          MC_REGISTRY_OK);
    CHECK(mc_client_achievement_callback_set(f.services.registry, achievements.named[0], legacy,
                                             &cause) == MC_REGISTRY_OK);
    mc_client_description_result description = {0};
    mc_client_notice_error error;
    CHECK(mc_client_achievement_description(f.services.registry, achievements.named[0],
                                            &description,
                                            &error) == MC_CLIENT_NOTICE_SOURCE_EXCEPTION);
    CHECK(error.source_pc == 14 && strcmp(error.source_class, "kh") == 0);

    const size_t cap = f.services.registry->options.max_allocation_bytes;
    f.services.registry->options.max_allocation_bytes =
        f.services.registry->allocation_bytes + f.services.registry->managed_current_bytes;
    const mc_achievement_description_callback *candidate = legacy;
    const void *head = f.services.registry->owned_inventory_callbacks;
    const size_t charged = f.services.registry->allocation_bytes;
    CHECK(mc_client_application_inventory_callback_create_owned(&app, &candidate, &error) ==
          MC_CLIENT_NOTICE_REGISTRY_FAILED);
    CHECK(error.source_pc == 346 && error.registry.status == MC_REGISTRY_LIMIT);
    CHECK(candidate == legacy && callback_holds(&app) == 0);
    CHECK(f.services.registry->allocation_bytes == charged &&
          f.services.registry->owned_inventory_callbacks == head);
    f.services.registry->options.max_allocation_bytes = cap;

    /* A real RAW callback allocation survives native App-admission failure.
     * No App reference/borrow was stored; output and source descriptor stay put. */
    app.constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_application_inventory_callback_create_owned(&app, &candidate, &error) ==
          MC_CLIENT_NOTICE_INVALID_PHASE);
    CHECK(candidate == legacy && callback_holds(&app) == 0);
    CHECK(f.services.registry->owned_inventory_callbacks != head &&
          f.services.registry->allocation_bytes > charged);
    CHECK(mc_statistic_achievement_fields_get(achievements.named[0])->client_callback == legacy);
    CHECK(mc_client_application_lifetime_retire(&app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_retire(&app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    const size_t retired_bytes = f.services.registry->allocation_bytes;
    const void *retired_head = f.services.registry->owned_inventory_callbacks;
    CHECK(mc_client_application_install_inventory_callback(&app, &error) ==
          MC_CLIENT_NOTICE_INVALID_PHASE);
    CHECK(f.services.registry->allocation_bytes == retired_bytes &&
          f.services.registry->owned_inventory_callbacks == retired_head);
    /* Legacy component/raw fixture performs the same real installer without
     * synthesizing lifetime registration. Its callback keeps borrowed policy. */
    borrowed.registry = f.services.registry;
    CHECK(borrowed.native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RAW);
    CHECK(mc_client_application_install_inventory_callback(&borrowed, &error) ==
          MC_CLIENT_NOTICE_OK);
    const mc_achievement_description_callback *raw_installed =
        mc_statistic_achievement_fields_get(achievements.named[0])->client_callback;
    CHECK(!mc_client_inventory_callback_is_owned(f.services.registry, raw_installed));
    CHECK(borrowed.native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RAW);
    CHECK(mc_client_inventory_callback_client(f.services.registry, raw_installed, &read_app,
                                              &cause) == MC_REGISTRY_OK &&
          read_app == &borrowed);
    CHECK(fixture_close(&f) == 0); /* RAW node has no hook; no stale App decrement. */
    return 0;
}

static int constructed_order(const char *root, bool namespace_first) {
    fixture f;
    CHECK(fixture_open(&f, root) == 0);
    client_application *a = NULL, *b = NULL;
    const mc_client_application_constructor_arguments args = {.width = 854, .height = 480};
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &a, f.error) ==
          MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_construct(a, &args, f.error) == MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_allocate(&f.services, NULL, &b, f.error) ==
          MC_CLIENT_APP_OK);
    CHECK(mc_client_application_constructor_construct(b, &args, f.error) == MC_CLIENT_APP_OK);
    CHECK(a != b && a->x != b->x && a->K == b->K);
    CHECK(atomic_load(&a->source_static_holds) == 0 && atomic_load(&b->source_static_holds) == 1);
    mc_client_notice_error notice;
    CHECK(mc_client_application_install_inventory_callback(a, &notice) == MC_CLIENT_NOTICE_OK);
    CHECK(callback_holds(a) == 1 && callback_holds(b) == 0);
    const mc_achievement_description_callback *unpublished = NULL;
    CHECK(mc_client_application_inventory_callback_create_owned(a, &unpublished, &notice) ==
          MC_CLIENT_NOTICE_OK);
    CHECK(callback_holds(a) == 2 && unpublished != NULL);
    mc_registry_error cause;
    CHECK(mc_client_achievement_callback_set(f.services.registry, NULL, unpublished, &cause) ==
          MC_REGISTRY_NULL_REFERENCE); /* Actual failed setter keeps constructed kh. */
    CHECK(callback_holds(a) == 2);
    CHECK(mc_client_application_install_inventory_callback(b, &notice) == MC_CLIENT_NOTICE_OK);
    CHECK(callback_holds(a) == 2 && callback_holds(b) == 1);
    mc_registry_achievement_view achievements;
    CHECK(mc_registry_classes_inspect_achievements(f.services.registry, &achievements, &cause) ==
          MC_REGISTRY_OK);
    const mc_achievement_description_callback *installed =
        mc_statistic_achievement_fields_get(achievements.named[0])->client_callback;
    client_application *read_app = NULL;
    CHECK(mc_client_inventory_callback_client(f.services.registry, installed, &read_app, &cause) ==
              MC_REGISTRY_OK &&
          read_app == b);
    CHECK(mc_client_inventory_callback_client(f.services.registry, unpublished, &read_app,
                                              &cause) == MC_REGISTRY_OK &&
          read_app == a);
    const void *head = f.services.registry->owned_inventory_callbacks;
    CHECK(mc_registry_classes_free(f.services.registry, &cause) == MC_REGISTRY_BUSY);
    CHECK(callback_holds(a) == 2 && callback_holds(b) == 1 &&
          f.services.registry->owned_inventory_callbacks == head);
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(a != NULL && a->constructor_phase == MC_CLIENT_APP_RETIRING && a->registry == NULL &&
          !a->source_namespace_borrowed && callback_holds(a) == 2);
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(callback_holds(a) == 2);
    CHECK(mc_registry_classes_free(f.services.registry, &cause) == MC_REGISTRY_BUSY);
    CHECK(callback_holds(a) == 2 && callback_holds(b) == 1);
    CHECK(mc_client_application_constructor_retire(&b, f.error) == MC_CLIENT_APP_BUSY);
    CHECK(b != NULL && b->registry == NULL && !b->source_namespace_borrowed &&
          callback_holds(b) == 1 && atomic_load(&b->source_static_holds) == 1);
    /* Own callbacks reject native RETIRING phase before the source Settings
     * read; earlier NULL-Settings Java failure is not fabricated during teardown. */
    mc_client_description_result result = {
        .string = mc_statistic_achievement_fields_get(achievements.named[0])->l};
    const mc_registry_string *sentinel = result.string;
    CHECK(mc_client_achievement_description(f.services.registry, achievements.named[0], &result,
                                            &notice) == MC_CLIENT_NOTICE_INVALID_PHASE);
    CHECK(notice.source_class == NULL && notice.source_pc == -1 && result.string == sentinel &&
          result.owner == NULL);
    CHECK(mc_statistic_achievement_fields_get(achievements.named[0])->client_callback == installed);
    CHECK(callback_holds(a) == 2 && callback_holds(b) == 1);
    if (namespace_first) {
        CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK);
        CHECK(atomic_load(&b->source_static_holds) == 0);
        CHECK(mc_client_application_constructor_retire(&b, f.error) == MC_CLIENT_APP_BUSY);
    }
    CHECK(mc_registry_classes_free(f.services.registry, &cause) == MC_REGISTRY_OK);
    f.services.registry = NULL;
    CHECK(callback_holds(a) == 0 && callback_holds(b) == 0);
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_OK && a == NULL);
    CHECK(mc_client_application_constructor_retire(&a, f.error) == MC_CLIENT_APP_OK);
    if (!namespace_first) {
        CHECK(mc_client_application_constructor_retire(&b, f.error) == MC_CLIENT_APP_BUSY);
        CHECK(mc_client_application_classes_release(&f.services.application_classes, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK);
    }
    CHECK(mc_client_application_constructor_retire(&b, f.error) == MC_CLIENT_APP_OK && b == NULL);
    CHECK(mc_client_application_constructor_retire(&b, f.error) == MC_CLIENT_APP_OK);
    CHECK(fixture_close(&f) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    CHECK(raw_contracts(argv[1]) == 0);
    CHECK(constructed_order(argv[1], false) == 0);
    CHECK(constructed_order(argv[1], true) == 0);
    printf("Owned kh lifetime: %u checks\n", checks);
    return 0;
}
