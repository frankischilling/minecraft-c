#include "../../block/registry/registry_internal.h"

/* These private entries run only on the shared owner's admitted synchronous
 * initialization flow. Public reentry is guarded by the root entry points.
 * Each actual declaring class retains its own phase, PC and first failure. */
static mc_registry_status cached_state(mc_registry_classes *classes,
                                       const mc_registry_class_state *state, bool *done)
{
    *done = state->phase != MC_REGISTRY_COLD;
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    return MC_REGISTRY_OK;
}

static void finish_state(mc_registry_classes *classes, mc_registry_class_state *state,
                         mc_registry_initializer *frame, mc_registry_status status)
{
    if (status == MC_REGISTRY_OK)
        state->phase = MC_REGISTRY_READY;
    else {
        state->phase = MC_REGISTRY_FAILED;
        state->failure = classes->last_error;
    }
    mc_registry_initializer_leave(classes, frame);
}

static mc_registry_status ancestor(mc_registry_classes *classes, mc_registry_class_state *state,
                                   const char *client, const char *server)
{
    bool done = false;
    mc_registry_status status = cached_state(classes, state, &done);
    if (done)
        return status;
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = -1;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, client, server, &state->pc);
    status = mc_registry_items_active_use(classes);
    finish_state(classes, state, &frame, status);
    return status;
}

static mc_registry_status parent_class(mc_registry_classes *classes, mc_item_kind kind)
{
    switch (kind) {
        case MC_ITEM_WC_NY:
        case MC_ITEM_AU_AH:
        case MC_ITEM_TA_LY:
            return ancestor(classes, &classes->item_dq_state, "dq", "cj");
        case MC_ITEM_BZ_BF:
        case MC_ITEM_QK_JY:
            return mc_registry_item_class_initialize(classes, MC_ITEM_YW_PX);
        case MC_ITEM_WR_OL:
            return ancestor(classes, &classes->item_cp_state, "cp", "bo");
        case MC_ITEM_BI_AP:
        case MC_ITEM_GO_EK:
        case MC_ITEM_EN_CW:
        case MC_ITEM_MR_IA:
        case MC_ITEM_UY_ND:
        case MC_ITEM_QR_KC:
            return mc_registry_item_class_initialize(classes, MC_ITEM_CK_BK);
        default:
            return mc_registry_items_active_use(classes);
    }
}

static mc_registry_status tool_blocks(mc_registry_classes *classes, mc_item_kind kind)
{
    mc_registry_block_array *array = NULL;
    size_t length = kind == MC_ITEM_WC_NY ? 8u : kind == MC_ITEM_AU_AH ? 17u : 4u;
    mc_registry_initializer_stage(classes, kind == MC_ITEM_TA_LY ? 1 : 2);
    mc_registry_status status = mc_registry_block_array_create(classes, length, &array);
    if (status != MC_REGISTRY_OK)
        return status;

        /* The indices are the literal indices of these freshly allocated arrays.
         * A live getstatic is a synchronous active use before each element store.
         * The arrays are not published until the actual final putstatic. */
#define BLOCK_ELEMENT(index, name, load_pc, store_pc)                                              \
    do {                                                                                           \
        mc_block *value = NULL;                                                                    \
        mc_registry_initializer_stage(classes, load_pc);                                           \
        status = mc_registry_block_static_load(classes, name, &value);                             \
        if (status != MC_REGISTRY_OK)                                                              \
            return status;                                                                         \
        mc_registry_initializer_stage(classes, store_pc);                                          \
        array->elements[index] = value;                                                            \
    } while (0)
    if (kind == MC_ITEM_WC_NY) {
        BLOCK_ELEMENT(0, "v", 7, 10);
        BLOCK_ELEMENT(1, "w", 13, 16);
        BLOCK_ELEMENT(2, "F", 19, 22);
        BLOCK_ELEMENT(3, "G", 25, 28);
        BLOCK_ELEMENT(4, "aT", 31, 34);
        BLOCK_ELEMENT(5, "aV", 37, 40);
        BLOCK_ELEMENT(6, "aX", 44, 47);
        BLOCK_ELEMENT(7, "aB", 51, 54);
        mc_registry_initializer_stage(classes, 55);
        classes->items.wc_bk = array;
        mc_registry_initializer_stage(classes, 58);
    } else if (kind == MC_ITEM_AU_AH) {
        BLOCK_ELEMENT(0, "x", 7, 10);
        BLOCK_ELEMENT(1, "ak", 13, 16);
        BLOCK_ELEMENT(2, "al", 19, 22);
        BLOCK_ELEMENT(3, "u", 25, 28);
        BLOCK_ELEMENT(4, "R", 31, 34);
        BLOCK_ELEMENT(5, "ap", 37, 40);
        BLOCK_ELEMENT(6, "I", 44, 47);
        BLOCK_ELEMENT(7, "aj", 51, 54);
        BLOCK_ELEMENT(8, "J", 58, 61);
        BLOCK_ELEMENT(9, "ai", 65, 68);
        BLOCK_ELEMENT(10, "H", 72, 75);
        BLOCK_ELEMENT(11, "ax", 79, 82);
        BLOCK_ELEMENT(12, "ay", 86, 89);
        BLOCK_ELEMENT(13, "aU", 93, 96);
        BLOCK_ELEMENT(14, "bc", 100, 103);
        BLOCK_ELEMENT(15, "O", 107, 110);
        BLOCK_ELEMENT(16, "P", 114, 117);
        mc_registry_initializer_stage(classes, 118);
        classes->items.au_bk = array;
        mc_registry_initializer_stage(classes, 121);
    } else {
        BLOCK_ELEMENT(0, "y", 6, 9);
        BLOCK_ELEMENT(1, "ao", 12, 15);
        BLOCK_ELEMENT(2, "K", 18, 21);
        BLOCK_ELEMENT(3, "av", 24, 27);
        mc_registry_initializer_stage(classes, 28);
        classes->items.ta_bk = array;
        mc_registry_initializer_stage(classes, 31);
    }
#undef BLOCK_ELEMENT
    return MC_REGISTRY_OK;
}

static mc_registry_status armor_arrays(mc_registry_classes *classes)
{
    mc_registry_initializer_stage(classes, 1);
    int32_t *protection = mc_registry_allocate(classes, 4u * sizeof(*protection));
    if (protection == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 6);
    protection[0] = 3;
    mc_registry_initializer_stage(classes, 11);
    protection[1] = 8;
    mc_registry_initializer_stage(classes, 16);
    protection[2] = 6;
    mc_registry_initializer_stage(classes, 20);
    protection[3] = 3;
    mc_registry_initializer_stage(classes, 21);
    classes->items.wa_bn = protection;
    mc_registry_initializer_stage(classes, 25);
    int32_t *durability = mc_registry_allocate(classes, 4u * sizeof(*durability));
    if (durability == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 31);
    durability[0] = 11;
    mc_registry_initializer_stage(classes, 36);
    durability[1] = 16;
    mc_registry_initializer_stage(classes, 41);
    durability[2] = 15;
    mc_registry_initializer_stage(classes, 46);
    durability[3] = 13;
    mc_registry_initializer_stage(classes, 47);
    classes->items.wa_bo = durability;
    mc_registry_initializer_stage(classes, 50);
    return MC_REGISTRY_OK;
}

static mc_registry_status dye_arrays(mc_registry_classes *classes)
{
    mc_registry_initializer_stage(classes, 2);
    const mc_registry_string **names = mc_registry_allocate(classes, 16u * sizeof(*names));
    if (names == NULL)
        return classes->last_error.status;
    mc_registry_status status;
#define NAME(index, text, load_pc, store_pc)                                                       \
    do {                                                                                           \
        const mc_registry_string *value = NULL;                                                    \
        mc_registry_initializer_stage(classes, load_pc);                                           \
        status = mc_registry_literal_ascii(classes, text, &value);                                 \
        if (status != MC_REGISTRY_OK)                                                              \
            return status;                                                                         \
        mc_registry_initializer_stage(classes, store_pc);                                          \
        names[index] = value;                                                                      \
    } while (0)
    NAME(0, "black", 7, 9);
    NAME(1, "red", 12, 14);
    NAME(2, "green", 17, 19);
    NAME(3, "brown", 22, 24);
    NAME(4, "blue", 27, 29);
    NAME(5, "purple", 32, 34);
    NAME(6, "cyan", 38, 40);
    NAME(7, "silver", 44, 46);
    NAME(8, "gray", 50, 52);
    NAME(9, "pink", 56, 58);
    NAME(10, "lime", 62, 64);
    NAME(11, "yellow", 68, 70);
    NAME(12, "lightBlue", 74, 76);
    NAME(13, "magenta", 80, 82);
    NAME(14, "orange", 86, 88);
    NAME(15, "white", 92, 94);
#undef NAME
    mc_registry_initializer_stage(classes, 95);
    classes->items.km_a = names;
    mc_registry_initializer_stage(classes, 100);
    int32_t *colors = mc_registry_allocate(classes, 16u * sizeof(*colors));
    if (colors == NULL)
        return classes->last_error.status;
#define COLOR(index, value, store_pc)                                                              \
    do {                                                                                           \
        mc_registry_initializer_stage(classes, store_pc);                                          \
        colors[index] = value;                                                                     \
    } while (0)
    COLOR(0, 1973019, 106);
    COLOR(1, 11743532, 111);
    COLOR(2, 3887386, 116);
    COLOR(3, 5320730, 121);
    COLOR(4, 2437522, 126);
    COLOR(5, 8073150, 131);
    COLOR(6, 2651799, 137);
    COLOR(7, 2651799, 143);
    COLOR(8, 4408131, 149);
    COLOR(9, 14188952, 155);
    COLOR(10, 4312372, 161);
    COLOR(11, 14602026, 167);
    COLOR(12, 6719955, 173);
    COLOR(13, 12801229, 179);
    COLOR(14, 15435844, 185);
    COLOR(15, 15790320, 191);
#undef COLOR
    mc_registry_initializer_stage(classes, 192);
    classes->items.km_bk = colors;
    mc_registry_initializer_stage(classes, 195);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_item_class_initialize(mc_registry_classes *classes,
                                                     mc_item_kind kind)
{
    static const char *const clients[MC_ITEM_KIND_COUNT] = {
        "gm", "wc", "au", "ta", "ye", "yw", "qz", "rx", "qd", "la", "rk", "wa", "ym",
        "vc", "hu", "bc", "rl", "sa", "ie", "dm", "vd", "lv", "mo", "cs", "km", "lp",
        "bl", "tr", "bz", "qk", "wr", "ck", "bi", "go", "en", "mr", "uy", "qr"};
    static const char *const servers[MC_ITEM_KIND_COUNT] = {
        "ej", "ny", "ah", "ly", "pl", "px", "kj", "lc", "jw", "gz", "kr", "nx", "ps",
        "ng", "fg", "al", "ks", "lg", "fk", "cf", "ni", "hn", "hx", "bv", "gu", "hk",
        "as", "mj", "bf", "jy", "ol", "bk", "ap", "ek", "cw", "ia", "nd", "kc"};
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if ((unsigned int)kind >= (unsigned int)MC_ITEM_KIND_COUNT)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Item class kind", NULL);
    if (kind == MC_ITEM_GM_EJ)
        return mc_registry_items_active_use(classes);
    mc_registry_class_state *state = &classes->item_states[kind];
    bool done = false;
    mc_registry_status status = cached_state(classes, state, &done);
    if (done)
        return status;
    state->phase = MC_REGISTRY_INITIALIZING;
    /* No source <clinit> exists for most kinds. The five actual bodies below
     * set their own first reached instruction; parent admission has no PC. */
    state->pc = -1;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, clients[kind], servers[kind], &state->pc);
    status = parent_class(classes, kind);
    if (status == MC_REGISTRY_OK) {
        switch (kind) {
            case MC_ITEM_WC_NY:
            case MC_ITEM_AU_AH:
            case MC_ITEM_TA_LY:
                status = tool_blocks(classes, kind);
                break;
            case MC_ITEM_WA_NX:
                status = armor_arrays(classes);
                break;
            case MC_ITEM_KM_GU:
                status = dye_arrays(classes);
                break;
            default:
                break;
        }
    }
    finish_state(classes, state, &frame, status);
    return status;
}

static mc_registry_status tool_entry(mc_registry_classes *classes, const char *name,
                                     int32_t ordinal, int32_t f, int32_t g, float h, int32_t i,
                                     int32_t allocation_pc, int32_t literal_pc,
                                     int32_t constructor_pc, int32_t publication_pc,
                                     mc_item_tool_material **destination)
{
    mc_registry_initializer_stage(classes, allocation_pc);
    mc_item_tool_material *value = mc_registry_allocate(classes, sizeof(*value));
    if (value == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, literal_pc);
    const mc_registry_string *argument = NULL;
    mc_registry_status status = mc_registry_literal_ascii(classes, name, &argument);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, constructor_pc);
    /* Enum(name,ordinal) precedes the four game-class stores. These fresh
     * native owners are distinct; the values array contains their identities. */
    value->name = argument;
    value->ordinal = ordinal;
    value->f = f;
    value->g = g;
    value->h = h;
    value->i = i;
    mc_registry_initializer_stage(classes, publication_pc);
    *destination = value;
    return MC_REGISTRY_OK;
}

static mc_registry_status tool_body(mc_registry_classes *classes)
{
    mc_registry_status status;
#define TOOL(name, ordinal, f, g, h, i, allocation, literal, constructor, publication, field)      \
    do {                                                                                           \
        status = tool_entry(classes, name, ordinal, f, g, h, i, allocation, literal, constructor,  \
                            publication, &classes->items.field);                                   \
        if (status != MC_REGISTRY_OK)                                                              \
            return status;                                                                         \
    } while (0)
    TOOL("WOOD", 0, 0, 59, 2.0f, 0, 0, 4, 12, 15, tool_a);
    TOOL("STONE", 1, 1, 131, 4.0f, 1, 18, 22, 32, 35, tool_b);
    TOOL("IRON", 2, 2, 250, 6.0f, 2, 38, 42, 52, 55, tool_c);
    TOOL("EMERALD", 3, 3, 1561, 8.0f, 3, 58, 62, 72, 75, tool_d);
    TOOL("GOLD", 4, 0, 32, 12.0f, 0, 78, 82, 91, 94, tool_e);
#undef TOOL
    mc_registry_initializer_stage(classes, 98);
    mc_item_tool_material **values = mc_registry_allocate(classes, 5u * sizeof(*values));
    if (values == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 103);
    mc_item_tool_material *value = classes->items.tool_a;
    mc_registry_initializer_stage(classes, 106);
    values[0] = value;
    mc_registry_initializer_stage(classes, 109);
    value = classes->items.tool_b;
    mc_registry_initializer_stage(classes, 112);
    values[1] = value;
    mc_registry_initializer_stage(classes, 115);
    value = classes->items.tool_c;
    mc_registry_initializer_stage(classes, 118);
    values[2] = value;
    mc_registry_initializer_stage(classes, 121);
    value = classes->items.tool_d;
    mc_registry_initializer_stage(classes, 124);
    values[3] = value;
    mc_registry_initializer_stage(classes, 127);
    value = classes->items.tool_e;
    mc_registry_initializer_stage(classes, 130);
    values[4] = value;
    mc_registry_initializer_stage(classes, 131);
    classes->items.tool_j = values;
    mc_registry_initializer_stage(classes, 134);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_item_tool_initialize(mc_registry_classes *classes)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_class_state *state = &classes->item_tool_state;
    bool done = false;
    mc_registry_status status = cached_state(classes, state, &done);
    if (done)
        return status;
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "bu", "bb", &state->pc);
    status = tool_body(classes);
    finish_state(classes, state, &frame, status);
    return status;
}
