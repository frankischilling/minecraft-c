#ifndef MC_ITEM_REGISTRY_INTERNAL_H
#define MC_ITEM_REGISTRY_INTERNAL_H

#include "mc/item/registry/registry.h"

typedef enum mc_item_kind {
    MC_ITEM_GM_EJ,
    MC_ITEM_WC_NY,
    MC_ITEM_AU_AH,
    MC_ITEM_TA_LY,
    MC_ITEM_YE_PL,
    MC_ITEM_YW_PX,
    MC_ITEM_QZ_KJ,
    MC_ITEM_RX_LC,
    MC_ITEM_QD_JW,
    MC_ITEM_LA_GZ,
    MC_ITEM_RK_KR,
    MC_ITEM_WA_NX,
    MC_ITEM_YM_PS,
    MC_ITEM_VC_NG,
    MC_ITEM_HU_FG,
    MC_ITEM_BC_AL,
    MC_ITEM_RL_KS,
    MC_ITEM_SA_LG,
    MC_ITEM_IE_FK,
    MC_ITEM_DM_CF,
    MC_ITEM_VD_NI,
    MC_ITEM_LV_HN,
    MC_ITEM_MO_HX,
    MC_ITEM_CS_BV,
    MC_ITEM_KM_GU,
    MC_ITEM_LP_HK,
    MC_ITEM_BL_AS,
    MC_ITEM_TR_MJ,
    MC_ITEM_BZ_BF,
    MC_ITEM_QK_JY,
    MC_ITEM_WR_OL,
    MC_ITEM_CK_BK,
    MC_ITEM_BI_AP,
    MC_ITEM_GO_EK,
    MC_ITEM_EN_CW,
    MC_ITEM_MR_IA,
    MC_ITEM_UY_ND,
    MC_ITEM_QR_KC,
    MC_ITEM_KIND_COUNT
} mc_item_kind;

typedef struct mc_registry_class_state {
    mc_registry_phase phase;
    int32_t pc;
    mc_registry_error failure;
} mc_registry_class_state;

typedef struct mc_registry_block_array {
    size_t length;
    mc_block **elements;
} mc_registry_block_array;

struct mc_registry_item_array {
    size_t length;
    mc_item **elements;
};

typedef struct mc_item_tool_material {
    const mc_registry_string *name;
    int32_t ordinal;
    int32_t f, g;
    float h;
    int32_t i;
} mc_item_tool_material;

typedef enum mc_item_constructor_phase {
    MC_ITEM_RAW,
    MC_ITEM_CONSTRUCTING,
    MC_ITEM_CONSTRUCTED,
    MC_ITEM_CONSTRUCTION_FAILED
} mc_item_constructor_phase;

struct mc_item {
    mc_registry_classes *classes;
    mc_item_kind kind;
    mc_item_constructor_phase phase;
    int32_t requested_argument;
    const char *constructor_class;
    int32_t constructor_pc;
    mc_item_fields fields;
    /* dq/cj fields are inherited independently of base bl/bk and own subtype
     * fields. No two declaring-class same-letter fields share a union slot. */
    struct {
        mc_registry_block_array *bk;
        float bl;
        int32_t bm;
        const mc_item_tool_material *a;
    } dq;
    struct {
        int32_t a;
        bool bk;
    } yw;
    int32_t ck_a;
    union {
        struct {
            int32_t a;
        } qd;
        struct {
            int32_t a;
        } rk;
        struct {
            int32_t a, bk, bl, bm;
        } wa;
        struct {
            mc_material *a;
        } hu;
        struct {
            int32_t a;
        } bc;
        struct {
            int32_t a;
        } rl;
        struct {
            int32_t a;
        } lv;
        struct {
            const mc_registry_string *a;
        } tr;
    } subtype;
};

/* Source arguments after the raw integer ID. integers[0..2] are the remaining
 * integer parameters in declaration order; other descriptors use typed slots. */
typedef struct mc_item_arguments {
    int32_t id;
    int32_t integers[3];
    bool flag;
    const mc_item_tool_material *tool;
    mc_material *material;
    mc_block *block;
    const mc_registry_string *text;
} mc_item_arguments;

typedef struct mc_registry_item_storage {
    mc_java_random *b;
    mc_registry_item_array *c;
    mc_item *named[106];
    size_t named_items;
    mc_registry_block_array *wc_bk, *au_bk, *ta_bk;
    int32_t *wa_bn, *wa_bo;
    const mc_registry_string **km_a;
    int32_t *km_bk;
    mc_item_tool_material *tool_a, *tool_b, *tool_c, *tool_d, *tool_e;
    mc_item_tool_material **tool_j;
} mc_registry_item_storage;

mc_registry_status mc_registry_items_active_use(mc_registry_classes *classes);
mc_registry_status mc_registry_item_class_initialize(mc_registry_classes *classes,
                                                     mc_item_kind kind);
mc_registry_status mc_registry_item_tool_initialize(mc_registry_classes *classes);
mc_registry_status mc_registry_item_allocate(mc_registry_classes *classes, mc_item_kind kind,
                                             mc_item **out);
mc_registry_status mc_registry_item_construct(mc_registry_classes *classes, mc_item *item,
                                              const mc_item_arguments *arguments);
void mc_registry_item_stage(mc_item *item, const char *client, const char *server, int32_t pc);
mc_registry_status mc_registry_item_base(mc_registry_classes *classes, mc_item *item, int32_t id);
mc_registry_status mc_registry_item_ck(mc_registry_classes *classes, mc_item *item, int32_t id);
void mc_registry_item_icon(mc_item *item, int32_t x, int32_t y);
void mc_registry_item_icon_direct(mc_item *item, int32_t icon);
void mc_registry_item_stack_limit(mc_item *item, int32_t count);
void mc_registry_item_subtypes(mc_item *item, bool value);
void mc_registry_item_durability(mc_item *item, int32_t maximum);
void mc_registry_item_flag(mc_item *item);
mc_registry_status mc_registry_item_name(mc_registry_classes *classes, mc_item *item,
                                         const char *suffix);
mc_registry_status mc_registry_item_container(mc_registry_classes *classes, mc_item *item,
                                              mc_item *container);
mc_registry_status mc_registry_register_items(mc_registry_classes *classes);
const char *mc_registry_item_static_name(size_t index);

mc_registry_status mc_registry_item_array_create(mc_registry_classes *classes, size_t length,
                                                 mc_registry_item_array **out);
mc_registry_status mc_registry_item_array_load(mc_registry_classes *classes,
                                               mc_registry_item_array *array, int32_t index,
                                               mc_item **out);
mc_registry_status mc_registry_item_array_store(mc_registry_classes *classes,
                                                mc_registry_item_array *array, int32_t index,
                                                mc_item *item);
mc_registry_status mc_registry_block_array_create(mc_registry_classes *classes, size_t length,
                                                  mc_registry_block_array **out);
mc_registry_status mc_registry_block_array_load(mc_registry_classes *classes,
                                                mc_registry_block_array *array, int32_t index,
                                                mc_block **out);
mc_registry_status mc_registry_block_static_load(mc_registry_classes *classes, const char *name,
                                                 mc_block **out);

#endif
