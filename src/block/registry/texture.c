#include "registry_internal.h"

/* uu/na.a(I)I virtual dispatch over the actual closed Block classes. These
 * queries do not latch native failures or change game/class state. The caller
 * owns its source failure context; out must be disjoint from owner storage. */
mc_registry_status mc_registry_block_texture_side(mc_registry_classes *classes,
                                                  const mc_block *block, int32_t side, int32_t *out)
{
    if (classes == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (block == NULL)
        return MC_REGISTRY_NULL_REFERENCE;
    if (block->classes != classes || (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT)
        return MC_REGISTRY_INVALID_ARGUMENT;

    int32_t texture;
    switch (block->kind) {
        /* These fifty kinds inherit the actual uu/na body: read bm1/return4. */
        case MC_BLOCK_UU_NA:
        case MC_BLOCK_EO_CX:
        case MC_BLOCK_WP_OJ:
        case MC_BLOCK_OT_JF:
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_GK_EI:
        case MC_BLOCK_NE_IG:
        case MC_BLOCK_MT_IC:
        case MC_BLOCK_VG_NK:
        case MC_BLOCK_BK_AR:
        case MC_BLOCK_XF_OV:
        case MC_BLOCK_FK_DO:
        case MC_BLOCK_VE_NJ:
        case MC_BLOCK_PC_JJ:
        case MC_BLOCK_PH_JN:
        case MC_BLOCK_JQ_GI:
        case MC_BLOCK_RN_KU:
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
        case MC_BLOCK_H_G:
        case MC_BLOCK_EE_CQ:
        case MC_BLOCK_UT_MZ:
        case MC_BLOCK_WB_NW:
        case MC_BLOCK_TL_MD:
        case MC_BLOCK_FB_DH:
        case MC_BLOCK_VM_NP:
        case MC_BLOCK_YQ_PT:
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_SM_LO:
        case MC_BLOCK_NI_IJ:
        case MC_BLOCK_VL_NO:
        case MC_BLOCK_UJ_MT:
        case MC_BLOCK_LE_HC:
        case MC_BLOCK_DP_CG:
        case MC_BLOCK_XR_PD:
        case MC_BLOCK_BV_BC:
        case MC_BLOCK_BS_AZ:
        case MC_BLOCK_DB_BY:
        case MC_BLOCK_OI_IZ:
        case MC_BLOCK_JR_GJ:
        case MC_BLOCK_NK_IK:
        case MC_BLOCK_AC_P:
        case MC_BLOCK_RZ_LF:
        case MC_BLOCK_RI_KO:
        case MC_BLOCK_JW_GN:
        case MC_BLOCK_YI_PO:
        case MC_BLOCK_OA_IU:
        case MC_BLOCK_LY_HR:
        case MC_BLOCK_AK_Z:
        case MC_BLOCK_OQ_JD:
        /* pt/jr and l/j each declare their own independently read bm body. */
        case MC_BLOCK_PT_JR:
        case MC_BLOCK_L_J:
            texture = block->fields.bm;
            break;

        /* Both fluid kinds select their rp/kv ancestor's a(I)I. */
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
            if (side == 0 || side == 1)
                texture = block->fields.bm;
            else
                texture = mc_java_i32_add(block->fields.bm, 1);
            break;

        case MC_BLOCK_XQ_PA:
            if (side == 1 || side == 0)
                texture = mc_java_i32_add(block->fields.bm, 17);
            else if (side == 3)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_RD_KL:
            if (side == 1)
                texture = mc_java_i32_sub(block->fields.bm, 16);
            else if (side == 0)
                texture = mc_java_i32_add(block->fields.bm, 16);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_YS_PW:
            /* a(I)I passes metadata0 atPC2 to the actual a(II)I atPC3.
             * Its selected metadata0 branch returns6 for side<=1, else5. */
            texture = side <= 1 ? 6 : 5;
            break;

        case MC_BLOCK_AH_T:
            if (side == 0)
                texture = mc_java_i32_add(block->fields.bm, 2);
            else if (side == 1)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_HB_ER:
            texture = side <= 1 ? 4 : block->fields.bm;
            break;

        case MC_BLOCK_SS_LU: {
            /* getfield a1 retains the actual source before virtual call5. */
            const mc_block *source = block->subtype.ss.a;
            return mc_registry_block_texture_side(classes, source, side, out);
        }

        /* e/e and ks/gv independently declare the same face/bm arithmetic. */
        case MC_BLOCK_E_E:
        case MC_BLOCK_KS_GV:
            if (side == 1 || side == 0)
                texture = mc_java_i32_sub(block->fields.bm, 1);
            else if (side == 3)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_FI_DN:
            if (side == 1)
                texture = mc_java_i32_sub(block->fields.bm, 16);
            else if (side == 0) {
                /* Actual getstatic uu.y/na.y17, followed by virtual a(0)21.
                 * named[4] is that declaring field, not an m-slot shortcut. */
                const mc_block *source = classes->blocks.named[4];
                return mc_registry_block_texture_side(classes, source, 0, out);
            } else if (side == 2 || side == 4)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_TC_LZ:
            if (side == 1 || side == 0)
                texture = mc_java_i32_add(block->fields.bm, 17);
            else if (side == 3)
                texture = mc_java_i32_sub(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_OR_JE:
            if (side == 1)
                texture = mc_java_i32_sub(block->fields.bm, 1);
            else if (side == 0)
                texture = mc_java_i32_add(block->fields.bm, 1);
            else
                texture = block->fields.bm;
            break;

        case MC_BLOCK_FO_DT:
            /* The source loads bm1 before choosing the increment5..13. */
            texture = block->fields.bm;
            texture = mc_java_i32_add(texture, side == 1 ? 1 : 0);
            break;

        case MC_BLOCK_FC_DI:
            if (side == 1 || side == 0)
                texture = block->fields.bm;
            else if (side == 3) {
                texture = mc_java_i32_add(block->fields.bm, 1);
                texture = mc_java_i32_add(texture, 16);
            } else
                texture = mc_java_i32_add(block->fields.bm, 16);
            break;

        case MC_BLOCK_UM_MW:
            if (side == 1)
                texture = block->fields.bm;
            else if (side == 0)
                texture = mc_java_i32_add(block->fields.bm, 3);
            else
                texture = mc_java_i32_add(block->fields.bm, 1);
            break;

        case MC_BLOCK_WO_OI:
            /* a(I)I calls the actual a(II)I with metadata0. That body ignores
             * metadata and reads its declaring-class c only for faces0/1. */
            if (side == 0)
                texture = block->subtype.wo.c ? 99 : 115;
            else if (side == 1)
                texture = block->subtype.wo.c ? 147 : 131;
            else
                texture = 5;
            break;

        case MC_BLOCK_KIND_COUNT:
        default:
            return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = texture;
    return MC_REGISTRY_OK;
}
