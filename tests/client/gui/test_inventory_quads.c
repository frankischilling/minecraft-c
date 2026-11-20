/* The runtime UB helpers use these same entered NW kernels. This CPU probe
 * retains each genuine open batch before draw; it performs no GL or reset. */
#include "client/gui/source_inventory_quad_kernels.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Inventory quad check%u line%d: %s\n", checks, __LINE__, #value);      \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static void words(const int32_t value[48]) {
    putchar('[');
    for (size_t i = 0; i < 48; ++i)
        printf("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", (uint32_t)value[i]);
    putchar(']');
}

static uint64_t bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool row(unsigned int profile, bool tile) {
    bool ok = false;
    mc_client_render_engine_classes *graphics = NULL;
    mc_inventory_quad_context prefix = {0};
    mc_inventory_quad_colors colors;
    const float source_k = 0.0f;
    mc_client_nw_fields fields;
    int32_t prior[48], packed[48];
    if (!tile)
        CHECK(mc_inventory_quad_gradient_colors_capture(&prefix, UINT32_C(0xc0101010),
                                                        UINT32_C(0xd0101010), &colors) &&
              prefix.source_pc == 114);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(
              graphics,
              (mc_client_graphics_site){"ub", tile ? "b" : "a", "(IIIIII)V", tile ? 8 : 147},
              &prefix.nw, &prefix.error) &&
          mc_client_nw_fields_get(prefix.nw, &fields) && fields.g != NULL && fields.B >= 48 &&
          !fields.w && fields.h == 0 && fields.o == 0 && fields.p == 0);
    memcpy(prior, fields.g, sizeof(prior));
    CHECK(tile ? mc_inventory_quad_tile_entered(&prefix, &source_k, 125, 37, 0, 0, 176, 166)
               : mc_inventory_quad_gradient_entered(&prefix, &colors, 0, 0, 427, 240));
    CHECK(prefix.vertices == 4 && prefix.source_pc == (tile ? 168u : 227u) &&
          !prefix.source_exception && mc_client_nw_fields_get(prefix.nw, &fields) && fields.w &&
          fields.h == 6 && fields.o == 48 && fields.p == 4 && fields.r == 7 && fields.l == !tile &&
          fields.m == tile && !fields.n && bits(fields.s) == 0 && bits(fields.t) == 0 &&
          bits(fields.u) == 0);
    memcpy(packed, fields.g, sizeof(packed));
    for (size_t i = 0; i < 48; ++i) {
        const size_t attribute = i % 8;
        const bool active = attribute < 3 || (fields.m && (attribute == 3 || attribute == 4)) ||
                            (fields.l && attribute == 5);
        if (!active)
            CHECK(packed[i] == prior[i]);
    }
    printf("INVENTORY_DRAW_GEOMETRY {\"profile\":%u,\"kind\":\"%s\",\"words\":", profile,
           tile ? "tile" : "gradient");
    words(packed);
    printf(",\"prior\":");
    words(prior);
    printf(",\"state\":{\"vertices\":%" PRId32 ",\"source-vertices\":%" PRId32 ",\"words\":%" PRId32
           ",\"mode\":%" PRId32 ",\"building\":%s,\"color\":%s,\"texture\":%s,\"normal\":%s,"
           "\"translation\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"]},\"completed-draws\":0}\n",
           fields.h, fields.p, fields.o, fields.r, fields.w ? "true" : "false",
           fields.l ? "true" : "false", fields.m ? "true" : "false", fields.n ? "true" : "false",
           bits(fields.s), bits(fields.t), bits(fields.u));
    /* A later actual begin refuses before touching this unfinished batch.
     * Re-read its counts and backing instead of trusting the earlier snapshot. */
    CHECK(!(tile ? mc_inventory_quad_tile_entered(&prefix, &source_k, 125, 37, 0, 0, 176, 166)
                 : mc_inventory_quad_gradient_entered(&prefix, &colors, 0, 0, 427, 240)) &&
          prefix.vertices == 0 && prefix.source_pc == (tile ? 15u : 154u) &&
          prefix.error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE &&
          !prefix.error.native_policy && !prefix.source_exception &&
          mc_client_nw_fields_get(prefix.nw, &fields) && fields.w && fields.h == 6 &&
          fields.o == 48 && fields.p == 4 && fields.r == 7 && fields.l == !tile &&
          fields.m == tile && !fields.n && memcmp(packed, fields.g, sizeof(packed)) == 0);
    ok = true;
cleanup:
    if (graphics != NULL &&
        mc_client_render_engine_classes_release(graphics) != MC_CLIENT_RENDER_ENGINE_OK)
        ok = false;
    return ok;
}

static bool null_controls(void) {
    bool ok = false;
    mc_client_render_engine_classes *graphics = NULL;
    mc_inventory_quad_context prefix = {0};
    mc_inventory_quad_colors colors;
    CHECK(mc_inventory_quad_gradient_colors_capture(&prefix, UINT32_C(0xc0101010),
                                                    UINT32_C(0xd0101010), &colors));
    CHECK(!mc_inventory_quad_gradient_entered(&prefix, &colors, 0, 0, 427, 240) &&
          prefix.source_pc == 154 && prefix.vertices == 0 && prefix.source_exception &&
          prefix.error.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE && !prefix.error.native_policy &&
          strcmp(prefix.exception_class, "java/lang/NullPointerException") == 0);
    /* The cached NW invocation precedes the source UB.k receiver read. */
    CHECK(!mc_inventory_quad_tile_entered(&prefix, NULL, 125, 37, 0, 0, 176, 166) &&
          prefix.source_pc == 15 && prefix.vertices == 0 && prefix.source_exception &&
          prefix.error.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE && !prefix.error.native_policy &&
          strcmp(prefix.error.site.method, "b") == 0);
    /* source_k is a native field borrow, not another Java receiver. Its
     * rejection occurs after the genuine cached NW begin, preserving it. */
    mc_client_nw_fields fields;
    int32_t prior[48];
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(graphics, (mc_client_graphics_site){"ub", "b", "(IIIIII)V", 8},
                                  &prefix.nw, &prefix.error) &&
          mc_client_nw_fields_get(prefix.nw, &fields) && fields.g != NULL && fields.B >= 48 &&
          !fields.w);
    memcpy(prior, fields.g, sizeof(prior));
    CHECK(!mc_inventory_quad_tile_entered(&prefix, NULL, 125, 37, 0, 0, 176, 166) &&
          prefix.source_pc == 30 && prefix.vertices == 0 && !prefix.source_exception &&
          prefix.exception_class == NULL &&
          prefix.error.status == MC_CLIENT_GRAPHICS_INVALID_ARGUMENT &&
          prefix.error.native_policy && strcmp(prefix.error.site.method, "b") == 0 &&
          mc_client_nw_fields_get(prefix.nw, &fields) && fields.w && fields.r == 7 &&
          fields.h == 0 && fields.o == 0 && fields.p == 0 && !fields.l && !fields.m && !fields.n &&
          memcmp(prior, fields.g, sizeof(prior)) == 0);
    ok = true;
cleanup:
    if (graphics != NULL &&
        mc_client_render_engine_classes_release(graphics) != MC_CLIENT_RENDER_ENGINE_OK)
        ok = false;
    return ok;
}

int main(void) {
    for (unsigned int profile = 0; profile < 2; ++profile)
        if (!row(profile, false) || !row(profile, true))
            return 1;
    if (!null_controls())
        return 1;
    printf("Inventory quad entered-prefix CPU controls: %u checks; four compared NW contexts; "
           "zero completed draws\n",
           checks);
    return 0;
}
