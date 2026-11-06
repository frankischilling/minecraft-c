/* The real dropped-Item renderer uses this same entered BB/NW prefix.
 * Fresh genuine contexts retain their own backing; no GL, draw or reset. */
#include "client/render/item_billboard.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Item billboard check%u line%d: %s\n", checks, __LINE__, #value);      \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t bits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static uint64_t double_bits(double value) {
    uint64_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static void words(const int32_t value[48]) {
    putchar('[');
    for (size_t i = 0; i < 48; ++i)
        printf("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", (uint32_t)value[i]);
    putchar(']');
}

static bool row(unsigned int profile, unsigned int copy) {
    bool ok = false;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_item_billboard_context prefix = {0};
    mc_client_nw_fields fields;
    int32_t prior[48], packed[48];
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(graphics,
                                  (mc_client_graphics_site){"bb", "a", "(Lhl;DDDFF)V", 403},
                                  &prefix.nw, &prefix.error) &&
          mc_client_nw_fields_get(prefix.nw, &fields) && fields.g != NULL && fields.B >= 48 &&
          !fields.w && fields.h == 0 && fields.o == 0 && fields.p == 0);
    memcpy(prior, fields.g, sizeof(prior));
    CHECK(mc_client_item_billboard_uv_capture(&prefix, 53) && prefix.source_pc == 474 &&
          bits(prefix.uv[0]) == UINT32_C(0x3ea00000) &&
          bits(prefix.uv[1]) == UINT32_C(0x3ec00000) &&
          bits(prefix.uv[2]) == UINT32_C(0x3e400000) && bits(prefix.uv[3]) == UINT32_C(0x3e800000));
    CHECK(mc_client_item_billboard_entered(&prefix) && prefix.source_pc == 776 &&
          prefix.vertices == 4 && !prefix.source_exception &&
          mc_client_nw_fields_get(prefix.nw, &fields) && fields.w && fields.h == 6 &&
          fields.p == 4 && fields.o == 48 && fields.r == 7 && !fields.l && fields.m && fields.n &&
          fields.v == 0x7f00 && double_bits(fields.s) == 0 && double_bits(fields.t) == 0 &&
          double_bits(fields.u) == 0);
    memcpy(packed, fields.g, sizeof(packed));
    static const unsigned int expanded[] = {0, 1, 2, 0, 2, 3};
    for (size_t vertex = 0; vertex < 6; ++vertex) {
        const unsigned int source = expanded[vertex];
        const uint32_t x = source == 0 || source == 3 ? UINT32_C(0xbf000000) : UINT32_C(0x3f000000);
        const uint32_t y = source < 2 ? UINT32_C(0xbe800000) : UINT32_C(0x3f400000);
        const uint32_t u = bits(prefix.uv[source == 0 || source == 3 ? 0 : 1]);
        const uint32_t v = bits(prefix.uv[source < 2 ? 3 : 2]);
        CHECK((uint32_t)packed[vertex * 8] == x && (uint32_t)packed[vertex * 8 + 1] == y &&
              (uint32_t)packed[vertex * 8 + 2] == 0 && (uint32_t)packed[vertex * 8 + 3] == u &&
              (uint32_t)packed[vertex * 8 + 4] == v);
        if (vertex != 3 && vertex != 4)
            CHECK(packed[vertex * 8 + 6] == 0x7f00);
    }
    for (size_t i = 0; i < 48; ++i) {
        const size_t attribute = i % 8, vertex = i / 8;
        const bool active = attribute < 5 || (attribute == 6 && vertex != 3 && vertex != 4);
        if (!active)
            CHECK(packed[i] == prior[i]);
    }
    printf("ITEM_BILLBOARD_GEOMETRY {\"profile\":%u,\"copy\":%u,\"icon\":53,\"uv\":[", profile,
           copy);
    for (size_t i = 0; i < 4; ++i)
        printf("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", bits(prefix.uv[i]));
    printf("],\"words\":");
    words(packed);
    printf(",\"prior\":");
    words(prior);
    printf(",\"state\":{\"vertices\":6,\"source-vertices\":4,\"words\":48,\"mode\":7,"
           "\"building\":true,\"color\":false,\"texture\":true,\"normal\":true,"
           "\"translation\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"]},\"completed-draws\":0}\n",
           double_bits(fields.s), double_bits(fields.t), double_bits(fields.u));
    CHECK(!mc_client_item_billboard_entered(&prefix) && prefix.source_pc == 673 &&
          prefix.vertices == 0 && !prefix.source_exception &&
          prefix.error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE &&
          !prefix.error.native_policy && mc_client_nw_fields_get(prefix.nw, &fields) && fields.w &&
          fields.h == 6 && fields.p == 4 && fields.o == 48 && fields.n && fields.m &&
          memcmp(packed, fields.g, sizeof(packed)) == 0);
    ok = true;
cleanup:
    if (graphics != NULL &&
        mc_client_render_engine_classes_release(graphics) != MC_CLIENT_RENDER_ENGINE_OK)
        ok = false;
    return ok;
}

static bool signed_uv_controls(void) {
    bool ok = false;

    static const struct {
        int32_t icon;
        uint32_t expected[4];
    } cases[] = {{-1, {0xbd800000u, 0, 0, 0x3d800000u}},
                 {-17, {0xbd800000u, 0, 0xbd800000u, 0}},
                 {INT32_MIN, {0, 0x3d800000u, 0xcb000000u, 0xcb000000u}},
                 {INT32_MAX, {0x3f700000u, 0x3f800000u, 0x4b000000u, 0xcb000000u}}};

    mc_client_item_billboard_context prefix = {0};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(mc_client_item_billboard_uv_capture(&prefix, cases[i].icon) &&
              prefix.source_pc == 474);
        for (size_t component = 0; component < 4; ++component)
            CHECK(bits(prefix.uv[component]) == cases[i].expected[component]);
    }
    CHECK(!mc_client_item_billboard_uv_capture(NULL, 0) && !mc_client_item_billboard_entered(NULL));
    const unsigned int csr = _mm_getcsr();
    const float before[4] = {prefix.uv[0], prefix.uv[1], prefix.uv[2], prefix.uv[3]};
    _mm_setcsr(csr | 0x8000u);
    const bool unsupported = mc_client_item_billboard_uv_capture(&prefix, 53);
    _mm_setcsr(csr);
    CHECK(!unsupported && prefix.source_pc == 421 &&
          prefix.error.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP && prefix.error.native_policy &&
          !prefix.source_exception && memcmp(before, prefix.uv, sizeof(before)) == 0);
    _mm_setcsr(csr | 0x8000u);
    const bool null_nw = mc_client_item_billboard_entered(&prefix);
    _mm_setcsr(csr);
    CHECK(!null_nw && prefix.source_pc == 673 && prefix.vertices == 0 && prefix.source_exception &&
          strcmp(prefix.exception_class, "java/lang/NullPointerException") == 0 &&
          prefix.error.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE && !prefix.error.native_policy);
    ok = true;
cleanup:
    return ok;
}

static bool failed_normal_prefix(void) {
    bool ok = false;
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_item_billboard_context prefix = {0};
    mc_client_nw_fields fields;
    int32_t prior[48];
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(graphics,
                                  (mc_client_graphics_site){"bb", "a", "(Lhl;DDDFF)V", 403},
                                  &prefix.nw, &prefix.error) &&
          mc_client_nw_fields_get(prefix.nw, &fields) && fields.g != NULL && fields.B >= 48);
    memcpy(prior, fields.g, sizeof(prior));
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8000u);
    const bool returned = mc_client_item_billboard_entered(&prefix);
    _mm_setcsr(csr);
    CHECK(!returned && prefix.source_pc == 681 && prefix.vertices == 0 &&
          !prefix.source_exception && prefix.error.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP &&
          prefix.error.native_policy && prefix.error.site.pc == 23 &&
          strcmp(prefix.error.site.class_name, "nw") == 0 &&
          mc_client_nw_fields_get(prefix.nw, &fields) && fields.w && fields.n && !fields.m &&
          !fields.l && fields.h == 0 && fields.p == 0 && fields.o == 0 &&
          memcmp(prior, fields.g, sizeof(prior)) == 0);
    ok = true;
cleanup:
    if (graphics != NULL &&
        mc_client_render_engine_classes_release(graphics) != MC_CLIENT_RENDER_ENGINE_OK)
        ok = false;
    return ok;
}

int main(void) {
    for (unsigned int profile = 0; profile < 4; ++profile)
        for (unsigned int copy = 0; copy <= profile; ++copy)
            if (!row(profile, copy))
                return 1;
    if (!signed_uv_controls() || !failed_normal_prefix())
        return 1;
    printf("Item billboard entered-prefix CPU controls: %u checks; ten genuine NW contexts; "
           "zero completed draws\n",
           checks);
    return 0;
}
