/* Included by canonical Display after source_surface.inc. */
#include "camera_surface.h"
#include <emmintrin.h>

static void camera_matrix_multiply(mc_client_source_state *state, const float right[16]) {
    mc_client_modelview before, result;
    source_matrix_copy(state, &before);
    for (size_t col = 0; col < 4; ++col)
        for (size_t row = 0; row < 4; ++row) {
            float value = mc_client_source_product(before.column_major[row], right[col * 4]);
            for (size_t k = 1; k < 4; ++k)
                value = mc_client_source_sum(
                    value,
                    mc_client_source_product(before.column_major[k * 4 + row], right[col * 4 + k]));
            result.column_major[col * 4 + row] = value;
        }
    source_matrix_store(state, &result);
}

static float camera_narrow(double value) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
}

bool mc_client_display_camera_matrices_inspect(const mc_client_display *display,
                                               float projection[16], float modelview[16]) {
    if (display == NULL || projection == NULL || modelview == NULL ||
        display->source.active_calls != 0 || !display->source.initialized)
        return false;
    memcpy(projection, display->source.projection, sizeof(display->source.projection));
    memcpy(modelview, display->source.modelview.column_major,
           sizeof(display->source.modelview.column_major));
    return true;
}

bool mc_client_display_source_scale(mc_client_display *display, double x, double y, double z,
                                    mc_client_display_error *error) {
    if (!source_enter(display, "source camera scale", error))
        return false;
    bool ok;
    if (display->source.compiling != NULL) {
        mc_client_source_command *command = source_append(display, SOURCE_COMMAND_SCALE, 0, error);
        ok = command != NULL;
        if (ok) {
            memcpy(&command->value.scale[0], &x, sizeof(x));
            memcpy(&command->value.scale[1], &y, sizeof(y));
            memcpy(&command->value.scale[2], &z, sizeof(z));
        }
    } else {
        ok = source_fp_admit("source camera scale FP", error);
        if (ok)
            source_matrix_scale(&display->source, x, y, z);
    }
    return source_leave(display, ok, error);
}

bool mc_client_display_source_rotate(mc_client_display *display, float angle, float x, float y,
                                     float z, mc_client_display_error *error) {
    if (!source_setting_enter(display, "source camera rotation", error))
        return false;
    if (!source_fp_admit("source camera rotation FP", error))
        return source_leave(display, false, error);
    if (angle == 0)
        return source_leave(display, true, error);
    const double norm = sqrt((double)x * x + (double)y * y + (double)z * z);
    if (norm == 0)
        return source_leave(display, true, error);
    x = camera_narrow((double)x / norm);
    y = camera_narrow((double)y / norm);
    z = camera_narrow((double)z / norm);
    const double radians = (double)angle * (3.14159265358979323846 / 180.0);
    const float c = camera_narrow(cos(radians)), s = camera_narrow(sin(radians));
    const float t = 1.0f - c;
    const float rotation[16] = {t * x * x + c,
                                t * x * y + s * z,
                                t * x * z - s * y,
                                0,
                                t * x * y - s * z,
                                t * y * y + c,
                                t * y * z + s * x,
                                0,
                                t * x * z + s * y,
                                t * y * z - s * x,
                                t * z * z + c,
                                0,
                                0,
                                0,
                                0,
                                1};
    camera_matrix_multiply(&display->source, rotation);
    return source_leave(display, true, error);
}

bool mc_client_display_source_perspective(mc_client_display *display, float fov, float aspect,
                                          float near_plane, float far_plane,
                                          mc_client_display_error *error) {
    if (!source_setting_enter(display, "source GLU perspective", error))
        return false;
    if (!source_fp_admit("source GLU perspective FP", error))
        return source_leave(display, false, error);
    /* Actual LWJGL Project.gluPerspective float expression order. */
    const float radians = mc_client_source_product(fov / 2.0f, 3.1415927f) / 180.0f;
    const float sine = camera_narrow(sin((double)radians));
    const float delta = far_plane - near_plane;
    if (delta == 0 || sine == 0 || aspect == 0)
        return source_leave(display, true, error);
    const float cotangent = camera_narrow(cos((double)radians)) / sine;
    float matrix[16] = {0};
    matrix[0] = cotangent / aspect;
    matrix[5] = cotangent;
    matrix[10] = -(far_plane + near_plane) / delta;
    matrix[11] = -1;
    matrix[14] =
        mc_client_source_product(mc_client_source_product(-2, near_plane), far_plane) / delta;
    camera_matrix_multiply(&display->source, matrix);
    return source_leave(display, true, error);
}
