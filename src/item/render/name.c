#include "name.h"
#include "../../block/registry/registry_internal.h"
#include "../stack/source_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static mc_item_render_site at(const char *c, const char *m, const char *d, int32_t pc) {
    return (mc_item_render_site){c, m, d, pc};
}

static bool fail(mc_item_render_error *e, mc_registry_status status, const char *message) {
    e->status = status;
    e->operation = message;
    e->native_policy = e->throwable == MC_ITEM_RENDER_NO_THROWABLE;
    return false;
}

static bool source(mc_item_render_error *e, mc_registry_status status, const char *message) {
    e->throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    e->source_exception = true;
    e->exception_class = status == MC_REGISTRY_INDEX_OUT_OF_RANGE
                             ? "java/lang/ArrayIndexOutOfBoundsException"
                             : "java/lang/NullPointerException";
    return fail(e, status, message);
}

static bool registry_failed(mc_registry_classes *classes, mc_item_render_error *e) {
    mc_item_render_registry_classification(classes, &e->registry, &e->throwable,
                                           &e->exception_class);
    e->source_exception = e->throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    if (e->registry.active_initializer_class != NULL)
        e->child = at(e->registry.active_initializer_class, "<clinit>", "()V",
                      e->registry.active_initializer_pc);
    return fail(e, e->registry.status, e->registry.message);
}

static bool admit(mc_registry_classes *classes, const mc_item *item, const void *out,
                  mc_item_render_error *e) {
    if (classes == NULL || item == NULL || out == NULL || item->classes != classes)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item name entered storage/output");
    if (classes->entered != 0)
        return fail(e, MC_REGISTRY_BUSY, "Item name Registry entered");
    if (classes->options.side != MC_REGISTRY_CLIENT)
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, "Item name client declarations");
    return true;
}

typedef enum name_body { NAME_GM = 1, NAME_CK, NAME_BI, NAME_EN, NAME_KM, NAME_RX } name_body;

/* Actual getMethod(a,IZ) resolutions for all38 client runtime Item classes. */
static const name_body methods[MC_ITEM_KIND_COUNT] = {[MC_ITEM_BASE] = NAME_GM,
                                                      [MC_ITEM_SHOVEL] = NAME_GM,
                                                      [MC_ITEM_PICKAXE] = NAME_GM,
                                                      [MC_ITEM_AXE] = NAME_GM,
                                                      [MC_ITEM_FLINT_AND_STEEL] = NAME_GM,
                                                      [MC_ITEM_FOOD] = NAME_GM,
                                                      [MC_ITEM_BOW] = NAME_GM,
                                                      [MC_ITEM_COAL] = NAME_RX,
                                                      [MC_ITEM_SWORD] = NAME_GM,
                                                      [MC_ITEM_HOE] = NAME_GM,
                                                      [MC_ITEM_SEEDS] = NAME_GM,
                                                      [MC_ITEM_ARMOR] = NAME_GM,
                                                      [MC_ITEM_PAINTING] = NAME_GM,
                                                      [MC_ITEM_SIGN] = NAME_GM,
                                                      [MC_ITEM_DOOR] = NAME_GM,
                                                      [MC_ITEM_BUCKET] = NAME_GM,
                                                      [MC_ITEM_MINECART] = NAME_GM,
                                                      [MC_ITEM_SADDLE] = NAME_GM,
                                                      [MC_ITEM_REDSTONE] = NAME_GM,
                                                      [MC_ITEM_SNOWBALL] = NAME_GM,
                                                      [MC_ITEM_BOAT] = NAME_GM,
                                                      [MC_ITEM_BLOCK_PLACER] = NAME_GM,
                                                      [MC_ITEM_EGG] = NAME_GM,
                                                      [MC_ITEM_FISHING_ROD] = NAME_GM,
                                                      [MC_ITEM_DYE] = NAME_KM,
                                                      [MC_ITEM_BED] = NAME_GM,
                                                      [MC_ITEM_SHEARS] = NAME_GM,
                                                      [MC_ITEM_RECORD] = NAME_GM,
                                                      [MC_ITEM_SOUP] = NAME_GM,
                                                      [MC_ITEM_COOKIE] = NAME_GM,
                                                      [MC_ITEM_MAP] = NAME_GM,
                                                      [MC_ITEM_BLOCK] = NAME_CK,
                                                      [MC_ITEM_WOOL_BLOCK] = NAME_BI,
                                                      [MC_ITEM_LOG_BLOCK] = NAME_CK,
                                                      [MC_ITEM_SLAB_BLOCK] = NAME_EN,
                                                      [MC_ITEM_SAPLING_BLOCK] = NAME_CK,
                                                      [MC_ITEM_LEAVES_BLOCK] = NAME_CK,
                                                      [MC_ITEM_PISTON_BLOCK] = NAME_CK};

typedef struct name_temporary {
    mc_registry_string string;
    uint16_t *units;
    size_t capacity;
} name_temporary;

bool mc_item_name_workspace_init(mc_item_name_workspace *w, size_t limit) {
    if (w == NULL || w->storage != NULL || w->limit != 0 || w->bytes != 0 || w->used != 0 ||
        limit < 4u * sizeof(name_temporary))
        return false;
    name_temporary *storage = calloc(4, sizeof(*storage));
    if (storage == NULL)
        return false;
    *w = (mc_item_name_workspace){storage, limit, 4u * sizeof(*storage), 0};
    return true;
}

bool mc_item_name_workspace_reset(mc_item_name_workspace *w) {
    if (w == NULL || w->storage == NULL)
        return false;
    w->used = 0;
    return true;
}

bool mc_item_name_workspace_destroy(mc_item_name_workspace *w) {
    if (w == NULL || w->storage == NULL || w->used != 0)
        return false;
    name_temporary *storage = w->storage;
    for (size_t i = 0; i < 4; ++i)
        free(storage[i].units);
    free(storage);
    memset(w, 0, sizeof(*w));
    return true;
}

static bool temporary(mc_item_name_workspace *w, size_t length, name_temporary **out,
                      mc_item_render_error *e) {
    if (w == NULL || w->storage == NULL || w->used >= 4)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item name temporary workspace lifecycle");
    name_temporary *slot = &((name_temporary *)w->storage)[w->used];
    if (length > SIZE_MAX / sizeof(*slot->units))
        return fail(e, MC_REGISTRY_LIMIT, "Item name temporary String size");
    if (length > slot->capacity) {
        const size_t old_bytes = slot->capacity * sizeof(*slot->units);
        const size_t new_bytes = length * sizeof(*slot->units);
        if (w->bytes < old_bytes || w->bytes - old_bytes > w->limit ||
            new_bytes > w->limit - (w->bytes - old_bytes))
            return fail(e, MC_REGISTRY_LIMIT, "Item name temporary workspace byte limit");
        uint16_t *units = realloc(slot->units, new_bytes);
        if (units == NULL)
            return fail(e, MC_REGISTRY_ALLOCATION_FAILED, "Item name temporary String allocation");
        slot->units = units;
        slot->capacity = length;
        w->bytes = w->bytes - old_bytes + new_bytes;
    }
    slot->string = (mc_registry_string){slot->units, length};
    ++w->used;
    *out = slot;
    return true;
}

/* The unexposed builder appends literal "null" for a NULL String. Distinct
 * successful builder results use distinct workspace slots until reset. */
static bool concatenate(mc_item_name_workspace *w, const mc_registry_string *first,
                        const mc_registry_string *second, const char *ascii,
                        const mc_registry_string **out, mc_item_render_error *e) {
    static const uint16_t null_units[] = {'n', 'u', 'l', 'l'};
    const size_t a = first == NULL ? 4u : first->length;
    const size_t b = second == NULL ? 4u : second->length;
    const size_t middle = strlen(ascii);
    if (a > (size_t)INT32_MAX || b > (size_t)INT32_MAX - a || middle > (size_t)INT32_MAX - a - b)
        return fail(e, MC_REGISTRY_LIMIT, "Item name native String length bound");
    const uint16_t *au = first == NULL ? null_units : first->units;
    const uint16_t *bu = second == NULL ? null_units : second->units;
    if ((a != 0 && au == NULL) || (b != 0 && bu == NULL))
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item name UTF16 storage");
    name_temporary *result;
    if (!temporary(w, a + middle + b, &result, e))
        return false;
    for (size_t i = 0; i < a; ++i)
        result->units[i] = au[i];
    for (size_t i = 0; i < middle; ++i)
        result->units[a + i] = (unsigned char)ascii[i];
    for (size_t i = 0; i < b; ++i)
        result->units[a + middle + i] = bu[i];
    *out = &result->string;
    return true;
}

static bool block_name(mc_registry_classes *classes, const mc_item *item,
                       const mc_registry_string **out, mc_item_render_error *e) {
    e->site = at("ck", "a", "()Ljava/lang/String;", 0);
    ++classes->entered;
    const mc_registry_status status = mc_registry_blocks_active_use(classes);
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        e->registry = classes->last_error;
        return registry_failed(classes, e);
    }
    mc_block *const *captured = classes->blocks.m;
    const size_t length = classes->blocks.m_length;
    e->site.pc = 4;
    const int32_t id = item->ck_a;
    e->site.pc = 7;
    if (captured == NULL)
        return source(e, MC_REGISTRY_NULL_REFERENCE, "BlockItem captured Block array is null");
    if (id < 0 || (size_t)id >= length)
        return source(e, MC_REGISTRY_INDEX_OUT_OF_RANGE, "BlockItem actual Block index");
    const mc_block *block = captured[(size_t)id];
    e->site.pc = 8;
    if (block == NULL)
        return source(e, MC_REGISTRY_NULL_REFERENCE, "BlockItem actual Block name receiver");
    e->child = at("uu", "o", "()Ljava/lang/String;", 4);
    *out = block->fields.a; /* Every actual UU.o override resolves to this field body. */
    e->site.pc = 11;
    return true;
}

static bool damage(mc_item_stack *stack, int32_t *out, mc_item_render_error *e) {
    if (stack == NULL)
        return source(e, MC_REGISTRY_NULL_REFERENCE, "Item name actual IZ.i null receiver");
    e->child = at("iz", "i", "()I", 1);
    e->stack = mc_item_stack_source_damage_get(stack, out);
    if (e->stack != MC_ITEM_STACK_OK)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item name actual IZ damage storage");
    e->damage = *out;
    e->damage_read = true;
    e->child.pc = 4;
    return true;
}

static bool entered(mc_registry_classes *classes, const mc_item *item, mc_item_stack *stack,
                    mc_item_name_workspace *w, const mc_registry_string **out,
                    mc_item_render_error *e) {
    if (!admit(classes, item, out, e))
        return false;
    if ((unsigned int)item->kind >= MC_ITEM_KIND_COUNT || methods[item->kind] == 0)
        return fail(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, "Item name actual declaration");
    const name_body body = methods[item->kind];
    const mc_registry_string *base = NULL;
    if (body == NAME_GM) {
        e->site = at("gm", "a", "(Liz;)Ljava/lang/String;", 1);
        *out = item->fields.bl;
        e->site.pc = 4;
        return true;
    }
    if (body == NAME_CK) {
        if (!block_name(classes, item, out, e)) {
            e->site.descriptor = "(Liz;)Ljava/lang/String;";
            return false;
        }
        e->site.descriptor = "(Liz;)Ljava/lang/String;";
        return true;
    }
    if (body == NAME_RX) {
        e->site = at("rx", "a", "(Liz;)Ljava/lang/String;", 1);
        int32_t value;
        if (!damage(stack, &value, e))
            return false;
        e->site.pc = value == 1 ? 8 : 11;
        if (mc_registry_literal_ascii(classes, value == 1 ? "item.charcoal" : "item.coal", out) !=
            MC_REGISTRY_OK) {
            e->registry = classes->last_error;
            return registry_failed(classes, e);
        }
        e->site.pc = value == 1 ? 10 : 13;
        return true;
    }
    const char *owner = body == NAME_BI ? "bi" : body == NAME_EN ? "en" : "km";
    if (body == NAME_KM)
        base = item->fields.bl;
    else if (!block_name(classes, item, &base, e))
        return false;
    e->site = at(owner, "a", "(Liz;)Ljava/lang/String;", 19);
    ++classes->entered;
    const mc_registry_status status =
        body == NAME_EN ? mc_registry_block_class_initialize(classes, MC_BLOCK_SLAB)
                        : mc_registry_item_class_initialize(classes, MC_ITEM_DYE);
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        e->registry = classes->last_error;
        return registry_failed(classes, e);
    }
    const mc_registry_string *const *captured =
        body == NAME_EN ? classes->ys_a : classes->items.km_a;
    e->site.pc = 23;
    int32_t index;
    if (!damage(stack, &index, e))
        return false;
    if (body == NAME_BI)
        index = (~index) & 15; /* Actual EE.d. */
    e->site.pc = body == NAME_BI ? 29 : 26;
    if (captured == NULL)
        return source(e, MC_REGISTRY_NULL_REFERENCE, "Item name captured metadata array is null");
    if (index < 0 || index >= (body == NAME_EN ? 4 : 16))
        return source(e, MC_REGISTRY_INDEX_OUT_OF_RANGE, "Item name actual metadata array index");
    e->site.pc = body == NAME_BI ? 33 : 30;
    if (!concatenate(w, base, captured[(size_t)index], ".", out, e))
        return false;
    e->site.pc = body == NAME_BI ? 36 : 33;
    return true;
}

bool mc_item_render_name(mc_registry_classes *classes, const mc_item *item, mc_item_stack *stack,
                         mc_item_name_workspace *w, const mc_registry_string **out,
                         mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->stack_identity = (uintptr_t)stack;
    e->item_identity = (uintptr_t)item;
    const bool result = entered(classes, item, stack, w, out, e);
    e->returned = result;
    return result;
}

bool mc_item_stack_render_name(mc_registry_classes *classes, mc_item_stack *stack,
                               mc_item_name_workspace *w, const mc_registry_string **out,
                               mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->stack_identity = (uintptr_t)stack;
    if (stack == NULL || out == NULL)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item name entered IZ storage/output");
    e->site = at("iz", "l", "()Ljava/lang/String;", 0);
    const mc_registry_item_array *array;
    if (mc_registry_item_c_get(classes, &array, &e->registry) != MC_REGISTRY_OK)
        return registry_failed(classes, e);
    e->array_identity = (uintptr_t)array;
    e->site.pc = 4;
    e->stack = mc_item_stack_source_id_get(stack, &e->id);
    if (e->stack != MC_ITEM_STACK_OK)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item name actual IZ id storage");
    e->id_read = true;
    e->site.pc = 7;
    const mc_item *item;
    if (mc_registry_item_c_at(classes, array, e->id, &item, &e->registry) != MC_REGISTRY_OK)
        return registry_failed(classes, e);
    e->item_identity = (uintptr_t)item;
    e->site.pc = 9;
    if (item == NULL)
        return source(e, MC_REGISTRY_NULL_REFERENCE, "Item name captured GM invoke receiver");
    e->ancestors[0] = e->site;
    e->ancestor_count = 1;
    if (!entered(classes, item, stack, w, out, e))
        return false;
    e->ancestor_count = 0;
    e->site = at("iz", "l", "()Ljava/lang/String;", 12);
    e->returned = true;
    return true;
}

bool mc_item_name_translation_get(mc_registry_classes *classes, const mc_translation **out,
                                  mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->site = at("nh", "a", "()Lnh;", 0);
    if (classes == NULL || out == NULL)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item tooltip NH output/storage");
    if (mc_registry_classes_initialize_translation_singleton(classes, &e->registry) !=
        MC_REGISTRY_OK)
        return registry_failed(classes, e);
    *out = classes->translations.singleton_a;
    e->site.pc = 3;
    e->returned = true;
    return true;
}

bool mc_item_name_tooltip_translate(mc_registry_classes *classes, const mc_translation *translation,
                                    const mc_registry_string *raw, mc_item_name_workspace *w,
                                    const mc_registry_string **out, mc_item_render_error *e) {
    if (e == NULL)
        return false;
    mc_item_render_error_init(e);
    e->site = at("nh", "b", "(Ljava/lang/String;)Ljava/lang/String;", 1);
    if (translation == NULL)
        return source(e, MC_REGISTRY_NULL_REFERENCE, "Item tooltip captured NH receiver");
    if (classes == NULL || out == NULL || translation->classes != classes)
        return fail(e, MC_REGISTRY_INVALID_ARGUMENT, "Item tooltip actual NH ownership/output");
    if (classes->entered != 0)
        return fail(e, MC_REGISTRY_BUSY, "Item tooltip Registry entered");
    const mc_registry_string *suffix, *empty;
    if (mc_registry_literal_ascii(classes, ".name", &suffix) != MC_REGISTRY_OK ||
        mc_registry_literal_ascii(classes, "", &empty) != MC_REGISTRY_OK) {
        e->registry = classes->last_error;
        return registry_failed(classes, e);
    }
    e->site.pc = 20;
    const mc_registry_string *key;
    if (!concatenate(w, raw, suffix, "", &key, e))
        return false;
    e->site.pc = 25;
    const mc_registry_string *translated;
    if (mc_translation_property(translation, key->units, key->length, &translated, &e->registry) !=
        MC_REGISTRY_OK)
        return registry_failed(classes, e);
    if (translated == NULL)
        translated = empty; /* NH.b uses getProperty(key, ""). */
    e->site = at("id", "a", "(IIF)V", 379);
    const mc_registry_string *outer;
    if (!concatenate(w, empty, translated, "", &outer, e))
        return false;
    e->site.pc = 382;
    size_t begin = 0, end = outer->length;
    while (begin < end && outer->units[begin] <= 0x20)
        ++begin;
    while (end > begin && outer->units[end - 1] <= 0x20)
        --end;
    if (begin == 0 && end == outer->length) {
        *out = outer;
    } else if (begin == end) {
        *out = empty;
    } else {
        name_temporary *trimmed;
        if (!temporary(w, 0, &trimmed, e))
            return false;
        trimmed->string = (mc_registry_string){outer->units + begin, end - begin};
        *out = &trimmed->string;
    }
    e->site.pc = 385;
    e->returned = true;
    return true;
}
