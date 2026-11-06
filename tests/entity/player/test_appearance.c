#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/player/player_internal.h"
#include "mc/entity/appearance.h"
#include "mc/entity/minecart/minecart.h"
#include "mc/entity/particles/particle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool cloak_text(const mc_player_string *string, const uint16_t *name, size_t length) {
    static const char prefix[] = "http://s3.amazonaws.com/MinecraftCloaks/";
    static const char suffix[] = ".png";
    mc_player_string_view text;
    if (!mc_player_string_read(string, &text) ||
        text.length != sizeof(prefix) - 1 + length + sizeof(suffix) - 1)
        return false;
    for (size_t i = 0; i < sizeof(prefix) - 1; ++i)
        if (text.units[i] != (uint8_t)prefix[i])
            return false;
    if (length != 0 && memcmp(text.units + sizeof(prefix) - 1, name, length * sizeof(*name)) != 0)
        return false;
    for (size_t i = 0; i < sizeof(suffix) - 1; ++i)
        if (text.units[sizeof(prefix) - 1 + length + i] != (uint8_t)suffix[i])
            return false;
    return true;
}

static bool name_case(mc_entity_classes *classes, const uint16_t *units, size_t length,
                      bool null_name, mc_player_error *error) {
    bool result = false;
    mc_entity *entity = NULL;
    mc_player_string *name = NULL, *kept = NULL;
    static const uint16_t null_units[] = {'n', 'u', 'l', 'l'};
    CHECK(mc_local_player_allocate(classes, &entity, NULL) == MC_ENTITY_OK);
    if (!null_name) {
        CHECK(mc_player_string_create(units, length, &name) == MC_PLAYER_OK);
        CHECK(mc_player_string_retain(name) == MC_PLAYER_OK);
        /* Authored RAW-field control, with the real dc allocation/Class and
         * independent l occurrence. Constructor behavior is checked separately. */
        entity->local_player->fields.player.l = name;
    }
    const size_t entity_holds = entity->holds;
    const size_t entity_bytes = mc_entity_native_bytes(entity);
    CHECK(mc_entity_update_cloak(entity, error) == MC_PLAYER_OK);
    const mc_player_string *first = mc_entity_cloak_url(entity);
    CHECK(first != NULL && first == entity->local_player->fields.player.n && first->holds == 2 &&
          entity->phase == MC_ENTITY_RAW && entity->holds == entity_holds &&
          mc_entity_native_bytes(entity) == entity_bytes && mc_entity_skin_url(entity) == NULL);
    CHECK(cloak_text(first, null_name ? null_units : units, null_name ? 4 : length));
    kept = (mc_player_string *)first;
    CHECK(mc_player_string_retain(kept) == MC_PLAYER_OK && kept->holds == 3);
    CHECK(mc_entity_update_cloak(entity, NULL) == MC_PLAYER_OK);
    CHECK(mc_entity_cloak_url(entity) != first && kept->holds == 1 &&
          entity->local_player->fields.player.n == mc_entity_cloak_url(entity) &&
          mc_entity_cloak_url(entity)->holds == 2 &&
          cloak_text(mc_entity_cloak_url(entity), null_name ? null_units : units,
                     null_name ? 4 : length));
    CHECK(mc_entity_retain(entity) == MC_ENTITY_OK && entity->holds == entity_holds + 1);
    const mc_player_string *before = mc_entity_cloak_url(entity);
    entity->holds = SIZE_MAX; /* Native counter admission, not source gameplay. */
    const mc_player_status limited = mc_entity_update_cloak(entity, error);
    entity->holds = entity_holds + 1;
    CHECK(limited == MC_PLAYER_ENTITY_FAILED && error->entity.status == MC_ENTITY_LIMIT &&
          mc_entity_cloak_url(entity) == before && entity->local_player->fields.player.n == before);
    mc_entity_release(entity);
    CHECK(entity->holds == entity_holds);
    mc_entity_release(entity);
    entity = NULL;
    CHECK(cloak_text(kept, null_name ? null_units : units, null_name ? 4 : length));
    result = true;
cleanup:
    mc_entity_release(entity);
    mc_player_string_release(name);
    mc_player_string_release(kept);
    return result;
}

typedef mc_entity_status (*allocate_entity)(mc_entity_classes *, mc_entity **, mc_entity_error *);

static bool base_dispatch(mc_entity_classes *classes, mc_player_error *error) {
    bool result = false;
    mc_entity *entity = NULL;
    const allocate_entity allocators[] = {mc_item_entity_allocate, mc_falling_entity_allocate,
                                          mc_minecart_entity_allocate, mc_pickup_particle_allocate};
    const mc_entity_class_kind kinds[] = {
        MC_ENTITY_CLASS_KIND_ITEM, MC_ENTITY_CLASS_KIND_FALLING_SAND, MC_ENTITY_CLASS_KIND_MINECART,
        MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE};
    for (size_t i = 0; i < sizeof(allocators) / sizeof(allocators[0]); ++i) {
        CHECK(allocators[i](classes, &entity, NULL) == MC_ENTITY_OK);
        mc_entity_class_literal_view literal;
        CHECK(mc_entity_class_literal_inspect(entity->class_literal, &literal) == MC_ENTITY_OK &&
              literal.kind == kinds[i] && literal.owner == classes);
        uint8_t before[sizeof(*entity)];
        memcpy(before, entity, sizeof(before));
        CHECK(mc_entity_update_cloak(entity, error) == MC_PLAYER_OK &&
              memcmp(before, entity, sizeof(before)) == 0 && mc_entity_skin_url(entity) == NULL &&
              mc_entity_cloak_url(entity) == NULL);
        mc_entity_release(entity);
        entity = NULL;
    }
    result = true;
cleanup:
    mc_entity_release(entity);
    return result;
}

static bool string_limit(mc_entity_classes *classes, mc_player_error *error) {
    bool result = false;
    mc_entity *entity = NULL;
    uint16_t *units = NULL;
    mc_player_string *name = NULL;
    CHECK(mc_local_player_allocate(classes, &entity, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_update_cloak(entity, error) == MC_PLAYER_OK);
    const mc_player_string *before = mc_entity_cloak_url(entity);
    units = calloc(MC_PLAYER_MAX_TEXT_UNITS, sizeof(*units));
    CHECK(units != NULL);
    CHECK(mc_player_string_create(units, MC_PLAYER_MAX_TEXT_UNITS, &name) == MC_PLAYER_OK);
    CHECK(mc_player_string_retain(name) == MC_PLAYER_OK);
    entity->local_player->fields.player.l = name;
    CHECK(mc_entity_update_cloak(entity, error) == MC_PLAYER_LIMIT &&
          strcmp(error->site.class_name, "gs") == 0 && strcmp(error->site.method, "u_") == 0 &&
          error->site.pc == 25 && entity->holds == 1 &&
          entity->local_player->fields.player.n == before &&
          mc_entity_cloak_url(entity) == before && before->holds == 2);
    result = true;
cleanup:
    free(units);
    mc_entity_release(entity);
    mc_player_string_release(name);
    return result;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *server = NULL;
    mc_entity *server_item = NULL;
    mc_player_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    const uint64_t uniquifier = atomic_load(&profile->uniquifier);
    static const uint16_t ascii[] = {'S', 't', 'e', 'v', 'e'};
    static const uint16_t unicode[] = {0, 0xd800, 0x00e9, 0xdc00, '/', '?', '#'};
    CHECK(name_case(classes, ascii, sizeof(ascii) / sizeof(ascii[0]), false, error));
    CHECK(name_case(classes, unicode, sizeof(unicode) / sizeof(unicode[0]), false, error));
    CHECK(name_case(classes, NULL, 0, false, error));
    CHECK(name_case(classes, NULL, 0, true, error));
    CHECK(base_dispatch(classes, error));
    CHECK(string_limit(classes, error));
    CHECK(mc_entity_update_cloak(NULL, error) == MC_PLAYER_NULL_REFERENCE &&
          mc_entity_skin_url(NULL) == NULL && mc_entity_cloak_url(NULL) == NULL);
    CHECK(mc_entity_classes_create(MC_ENTITY_SERVER, profile, NULL, &server) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(server, &server_item, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_update_cloak(server_item, error) == MC_PLAYER_INVALID_ARGUMENT &&
          server_item->holds == 1 && server_item->phase == MC_ENTITY_RAW);
    CHECK(atomic_load(&profile->uniquifier) == uniquifier && profile->math == NULL &&
          atomic_load(&profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD &&
          !atomic_load(&profile->constructor_admitted));
    result = 0;
cleanup:
    free(error);
    mc_entity_release(server_item);
    if (mc_entity_classes_free(server) != MC_ENTITY_OK)
        result = 1;
    if (mc_entity_classes_free(classes) != MC_ENTITY_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    if (result == 0 && printf("Entity appearance controls: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
