#include "../../block/registry/registry_internal.h"
#include "registry_internal.h"

#include <limits.h>

static mc_registry_status name_error(mc_registry_classes *classes, const char *client,
                                     const char *server, int32_t pc)
{
    classes->last_error.constructor_class =
        classes->options.side == MC_REGISTRY_CLIENT ? client : server;
    classes->last_error.constructor_pc = pc;
    return classes->last_error.status;
}

static mc_registry_status name_fail(mc_registry_classes *classes, const char *client,
                                    const char *server, int32_t pc, mc_registry_status status,
                                    const char *message)
{
    (void)mc_registry_fail(classes, status, message, NULL);
    return name_error(classes, client, server, pc);
}

/* uu.n/na.k and gm.k/ej.j append the raw virtual name to ".name".
 * AbstractStringBuilder.append(String) appends literal "null" for NULL.
 * Native immutable result storage replaces the unexposed temporary builder;
 * it is charged to the same retained class arena. */
static mc_registry_status translated_name(mc_registry_classes *classes,
                                          const mc_registry_string *raw, const char *client,
                                          const char *server, const mc_registry_string **out)
{
    static const uint16_t null_units[] = {'n', 'u', 'l', 'l'};
    const mc_registry_string *suffix = NULL;
    mc_registry_status status = mc_registry_literal_ascii(classes, ".name", &suffix);
    if (status != MC_REGISTRY_OK)
        return name_error(classes, client, server, 14);

    const size_t raw_length = raw == NULL ? 4u : raw->length;
    const uint16_t *raw_units = raw == NULL ? null_units : raw->units;
    if ((raw_length != 0 && raw_units == NULL) || suffix == NULL)
        return name_fail(classes, client, server, 19, MC_REGISTRY_INVALID_ARGUMENT,
                         "Invalid native raw-name storage");
    if (raw_length > (size_t)INT32_MAX || suffix->length > (size_t)INT32_MAX - raw_length)
        return name_fail(classes, client, server, 19, MC_REGISTRY_LIMIT,
                         "Translated name length bound");
    const size_t length = raw_length + suffix->length;
    if (length > SIZE_MAX / sizeof(uint16_t))
        return name_fail(classes, client, server, 19, MC_REGISTRY_LIMIT,
                         "Translated name allocation bound");
    mc_registry_string *key = mc_registry_allocate(classes, sizeof(*key));
    if (key == NULL)
        return name_error(classes, client, server, 19);
    uint16_t *units = mc_registry_allocate(classes, length * sizeof(*units));
    if (units == NULL)
        return name_error(classes, client, server, 19);
    for (size_t i = 0; i < raw_length; ++i)
        units[i] = raw_units[i];
    for (size_t i = 0; i < suffix->length; ++i)
        units[raw_length + i] = suffix->units[i];
    *key = (mc_registry_string){units, length};

    /* Preserve a reached Translation failure and its nested initializer cause. */
    return mc_registry_translate(classes, key, out);
}

mc_registry_status mc_registry_statistic_block_name(mc_registry_classes *classes,
                                                    const mc_block *block,
                                                    const mc_registry_string **out)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0 || block == NULL || block->classes != classes || out == NULL)
        return name_fail(classes, "uu", "na", 0, MC_REGISTRY_INVALID_ARGUMENT,
                         "Block name requires an admitted live owner and output");
    /* Virtual raw o/l is the base getfield a at PC1 for every Block kind. */
    const mc_registry_string *raw = block->fields.a;
    return translated_name(classes, raw, "uu", "na", out);
}

static bool block_item_name(mc_item_kind kind)
{
    switch (kind) {
        case MC_ITEM_CK_BK:
        case MC_ITEM_BI_AP:
        case MC_ITEM_GO_EK:
        case MC_ITEM_EN_CW:
        case MC_ITEM_MR_IA:
        case MC_ITEM_UY_ND:
        case MC_ITEM_QR_KC:
            return true;
        default:
            return false;
    }
}

mc_registry_status mc_registry_statistic_item_name(mc_registry_classes *classes,
                                                   const mc_item *item,
                                                   const mc_registry_string **out)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0 || item == NULL || item->classes != classes || out == NULL)
        return name_fail(classes, "gm", "ej", 0, MC_REGISTRY_INVALID_ARGUMENT,
                         "Item name requires an admitted live owner and output");
    const mc_registry_string *raw;
    if (block_item_name(item->kind)) {
        /* ck/bk.a: getstatic Block.m at0, aaload7, raw Block call8.
         * Reload the live array, rather than retaining an ID-table projection. */
        mc_registry_status status = mc_registry_blocks_active_use(classes);
        if (status != MC_REGISTRY_OK)
            return status;
        mc_block **blocks = classes->blocks.m;
        if (blocks == NULL)
            return name_fail(classes, "ck", "bk", 7, MC_REGISTRY_NULL_REFERENCE,
                             "BlockItem raw name has a null Block array");
        const int32_t id = item->ck_a;
        if (id < 0 || (size_t)id >= classes->blocks.m_length)
            return name_fail(classes, "ck", "bk", 7, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                             "BlockItem raw name Block index");
        const mc_block *block = blocks[(size_t)id];
        if (block == NULL)
            return name_fail(classes, "ck", "bk", 8, MC_REGISTRY_NULL_REFERENCE,
                             "BlockItem raw name has a null Block receiver");
        raw = block->fields.a;
    } else {
        raw = item->fields.bl;
    }
    return translated_name(classes, raw, "gm", "ej", out);
}

mc_registry_status mc_registry_statistic_block_admitted(mc_registry_classes *classes,
                                                        const mc_block *block, bool *out)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0 || block == NULL || block->classes != classes || out == NULL)
        return name_fail(classes, "uu", "na", 0, MC_REGISTRY_INVALID_ARGUMENT,
                         "Block statistics predicate requires an admitted live owner");
    *out = block->fields.br;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_item_damageable(mc_registry_classes *classes,
                                                         const mc_item *item, bool *out)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0 || item == NULL || item->classes != classes || out == NULL)
        return name_fail(classes, "gm", "ej", 0, MC_REGISTRY_INVALID_ARGUMENT,
                         "Item durability predicate requires an admitted live owner");
    /* gm.g/ej.f: positive maximum durability, then the subtype flag gate. */
    *out = item->fields.a > 0 && !item->fields.bj;
    return MC_REGISTRY_OK;
}
