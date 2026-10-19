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
    if (classes->blocks_phase == MC_REGISTRY_INITIALIZING ||
        classes->blocks_phase == MC_REGISTRY_READY)
        return MC_REGISTRY_OK;
    return mc_registry_fail(classes, MC_REGISTRY_FAILED_INITIALIZATION,
                            "Block superclass is not initializing", NULL);
}

static mc_registry_status intermediate(mc_registry_classes *classes, mc_registry_phase *phase)
{
    if (*phase == MC_REGISTRY_READY || *phase == MC_REGISTRY_INITIALIZING)
        return MC_REGISTRY_OK;
    if (*phase == MC_REGISTRY_FAILED)
        return mc_registry_fail(classes, MC_REGISTRY_FAILED_INITIALIZATION,
                                "Block ancestor initialization previously failed", NULL);
    *phase = MC_REGISTRY_INITIALIZING;
    mc_registry_status status = base_class(classes);
    *phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
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
            return intermediate(classes, &classes->rp_phase);
        case MC_BLOCK_NE_IG:
            return mc_registry_block_class_initialize(classes, MC_BLOCK_GK_EI);
        case MC_BLOCK_BK_AR:
            return intermediate(classes, &classes->nr_phase);
        case MC_BLOCK_FK_DO:
        case MC_BLOCK_NK_IK:
        case MC_BLOCK_AK_Z:
            return intermediate(classes, &classes->jp_phase);
        case MC_BLOCK_XQ_PA:
        case MC_BLOCK_PT_JR:
        case MC_BLOCK_UT_MZ:
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_E_E:
        case MC_BLOCK_TC_LZ:
        case MC_BLOCK_UJ_MT:
        case MC_BLOCK_FO_DT:
            return intermediate(classes, &classes->rw_phase);
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
    int32_t **rows = mc_registry_allocate(classes, 4u * sizeof(*rows));
    if (rows == NULL)
        return classes->last_error.status;
    rows[0] = mc_registry_allocate(classes, 2u * sizeof(*rows[0]));
    if (rows[0] == NULL)
        return classes->last_error.status;
    rows[0][0] = 0;
    rows[0][1] = 1;
    rows[1] = mc_registry_allocate(classes, 2u * sizeof(*rows[1]));
    if (rows[1] == NULL)
        return classes->last_error.status;
    rows[1][0] = -1;
    rows[1][1] = 0;
    rows[2] = mc_registry_allocate(classes, 2u * sizeof(*rows[2]));
    if (rows[2] == NULL)
        return classes->last_error.status;
    rows[2][0] = 0;
    rows[2][1] = -1;
    rows[3] = mc_registry_allocate(classes, 2u * sizeof(*rows[3]));
    if (rows[3] == NULL)
        return classes->last_error.status;
    rows[3][0] = 1;
    rows[3][1] = 0;
    classes->ve_a = rows;
    return MC_REGISTRY_OK;
}

static mc_registry_status slab_names(mc_registry_classes *classes)
{
    const mc_registry_string **array = mc_registry_allocate(classes, 4u * sizeof(*array));
    if (array == NULL)
        return classes->last_error.status;
    mc_registry_status status = mc_registry_literal_ascii(classes, "stone", &array[0]);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_literal_ascii(classes, "sand", &array[1]);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_literal_ascii(classes, "wood", &array[2]);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_literal_ascii(classes, "cobble", &array[3]);
    if (status != MC_REGISTRY_OK)
        return status;
    classes->ys_a = array;
    return MC_REGISTRY_OK;
}

static mc_registry_status repeater_arrays(mc_registry_classes *classes)
{
    double *offsets = mc_registry_allocate(classes, 4u * sizeof(*offsets));
    if (offsets == NULL)
        return classes->last_error.status;
    offsets[0] = -0.0625;
    offsets[1] = 0.0625;
    offsets[2] = 0.1875;
    offsets[3] = 0.3125;
    classes->wo_a = offsets;
    int32_t *delays = mc_registry_allocate(classes, 4u * sizeof(*delays));
    if (delays == NULL)
        return classes->last_error.status;
    delays[0] = 1;
    delays[1] = 2;
    delays[2] = 3;
    delays[3] = 4;
    classes->wo_b = delays;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_block_class_initialize(mc_registry_classes *classes,
                                                      mc_block_kind kind)
{
    if (kind == MC_BLOCK_UU_NA)
        return base_class(classes);
    mc_registry_phase *phase = &classes->kind_phases[kind];
    if (*phase == MC_REGISTRY_READY || *phase == MC_REGISTRY_INITIALIZING)
        return MC_REGISTRY_OK;
    if (*phase == MC_REGISTRY_FAILED)
        return mc_registry_fail(classes, MC_REGISTRY_FAILED_INITIALIZATION,
                                "Block subtype initialization previously failed", NULL);
    *phase = MC_REGISTRY_INITIALIZING;
    mc_registry_status status = parent_class(classes, kind);
    if (status == MC_REGISTRY_OK) {
        switch (kind) {
            case MC_BLOCK_GK_EI:
                classes->gk_a = false;
                break;
            case MC_BLOCK_VE_NJ:
                status = directions(classes);
                break;
            case MC_BLOCK_YS_PW:
                status = slab_names(classes);
                break;
            case MC_BLOCK_TC_LZ:
                classes->tc_c = false;
                break;
            case MC_BLOCK_DB_BY: {
                mc_registry_list *list = mc_registry_allocate(classes, sizeof(*list));
                if (list == NULL)
                    status = classes->last_error.status;
                else
                    classes->db_b = list;
                break;
            }
            case MC_BLOCK_WO_OI:
                status = repeater_arrays(classes);
                break;
            default:
                break;
        }
    }
    *phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    return status;
}

static mc_registry_status pressure_category(mc_registry_classes *classes, const char *name,
                                            int32_t ordinal, mc_pressure_category **destination)
{
    mc_pressure_category *category = mc_registry_allocate(classes, sizeof(*category));
    if (category == NULL)
        return classes->last_error.status;
    const mc_registry_string *argument = NULL;
    mc_registry_status status = mc_registry_literal_ascii(classes, name, &argument);
    if (status != MC_REGISTRY_OK)
        return status;
    category->name = argument;
    category->ordinal = ordinal;
    *destination = category;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_pressure_initialize(mc_registry_classes *classes)
{
    if (classes->pressure_phase == MC_REGISTRY_READY ||
        classes->pressure_phase == MC_REGISTRY_INITIALIZING)
        return MC_REGISTRY_OK;
    if (classes->pressure_phase == MC_REGISTRY_FAILED)
        return mc_registry_fail(classes, MC_REGISTRY_FAILED_INITIALIZATION,
                                "pressure enum initialization previously failed", NULL);
    classes->pressure_phase = MC_REGISTRY_INITIALIZING;
    mc_registry_status status = pressure_category(classes, "everything", 0, &classes->pressure_a);
    if (status == MC_REGISTRY_OK)
        status = pressure_category(classes, "mobs", 1, &classes->pressure_b);
    if (status == MC_REGISTRY_OK)
        status = pressure_category(classes, "players", 2, &classes->pressure_c);
    if (status == MC_REGISTRY_OK) {
        mc_pressure_category **values = mc_registry_allocate(classes, 3u * sizeof(*values));
        if (values == NULL)
            status = classes->last_error.status;
        else {
            values[0] = classes->pressure_a;
            values[1] = classes->pressure_b;
            values[2] = classes->pressure_c;
            classes->pressure_values = values;
        }
    }
    classes->pressure_phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    return status;
}
