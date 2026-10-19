#include "mc/world/saved_data/storage.h"

#include "files.h"
#include "mc/core/java/java.h"
#include "mc/core/text/mutf8.h"
#include "mc/world/storage/store.h"

#include <stdlib.h>
#include <string.h>

typedef struct saved_id_entry {
    mc_world_saved_id id;
    struct saved_id_entry *next;
    uint16_t units[];
} saved_id_entry;

struct mc_world_saved_data {
    mc_world_saved_data_limits limits;
    char *path;
    saved_id_entry *first, *last;
    size_t count, key_units, allocated;
    mc_world_storage_error last_caught;
};

static mc_world_storage_status saved_error(mc_world_storage_error *error,
                                           mc_world_storage_status status, const char *operation)
{
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status saved_nbt_error(mc_world_storage_error *error, mc_nbt_status status)
{
    mc_world_storage_status mapped = status == MC_NBT_ALLOCATION_FAILED
                                         ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                     : status == MC_NBT_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                              : MC_WORLD_STORAGE_INVALID_DATA;
    return saved_error(error, mapped, mc_nbt_status_string(status));
}

static bool saved_catch(mc_world_saved_data *owner, mc_world_storage_status status,
                        const mc_world_storage_error *error)
{
    if (status != MC_WORLD_STORAGE_IO_ERROR && status != MC_WORLD_STORAGE_MISSING &&
        status != MC_WORLD_STORAGE_INVALID_DATA)
        return false;
    owner->last_caught = *error;
    return true;
}

mc_world_saved_data_limits mc_world_saved_data_default_limits(void)
{
    return (mc_world_saved_data_limits){4096, 65536, 4u * 1024u * 1024u, mc_nbt_default_limits()};
}

static bool saved_key_valid(const mc_nbt_string *key)
{
    return key == NULL || ((key->length == 0 || key->units != NULL) &&
                           key->length <= SIZE_MAX / sizeof(*key->units));
}

static saved_id_entry *saved_find(mc_world_saved_data *owner, const mc_nbt_string *key)
{
    for (saved_id_entry *entry = owner->first; entry != NULL; entry = entry->next) {
        if (entry->id.null_name != (key == NULL))
            continue;
        if (key == NULL || (entry->id.name.length == key->length &&
                            (key->length == 0 || memcmp(entry->id.name.units, key->units,
                                                        key->length * sizeof(*key->units)) == 0)))
            return entry;
    }
    return NULL;
}

static mc_world_storage_status saved_insert(mc_world_saved_data *owner, const mc_nbt_string *key,
                                            int16_t value, saved_id_entry **out,
                                            mc_world_storage_error *error)
{
    size_t units = key == NULL ? 0 : key->length;
    if (owner->count >= owner->limits.max_ids ||
        units > owner->limits.max_key_units - owner->key_units ||
        units > (SIZE_MAX - sizeof(saved_id_entry)) / sizeof(uint16_t))
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data ID capacity");
    size_t bytes = sizeof(saved_id_entry) + units * sizeof(uint16_t);
    if (bytes > owner->limits.max_allocation_bytes - owner->allocated)
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data ID allocation budget");
    saved_id_entry *entry = malloc(bytes);
    if (entry == NULL)
        return saved_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "saved data ID entry");
    *entry = (saved_id_entry){.id = {.null_name = key == NULL, .value = value}};
    if (key != NULL) {
        if (units != 0)
            memcpy(entry->units, key->units, units * sizeof(*key->units));
        entry->id.name = (mc_nbt_string){entry->units, units};
    }
    if (owner->last == NULL)
        owner->first = entry;
    else
        owner->last->next = entry;
    owner->last = entry;
    ++owner->count;
    owner->key_units += units;
    owner->allocated += bytes;
    *out = entry;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status saved_load(mc_world_saved_data *owner, mc_world_storage_error *error)
{
    if (owner->path == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_ws_file_bytes bytes = {0};
    mc_world_storage_status status =
        mc_ws_read_file(owner->path, owner->limits.nbt.max_input_bytes, &bytes, error);
    if (status == MC_WORLD_STORAGE_MISSING)
        return MC_WORLD_STORAGE_OK;
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_nbt_document *document = NULL;
    mc_nbt_status decoded =
        mc_nbt_read_compound(bytes.data, bytes.length, &owner->limits.nbt, &document, NULL, NULL);
    mc_ws_free_file_bytes(&bytes);
    if (decoded != MC_NBT_OK)
        return saved_nbt_error(error, decoded);
    const mc_nbt_tag *root = mc_nbt_document_root(document);
    for (size_t i = 0; i < root->value.compound.length; ++i) {
        const mc_nbt_tag *tag = root->value.compound.items[i];
        if (tag->type != MC_NBT_SHORT)
            continue;
        saved_id_entry *entry = saved_find(owner, &tag->name);
        if (entry != NULL) {
            entry->id.value = tag->value.short_value;
        } else {
            status = saved_insert(owner, &tag->name, tag->value.short_value, &entry, error);
            if (status != MC_WORLD_STORAGE_OK)
                break;
        }
    }
    mc_nbt_document_free(document);
    return status;
}

mc_world_storage_status mc_world_saved_data_create(const mc_world_store *store,
                                                   const mc_world_saved_data_limits *limits,
                                                   mc_world_saved_data **out,
                                                   mc_world_storage_error *error)
{
    mc_world_saved_data_limits chosen =
        limits == NULL ? mc_world_saved_data_default_limits() : *limits;
    if (out == NULL || chosen.nbt.max_depth == 0 || chosen.nbt.max_depth > 128 ||
        chosen.nbt.max_input_bytes > MC_WS_FILE_MAX_BYTES ||
        chosen.nbt.max_output_bytes > MC_WS_FILE_MAX_BYTES)
        return saved_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "saved data arguments");
    if (chosen.max_allocation_bytes < sizeof(mc_world_saved_data))
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data owner budget");
    mc_world_saved_data *owner = calloc(1, sizeof(*owner));
    if (owner == NULL)
        return saved_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "saved data owner");
    owner->limits = chosen;
    owner->allocated = sizeof(*owner);
    mc_world_storage_error detail = {0};
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    if (store != NULL) {
        const char *root = mc_world_store_root(store);
        size_t bound = strlen(root) + sizeof("data/idcounts.dat") + 1u;
        if (bound > chosen.max_allocation_bytes - owner->allocated) {
            status = saved_error(&detail, MC_WORLD_STORAGE_LIMIT, "saved data path budget");
        } else {
            status = mc_ws_path_join(root, "data/idcounts.dat", &owner->path, &detail);
            if (status == MC_WORLD_STORAGE_OK)
                owner->allocated += strlen(owner->path) + 1u;
        }
    }
    if (status == MC_WORLD_STORAGE_OK)
        status = saved_load(owner, &detail);
    if (saved_catch(owner, status, &detail))
        status = MC_WORLD_STORAGE_OK;
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_saved_data_free(owner);
        if (error != NULL)
            *error = detail;
        return status;
    }
    *out = owner;
    return saved_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status saved_write_name(mc_ws_output *output, mc_nbt_string name,
                                                mc_world_storage_error *error)
{
    size_t encoded = 0;
    if (mc_mutf8_encoded_length(name.units, name.length, &encoded) != MC_MUTF8_OK ||
        encoded > UINT16_MAX)
        return saved_error(error, MC_WORLD_STORAGE_INVALID_DATA, "saved data name writeUTF");
    uint8_t length[2] = {(uint8_t)(encoded >> 8), (uint8_t)encoded};
    mc_world_storage_status status = mc_ws_output_write(output, length, sizeof(length), error);
    for (size_t i = 0; status == MC_WORLD_STORAGE_OK && i < name.length; ++i) {
        uint8_t bytes[3];
        size_t written = 0;
        (void)mc_mutf8_encode(&name.units[i], 1, bytes, sizeof(bytes), &written);
        status = mc_ws_output_write(output, bytes, written, error);
    }
    return status;
}

static mc_world_storage_status saved_write(mc_world_saved_data *owner,
                                           mc_world_storage_error *error)
{
    if (owner->path == NULL)
        return MC_WORLD_STORAGE_OK;
    const mc_nbt_string empty = {0};
    if (saved_find(owner, NULL) != NULL && saved_find(owner, &empty) != NULL)
        return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "saved data duplicate-name collection order");
    const mc_nbt_limits *limits = &owner->limits.nbt;
    if (owner->count >= limits->max_tags || owner->count > limits->max_elements ||
        (owner->count != 0 && limits->max_depth < 2))
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data output tree bounds");
    mc_ws_output output;
    mc_world_storage_status status =
        mc_ws_output_open(owner->path, limits->max_output_bytes, &output, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    const uint8_t header[] = {MC_NBT_COMPOUND, 0, 0};
    status = mc_ws_output_write(&output, header, sizeof(header), error);
    for (saved_id_entry *entry = owner->first; status == MC_WORLD_STORAGE_OK && entry != NULL;
         entry = entry->next) {
        const uint8_t type = MC_NBT_SHORT;
        status = mc_ws_output_write(&output, &type, 1, error);
        if (status == MC_WORLD_STORAGE_OK)
            status = saved_write_name(&output, entry->id.name, error);
        if (status == MC_WORLD_STORAGE_OK) {
            uint16_t bits = (uint16_t)entry->id.value;
            uint8_t value[] = {(uint8_t)(bits >> 8), (uint8_t)bits};
            status = mc_ws_output_write(&output, value, sizeof(value), error);
        }
    }
    if (status == MC_WORLD_STORAGE_OK) {
        const uint8_t end = MC_NBT_END;
        status = mc_ws_output_write(&output, &end, 1, error);
    }
    mc_world_storage_error close_error;
    mc_world_storage_status closed = mc_ws_output_close(&output, &close_error);
    if (status == MC_WORLD_STORAGE_OK && closed != MC_WORLD_STORAGE_OK) {
        status = closed;
        if (error != NULL)
            *error = close_error;
    }
    return status;
}

mc_world_storage_status mc_world_saved_data_next_id(mc_world_saved_data *owner,
                                                    const mc_nbt_string *key, int32_t *out,
                                                    mc_world_storage_error *error)
{
    if (owner == NULL || out == NULL || !saved_key_valid(key))
        return saved_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "saved data ID arguments");
    owner->last_caught = (mc_world_storage_error){0};
    saved_id_entry *entry = saved_find(owner, key);
    mc_world_storage_error detail = {0};
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    if (entry == NULL)
        status = saved_insert(owner, key, 0, &entry, &detail);
    else
        entry->id.value = mc_java_i16_narrow((int32_t)entry->id.value + 1);
    if (status == MC_WORLD_STORAGE_OK)
        status = saved_write(owner, &detail);
    if (saved_catch(owner, status, &detail))
        status = MC_WORLD_STORAGE_OK;
    if (status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = detail;
        return status;
    }
    *out = entry->id.value;
    return saved_error(error, MC_WORLD_STORAGE_OK, NULL);
}

size_t mc_world_saved_data_count(const mc_world_saved_data *owner)
{
    return owner == NULL ? 0 : owner->count;
}

const mc_world_saved_id *mc_world_saved_data_id_at(const mc_world_saved_data *owner, size_t index)
{
    if (owner == NULL)
        return NULL;
    const saved_id_entry *entry = owner->first;
    while (entry != NULL && index != 0) {
        entry = entry->next;
        --index;
    }
    return entry == NULL ? NULL : &entry->id;
}

const mc_world_storage_error *mc_world_saved_data_last_caught(const mc_world_saved_data *owner)
{
    return owner == NULL ? NULL : &owner->last_caught;
}

void mc_world_saved_data_free(mc_world_saved_data *owner)
{
    if (owner == NULL)
        return;
    while (owner->first != NULL) {
        saved_id_entry *entry = owner->first;
        owner->first = entry->next;
        free(entry);
    }
    free(owner->path);
    free(owner);
}
