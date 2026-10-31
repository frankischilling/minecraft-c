#include "player_internal.h"

#include <stdlib.h>
#include <string.h>

static const uint16_t texture_units[] = {'/', 'm', 'o', 'b', '/', 'c', 'h',
                                         'a', 'r', '.', 'p', 'n', 'g'};
static const uint16_t model_units[] = {'h', 'u', 'm', 'a', 'n', 'o', 'i', 'd'};
static const mc_player_string texture = {SIZE_MAX, sizeof(texture_units) / sizeof(texture_units[0]),
                                         texture_units};
static const mc_player_string model = {SIZE_MAX, sizeof(model_units) / sizeof(model_units[0]),
                                       model_units};
static const mc_player_string empty = {SIZE_MAX, 0, NULL};

const mc_player_string *mc_player_empty_literal(void) {
    return &empty;
}

void mc_player_error_clear(mc_player_error *error) {
    if (error != NULL)
        *error = (mc_player_error){0};
}

mc_player_status mc_player_fail(mc_player_error *error, mc_player_status status,
                                const char *operation, const char *class_name, const char *method,
                                const char *descriptor, unsigned int pc) {
    if (error != NULL) {
        error->status = status;
        error->operation = operation;
        error->site = (mc_player_site){class_name, method, descriptor, pc};
    }
    return status;
}

static mc_player_status allocate_string(size_t length, mc_player_string **out) {
    if (length > MC_PLAYER_MAX_TEXT_UNITS ||
        length > (SIZE_MAX - sizeof(mc_player_string)) / sizeof(uint16_t))
        return MC_PLAYER_LIMIT;
    mc_player_string *string = malloc(sizeof(*string) + length * sizeof(uint16_t));
    if (string == NULL)
        return MC_PLAYER_ALLOCATION_FAILED;
    *string = (mc_player_string){1, length, (const uint16_t *)(string + 1)};
    *out = string;
    return MC_PLAYER_OK;
}

mc_player_status mc_player_string_create(const uint16_t *units, size_t length,
                                         mc_player_string **out) {
    if (out == NULL || (length != 0 && units == NULL))
        return MC_PLAYER_INVALID_ARGUMENT;
    mc_player_string *string = NULL;
    const mc_player_status status = allocate_string(length, &string);
    if (status != MC_PLAYER_OK)
        return status;
    if (length != 0)
        memcpy((uint16_t *)(string + 1), units, length * sizeof(*units));
    *out = string;
    return MC_PLAYER_OK;
}

mc_player_status mc_player_string_retain(mc_player_string *string) {
    if (string == NULL || string->holds == SIZE_MAX)
        return MC_PLAYER_OK;
    if (string->holds == SIZE_MAX - 1u)
        return MC_PLAYER_LIMIT;
    ++string->holds;
    return MC_PLAYER_OK;
}

void mc_player_string_release(mc_player_string *string) {
    if (string != NULL && string->holds != SIZE_MAX && --string->holds == 0)
        free(string);
}

bool mc_player_string_read(const mc_player_string *string, mc_player_string_view *out) {
    if (string == NULL || out == NULL)
        return false;
    *out = (mc_player_string_view){string->units, string->length};
    return true;
}

const mc_player_string *mc_player_texture_literal(void) {
    return &texture;
}

const mc_player_string *mc_player_model_literal(void) {
    return &model;
}

mc_player_status mc_player_skin_string(const mc_player_string *name, mc_player_string **out) {
    static const char prefix[] = "http://s3.amazonaws.com/MinecraftSkins/";
    static const char suffix[] = ".png";
    const size_t before = sizeof(prefix) - 1u, after = sizeof(suffix) - 1u;
    if (name == NULL || out == NULL)
        return MC_PLAYER_INVALID_ARGUMENT;
    if (name->length > MC_PLAYER_MAX_TEXT_UNITS - before - after)
        return MC_PLAYER_LIMIT;
    mc_player_string *string = NULL;
    const mc_player_status status = allocate_string(before + name->length + after, &string);
    if (status != MC_PLAYER_OK)
        return status;
    uint16_t *units = (uint16_t *)(string + 1);
    for (size_t i = 0; i < before; ++i)
        units[i] = (uint8_t)prefix[i];
    memcpy(units + before, name->units, name->length * sizeof(*units));
    for (size_t i = 0; i < after; ++i)
        units[before + name->length + i] = (uint8_t)suffix[i];
    *out = string;
    return MC_PLAYER_OK;
}

mc_player_status mc_player_cloak_string(const mc_player_string *name, mc_player_string **out) {
    static const char prefix[] = "http://s3.amazonaws.com/MinecraftCloaks/";
    static const char suffix[] = ".png";
    static const uint16_t null_units[] = {'n', 'u', 'l', 'l'};
    const size_t before = sizeof(prefix) - 1u, after = sizeof(suffix) - 1u;
    const size_t length = name == NULL ? sizeof(null_units) / sizeof(null_units[0]) : name->length;
    const uint16_t *text = name == NULL ? null_units : name->units;
    if (out == NULL)
        return MC_PLAYER_INVALID_ARGUMENT;
    if (length > MC_PLAYER_MAX_TEXT_UNITS - before - after)
        return MC_PLAYER_LIMIT;
    mc_player_string *string = NULL;
    const mc_player_status status = allocate_string(before + length + after, &string);
    if (status != MC_PLAYER_OK)
        return status;
    uint16_t *units = (uint16_t *)(string + 1);
    for (size_t i = 0; i < before; ++i)
        units[i] = (uint8_t)prefix[i];
    if (length != 0)
        memcpy(units + before, text, length * sizeof(*units));
    for (size_t i = 0; i < after; ++i)
        units[before + length + i] = (uint8_t)suffix[i];
    *out = string;
    return MC_PLAYER_OK;
}
