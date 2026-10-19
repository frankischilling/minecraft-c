#include "registry_internal.h"

bool mc_registry_block_opaque(const mc_block *block)
{
    if (block == NULL)
        return false;

    switch (block->kind) {
        /* These concrete classes inherit uu.c()/na.a(). */
        case MC_BLOCK_UU_NA:
        case MC_BLOCK_EO_CX:
        case MC_BLOCK_WP_OJ:
        case MC_BLOCK_OT_JF:
        case MC_BLOCK_GK_EI:
        case MC_BLOCK_NE_IG:
        case MC_BLOCK_MT_IC:
        case MC_BLOCK_VG_NK:
        case MC_BLOCK_XF_OV:
        case MC_BLOCK_XQ_PA:
        case MC_BLOCK_RD_KL:
        case MC_BLOCK_PT_JR:
        case MC_BLOCK_EE_CQ:
        case MC_BLOCK_L_J:
        case MC_BLOCK_AH_T:
        case MC_BLOCK_HB_ER:
        case MC_BLOCK_FB_DH:
        case MC_BLOCK_E_E:
        case MC_BLOCK_FI_DN:
        case MC_BLOCK_TC_LZ:
        case MC_BLOCK_BS_AZ:
        case MC_BLOCK_AC_P:
        case MC_BLOCK_RZ_LF:
        case MC_BLOCK_FO_DT:
        case MC_BLOCK_FC_DI:
        case MC_BLOCK_YI_PO:
        case MC_BLOCK_OA_IU:
        case MC_BLOCK_LY_HR:
        case MC_BLOCK_KS_GV:
            return true;

        /* Each selected declaring body returns the constant false. */
        case MC_BLOCK_HE_EU:
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
        case MC_BLOCK_FK_DO:
        case MC_BLOCK_VE_NJ:
        case MC_BLOCK_PC_JJ:
        case MC_BLOCK_PH_JN:
        case MC_BLOCK_JQ_GI:
        case MC_BLOCK_RN_KU:
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
        case MC_BLOCK_H_G:
        case MC_BLOCK_UT_MZ:
        case MC_BLOCK_WB_NW:
        case MC_BLOCK_TL_MD:
        case MC_BLOCK_VM_NP:
        case MC_BLOCK_YQ_PT:
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_SS_LU:
        case MC_BLOCK_SM_LO:
        case MC_BLOCK_NI_IJ:
        case MC_BLOCK_VL_NO:
        case MC_BLOCK_UJ_MT:
        case MC_BLOCK_LE_HC:
        case MC_BLOCK_DP_CG:
        case MC_BLOCK_XR_PD:
        case MC_BLOCK_BV_BC:
        case MC_BLOCK_DB_BY:
        case MC_BLOCK_OI_IZ:
        case MC_BLOCK_JR_GJ:
        case MC_BLOCK_NK_IK:
        case MC_BLOCK_OR_JE:
        case MC_BLOCK_RI_KO:
        case MC_BLOCK_JW_GN:
        case MC_BLOCK_AK_Z:
        case MC_BLOCK_UM_MW:
        case MC_BLOCK_WO_OI:
        case MC_BLOCK_OQ_JD:
            return false;

        case MC_BLOCK_BK_AR:
            return !block->nr_b;
        case MC_BLOCK_YS_PW:
            return block->subtype.ys.b;
        case MC_BLOCK_KIND_COUNT:
            break;
    }
    return false;
}
