#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "mc/core/java/java.h"

static bool tick_empty(mc_world_mutation_error *error)
{
    error->hook_class = NULL;
    error->hook_bytecode = 0;
    return true;
}

static bool tick_unavailable(mc_world_runtime *world, const mc_block *block, const char *client,
                             const char *server, mc_world_mutation_error *error)
{
    error->hook_class = block->classes->options.side == MC_REGISTRY_CLIENT ? client : server;
    error->hook_bytecode = 0;
    return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Block tick body is not implemented");
}

/* uu/na.a(World,III,Random)V virtual resolution. The empty base does not
 * dereference World or Random. Concrete fluid/falling overrides execute their
 * actual bodies; remaining overrides stop at their declaring entry until
 * those concrete bodies are implemented. */
bool mc_world_mutation_block_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error)
{
    (void)x;
    (void)y;
    (void)z;
    (void)random;
    if (block == NULL) {
        error->hook_class = NULL;
        error->hook_bytecode = 0;
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Block tick receiver is null");
    }
    switch (block->kind) {
        case MC_BLOCK_UU_NA:
        case MC_BLOCK_EO_CX:
        case MC_BLOCK_OT_JF:
        case MC_BLOCK_MT_IC:
        case MC_BLOCK_VG_NK:
        case MC_BLOCK_XF_OV:
        case MC_BLOCK_FK_DO:
        case MC_BLOCK_RD_KL:
        case MC_BLOCK_PT_JR:
        case MC_BLOCK_VE_NJ:
        case MC_BLOCK_PC_JJ:
        case MC_BLOCK_JQ_GI:
        case MC_BLOCK_RN_KU:
        case MC_BLOCK_H_G:
        case MC_BLOCK_EE_CQ:
        case MC_BLOCK_UT_MZ:
        case MC_BLOCK_L_J:
        case MC_BLOCK_YS_PW:
        case MC_BLOCK_AH_T:
        case MC_BLOCK_HB_ER:
        case MC_BLOCK_FB_DH:
        case MC_BLOCK_DD_BZ:
        case MC_BLOCK_E_E:
        case MC_BLOCK_SM_LO:
        case MC_BLOCK_FI_DN:
        case MC_BLOCK_TC_LZ:
        case MC_BLOCK_UJ_MT:
        case MC_BLOCK_LE_HC:
        case MC_BLOCK_DP_CG:
        case MC_BLOCK_XR_PD:
        case MC_BLOCK_RZ_LF:
        case MC_BLOCK_FO_DT:
        case MC_BLOCK_JW_GN:
        case MC_BLOCK_FC_DI:
        case MC_BLOCK_YI_PO:
        case MC_BLOCK_OA_IU:
        case MC_BLOCK_LY_HR:
        case MC_BLOCK_AK_Z:
        case MC_BLOCK_UM_MW:
        case MC_BLOCK_OQ_JD:
            return tick_empty(error);
        case MC_BLOCK_WP_OJ:
            return tick_unavailable(world, block, "wp", "oj", error);
        case MC_BLOCK_HE_EU:
            return tick_unavailable(world, block, "he", "eu", error);
        case MC_BLOCK_OM_JC:
        case MC_BLOCK_NX_IS:
            return mc_world_mutation_fluid_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_GK_EI:
        case MC_BLOCK_NE_IG:
            return mc_world_mutation_falling_tick(world, block, x, y, z, random, error);
        case MC_BLOCK_BK_AR:
            return tick_unavailable(world, block, "bk", "ar", error);
        case MC_BLOCK_XQ_PA:
            return tick_unavailable(world, block, "xq", "pa", error);
        case MC_BLOCK_PH_JN:
            return tick_unavailable(world, block, "ph", "jn", error);
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ:
        case MC_BLOCK_WB_NW:
            return tick_unavailable(world, block, "wb", "nw", error);
        case MC_BLOCK_TL_MD:
            return tick_unavailable(world, block, "tl", "md", error);
        case MC_BLOCK_VM_NP:
            return tick_unavailable(world, block, "vm", "np", error);
        case MC_BLOCK_YQ_PT:
            return tick_unavailable(world, block, "yq", "pt", error);
        case MC_BLOCK_SS_LU:
            return tick_unavailable(world, block, "ss", "lu", error);
        case MC_BLOCK_NI_IJ:
            return tick_unavailable(world, block, "ni", "ij", error);
        case MC_BLOCK_VL_NO:
            return tick_unavailable(world, block, "vl", "no", error);
        case MC_BLOCK_BV_BC:
            return tick_unavailable(world, block, "bv", "bc", error);
        case MC_BLOCK_BS_AZ:
            return tick_unavailable(world, block, "bs", "az", error);
        case MC_BLOCK_DB_BY:
            return tick_unavailable(world, block, "db", "by", error);
        case MC_BLOCK_OI_IZ:
            return tick_unavailable(world, block, "oi", "iz", error);
        case MC_BLOCK_JR_GJ:
            return tick_unavailable(world, block, "jr", "gj", error);
        case MC_BLOCK_NK_IK:
            return tick_unavailable(world, block, "nk", "ik", error);
        case MC_BLOCK_AC_P:
            return tick_unavailable(world, block, "ac", "p", error);
        case MC_BLOCK_OR_JE:
            return tick_unavailable(world, block, "or", "je", error);
        case MC_BLOCK_RI_KO:
            return tick_unavailable(world, block, "ri", "ko", error);
        case MC_BLOCK_WO_OI:
            return tick_unavailable(world, block, "wo", "oi", error);
        case MC_BLOCK_KS_GV:
            return tick_unavailable(world, block, "ks", "gv", error);
        default:
            error->hook_class = NULL;
            error->hook_bytecode = 0;
            return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Unknown concrete Block tick dispatch");
    }
}
