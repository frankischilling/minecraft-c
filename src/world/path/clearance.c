#include "path_internal.h"

static bool door_id(mc_path_finder *w, const char *name, int32_t pc, int32_t field_pc,
                    int32_t *out) {
    const mc_block *block;
    pf_at(w, "a", "(Lsn;IIILd;)I", pc);
    if (mc_registry_block_static_get(w->registry, name, &block, &w->error.registry) !=
        MC_REGISTRY_OK)
        return pf_registry_fail(w, "Path actual Door static active use");
    pf_at(w, "a", "(Lsn;IIILd;)I", field_pc);
    if (block == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path null Door static receiver");
    *out = block->fields.bn;
    return true;
}

bool pf_clearance(mc_path_finder *w, int32_t x, int32_t y, int32_t z, mc_path_point *extent,
                  int32_t *out) {
    for (int32_t xx = x; xx < mc_java_i32_add(x, extent->a); xx = mc_java_i32_add(xx, 1)) {
        for (int32_t yy = y; yy < mc_java_i32_add(y, extent->b); yy = mc_java_i32_add(yy, 1)) {
            for (int32_t zz = z; zz < mc_java_i32_add(z, extent->c); zz = mc_java_i32_add(zz, 1)) {
                pf_at(w, "a", "(Lsn;IIILd;)I", 57);
                if (!pf_work(w))
                    return false;
                int32_t id;
                if (!pf_query(w, false, xx, yy, zz, &id))
                    return false;
                if (id <= 0)
                    continue;
                int32_t selected;
                if (!door_id(w, "aM", 71, 74, &selected))
                    return false;
                bool door = id == selected;
                if (!door) {
                    if (!door_id(w, "aF", 82, 85, &selected))
                        return false;
                    door = id == selected;
                }
                if (door) {
                    int32_t metadata;
                    pf_at(w, "a", "(Lsn;IIILd;)I", 101);
                    if (!pf_query(w, true, xx, yy, zz, &metadata))
                        return false;
                    pf_at(w, "a", "(Lsn;IIILd;)I", 110);
                    const mc_registry_status status =
                        mc_registry_block_class_initialize(w->registry, MC_BLOCK_DOOR);
                    if (status != MC_REGISTRY_OK) {
                        w->error.registry = w->registry->last_error;
                        return pf_registry_fail(w, "Path actual LE active use");
                    }
                    if ((metadata & 4) == 0) {
                        *out = 0;
                        return true;
                    }
                    continue;
                }
                mc_registry_block_m_view array;
                const mc_block *block;
                pf_at(w, "a", "(Lsn;IIILd;)I", 121);
                if (mc_registry_block_m_get(w->registry, &array, &w->error.registry) !=
                    MC_REGISTRY_OK)
                    return pf_registry_fail(w, "Path actual m active use");
                pf_at(w, "a", "(Lsn;IIILd;)I", 126);
                if (mc_registry_block_m_at(&array, id, &block, &w->error.registry) !=
                    MC_REGISTRY_OK)
                    return pf_registry_fail(w, "Path actual captured m element");
                pf_at(w, "a", "(Lsn;IIILd;)I", 127);
                if (block == NULL)
                    return pf_source(w, "java/lang/NullPointerException",
                                     "Path null Block receiver");
                mc_material *material = block->fields.bA;
                pf_at(w, "a", "(Lsn;IIILd;)I", 134);
                if (material == NULL)
                    return pf_source(w, "java/lang/NullPointerException",
                                     "Path null Material receiver");
                if (mc_material_c(material)) {
                    *out = 0;
                    return true;
                }
                mc_material *water, *lava;
                pf_at(w, "a", "(Lsn;IIILd;)I", 144);
                if (mc_registry_material_get(w->registry, MC_MATERIAL_g, &water,
                                             &w->error.registry) != MC_REGISTRY_OK)
                    return pf_registry_fail(w, "Path actual water Material static");
                if (material == water) {
                    *out = -1;
                    return true;
                }
                pf_at(w, "a", "(Lsn;IIILd;)I", 154);
                if (mc_registry_material_get(w->registry, MC_MATERIAL_h, &lava,
                                             &w->error.registry) != MC_REGISTRY_OK)
                    return pf_registry_fail(w, "Path actual lava Material static");
                if (material == lava) {
                    *out = -2;
                    return true;
                }
            }
        }
    }
    pf_at(w, "a", "(Lsn;IIILd;)I", 182);
    *out = 1;
    return true;
}
