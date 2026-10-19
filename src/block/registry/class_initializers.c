#include "registry_internal.h"

static mc_registry_status sound(mc_registry_classes *classes, mc_block_sound **destination,
                                unsigned int kind, const char *name, float pitch,
                                int32_t allocation_pc, int32_t literal_pc, int32_t constructor_pc,
                                int32_t publication_pc)
{
    classes->block_initializer_pc = allocation_pc;
    mc_block_sound *value = mc_registry_allocate(classes, sizeof(*value));
    if (value == NULL)
        return classes->last_error.status;
    value->kind = kind;
    classes->block_initializer_pc = literal_pc;
    const mc_registry_string *argument = NULL;
    mc_registry_status status = mc_registry_literal_ascii(classes, name, &argument);
    if (status != MC_REGISTRY_OK)
        return status;
    classes->block_initializer_pc = constructor_pc;
    value->fields.a = argument;
    value->fields.b = 1.0f;
    value->fields.c = pitch;
    classes->block_initializer_pc = publication_pc;
    *destination = value;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_block_prefix(mc_registry_classes *classes)
{
    mc_registry_status status;
#define SOUND(field, kind, name, pitch, allocation, literal, constructor, publication)             \
    do {                                                                                           \
        status = sound(classes, &classes->blocks.field, kind, name, pitch, allocation, literal,    \
                       constructor, publication);                                                  \
        if (status != MC_REGISTRY_OK)                                                              \
            return status;                                                                         \
    } while (0)
    SOUND(d, 0u, "stone", 1.0f, 0, 4, 8, 11);
    SOUND(e, 0u, "wood", 1.0f, 14, 18, 22, 25);
    SOUND(f, 0u, "gravel", 1.0f, 28, 32, 36, 39);
    SOUND(g, 0u, "grass", 1.0f, 42, 46, 50, 53);
    SOUND(h, 0u, "stone", 1.0f, 56, 60, 64, 67);
    SOUND(i, 0u, "stone", 1.5f, 70, 74, 79, 82);
    SOUND(j, 1u, "stone", 1.0f, 85, 89, 93, 96);
    SOUND(k, 0u, "cloth", 1.0f, 99, 103, 107, 110);
    SOUND(l, 2u, "sand", 1.0f, 113, 117, 121, 124);
#undef SOUND

#define ARRAY(field, type, allocation, publication)                                                \
    do {                                                                                           \
        classes->block_initializer_pc = allocation;                                                \
        type *value = mc_registry_allocate(classes, MC_REGISTRY_BLOCK_SLOTS * sizeof(*value));     \
        if (value == NULL)                                                                         \
            return classes->last_error.status;                                                     \
        classes->block_initializer_pc = publication;                                               \
        classes->blocks.field = value;                                                             \
    } while (0)
    ARRAY(m, mc_block *, 130, 133);
    ARRAY(n, bool, 139, 141);
    ARRAY(o, bool, 147, 149);
    ARRAY(p, bool, 155, 157);
    ARRAY(q, int32_t, 163, 165);
    ARRAY(r, bool, 171, 173);
    ARRAY(s, int32_t, 179, 181);
    ARRAY(t, bool, 187, 189);
#undef ARRAY
    return MC_REGISTRY_OK;
}

static mc_registry_status base_class(mc_registry_classes *classes)
{
    /* The real driver is already executing uu/na.<clinit> on this same flow.
     * A subclass's superclass initialization therefore sees that prefix.
     * No independent successful Block initializer is synthesized here. */
    return mc_registry_blocks_active_use(classes);
}

/* Private initialization runs on the shared owner's admitted synchronous flow.
 * Each declaring class keeps its own first failure before its frame unwinds. */
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

static mc_registry_status intermediate(mc_registry_classes *classes, mc_registry_class_state *state,
                                       const char *client, const char *server)
{
    bool done = false;
    mc_registry_status status = cached_state(classes, state, &done);
    if (done)
        return status;
    state->phase = MC_REGISTRY_INITIALIZING;
    /* These actual ancestors have no own <clinit> body. */
    state->pc = -1;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, client, server, &state->pc);
    status = base_class(classes);
    finish_state(classes, state, &frame, status);
    return status;
}

static mc_registry_status parent_class(mc_registry_classes *classes, mc_block_kind kind)
{
    switch (kind) {
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
        case MC_BLOCK_TL_MD:
        case MC_BLOCK_NI_IJ:
            return mc_registry_block_class_initialize(classes, MC_BLOCK_WB_NW);
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
            return intermediate(classes, &classes->rp_state, "rp", "kv");
        case MC_BLOCK_NE_IG:
            return mc_registry_block_class_initialize(classes, MC_BLOCK_GK_EI);
        case MC_BLOCK_BK_AR:
            return intermediate(classes, &classes->nr_state, "nr", "in");
        case MC_BLOCK_FK_DO:
        case MC_BLOCK_NK_IK:
        case MC_BLOCK_AK_Z:
            return intermediate(classes, &classes->jp_state, "jp", "gh");
        case MC_BLOCK_XQ_PA:
        case MC_BLOCK_PT_JR:
        case MC_BLOCK_UT_MZ:
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_E_E:
        case MC_BLOCK_TC_LZ:
        case MC_BLOCK_UJ_MT:
        case MC_BLOCK_FO_DT:
            return intermediate(classes, &classes->rw_state, "rw", "lb");
        case MC_BLOCK_PH_JN:
            return mc_registry_block_class_initialize(classes, MC_BLOCK_PC_JJ);
        case MC_BLOCK_FB_DH:
            return mc_registry_block_class_initialize(classes, MC_BLOCK_EO_CX);
        case MC_BLOCK_DB_BY:
            return mc_registry_block_class_initialize(classes, MC_BLOCK_VM_NP);
        default:
            return base_class(classes);
    }
}

static mc_registry_status directions(mc_registry_classes *classes)
{
    mc_registry_initializer_stage(classes, 1);
    int32_t **rows = mc_registry_allocate(classes, 4u * sizeof(*rows));
    if (rows == NULL)
        return classes->last_error.status;
#define ROW(index, allocation_pc, first, first_pc, second, second_pc, store_pc)                    \
    do {                                                                                           \
        mc_registry_initializer_stage(classes, allocation_pc);                                     \
        int32_t *row = mc_registry_allocate(classes, 2u * sizeof(*row));                           \
        if (row == NULL)                                                                           \
            return classes->last_error.status;                                                     \
        mc_registry_initializer_stage(classes, first_pc);                                          \
        row[0] = first;                                                                            \
        mc_registry_initializer_stage(classes, second_pc);                                         \
        row[1] = second;                                                                           \
        mc_registry_initializer_stage(classes, store_pc);                                          \
        rows[index] = row;                                                                         \
    } while (0)
    ROW(0, 7, 0, 12, 1, 16, 17);
    ROW(1, 21, -1, 26, 0, 30, 31);
    ROW(2, 35, 0, 40, -1, 44, 45);
    ROW(3, 49, 1, 54, 0, 58, 59);
#undef ROW
    mc_registry_initializer_stage(classes, 60);
    classes->ve_a = rows;
    mc_registry_initializer_stage(classes, 63);
    return MC_REGISTRY_OK;
}

static mc_registry_status slab_names(mc_registry_classes *classes)
{
    mc_registry_initializer_stage(classes, 1);
    const mc_registry_string **array = mc_registry_allocate(classes, 4u * sizeof(*array));
    if (array == NULL)
        return classes->last_error.status;
    mc_registry_status status;
#define NAME(index, text, literal_pc, store_pc)                                                    \
    do {                                                                                           \
        const mc_registry_string *value = NULL;                                                    \
        mc_registry_initializer_stage(classes, literal_pc);                                        \
        status = mc_registry_literal_ascii(classes, text, &value);                                 \
        if (status != MC_REGISTRY_OK)                                                              \
            return status;                                                                         \
        mc_registry_initializer_stage(classes, store_pc);                                          \
        array[index] = value;                                                                      \
    } while (0)
    NAME(0, "stone", 6, 8);
    NAME(1, "sand", 11, 13);
    NAME(2, "wood", 16, 18);
    NAME(3, "cobble", 21, 23);
#undef NAME
    mc_registry_initializer_stage(classes, 24);
    classes->ys_a = array;
    mc_registry_initializer_stage(classes, 27);
    return MC_REGISTRY_OK;
}

static mc_registry_status repeater_arrays(mc_registry_classes *classes)
{
    mc_registry_initializer_stage(classes, 1);
    double *offsets = mc_registry_allocate(classes, 4u * sizeof(*offsets));
    if (offsets == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 5);
    double offset = -0.0625;
    mc_registry_initializer_stage(classes, 8);
    offsets[0] = offset;
    mc_registry_initializer_stage(classes, 11);
    offset = 0.0625;
    mc_registry_initializer_stage(classes, 14);
    offsets[1] = offset;
    mc_registry_initializer_stage(classes, 17);
    offset = 0.1875;
    mc_registry_initializer_stage(classes, 20);
    offsets[2] = offset;
    mc_registry_initializer_stage(classes, 23);
    offset = 0.3125;
    mc_registry_initializer_stage(classes, 26);
    offsets[3] = offset;
    mc_registry_initializer_stage(classes, 27);
    classes->wo_a = offsets;
    mc_registry_initializer_stage(classes, 31);
    int32_t *delays = mc_registry_allocate(classes, 4u * sizeof(*delays));
    if (delays == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 36);
    delays[0] = 1;
    mc_registry_initializer_stage(classes, 40);
    delays[1] = 2;
    mc_registry_initializer_stage(classes, 44);
    delays[2] = 3;
    mc_registry_initializer_stage(classes, 48);
    delays[3] = 4;
    mc_registry_initializer_stage(classes, 49);
    classes->wo_b = delays;
    mc_registry_initializer_stage(classes, 52);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_block_class_initialize(mc_registry_classes *classes,
                                                      mc_block_kind kind)
{
    static const char *const clients[MC_BLOCK_KIND_COUNT] = {
        "uu", "eo", "wp", "ot", "he", "om", "nx", "gk", "ne", "mt", "vg", "bk", "xf", "fk",
        "xq", "rd", "pt", "ve", "pc", "ph", "jq", "rn", "ru", "jb", "h",  "ee", "ut", "wb",
        "tl", "l",  "ys", "ah", "hb", "fb", "vm", "yq", "dd", "ss", "e",  "sm", "fi", "ni",
        "vl", "tc", "uj", "le", "dp", "xr", "bv", "bs", "db", "oi", "jr", "nk", "ac", "or",
        "rz", "ri", "fo", "jw", "fc", "yi", "oa", "ly", "ak", "um", "wo", "ks", "oq"};
    static const char *const servers[MC_BLOCK_KIND_COUNT] = {
        "na", "cx", "oj", "jf", "eu", "jc", "is", "ei", "ig", "ic", "nk", "ar", "ov", "do",
        "pa", "kl", "jr", "nj", "jj", "jn", "gi", "ku", "kz", "fz", "g",  "cq", "mz", "nw",
        "md", "j",  "pw", "t",  "er", "dh", "np", "pt", "bz", "lu", "e",  "lo", "dn", "ij",
        "no", "lz", "mt", "hc", "cg", "pd", "bc", "az", "by", "iz", "gj", "ik", "p",  "je",
        "lf", "ko", "dt", "gn", "di", "po", "iu", "hr", "z",  "mw", "oi", "gv", "jd"};
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if ((unsigned int)kind >= (unsigned int)MC_BLOCK_KIND_COUNT)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Block class kind", NULL);
    if (kind == MC_BLOCK_UU_NA)
        return base_class(classes);
    mc_registry_class_state *state = &classes->block_states[kind];
    bool done = false;
    mc_registry_status status = cached_state(classes, state, &done);
    if (done)
        return status;
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = -1;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, clients[kind], servers[kind], &state->pc);
    status = parent_class(classes, kind);
    if (status == MC_REGISTRY_OK) {
        switch (kind) {
            case MC_BLOCK_GK_EI:
                mc_registry_initializer_stage(classes, 1);
                classes->gk_a = false;
                mc_registry_initializer_stage(classes, 4);
                break;
            case MC_BLOCK_VE_NJ:
                status = directions(classes);
                break;
            case MC_BLOCK_YS_PW:
                status = slab_names(classes);
                break;
            case MC_BLOCK_TC_LZ:
                mc_registry_initializer_stage(classes, 1);
                classes->tc_c = false;
                mc_registry_initializer_stage(classes, 4);
                break;
            case MC_BLOCK_DB_BY: {
                mc_registry_initializer_stage(classes, 0);
                mc_registry_list *list = mc_registry_allocate(classes, sizeof(*list));
                if (list == NULL)
                    status = classes->last_error.status;
                else {
                    /* The existing empty ArrayList representation has no
                     * fallible operation at the reached constructor call. */
                    mc_registry_initializer_stage(classes, 4);
                    mc_registry_initializer_stage(classes, 7);
                    classes->db_b = list;
                    mc_registry_initializer_stage(classes, 10);
                }
                break;
            }
            case MC_BLOCK_WO_OI:
                status = repeater_arrays(classes);
                break;
            default:
                break;
        }
    }
    finish_state(classes, state, &frame, status);
    return status;
}

static mc_registry_status pressure_category(mc_registry_classes *classes, const char *name,
                                            int32_t ordinal, mc_pressure_category **destination,
                                            int32_t allocation_pc, int32_t literal_pc,
                                            int32_t ordinal_pc, int32_t constructor_pc,
                                            int32_t publication_pc)
{
    mc_registry_initializer_stage(classes, allocation_pc);
    mc_pressure_category *category = mc_registry_allocate(classes, sizeof(*category));
    if (category == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, literal_pc);
    const mc_registry_string *argument = NULL;
    mc_registry_status status = mc_registry_literal_ascii(classes, name, &argument);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, ordinal_pc);
    mc_registry_initializer_stage(classes, constructor_pc);
    category->name = argument;
    category->ordinal = ordinal;
    mc_registry_initializer_stage(classes, publication_pc);
    *destination = category;
    return MC_REGISTRY_OK;
}

static mc_registry_status pressure_body(mc_registry_classes *classes)
{
    mc_registry_status status =
        pressure_category(classes, "everything", 0, &classes->pressure_a, 0, 4, 6, 7, 10);
    if (status == MC_REGISTRY_OK)
        status = pressure_category(classes, "mobs", 1, &classes->pressure_b, 13, 17, 19, 20, 23);
    if (status == MC_REGISTRY_OK)
        status = pressure_category(classes, "players", 2, &classes->pressure_c, 26, 30, 32, 33, 36);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_initializer_stage(classes, 40);
    mc_pressure_category **values = mc_registry_allocate(classes, 3u * sizeof(*values));
    if (values == NULL)
        return classes->last_error.status;
    mc_registry_initializer_stage(classes, 45);
    mc_pressure_category *value = classes->pressure_a;
    mc_registry_initializer_stage(classes, 48);
    values[0] = value;
    mc_registry_initializer_stage(classes, 51);
    value = classes->pressure_b;
    mc_registry_initializer_stage(classes, 54);
    values[1] = value;
    mc_registry_initializer_stage(classes, 57);
    value = classes->pressure_c;
    mc_registry_initializer_stage(classes, 60);
    values[2] = value;
    mc_registry_initializer_stage(classes, 61);
    classes->pressure_values = values;
    mc_registry_initializer_stage(classes, 64);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_pressure_initialize(mc_registry_classes *classes)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_class_state *state = &classes->pressure_state;
    bool done = false;
    mc_registry_status status = cached_state(classes, state, &done);
    if (done)
        return status;
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "rt", "kx", &state->pc);
    status = pressure_body(classes);
    finish_state(classes, state, &frame, status);
    return status;
}
