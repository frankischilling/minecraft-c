#include "mc/block/material/material.h"

#include <inttypes.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int palette_id(const mc_materials *owner, const mc_material_palette *palette) {
    if (palette == NULL)
        return -1;
    for (int id = 0; id < MC_PALETTE_COUNT; ++id)
        if (mc_materials_palette(owner, (mc_material_palette_id)id) == palette)
            return id;
    return -2;
}

static void observe(const mc_materials *owner, char name, const mc_material *material,
                    const char *phase) {
    printf("M\t%s\t%c\t%d\t%d%d%d%d%d%d%d%d\t%" PRId32 "\n", phase, name,
           palette_id(owner, mc_material_palette_of(material)), mc_material_a(material),
           mc_material_b(material), mc_material_c(material), mc_material_d(material),
           mc_material_e(material), mc_material_g(material), mc_material_h(material),
           mc_material_i(material), mc_material_j(material));
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
#endif
    mc_materials *owner = NULL;
    if (!mc_materials_create(&owner))
        return 1;
    puts("MATERIAL_V1");
    for (int id = 0; id < MC_PALETTE_COUNT; ++id) {
        const mc_material_palette *palette =
            mc_materials_palette(owner, (mc_material_palette_id)id);
        printf("P\t%c\t%" PRId32 "\t%" PRId32 "\n", 'b' + id, mc_material_palette_index(palette),
               mc_material_palette_rgb(palette));
    }
    for (int slot = 0; slot < (int)MC_MATERIAL_PALETTE_SLOTS; ++slot)
        printf("T\t%d\t%d\n", slot, palette_id(owner, mc_materials_palette_at(owner, slot)));
    for (int id = 0; id < MC_MATERIAL_COUNT; ++id) {
        mc_material *material = mc_materials_get(owner, (mc_material_id)id);
        const char name = (char)(id < 26 ? 'a' + id : 'A' + id - 26);
        observe(owner, name, material, "initial");
        mc_material *alias = mc_material_f(material);
        printf("F\t%c\t%d\n", name, alias == material);
        observe(owner, name, alias, "after-f");
    }
    puts("END\t28\t14\t16");
    mc_materials_destroy(owner);
    return fflush(stdout) == 0 && !ferror(stdout) ? 0 : 1;
}
