#include "mc/client/controller/render_interpolation.h"

#include "../application_state.h"
#include "../gui/gui_ingame_internal.h"
#include "../world/render_global_internal.h"
#include "singleplayer_internal.h"

#include <string.h>
#include <xmmintrin.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Controller interpolation requires the qualified x86-64 profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Controller interpolation requires strict floating transformations"
#endif

static mc_singleplayer_render_interpolation_status
result(mc_singleplayer_render_interpolation_error *error,
       mc_singleplayer_render_interpolation_status status, const char *operation, unsigned int pc,
       bool native_policy) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->status = status;
        error->operation = operation;
        error->native_policy = native_policy;
        if (!native_policy || pc != 0)
            error->site = (mc_player_site){"os", "a", "(F)V", pc};
    }
    return status;
}

static mc_singleplayer_render_interpolation_status
body(mc_client_singleplayer_controller *controller, float partial,
     mc_singleplayer_render_interpolation_report *report,
     mc_singleplayer_render_interpolation_error *error) {
    uint32_t branch;
    memcpy(&branch, &controller->fields.f, sizeof(branch));
    memcpy(&report->branch_f, &branch, sizeof(branch));
    memcpy(&report->partial, &partial, sizeof(partial));
    report->source_entered = true;
    report->source_pc = 5;
    const uint32_t magnitude = branch & UINT32_C(0x7fffffff);
    /* fcmpg returns +1 for NaN. The following ifgt therefore chooses the
     * arithmetic branch for both signs of NaN, unlike a positive-only test. */
    report->arithmetic_branch = magnitude > UINT32_C(0x7f800000) ||
                                (magnitude != 0 && (branch & UINT32_C(0x80000000)) == 0);
    float value = 0.0f;
    if (report->arithmetic_branch) {
        memcpy(&report->g, &controller->fields.g, sizeof(float)); /* Source g35. */
        memcpy(&report->f, &controller->fields.f, sizeof(float)); /* Source f39. */
        report->source_pc = 46;
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
            return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_FLOATING,
                          "controller interpolation native floating environment", 46, true);
        const __m128 difference =
            _mm_sub_ss(_mm_set_ss(controller->fields.f), _mm_set_ss(controller->fields.g));
        const __m128 product = _mm_mul_ss(difference, _mm_set_ss(partial));
        value = _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(report->g), product));
        report->arithmetic_returned = true;
    }
    memcpy(&report->value, &value, sizeof(value));
    const unsigned int gui_app_pc = report->arithmetic_branch ? 55u : 13u;
    const unsigned int gui_store_pc = report->arithmetic_branch ? 59u : 17u;
    report->source_pc = gui_app_pc;
    client_application *app = controller->fields.a;
    report->gui_app = app;
    if (app == NULL)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_SOURCE_NULL,
                      "actual os.a(F) null App.v receiver", gui_app_pc, false);
    mc_client_source_uq *gui = app->v;
    report->source_pc = gui_store_pc;
    if (gui == NULL)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_SOURCE_NULL,
                      "actual os.a(F) null GuiIngame field receiver", gui_store_pc, false);
    if (gui->phase == MC_CLIENT_GUI_INGAME_CONSTRUCTING ||
        gui->phase == MC_CLIENT_GUI_INGAME_RETIRING)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_GUI_OWNER,
                      "controller interpolation constructing/retiring GuiIngame owner",
                      gui_store_pc, true);
    memcpy(&gui->fields.b, &value, sizeof(value));
    report->gui_stored = true;
    const unsigned int global_app_pc = report->arithmetic_branch ? 66u : 24u;
    const unsigned int global_store_pc = report->arithmetic_branch ? 70u : 28u;
    report->source_pc = global_app_pc;
    app = controller->fields.a;
    report->global_app = app;
    if (app == NULL)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_SOURCE_NULL,
                      "actual os.a(F) null App.g receiver", global_app_pc, false);
    mc_client_render_global *global = app->g;
    report->source_pc = global_store_pc;
    if (global == NULL)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_SOURCE_NULL,
                      "actual os.a(F) null RenderGlobal field receiver", global_store_pc, false);
    if (global->active || global->phase == MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING ||
        global->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_GLOBAL_OWNER,
                      "controller interpolation active/retiring RenderGlobal owner",
                      global_store_pc, true);
    memcpy(&global->fields.i, &value, sizeof(value));
    report->global_stored = true;
    report->source_returned = true;
    report->source_pc = 73;
    return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_OK, NULL, 73, false);
}

mc_singleplayer_render_interpolation_status mc_client_singleplayer_controller_render_interpolation(
    mc_client_singleplayer_controller *controller, float partial,
    mc_singleplayer_render_interpolation_report *out,
    mc_singleplayer_render_interpolation_error *error) {
    if (controller == NULL)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_ARGUMENT,
                      "controller interpolation native receiver", 0, true);
    if (controller->fields.entered)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_BUSY,
                      "controller interpolation entered owner", 0, true);
    if (controller->fields.phase != MC_SINGLEPLAYER_CONTROLLER_READY)
        return result(error, MC_SINGLEPLAYER_RENDER_INTERPOLATION_PHASE,
                      "controller interpolation READY owner", 0, true);
    mc_singleplayer_render_interpolation_report report = {0};
    controller->fields.entered = true;
    const mc_singleplayer_render_interpolation_status status =
        body(controller, partial, &report, error);
    controller->fields.entered = false;
    if (out != NULL)
        *out = report;
    return status;
}
