#include "mc/world/saved_data/storage.h"

#include "files.h"
#include "mc/core/java/java.h"
#include "mc/core/text/mutf8.h"
#include "mc/storage/compression/compression.h"
#include "mc/world/storage/store.h"

#include <stdlib.h>
#include <string.h>

/* Older MSVC diagnoses this standard C17 flexible array as an extension. */
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
typedef struct saved_id_entry {
    mc_world_saved_id id;
    struct saved_id_entry *next;
    uint16_t units[];
} saved_id_entry;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

typedef struct saved_named_entry {
    mc_world_info_name *key;
    mc_saved_data_value *value;
    struct saved_named_entry *next;
} saved_named_entry;

typedef struct saved_named_cache {
    saved_named_entry *first, *last;
    size_t count;
} saved_named_cache;

typedef struct saved_ordered_list {
    mc_saved_data_value **items;
    size_t count, capacity;
} saved_ordered_list;

typedef struct saved_id_map {
    saved_id_entry *first, *last;
    size_t count;
} saved_id_map;

typedef struct saved_storage {
    char *path;
} saved_storage;

struct mc_world_saved_data {
    mc_world_saved_data_limits limits;
    saved_named_cache named_cache;
    saved_ordered_list ordered_list;
    saved_id_map id_map;
    saved_storage storage;
    size_t key_units, allocated;
    mc_world_storage_error last_caught;
    mc_saved_data_error last_value_error;
    bool class_bound;
    mc_world_info_side side;
};

static mc_world_storage_status saved_error(mc_world_storage_error *error,
                                           mc_world_storage_status status, const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status saved_nbt_error(mc_world_storage_error *error,
                                               mc_nbt_status status) {
    mc_world_storage_status mapped = status == MC_NBT_ALLOCATION_FAILED
                                         ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                     : status == MC_NBT_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                              : MC_WORLD_STORAGE_INVALID_DATA;
    return saved_error(error, mapped, mc_nbt_status_string(status));
}

static bool saved_catch(mc_world_saved_data *owner, mc_world_storage_status status,
                        const mc_world_storage_error *error) {
    if (status != MC_WORLD_STORAGE_IO_ERROR && status != MC_WORLD_STORAGE_MISSING &&
        status != MC_WORLD_STORAGE_INVALID_DATA)
        return false;
    owner->last_caught = *error;
    return true;
}

mc_world_saved_data_limits mc_world_saved_data_default_limits(void) {
    return (mc_world_saved_data_limits){4096, 65536, 4u * 1024u * 1024u, mc_nbt_default_limits(),
                                        4096, 4096};
}

static bool saved_key_valid(const mc_nbt_string *key) {
    return key == NULL || ((key->length == 0 || key->units != NULL) &&
                           key->length <= SIZE_MAX / sizeof(*key->units));
}

static saved_id_entry *saved_find(mc_world_saved_data *owner, const mc_nbt_string *key) {
    for (saved_id_entry *entry = owner->id_map.first; entry != NULL; entry = entry->next) {
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
                                            mc_world_storage_error *error) {
    size_t units = key == NULL ? 0 : key->length;
    if (owner->id_map.count >= owner->limits.max_ids ||
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
    if (owner->id_map.last == NULL)
        owner->id_map.first = entry;
    else
        owner->id_map.last->next = entry;
    owner->id_map.last = entry;
    ++owner->id_map.count;
    owner->key_units += units;
    owner->allocated += bytes;
    *out = entry;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status saved_load(mc_world_saved_data *owner,
                                          mc_world_storage_error *error) {
    if (owner->storage.path == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_ws_file_bytes bytes = {0};
    mc_world_storage_status status =
        mc_ws_read_file(owner->storage.path, owner->limits.nbt.max_input_bytes, &bytes, error);
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
                                                   mc_world_storage_error *error) {
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
    /* Distinct native collection headers live inside the charged owner. Their
     * construction does not stand in for Java collection allocation timing. */
    owner->named_cache = (saved_named_cache){0};
    owner->ordered_list = (saved_ordered_list){0};
    owner->id_map = (saved_id_map){0};
    mc_world_storage_error detail = {0};
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    if (store != NULL) {
        const char *root = mc_world_store_root(store);
        size_t bound = strlen(root) + sizeof("data/idcounts.dat") + 1u;
        if (bound > chosen.max_allocation_bytes - owner->allocated) {
            status = saved_error(&detail, MC_WORLD_STORAGE_LIMIT, "saved data path budget");
        } else {
            status = mc_ws_path_join(root, "data/idcounts.dat", &owner->storage.path, &detail);
            if (status == MC_WORLD_STORAGE_OK)
                owner->allocated += strlen(owner->storage.path) + 1u;
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

mc_world_storage_status
mc_world_saved_data_create_for_side(const mc_world_store *store, mc_world_info_side side,
                                    const mc_world_saved_data_limits *limits,
                                    mc_world_saved_data **out, mc_world_storage_error *error) {
    if (side != MC_WORLD_INFO_CLIENT && side != MC_WORLD_INFO_SERVER)
        return saved_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "saved data manager side");
    mc_world_storage_status status = mc_world_saved_data_create(store, limits, out, error);
    if (status == MC_WORLD_STORAGE_OK) {
        (*out)->class_bound = true;
        (*out)->side = side;
    }
    return status;
}

static bool named_key_equal(const mc_world_info_name *left, const mc_world_info_name *right) {
    if (left == NULL || right == NULL)
        return left == right;
    mc_nbt_string a = mc_world_info_name_text(left), b = mc_world_info_name_text(right);
    return a.length == b.length &&
           (a.length == 0 || memcmp(a.units, b.units, a.length * sizeof(*a.units)) == 0);
}

static saved_named_entry **named_find(mc_world_saved_data *owner, const mc_world_info_name *key) {
    saved_named_entry **link = &owner->named_cache.first;
    while (*link != NULL && !named_key_equal((*link)->key, key))
        link = &(*link)->next;
    return link;
}

static void named_remove(mc_world_saved_data *owner, saved_named_entry **link) {
    saved_named_entry *entry = *link;
    *link = entry->next;
    if (owner->named_cache.last == entry) {
        owner->named_cache.last = owner->named_cache.first;
        while (owner->named_cache.last != NULL && owner->named_cache.last->next != NULL)
            owner->named_cache.last = owner->named_cache.last->next;
    }
    --owner->named_cache.count;
    owner->key_units -= mc_world_info_name_text(entry->key).length;
    owner->allocated -= sizeof(*entry);
    for (size_t i = 0; i < owner->ordered_list.count; ++i) {
        if (owner->ordered_list.items[i] == entry->value) {
            mc_saved_data_value_release(owner->ordered_list.items[i]);
            --owner->ordered_list.count;
            memmove(&owner->ordered_list.items[i], &owner->ordered_list.items[i + 1],
                    (owner->ordered_list.count - i) * sizeof(*owner->ordered_list.items));
            break;
        }
    }
    mc_world_info_name_release(entry->key);
    mc_saved_data_value_release(entry->value);
    free(entry);
}

static mc_world_storage_status named_append(mc_world_saved_data *owner, mc_saved_data_value *value,
                                            mc_world_storage_error *error) {
    saved_ordered_list *list = &owner->ordered_list;
    if (list->count >= owner->limits.max_ordered_entries)
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data ordered capacity");
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 4 : list->capacity;
        if (capacity <= SIZE_MAX / 2 && list->capacity != 0)
            capacity *= 2;
        if (capacity > owner->limits.max_ordered_entries)
            capacity = owner->limits.max_ordered_entries;
        if (capacity <= list->count || capacity > SIZE_MAX / sizeof(*list->items))
            return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data ordered growth");
        size_t bytes = capacity * sizeof(*list->items);
        /* realloc may hold both buffers, so budget its entire new allocation. */
        if (bytes > owner->limits.max_allocation_bytes - owner->allocated)
            return saved_error(error, MC_WORLD_STORAGE_LIMIT,
                               "saved data ordered allocation budget");
        mc_saved_data_value **items = realloc(list->items, bytes);
        if (items == NULL)
            return saved_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                               "saved data ordered list");
        owner->allocated += (capacity - list->capacity) * sizeof(*list->items);
        list->items = items;
        list->capacity = capacity;
    }
    mc_world_storage_status status = mc_saved_data_value_retain(value);
    if (status != MC_WORLD_STORAGE_OK)
        return saved_error(error, status, "saved data ordered value hold");
    list->items[list->count++] = value;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_saved_data_put(mc_world_saved_data *owner, mc_world_info_name *key,
                                                mc_saved_data_value *value,
                                                mc_world_storage_error *error) {
    if (owner == NULL || value == NULL)
        return saved_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "saved data put arguments");
    if (owner->class_bound &&
        mc_saved_data_value_class(value) != mc_saved_data_map_class(owner->side))
        return saved_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "saved data value base type");
    /* Java arguments keep even a borrowed old entry alive through removal. */
    mc_world_storage_status status = mc_saved_data_value_retain(value);
    if (status != MC_WORLD_STORAGE_OK)
        return saved_error(error, status, "saved data put argument hold");
    if (key != NULL && mc_world_info_name_retain(key) != MC_WORLD_INFO_OK) {
        mc_saved_data_value_release(value);
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data put key hold");
    }
    saved_named_entry **link = named_find(owner, key);
    if (*link != NULL)
        named_remove(owner, link);
    size_t units = mc_world_info_name_text(key).length;
    if (owner->named_cache.count >= owner->limits.max_named_entries ||
        units > owner->limits.max_key_units - owner->key_units ||
        sizeof(saved_named_entry) > owner->limits.max_allocation_bytes - owner->allocated) {
        status = saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data named capacity");
    } else {
        saved_named_entry *entry = malloc(sizeof(*entry));
        if (entry == NULL) {
            status =
                saved_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "saved data named entry");
        } else {
            *entry = (saved_named_entry){key, value, NULL};
            if (owner->named_cache.last == NULL)
                owner->named_cache.first = entry;
            else
                owner->named_cache.last->next = entry;
            owner->named_cache.last = entry;
            ++owner->named_cache.count;
            owner->key_units += units;
            owner->allocated += sizeof(*entry);
            /* Transfer the argument holds to the actual map before list append. */
            key = NULL;
            value = NULL;
            status = named_append(owner, entry->value, error);
        }
    }
    mc_world_info_name_release(key);
    mc_saved_data_value_release(value);
    return status == MC_WORLD_STORAGE_OK ? saved_error(error, status, NULL) : status;
}

/* The copied idcounts path also binds the actual handler's data directory.
 * Valid UTF-16 scalar sequences use native UTF-8 transport; malformed strings
 * have an explicit unqualified native filename failure, rather than lossy text. */
static mc_world_storage_status named_path(mc_world_saved_data *owner, mc_world_info_name *name,
                                          char **out, mc_world_storage_error *error) {
    mc_nbt_string text = mc_world_info_name_text(name);
    size_t prefix = strlen(owner->storage.path) - strlen("idcounts.dat");
    size_t encoded = name == NULL ? 4 : 0;
    for (size_t i = 0; name != NULL && i < text.length; ++i) {
        uint32_t unit = text.units[i];
        if (unit == 0 || (unit >= 0xdc00 && unit <= 0xdfff))
            return saved_error(error, MC_WORLD_STORAGE_INVALID_DATA, "saved data native filename");
        if (unit >= 0xd800 && unit <= 0xdbff) {
            if (++i == text.length || text.units[i] < 0xdc00 || text.units[i] > 0xdfff)
                return saved_error(error, MC_WORLD_STORAGE_INVALID_DATA,
                                   "saved data native filename");
            encoded += 4;
        } else {
            encoded += unit < 0x80 ? 1u : unit < 0x800 ? 2u : 3u;
        }
        if (encoded > MC_WS_PATH_MAX_BYTES)
            return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data filename length");
    }
    if (prefix > MC_WS_PATH_MAX_BYTES || encoded + 4 > MC_WS_PATH_MAX_BYTES - prefix)
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data filename length");
    size_t bytes = prefix + encoded + sizeof(".dat");
    if (bytes > owner->limits.max_allocation_bytes - owner->allocated)
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data filename budget");
    char *path = malloc(bytes);
    if (path == NULL)
        return saved_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "saved data filename");
    memcpy(path, owner->storage.path, prefix);
    size_t at = prefix;
    if (name == NULL) {
        memcpy(path + at, "null", 4);
        at += 4;
    } else {
        for (size_t i = 0; i < text.length; ++i) {
            uint32_t point = text.units[i];
            if (point >= 0xd800 && point <= 0xdbff) {
                point = 0x10000u + ((point - 0xd800u) << 10) + (text.units[++i] - 0xdc00u);
            }
            if (point < 0x80) {
                path[at++] = (char)point;
            } else if (point < 0x800) {
                path[at++] = (char)(0xc0u | (point >> 6));
                path[at++] = (char)(0x80u | (point & 0x3fu));
            } else if (point < 0x10000) {
                path[at++] = (char)(0xe0u | (point >> 12));
                path[at++] = (char)(0x80u | ((point >> 6) & 0x3fu));
                path[at++] = (char)(0x80u | (point & 0x3fu));
            } else {
                path[at++] = (char)(0xf0u | (point >> 18));
                path[at++] = (char)(0x80u | ((point >> 12) & 0x3fu));
                path[at++] = (char)(0x80u | ((point >> 6) & 0x3fu));
                path[at++] = (char)(0x80u | (point & 0x3fu));
            }
        }
    }
    memcpy(path + at, ".dat", sizeof(".dat"));
    owner->allocated += bytes;
    *out = path;
    return MC_WORLD_STORAGE_OK;
}

static void named_path_free(mc_world_saved_data *owner, char *path) {
    if (path != NULL) {
        owner->allocated -= strlen(path) + 1;
        free(path);
    }
}

static mc_compression_limits named_compression_limits(const mc_world_saved_data *owner,
                                                      bool writing) {
    return (mc_compression_limits){
        writing ? owner->limits.nbt.max_output_bytes : owner->limits.nbt.max_input_bytes,
        writing ? owner->limits.nbt.max_output_bytes : owner->limits.nbt.max_input_bytes,
        owner->limits.max_allocation_bytes, 1024};
}

static mc_world_storage_status named_compression_error(mc_world_storage_error *error,
                                                       mc_compression_status status) {
    mc_world_storage_status mapped = status == MC_COMPRESSION_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                     : status == MC_COMPRESSION_ALLOCATION_FAILED
                                         ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                         : MC_WORLD_STORAGE_INVALID_DATA;
    return saved_error(error, mapped, mc_compression_status_string(status));
}

static mc_world_storage_status named_read(mc_world_saved_data *owner, const char *path,
                                          mc_saved_data_value *value,
                                          mc_world_storage_error *error) {
    mc_ws_file_bytes file = {0};
    mc_world_storage_status status =
        mc_ws_read_file(path, owner->limits.nbt.max_input_bytes, &file, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_compression_limits limits = named_compression_limits(owner, false);
    mc_compression_buffer decoded = {0};
    mc_compression_status compressed =
        mc_compression_decode(MC_COMPRESSION_GZIP, file.data, file.length, &limits, &decoded, NULL);
    mc_ws_free_file_bytes(&file);
    if (compressed != MC_COMPRESSION_OK)
        return named_compression_error(error, compressed);
    mc_nbt_document *document = NULL;
    mc_nbt_status nbt = mc_nbt_read_compound(decoded.data, decoded.length, &owner->limits.nbt,
                                             &document, NULL, NULL);
    mc_compression_buffer_free(&decoded);
    if (nbt != MC_NBT_OK)
        return saved_nbt_error(error, nbt);
    static const uint16_t data_name[] = {'d', 'a', 't', 'a'};
    const mc_nbt_tag *data = mc_nbt_compound_get(mc_nbt_document_root(document), data_name, 4);
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    if (data != NULL && data->type != MC_NBT_COMPOUND) {
        owner->last_value_error = (mc_saved_data_error){
            .cause = {MC_WORLD_STORAGE_INVALID_DATA, "saved data Compound cast", 0},
            .exception = MC_SAVED_DATA_CLASS_CAST,
            .bytecode = 145,
            .getter_bytecode = 24,
            .field = "data"};
        status = MC_WORLD_STORAGE_INVALID_DATA;
    } else {
        status = mc_saved_data_map_read_document(value, document, data == NULL ? &empty : data,
                                                 owner->limits.max_allocation_bytes,
                                                 &owner->last_value_error);
    }
    if (status != MC_WORLD_STORAGE_OK && error != NULL)
        *error = owner->last_value_error.cause;
    mc_nbt_document_free(document);
    return status;
}

mc_world_storage_status mc_world_saved_data_get(mc_world_saved_data *owner,
                                                const mc_saved_data_class *type,
                                                mc_world_info_name *key, mc_saved_data_value **out,
                                                mc_world_storage_error *error) {
    if (owner == NULL || out == NULL)
        return saved_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "saved data get arguments");
    saved_named_entry *entry = *named_find(owner, key);
    if (entry != NULL) {
        mc_world_storage_status status = mc_saved_data_value_retain(entry->value);
        if (status != MC_WORLD_STORAGE_OK)
            return saved_error(error, status, "saved data cache-hit hold");
        *out = entry->value;
        return saved_error(error, MC_WORLD_STORAGE_OK, NULL);
    }
    if (owner->storage.path == NULL) {
        *out = NULL;
        return saved_error(error, MC_WORLD_STORAGE_OK, NULL);
    }
    char *path = NULL;
    mc_world_storage_error detail = {0};
    mc_world_storage_status status = named_path(owner, key, &path, &detail);
    bool exists = false;
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_ws_path_exists(path, &exists, &detail);
    mc_saved_data_value *value = NULL;
    if (status == MC_WORLD_STORAGE_OK && exists) {
        mc_world_info_side side;
        if (type == NULL) {
            owner->last_value_error = (mc_saved_data_error){
                .cause = {MC_WORLD_STORAGE_INVALID_DATA, "saved data null Class", 0},
                .exception = MC_SAVED_DATA_NULL_REFERENCE,
                .bytecode = 62};
            detail = owner->last_value_error.cause;
            status = detail.status;
        } else if (!mc_saved_data_class_side(type, &side)) {
            status = saved_error(&detail, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "saved data constructor Class");
        } else {
            status = mc_saved_data_map_create(type, key, owner->limits.max_allocation_bytes, &value,
                                              &owner->last_value_error);
            if (status != MC_WORLD_STORAGE_OK)
                detail = owner->last_value_error.cause;
            else if (owner->class_bound && side != owner->side) {
                /* newInstance73 runs before checkcast76; local79 is not set. */
                mc_saved_data_value_release(value);
                value = NULL;
                owner->last_value_error = (mc_saved_data_error){
                    .cause = {MC_WORLD_STORAGE_INVALID_DATA, "saved data constructed base cast", 0},
                    .exception = MC_SAVED_DATA_CLASS_CAST,
                    .bytecode = 76};
                detail = owner->last_value_error.cause;
                status = detail.status;
            } else
                status = named_read(owner, path, value, &detail);
        }
    }
    named_path_free(owner, path);
    if (saved_catch(owner, status, &detail))
        status = MC_WORLD_STORAGE_OK;
    if (status == MC_WORLD_STORAGE_OK && value != NULL)
        status = mc_world_saved_data_put(owner, key, value, &detail);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_saved_data_value_release(value);
        if (error != NULL)
            *error = detail;
        return status;
    }
    *out = value;
    return saved_error(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status named_write(mc_world_saved_data *owner, mc_saved_data_value *value,
                                           mc_world_storage_error *error) {
    if (owner->storage.path == NULL)
        return MC_WORLD_STORAGE_OK;
    char *path = NULL;
    mc_world_storage_status status =
        named_path(owner, mc_saved_data_value_name(value), &path, error);
    mc_saved_data_record *record = NULL;
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_saved_data_map_write(value, owner->limits.max_allocation_bytes, &record,
                                         &owner->last_value_error);
        if (status != MC_WORLD_STORAGE_OK && error != NULL)
            *error = owner->last_value_error.cause;
    }
    mc_ws_output output = {0};
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_ws_output_open(path, owner->limits.nbt.max_output_bytes, &output, error);
    if (status == MC_WORLD_STORAGE_OK) {
        const mc_nbt_tag *data = mc_saved_data_record_root(record, &owner->last_value_error);
        if (data == NULL) {
            status = owner->last_value_error.cause.status;
            if (error != NULL)
                *error = owner->last_value_error.cause;
        } else {
            static const uint16_t data_name[] = {'d', 'a', 't', 'a'};
            mc_nbt_tag named_data = *data;
            named_data.name = (mc_nbt_string){data_name, 4};
            const mc_nbt_tag *items[] = {&named_data};
            const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {items, 1}};
            size_t length = 0;
            mc_nbt_status nbt = mc_nbt_measure(&root, &owner->limits.nbt, &length, NULL);
            uint8_t *raw = NULL;
            if (nbt != MC_NBT_OK) {
                status = saved_nbt_error(error, nbt);
            } else if (length > owner->limits.max_allocation_bytes - owner->allocated) {
                status =
                    saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data NBT staging budget");
            } else if ((raw = malloc(length)) == NULL) {
                status = saved_error(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                     "saved data NBT staging");
            } else {
                nbt = mc_nbt_write(&root, &owner->limits.nbt, raw, length, &length, NULL);
                if (nbt != MC_NBT_OK) {
                    status = saved_nbt_error(error, nbt);
                } else {
                    mc_compression_limits limits = named_compression_limits(owner, true);
                    mc_compression_buffer compressed = {0};
                    mc_compression_status result = mc_compression_encode(
                        MC_COMPRESSION_GZIP, raw, length, &limits, &compressed);
                    if (result != MC_COMPRESSION_OK)
                        status = named_compression_error(error, result);
                    else
                        status =
                            mc_ws_output_write(&output, compressed.data, compressed.length, error);
                    mc_compression_buffer_free(&compressed);
                }
            }
            free(raw);
        }
    }
    if (output.file != NULL) {
        mc_world_storage_error close_error;
        mc_world_storage_status closed = mc_ws_output_close(&output, &close_error);
        if (status == MC_WORLD_STORAGE_OK && closed != MC_WORLD_STORAGE_OK) {
            status = closed;
            if (error != NULL)
                *error = close_error;
        }
    }
    mc_saved_data_record_release(record);
    named_path_free(owner, path);
    return status;
}

static mc_world_storage_status saved_write_name(mc_ws_output *output, mc_nbt_string name,
                                                mc_world_storage_error *error) {
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
                                           mc_world_storage_error *error) {
    if (owner->storage.path == NULL)
        return MC_WORLD_STORAGE_OK;
    const mc_nbt_string empty = {0};
    if (saved_find(owner, NULL) != NULL && saved_find(owner, &empty) != NULL)
        return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "saved data duplicate-name collection order");
    const mc_nbt_limits *limits = &owner->limits.nbt;
    if (owner->id_map.count >= limits->max_tags || owner->id_map.count > limits->max_elements ||
        (owner->id_map.count != 0 && limits->max_depth < 2))
        return saved_error(error, MC_WORLD_STORAGE_LIMIT, "saved data output tree bounds");
    mc_ws_output output;
    mc_world_storage_status status =
        mc_ws_output_open(owner->storage.path, limits->max_output_bytes, &output, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    const uint8_t header[] = {MC_NBT_COMPOUND, 0, 0};
    status = mc_ws_output_write(&output, header, sizeof(header), error);
    for (saved_id_entry *entry = owner->id_map.first;
         status == MC_WORLD_STORAGE_OK && entry != NULL; entry = entry->next) {
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
                                                    mc_world_storage_error *error) {
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

mc_world_storage_status mc_world_saved_data_save(mc_world_saved_data *owner,
                                                 mc_world_storage_error *error) {
    if (owner == NULL)
        return saved_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "saved data save receiver");
    /* Reread the actual list size, and test the SAME value for each alias. */
    for (size_t i = 0; i < owner->ordered_list.count; ++i) {
        mc_saved_data_value *value = owner->ordered_list.items[i];
        if (!mc_saved_data_value_dirty(value))
            continue;
        mc_world_storage_error detail = {0};
        mc_world_storage_status status = named_write(owner, value, &detail);
        if (saved_catch(owner, status, &detail))
            status = MC_WORLD_STORAGE_OK;
        if (status != MC_WORLD_STORAGE_OK) {
            if (error != NULL)
                *error = detail;
            return status;
        }
        (void)mc_saved_data_value_dirty_set(value, false);
    }
    return saved_error(error, MC_WORLD_STORAGE_OK, NULL);
}

bool mc_world_saved_data_named_at(const mc_world_saved_data *owner, size_t index,
                                  mc_world_info_name **key, mc_saved_data_value **value) {
    if (owner == NULL || key == NULL || value == NULL)
        return false;
    saved_named_entry *entry = owner->named_cache.first;
    while (entry != NULL && index != 0) {
        entry = entry->next;
        --index;
    }
    if (entry == NULL)
        return false;
    *key = entry->key;
    *value = entry->value;
    return true;
}

mc_saved_data_value *mc_world_saved_data_ordered_at(const mc_world_saved_data *owner,
                                                    size_t index) {
    return owner == NULL || index >= owner->ordered_list.count ? NULL
                                                               : owner->ordered_list.items[index];
}

const mc_saved_data_error *mc_world_saved_data_last_value_error(const mc_world_saved_data *owner) {
    return owner == NULL ? NULL : &owner->last_value_error;
}

mc_world_storage_status mc_world_saved_data_inspect(const mc_world_saved_data *owner,
                                                    mc_world_saved_data_view *out) {
    if (owner == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out =
        (mc_world_saved_data_view){.storage = owner->storage.path == NULL ? NULL : &owner->storage,
                                   .named_cache = &owner->named_cache,
                                   .ordered_list = &owner->ordered_list,
                                   .id_map = &owner->id_map,
                                   .named_count = owner->named_cache.count,
                                   .ordered_count = owner->ordered_list.count,
                                   .id_count = owner->id_map.count,
                                   .class_bound = owner->class_bound,
                                   .side = owner->side};
    return MC_WORLD_STORAGE_OK;
}

size_t mc_world_saved_data_count(const mc_world_saved_data *owner) {
    return owner == NULL ? 0 : owner->id_map.count;
}

const mc_world_saved_id *mc_world_saved_data_id_at(const mc_world_saved_data *owner, size_t index) {
    if (owner == NULL)
        return NULL;
    const saved_id_entry *entry = owner->id_map.first;
    while (entry != NULL && index != 0) {
        entry = entry->next;
        --index;
    }
    return entry == NULL ? NULL : &entry->id;
}

const mc_world_storage_error *mc_world_saved_data_last_caught(const mc_world_saved_data *owner) {
    return owner == NULL ? NULL : &owner->last_caught;
}

void mc_world_saved_data_free(mc_world_saved_data *owner) {
    if (owner == NULL)
        return;
    while (owner->named_cache.first != NULL) {
        saved_named_entry *entry = owner->named_cache.first;
        owner->named_cache.first = entry->next;
        mc_world_info_name_release(entry->key);
        mc_saved_data_value_release(entry->value);
        free(entry);
    }
    for (size_t i = 0; i < owner->ordered_list.count; ++i)
        mc_saved_data_value_release(owner->ordered_list.items[i]);
    free(owner->ordered_list.items);
    while (owner->id_map.first != NULL) {
        saved_id_entry *entry = owner->id_map.first;
        owner->id_map.first = entry->next;
        free(entry);
    }
    free(owner->storage.path);
    free(owner);
}
