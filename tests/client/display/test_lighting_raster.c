/* Authored device scenes against the canonical source GL3 lighting bridge. */
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/gl_internal.h"
#include "client/display/source_surface.h"
#include "client/display/startup_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render_engine/nw.h"
#include "mc/client/render_engine/render_engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
static mc_client_display_error display_error;
static mc_client_graphics_error graphics_error;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Raster check%u at%d: %s; Display%d %s (%s); graphics%d %s\n", checks, \
                    __LINE__, #value, (int)display_error.status,                                   \
                    display_error.operation == NULL ? "-" : display_error.operation,               \
                    display_error.detail, (int)graphics_error.status,                              \
                    graphics_error.operation == NULL ? "-" : graphics_error.operation);            \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static const char *const names[] = {"byte_m128_negative",
                                    "byte_m127_negative",
                                    "byte_m1_negative",
                                    "byte_zero_positive",
                                    "byte_p1_positive",
                                    "byte_p64_positive",
                                    "byte_p126_positive",
                                    "byte_p127_positive",
                                    "byte_x_zero",
                                    "byte_mixed",
                                    "current_zero",
                                    "current_half",
                                    "current_unit",
                                    "current_controls_00",
                                    "current_controls_10",
                                    "current_controls_01",
                                    "current_controls_11",
                                    "byte_controls_00",
                                    "byte_controls_10",
                                    "byte_controls_01",
                                    "byte_controls_11",
                                    "triangle_flat",
                                    "triangle_smooth",
                                    "fan_flat",
                                    "fan_smooth",
                                    "rotate_after_upload",
                                    "rotate_before_upload",
                                    "mirrored_scale",
                                    "material_ambient",
                                    "material_diffuse",
                                    "material_emission",
                                    "material_specular",
                                    "material_switch_freeze",
                                    "list_replay",
                                    "texture_alpha",
                                    "texture_fog",
                                    "converted_quad_flat",
                                    "converted_quad_smooth",
                                    "specular_nonlocal",
                                    "specular_local",
                                    "specular_shininess_zero",
                                    "specular_shininess_32",
                                    "twoside_front",
                                    "twoside_back",
                                    "oneside_back",
                                    "back_tracking_color",
                                    "single_specular_texture",
                                    "separate_specular_texture",
                                    "separate_specular_texture_fog",
                                    "unlit_triangle_flat",
                                    "unlit_triangle_smooth",
                                    "unlit_fan_flat",
                                    "unlit_fan_smooth",
                                    "clamped_lighting_smooth",
                                    "clamped_lighting_flat",
                                    "positional_local_specular",
                                    "actual_quad_flat",
                                    "actual_quad_smooth"};

static const uint8_t colors[6][4] = {{128, 64, 32, 255},  {32, 192, 64, 160},  {64, 32, 224, 96},
                                     {224, 128, 64, 224}, {64, 224, 128, 128}, {128, 64, 32, 255}};

static void material(mc_client_display *display, int32_t face, int32_t parameter, float r, float g,
                     float b, float a) {
    const float values[4] = {r, g, b, a};
    CHECK(mc_client_display_source_material(display, face, parameter, values, 8, &display_error));
}

static void light(mc_client_display *display, unsigned int index, int32_t parameter, float r,
                  float g, float b, float a) {
    const float values[4] = {r, g, b, a};
    CHECK(mc_client_display_source_light_parameter(display, index, parameter, values, 4,
                                                   &display_error));
}

static void words(const float *values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint32_t value;
        memcpy(&value, &values[i], sizeof(value));
        printf("%s%08" PRIx32, i == 0 ? "" : ",", value);
    }
}

static mc_client_display *create(void) {
    mc_client_display *display = NULL;
    CHECK(mc_client_display_create("Source lighting raster controls", 64, 64, true, &display,
                                   &display_error));
    mc_client_startup_projection_stage stage;
    CHECK(mc_client_display_startup_projection(display, 64, 64, &stage, &display_error));
    CHECK(mc_client_display_startup_viewport(display, 64, 64, &stage, &display_error));
    /* Declared native fixture environment, matching the legacy control's
     * glDisable(DITHER). This does not supply a source lighting draw or value. */
    SDL_FunctionPointer address = SDL_GL_GetProcAddress("glDisable");
    void(GLAPIENTRY * disable)(GLenum) = NULL;
    _Static_assert(sizeof(disable) == sizeof(address), "GL function pointer representation");
    CHECK(address != NULL);
    memcpy(&disable, &address, sizeof(disable));
    disable(GL_DITHER);
    SDL_FunctionPointer string_address = SDL_GL_GetProcAddress("glGetString");
    SDL_FunctionPointer integer_address = SDL_GL_GetProcAddress("glGetIntegerv");
    mc_client_gl_get_string_fn get_string = NULL;
    mc_client_gl_get_integer_fn get_integer = NULL;
    _Static_assert(sizeof(get_string) == sizeof(string_address),
                   "GL string pointer representation");
    _Static_assert(sizeof(get_integer) == sizeof(integer_address),
                   "GL query pointer representation");
    CHECK(string_address != NULL && integer_address != NULL);
    memcpy(&get_string, &string_address, sizeof(get_string));
    memcpy(&get_integer, &integer_address, sizeof(get_integer));
    const char *vendor = (const char *)get_string(GL_VENDOR);
    const char *renderer = (const char *)get_string(GL_RENDERER);
    const char *version = (const char *)get_string(GL_VERSION);
    CHECK(vendor != NULL && renderer != NULL && version != NULL);
    GLint dither = 0;
    get_integer(GL_DITHER, &dither);
    mc_client_framebuffer_info format;
    CHECK(mc_client_display_framebuffer_info_get(display, &format, &display_error));
    printf("RASTER_NATIVE_DRIVER vendor=%.160s | renderer=%.240s | version=%.160s\n", vendor,
           renderer, version);
    printf("RASTER_NATIVE_FORMAT rgba=%d,%d,%d,%d depth=%d stencil=%d sample_buffers=%d "
           "samples=%d double_buffer=%u dither=%d\n",
           format.red_bits, format.green_bits, format.blue_bits, format.alpha_bits,
           format.depth_bits, format.stencil_bits, format.sample_buffers, format.samples,
           format.double_buffer ? 1u : 0u, dither);
    return display;
}

static void normal_controls(mc_client_display *display, unsigned int controls) {
    CHECK(mc_client_display_source_normalize(display, (controls & 1u) != 0, &display_error));
    CHECK(mc_client_display_source_rescale_normal(display, (controls & 2u) != 0, &display_error));
}

static void setup(mc_client_display *display, bool simple) {
    CHECK(mc_client_display_source_viewport(display, 64, 64, &display_error));
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, &display_error));
    CHECK(mc_client_display_source_load_identity(display, &display_error));
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &display_error));
    CHECK(mc_client_display_source_load_identity(display, &display_error));
    CHECK(mc_client_display_source_blend_enable(display, false, &display_error));
    CHECK(mc_client_display_source_alpha_test(display, false, &display_error));
    CHECK(mc_client_display_source_depth_test(display, false, &display_error));
    CHECK(mc_client_display_source_cull_test(display, false, &display_error));
    CHECK(mc_client_display_source_texture_enable(display, false, &display_error));
    CHECK(mc_client_display_source_fog_enable(display, false, &display_error));
    CHECK(mc_client_display_source_color_material_enable(display, false, &display_error));
    CHECK(mc_client_display_source_lighting_enable(display, true, &display_error));
    CHECK(mc_client_display_source_light_enable(display, 0, true, &display_error));
    CHECK(mc_client_display_source_light_enable(display, 1, !simple, &display_error));
    CHECK(mc_client_display_source_light_model_i(display, GL_LIGHT_MODEL_LOCAL_VIEWER, 0,
                                                 &display_error));
    CHECK(mc_client_display_source_light_model_i(display, GL_LIGHT_MODEL_TWO_SIDE, 0,
                                                 &display_error));
    const float ambient[4] = {simple ? 0.0f : .125f, simple ? 0.0f : .25f, simple ? 0.0f : .125f,
                              1};
    CHECK(mc_client_display_source_light_model(display, GL_LIGHT_MODEL_AMBIENT, ambient, 4,
                                               &display_error));
    material(display, GL_FRONT_AND_BACK, GL_AMBIENT, simple ? 0.0f : .2f, simple ? 0.0f : .2f,
             simple ? 0.0f : .2f, 1);
    material(display, GL_FRONT_AND_BACK, GL_DIFFUSE, simple ? 1.0f : .8f, simple ? 1.0f : .8f,
             simple ? 1.0f : .8f, 1);
    material(display, GL_FRONT_AND_BACK, GL_SPECULAR, 0, 0, 0, 1);
    material(display, GL_FRONT_AND_BACK, GL_EMISSION, 0, 0, 0, 1);
    material(display, GL_FRONT_AND_BACK, GL_SHININESS, 0, 0, 0, 0);
    light(display, 0, GL_POSITION, 0, 0, 1, 0);
    light(display, 1, GL_POSITION, 1, 0, 0, 0);
    light(display, 0, GL_DIFFUSE, simple ? 1.0f : .5f, simple ? 1.0f : .25f, simple ? 1.0f : .125f,
          1);
    light(display, 1, GL_DIFFUSE, .125f, .5f, .25f, 1);
    for (unsigned int index = 0; index < 2; ++index) {
        light(display, index, GL_AMBIENT, 0, 0, 0, 1);
        light(display, index, GL_SPECULAR, 0, 0, 0, 1);
    }
    CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK,
                                                  GL_AMBIENT_AND_DIFFUSE, &display_error));
    CHECK(mc_client_display_source_color(display, 1, 1, 1, 1, &display_error));
    CHECK(mc_client_display_source_normal(display, 0, 0, 1, &display_error));
    normal_controls(display, 0);
    CHECK(mc_client_display_source_shade_model(display, GL_SMOOTH, &display_error));
    CHECK(mc_client_display_source_alpha_function(display, GL_GREATER, .1f, &display_error));
    CHECK(mc_client_display_source_fog_i(display, GL_FOG_MODE, GL_LINEAR, &display_error));
    CHECK(mc_client_display_source_fog_f(display, GL_FOG_START, 0, &display_error));
    CHECK(mc_client_display_source_fog_f(display, GL_FOG_END, 1, &display_error));
    const float fog[4] = {.25f, .5f, .75f, 1};
    CHECK(mc_client_display_source_fog_color(display, fog, &display_error));
    CHECK(mc_client_display_source_clear_color(display, 0, 0, 0, 0, &display_error));
    CHECK(mc_client_display_source_clear(display, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                                         &display_error));
}

static int geometry(uint8_t bytes[192], bool fan, bool scaled, bool fog) {
    static const float triangle[3][3] = {{-.75f, -.75f, 0}, {.75f, -.75f, 0}, {-.75f, .75f, 0}};
    static const float perimeter[6][3] = {{0, 0, 0},       {-.75f, -.75f, 0}, {.75f, -.75f, 0},
                                          {.75f, .75f, 0}, {-.75f, .75f, 0},  {-.75f, -.75f, 0}};
    static const int8_t normals[6][3] = {{0, 0, 127},   {64, 0, 64},   {0, 127, 0},
                                         {-64, 64, 64}, {0, -64, 127}, {0, 0, 127}};
    static const float depths[6] = {-.5f, -.25f, -.75f, -.5f, -.25f, -.75f};
    const int count = fan ? 6 : 3;
    memset(bytes, 0, 192);
    for (int i = 0; i < count; ++i) {
        const float *position = fan ? perimeter[i] : triangle[i];
        const float values[5] = {position[0] / (scaled ? 2.0f : 1.0f),
                                 position[1] / (scaled ? 3.0f : 1.0f), fog ? depths[i] : 0,
                                 (position[0] + .75f) / 1.5f, (position[1] + .75f) / 1.5f};
        memcpy(bytes + (size_t)i * 32, values, sizeof(values));
        memcpy(bytes + (size_t)i * 32 + 20, colors[i], sizeof(colors[i]));
        memcpy(bytes + (size_t)i * 32 + 24, normals[i], sizeof(normals[i]));
    }
    return count;
}

static void constant_bytes(uint8_t *bytes, int count, int8_t x, int8_t y, int8_t z) {
    const int8_t normal[3] = {x, y, z};
    for (int i = 0; i < count; ++i)
        memcpy(bytes + (size_t)i * 32 + 24, normal, sizeof(normal));
}

static void arrays(mc_client_display *display, mc_client_display_context_lease *lease,
                   uint8_t *bytes, bool normals, bool color, bool texture) {
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, bytes, 0,
                                           &display_error));
    CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true,
                                         &display_error));
    const bool enabled[3] = {texture, color, normals};
    const size_t offsets[3] = {12, 20, 24};
    const mc_client_source_array kinds[3] = {
        MC_CLIENT_SOURCE_UV_ARRAY, MC_CLIENT_SOURCE_COLOR_ARRAY, MC_CLIENT_SOURCE_NORMAL_ARRAY};
    for (size_t i = 0; i < 3; ++i) {
        if (enabled[i])
            CHECK(mc_client_display_source_pointer(lease, kinds[i], bytes, offsets[i],
                                                   &display_error));
        CHECK(mc_client_display_source_array(display, kinds[i], enabled[i], &display_error));
    }
}

static void pixels(mc_client_display *display, const char *name, unsigned int frame) {
    mc_client_capture capture = {0};
    CHECK(mc_client_display_capture_rgba(display, &capture, &display_error));
    CHECK(capture.width == 64 && capture.height == 64 && capture.stride == 256);
    printf("RASTER %s %u ", name, frame);
    for (size_t i = 0; i < capture.stride * (size_t)capture.height; ++i)
        printf("%02x", (unsigned int)capture.pixels[i]);
    putchar('\n');
    mc_client_capture_free(&capture);
    mc_client_source_fog_state state;
    mc_client_source_draw_state draw;
    CHECK(mc_client_display_source_fog_inspect(display, &state));
    CHECK(mc_client_display_source_draw_inspect(display, &draw));
    printf("RASTER_STATE %s %u %u%u%u%u%u%u%u%u%u %d %d %d %d\n", name, frame,
           state.lighting_enabled ? 1u : 0u, state.lights[0] ? 1u : 0u, state.lights[1] ? 1u : 0u,
           state.normalize_enabled ? 1u : 0u, state.rescale_normal_enabled ? 1u : 0u,
           state.color_material ? 1u : 0u, draw.texture_enabled ? 1u : 0u,
           draw.alpha_test ? 1u : 0u, state.enabled ? 1u : 0u, state.material_face,
           state.material_mode, draw.shade_flat ? GL_FLAT : GL_SMOOTH, state.mode);
}

static mc_client_texture texture(mc_client_display *display,
                                 mc_client_texture_resource **resource) {
    static const uint8_t texels[16] = {255, 64, 32,  0,   32,  255, 64,  128,
                                       64,  32, 255, 255, 255, 255, 255, 255};
    const mc_image_view image = {2, 2, 8, texels};
    mc_client_texture id = 0;
    mc_client_texture_generation generation = {0};
    CHECK(mc_client_display_generate_texture(display, &id, &generation, &display_error));
    CHECK(generation.resource != NULL && generation.driver_generated &&
          generation.buffer_value == id);
    *resource = generation.resource;
    CHECK(mc_client_display_bind_texture(*resource, &display_error));
    CHECK(mc_client_display_texture_parameter(*resource, MC_CLIENT_TEXTURE_MIN_FILTER, GL_NEAREST,
                                              &display_error));
    CHECK(mc_client_display_texture_parameter(*resource, MC_CLIENT_TEXTURE_MAG_FILTER, GL_NEAREST,
                                              &display_error));
    CHECK(mc_client_display_texture_parameter(*resource, MC_CLIENT_TEXTURE_WRAP_S, GL_REPEAT,
                                              &display_error));
    CHECK(mc_client_display_texture_parameter(*resource, MC_CLIENT_TEXTURE_WRAP_T, GL_REPEAT,
                                              &display_error));
    CHECK(mc_client_display_upload_existing_texture(*resource, &image, &display_error));
    CHECK(mc_client_display_source_bind_id(display, id, &display_error));
    CHECK(mc_client_display_source_texture_enable(display, true, &display_error));
    CHECK(mc_client_display_source_alpha_test(display, true, &display_error));
    return id;
}

static void converted(mc_client_display *display, bool flat,
                      mc_client_render_engine_classes **classes) {
    CHECK(mc_client_display_source_color_material_enable(display, true, &display_error));
    CHECK(
        mc_client_display_source_shade_model(display, flat ? GL_FLAT : GL_SMOOTH, &display_error));
    CHECK(mc_client_render_engine_classes_create(NULL, classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_tessellator *tessellator = NULL;
    const mc_client_graphics_site site = {"fixture", "lighting_raster", "()V", -1};
    CHECK(mc_client_nw_active_use(*classes, site, &tessellator, &graphics_error));
    CHECK(mc_client_nw_begin_quads(tessellator, &graphics_error));
    static const float positions[4][3] = {
        {-.75f, -.75f, 0}, {.75f, -.75f, 0}, {.75f, .75f, 0}, {-.75f, .75f, 0}};
    static const float normals[4][3] = {{0, 0, 1}, {.5f, 0, .5f}, {0, 1, 0}, {0, 0, -1}};
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_client_nw_color(tessellator, colors[i][0], colors[i][1], colors[i][2],
                                 colors[i][3], &graphics_error));
        CHECK(mc_client_nw_normal(tessellator, normals[i][0], normals[i][1], normals[i][2],
                                  &graphics_error));
        /* Frozen A's Java float[][] selects a(FFF)V, the color overload.
         * These two historical cases reach a genuine empty NW draw. */
        CHECK(mc_client_nw_color_float(tessellator, positions[i][0], positions[i][1],
                                       positions[i][2], &graphics_error));
    }
    mc_client_nw_state state;
    CHECK(mc_client_nw_inspect(*classes, &state) && state.fields.h == 0 && state.fields.o == 0);
    CHECK(mc_client_nw_draw(tessellator, display, &graphics_error));
}

static void reverse_triangle(uint8_t bytes[192]) {
    uint8_t first[32];
    memcpy(first, bytes + 32, sizeof(first));
    memcpy(bytes + 32, bytes + 64, sizeof(first));
    memcpy(bytes + 64, first, sizeof(first));
}

static void model_state(mc_client_display *display, const char *name) {
    mc_client_source_fog_state state;
    CHECK(mc_client_display_source_fog_inspect(display, &state));
    printf("RASTER_MODEL %s 0 %u %u %d\n", name, state.light_model_local_viewer ? 1u : 0u,
           state.light_model_two_side ? 1u : 0u, state.light_model_color_control);
}

static void extended_scene(mc_client_display *display, int index,
                           mc_client_display_context_lease **lease,
                           mc_client_texture_resource **texture_resource) {
    const char *name = names[38 + index];
    setup(display, false);
    const float ambient[4] = {0, 0, 0, 1};
    CHECK(mc_client_display_source_light_model(display, GL_LIGHT_MODEL_AMBIENT, ambient, 4,
                                               &display_error));
    CHECK(mc_client_display_source_light_model_i(display, GL_LIGHT_MODEL_LOCAL_VIEWER, 0,
                                                 &display_error));
    CHECK(mc_client_display_source_light_model_i(display, GL_LIGHT_MODEL_TWO_SIDE, 0,
                                                 &display_error));
    CHECK(mc_client_display_source_light_model_i(display, GL_LIGHT_MODEL_COLOR_CONTROL,
                                                 GL_SINGLE_COLOR, &display_error));
    CHECK(mc_client_display_source_light_enable(display, 1, false, &display_error));
    const bool unlit = index >= 11 && index <= 14;
    const bool fan = index == 13 || index == 14 || (index >= 8 && index <= 10);
    const bool color_array = unlit || index == 7;
    const bool normal_array = index == 7 || index == 15 || index == 16;
    uint8_t bytes[192];
    const int count = geometry(bytes, fan, false, !unlit);
    material(display, GL_FRONT_AND_BACK, GL_AMBIENT, 0, 0, 0, 1);
    material(display, GL_FRONT_AND_BACK, GL_DIFFUSE, 0, 0, 0, .625f);
    material(display, GL_FRONT_AND_BACK, GL_SPECULAR, 1, 1, 1, 1);
    material(display, GL_FRONT_AND_BACK, GL_EMISSION, 0, 0, 0, 1);
    material(display, GL_FRONT_AND_BACK, GL_SHININESS, 8, 0, 0, 0);
    light(display, 0, GL_DIFFUSE, 0, 0, 0, 1);
    light(display, 0, GL_SPECULAR, 1, .5f, .25f, 1);
    CHECK(mc_client_display_source_normal(display, 0, .6f, .8f, &display_error));
    mc_client_texture texture_id = 0;
    if (index <= 3) {
        if (index == 1)
            CHECK(mc_client_display_source_light_model_i(display, GL_LIGHT_MODEL_LOCAL_VIEWER, 1,
                                                         &display_error));
        if (index == 2)
            material(display, GL_FRONT_AND_BACK, GL_SHININESS, 0, 0, 0, 0);
        if (index == 3)
            material(display, GL_FRONT_AND_BACK, GL_SHININESS, 32, 0, 0, 0);
    } else if (index >= 4 && index <= 7) {
        material(display, GL_FRONT_AND_BACK, GL_SPECULAR, 0, 0, 0, 1);
        material(display, GL_FRONT, GL_DIFFUSE, 1, 0, 0, .25f);
        material(display, GL_BACK, GL_DIFFUSE, 0, 1, 0, .75f);
        light(display, 0, GL_DIFFUSE, 1, 1, 1, 1);
        light(display, 0, GL_SPECULAR, 0, 0, 0, 1);
        if (index != 6)
            CHECK(mc_client_display_source_light_model_i(display, GL_LIGHT_MODEL_TWO_SIDE, 1,
                                                         &display_error));
        CHECK(mc_client_display_source_normal(display, 0, 0, index == 4 ? 1.0f : -1.0f,
                                              &display_error));
        if (index != 4)
            reverse_triangle(bytes);
        if (index == 7) {
            constant_bytes(bytes, count, 0, 0, -128);
            CHECK(mc_client_display_source_color_material(display, GL_BACK, GL_DIFFUSE,
                                                          &display_error));
            CHECK(mc_client_display_source_color(display, .25f, .75f, .5f, .625f, &display_error));
            CHECK(mc_client_display_source_color_material_enable(display, true, &display_error));
        }
    } else if (index >= 8 && index <= 10) {
        material(display, GL_FRONT_AND_BACK, GL_DIFFUSE, .5f, .5f, .5f, .75f);
        material(display, GL_FRONT_AND_BACK, GL_SPECULAR, .5f, .75f, .25f, 1);
        light(display, 0, GL_DIFFUSE, .5f, .25f, .125f, 1);
        light(display, 0, GL_SPECULAR, .5f, .5f, .5f, 1);
        CHECK(mc_client_display_source_normal(display, 0, 0, 1, &display_error));
        texture_id = texture(display, texture_resource);
        if (index != 8)
            CHECK(mc_client_display_source_light_model_i(
                display, GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR, &display_error));
        if (index == 10)
            CHECK(mc_client_display_source_fog_enable(display, true, &display_error));
    } else if (unlit) {
        CHECK(mc_client_display_source_lighting_enable(display, false, &display_error));
        CHECK(mc_client_display_source_shade_model(
            display, index == 11 || index == 13 ? GL_FLAT : GL_SMOOTH, &display_error));
    } else if (index == 15 || index == 16) {
        CHECK(mc_client_display_source_light_enable(display, 1, true, &display_error));
        material(display, GL_FRONT_AND_BACK, GL_DIFFUSE, 1, 1, 1, 1);
        material(display, GL_FRONT_AND_BACK, GL_SPECULAR, 0, 0, 0, 1);
        for (unsigned int light_index = 0; light_index < 2; ++light_index) {
            light(display, light_index, GL_POSITION, 0, 0, 1, 0);
            light(display, light_index, GL_DIFFUSE, 1, 1, 1, 1);
            light(display, light_index, GL_SPECULAR, 0, 0, 0, 1);
        }
        static const int8_t z[3] = {127, 64, 0};
        for (size_t i = 0; i < 3; ++i) {
            bytes[i * 32 + 24] = bytes[i * 32 + 25] = 0;
            memcpy(bytes + i * 32 + 26, &z[i], sizeof(z[i]));
        }
        CHECK(mc_client_display_source_shade_model(display, index == 16 ? GL_FLAT : GL_SMOOTH,
                                                   &display_error));
    } else if (index == 17) {
        CHECK(mc_client_display_source_light_model_i(display, GL_LIGHT_MODEL_LOCAL_VIEWER, 1,
                                                     &display_error));
        light(display, 0, GL_POSITION, .25f, .25f, .5f, 1);
        light(display, 0, GL_DIFFUSE, .25f, .5f, .25f, 1);
        material(display, GL_FRONT_AND_BACK, GL_DIFFUSE, .5f, .25f, .75f, .625f);
    }
    CHECK(mc_client_display_context_acquire(display, lease, &display_error));
    arrays(display, *lease, bytes, normal_array, color_array, texture_id != 0);
    CHECK(mc_client_display_source_batch(*lease, bytes, bytes, (size_t)count * 32, count,
                                         fan ? GL_TRIANGLE_FAN : GL_TRIANGLES, &display_error));
    pixels(display, name, 0);
    model_state(display, name);
}

static void actual_quad(mc_client_display *display, const char *name, bool flat,
                        mc_client_render_engine_classes **classes) {
    setup(display, false);
    CHECK(mc_client_display_source_color_material_enable(display, true, &display_error));
    CHECK(
        mc_client_display_source_shade_model(display, flat ? GL_FLAT : GL_SMOOTH, &display_error));
    CHECK(mc_client_render_engine_classes_create(NULL, classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_tessellator *tessellator = NULL;
    const mc_client_graphics_site site = {"fixture", "lighting_actual_quad", "()V", -1};
    CHECK(mc_client_nw_active_use(*classes, site, &tessellator, &graphics_error));
    CHECK(mc_client_nw_begin_quads(tessellator, &graphics_error));
    static const double positions[4][3] = {
        {-.75, -.75, 0}, {.75, -.75, 0}, {.75, .75, 0}, {-.75, .75, 0}};
    static const float normals[4][3] = {{0, 0, 1}, {.5f, 0, .5f}, {0, 1, 0}, {0, 0, -1}};
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_client_nw_color(tessellator, colors[i][0], colors[i][1], colors[i][2],
                                 colors[i][3], &graphics_error));
        CHECK(mc_client_nw_normal(tessellator, normals[i][0], normals[i][1], normals[i][2],
                                  &graphics_error));
        CHECK(mc_client_nw_vertex(tessellator, positions[i][0], positions[i][1], positions[i][2],
                                  &graphics_error));
    }
    mc_client_nw_state state;
    CHECK(mc_client_nw_inspect(*classes, &state) && state.static_b && !state.static_c &&
          state.fields.h == 6 && state.fields.o == 48 && state.fields.g != NULL && state.fields.n &&
          state.fields.l && !state.fields.m && state.fields.w);
    printf("CONVERTED_WORDS %s 6 48 ", name);
    for (size_t i = 0; i < 48; ++i) {
        uint32_t value;
        memcpy(&value, &state.fields.g[i], sizeof(value));
        printf("%s%08" PRIx32, i == 0 ? "" : ",", value);
    }
    putchar('\n');
    CHECK(mc_client_nw_draw(tessellator, display, &graphics_error));
    pixels(display, name, 0);
}

static void queried_material_words(mc_client_display *display, int32_t face, int32_t parameter,
                                   size_t count) {
    static const float sentinel[8] = {-123, -456, -789, -321, -654, -987, -111, -222};
    float values[8];
    CHECK(count >= 1 && count <= 4);
    memcpy(values, sentinel, sizeof(values));
    CHECK(
        mc_client_display_source_material_get(display, face, parameter, values, 8, &display_error));
    CHECK(memcmp(values + count, sentinel + count, (8u - count) * sizeof(float)) == 0);
    words(values, count);
}

static void queried_current_words(mc_client_display *display, int32_t parameter, size_t count) {
    static const float sentinel[8] = {-123, -456, -789, -321, -654, -987, -111, -222};
    float values[8];
    CHECK(count >= 1 && count <= 4);
    memcpy(values, sentinel, sizeof(values));
    CHECK(
        mc_client_display_source_current_attribute(display, parameter, values, 8, &display_error));
    CHECK(memcmp(values + count, sentinel + count, (8u - count) * sizeof(float)) == 0);
    words(values, count);
}

static void material_state(mc_client_display *display, const char *name) {
    /* Match the original getter order: each query can commit staged source
     * attributes. A passive inspection is not a substitute for these calls. */
    printf("MATERIAL %s", name);
    for (size_t face = 0; face < 2; ++face) {
        static const int32_t parameters[4] = {GL_AMBIENT, GL_DIFFUSE, GL_SPECULAR, GL_EMISSION};
        for (size_t parameter = 0; parameter < 4; ++parameter) {
            putchar(' ');
            queried_material_words(display, face == 0 ? GL_FRONT : GL_BACK, parameters[parameter],
                                   4);
        }
    }
    putchar(' ');
    queried_material_words(display, GL_FRONT, GL_SHININESS, 1);
    putchar(' ');
    queried_material_words(display, GL_BACK, GL_SHININESS, 1);
    putchar(' ');
    queried_current_words(display, GL_CURRENT_COLOR, 4);
    mc_client_source_fog_state state;
    CHECK(mc_client_display_source_fog_inspect(display, &state));
    CHECK(state.current_color_known);
    printf(" %u %d %d\n", state.color_material ? 1u : 0u, state.material_face, state.material_mode);
}

static void material_controls(mc_client_display *display) {
    material_state(display, "initial");
    CHECK(mc_client_display_source_color(display, .25f, .5f, .75f, .375f, &display_error));
    CHECK(mc_client_display_source_color_material(display, GL_FRONT, GL_AMBIENT, &display_error));
    CHECK(mc_client_display_source_color_material_enable(display, true, &display_error));
    material_state(display, "ambient_enabled");
    material(display, GL_FRONT, GL_AMBIENT, .875f, .125f, .25f, .625f);
    material_state(display, "tracked_ambient_direct_write");
    material(display, GL_FRONT, GL_AMBIENT_AND_DIFFUSE, .5f, .625f, .125f, .75f);
    material_state(display, "ambient_tracked_ambient_and_diffuse_write");
    CHECK(mc_client_display_source_color(display, .125f, .25f, .5f, .875f, &display_error));
    material_state(display, "next_color");
    CHECK(mc_client_display_source_color_material(display, GL_FRONT, GL_DIFFUSE, &display_error));
    material_state(display, "diffuse_selected");
    CHECK(mc_client_display_source_color_material(display, GL_BACK, GL_EMISSION, &display_error));
    material_state(display, "back_emission_selected");
    material(display, GL_BACK, GL_EMISSION, .875f, .125f, .25f, .625f);
    material_state(display, "tracked_emission_direct_write");
    CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK, GL_SPECULAR,
                                                  &display_error));
    material_state(display, "both_specular_selected");
    CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK,
                                                  GL_AMBIENT_AND_DIFFUSE, &display_error));
    material_state(display, "both_ambient_diffuse_selected");
    CHECK(mc_client_display_source_color_material_enable(display, false, &display_error));
    material_state(display, "tracking_disabled");
    CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .125f, &display_error));
    material_state(display, "disabled_next_color");
    material(display, GL_FRONT, GL_AMBIENT_AND_DIFFUSE, .5f, .625f, .125f, .75f);
    material_state(display, "disabled_direct_write");
    CHECK(mc_client_display_source_color_material_enable(display, true, &display_error));
    material_state(display, "tracking_reenabled");
    material(display, GL_BACK, GL_SHININESS, 32, 0, 0, 0);
    material_state(display, "untracked_shininess_write");
    material(display, GL_FRONT_AND_BACK, GL_EMISSION, 1.25f, -.125f, .375f, .5f);
    material_state(display, "untracked_out_of_range_emission");
}

static void material_prefix_controls(mc_client_display *display, int prefix) {
    /* No query or draw intervenes in these declared contiguous call groups. */
    setup(display, false);
    if (prefix >= 1) {
        CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK, GL_AMBIENT,
                                                      &display_error));
        CHECK(mc_client_display_source_color_material_enable(display, true, &display_error));
    }
    if (prefix >= 2)
        CHECK(mc_client_display_source_color(display, .25f, .5f, .75f, .5f, &display_error));
    if (prefix >= 3)
        CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK, GL_DIFFUSE,
                                                      &display_error));
    if (prefix >= 4)
        CHECK(mc_client_display_source_color(display, .75f, .25f, .5f, .75f, &display_error));
    if (prefix >= 5)
        CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK, GL_EMISSION,
                                                      &display_error));
    if (prefix >= 6)
        CHECK(mc_client_display_source_color(display, .125f, .25f, .125f, 1, &display_error));
    if (prefix >= 7)
        CHECK(mc_client_display_source_color_material_enable(display, false, &display_error));
    if (prefix >= 8)
        CHECK(mc_client_display_source_color(display, .875f, .5f, .25f, .125f, &display_error));
    char name[32];
    CHECK(snprintf(name, sizeof(name), "after_prefix_%d", prefix) > 0);
    material_state(display, name);
}

static void list_state(mc_client_display *display, const char *label) {
    printf("NORMAL_LIST_STATE %s ", label);
    queried_current_words(display, GL_CURRENT_NORMAL, 3);
    putchar(' ');
    queried_current_words(display, GL_CURRENT_COLOR, 4);
    putchar(' ');
    queried_material_words(display, GL_FRONT, GL_AMBIENT, 4);
    putchar(' ');
    queried_material_words(display, GL_FRONT, GL_DIFFUSE, 4);
    putchar(' ');
    queried_material_words(display, GL_BACK, GL_AMBIENT, 4);
    putchar(' ');
    queried_material_words(display, GL_BACK, GL_DIFFUSE, 4);
    mc_client_source_fog_state state;
    CHECK(mc_client_display_source_fog_inspect(display, &state));
    CHECK(state.normal_known && state.current_color_known);
    CHECK(state.materials[0].ambient_known && state.materials[0].diffuse_known &&
          state.materials[1].ambient_known && state.materials[1].diffuse_known);
    printf(" %u %d %d\n", state.color_material ? 1u : 0u, state.material_face, state.material_mode);
}

static void submitted_attribute(GLuint location, GLint components, float *out) {
    /* Observe submitted vertices separately from the driver's persistent
     * current attributes. Legacy list replay differs between those states. */
#define LIST_OBSERVATION_PROC(type, name, symbol)                                                  \
    type name = NULL;                                                                              \
    do {                                                                                           \
        SDL_FunctionPointer address = SDL_GL_GetProcAddress(symbol);                               \
        _Static_assert(sizeof(name) == sizeof(address), "GL observation pointer representation");  \
        CHECK(address != NULL);                                                                    \
        memcpy(&name, &address, sizeof(name));                                                     \
    } while (0)
    LIST_OBSERVATION_PROC(mc_client_gl_get_integer_fn, get_integer, "glGetIntegerv");
    LIST_OBSERVATION_PROC(mc_client_gl_get_error_fn, get_error, "glGetError");
    LIST_OBSERVATION_PROC(PFNGLGETVERTEXATTRIBIVPROC, get_attribute, "glGetVertexAttribiv");
    LIST_OBSERVATION_PROC(PFNGLGETVERTEXATTRIBPOINTERVPROC, get_pointer,
                          "glGetVertexAttribPointerv");
    LIST_OBSERVATION_PROC(PFNGLGETBUFFERPARAMETERIVPROC, get_buffer, "glGetBufferParameteriv");
    LIST_OBSERVATION_PROC(PFNGLGETBUFFERSUBDATAPROC, read_buffer, "glGetBufferSubData");
#undef LIST_OBSERVATION_PROC
    CHECK((location == 3 && components == 3) || (location == 2 && components == 4));
    CHECK(get_error() == GL_NO_ERROR);
    GLint binding = 0, capacity = 0, enabled = 0, size = 0, type = 0, stride = 0, normalized = 0,
          attribute_buffer = 0;
    void *pointer = NULL;
    get_integer(GL_ARRAY_BUFFER_BINDING, &binding);
    get_buffer(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &capacity);
    get_attribute(location, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    get_attribute(location, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    get_attribute(location, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
    get_attribute(location, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    get_attribute(location, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
    get_attribute(location, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &attribute_buffer);
    get_pointer(location, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
    const size_t offset = (size_t)(uintptr_t)pointer;
    CHECK(binding > 0 && capacity > 0 && capacity <= 4096 && enabled == GL_TRUE &&
          size == components && type == GL_FLOAT && normalized == GL_FALSE &&
          attribute_buffer == binding && stride >= components * (GLint)sizeof(float) &&
          stride <= 4096 && offset <= 4096 &&
          offset + 2u * (size_t)stride + (size_t)components * sizeof(float) <= (size_t)capacity);
    uint8_t uploaded[4096];
    read_buffer(GL_ARRAY_BUFFER, 0, (GLsizeiptr)capacity, uploaded);
    CHECK(get_error() == GL_NO_ERROR);
    for (size_t vertex = 0; vertex < 3; ++vertex)
        memcpy(out + vertex * (size_t)components, uploaded + offset + vertex * (size_t)stride,
               (size_t)components * sizeof(float));
}

static void submitted_list_normals(float out[9]) {
    submitted_attribute(3, 3, out);
}

static void list_controls(mc_client_display *display, mc_client_display_context_lease **lease,
                          mc_client_display_list_range **range) {
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, &display_error));
    CHECK(mc_client_display_source_load_identity(display, &display_error));
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &display_error));
    CHECK(mc_client_display_source_load_identity(display, &display_error));
    CHECK(mc_client_display_source_lighting_enable(display, true, &display_error));
    CHECK(mc_client_display_source_light_enable(display, 0, true, &display_error));
    CHECK(mc_client_display_source_light_enable(display, 1, true, &display_error));
    CHECK(mc_client_display_source_color_material_enable(display, false, &display_error));
    normal_controls(display, 0);
    CHECK(mc_client_display_source_depth_test(display, false, &display_error));
    CHECK(mc_client_display_source_cull_test(display, false, &display_error));
    CHECK(mc_client_display_source_texture_enable(display, false, &display_error));
    CHECK(mc_client_display_context_acquire(display, lease, &display_error));
    int32_t id = 0;
    CHECK(mc_client_display_lists_generate(display, 1, 8, range, &id, &display_error));
    uint8_t bytes[192];
    for (int value = -128; value <= 127; ++value) {
        CHECK(geometry(bytes, false, false, false) == 3);
        for (size_t vertex = 0; vertex < 3; ++vertex)
            memset(bytes + vertex * 32 + 12, 0, 20);
        constant_bytes(bytes, 3, (int8_t)value, (int8_t)value, (int8_t)value);
        arrays(display, *lease, bytes, true, false, false);
        CHECK(mc_client_display_lists_begin(display, id, &display_error));
        CHECK(mc_client_display_source_batch(*lease, bytes, bytes, 96, 3, GL_TRIANGLES,
                                             &display_error));
        CHECK(mc_client_display_lists_end(display, &display_error));
        memset(bytes, 0, sizeof(bytes));
        const uint8_t poison = (uint8_t)(UINT8_MAX ^ (uint8_t)value);
        for (size_t vertex = 0; vertex < 3; ++vertex)
            memset(bytes + vertex * 32 + 24, poison, 3);
        CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_NORMAL_ARRAY, false,
                                             &display_error));
        CHECK(mc_client_display_source_normal(display, 123, 456, 789, &display_error));
        CHECK(mc_client_display_list_call(display, id, &display_error));
        mc_client_source_fog_state state;
        CHECK(mc_client_display_source_fog_inspect(display, &state) && state.normal_known);
        float submitted[9];
        submitted_list_normals(submitted);
        CHECK(memcmp(submitted, submitted + 6, 3u * sizeof(float)) == 0 &&
              memcmp(submitted + 3, submitted + 6, 3u * sizeof(float)) == 0);
        printf("NATIVE_LIST_NORMAL %d ", value);
        words(submitted + 6, 3);
        putchar('\n');
        const uint8_t second_poison = (uint8_t)((uint8_t)value ^ UINT8_C(0x55));
        for (size_t vertex = 0; vertex < 3; ++vertex)
            memset(bytes + vertex * 32 + 24, second_poison, 3);
        arrays(display, *lease, bytes, true, false, false);
        CHECK(mc_client_display_source_normal(display, -123, -456, -789, &display_error));
        CHECK(mc_client_display_list_call(display, id, &display_error));
        mc_client_source_fog_state repeated;
        CHECK(mc_client_display_source_fog_inspect(display, &repeated) && repeated.normal_known);
        float repeated_submission[9];
        submitted_list_normals(repeated_submission);
        CHECK(memcmp(submitted, repeated_submission, sizeof(submitted)) == 0);
    }
    CHECK(geometry(bytes, false, false, false) == 3);
    for (size_t vertex = 0; vertex < 3; ++vertex)
        memset(bytes + vertex * 32 + 12, 0, 8);
    arrays(display, *lease, bytes, true, true, false);
    CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK,
                                                  GL_AMBIENT_AND_DIFFUSE, &display_error));
    CHECK(mc_client_display_source_color_material_enable(display, true, &display_error));
    CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .625f, &display_error));
    CHECK(mc_client_display_source_normal(display, .25f, -.5f, .75f, &display_error));
    uint8_t prepared[96];
    memcpy(prepared, bytes, sizeof(prepared));
    list_state(display, "before_compile");
    CHECK(mc_client_display_lists_begin(display, id, &display_error));
    CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .625f, &display_error));
    CHECK(mc_client_display_source_push(display, &display_error));
    CHECK(mc_client_display_source_scale(display, 1, 1, .5, &display_error));
    CHECK(
        mc_client_display_source_batch(*lease, bytes, bytes, 96, 3, GL_TRIANGLES, &display_error));
    CHECK(mc_client_display_source_pop(display, &display_error));
    CHECK(mc_client_display_lists_end(display, &display_error));
    CHECK(memcmp(prepared, bytes, sizeof(prepared)) == 0);
    list_state(display, "after_compile");
    memset(bytes, 0, sizeof(bytes));
    CHECK(mc_client_display_source_color(display, .125f, .25f, .5f, .875f, &display_error));
    CHECK(mc_client_display_source_normal(display, -.25f, .75f, .5f, &display_error));
    list_state(display, "before_call");
    CHECK(mc_client_display_list_call(display, id, &display_error));
    list_state(display, "after_call");
    CHECK(mc_client_display_source_color(display, .875f, .625f, .375f, .125f, &display_error));
    CHECK(mc_client_display_source_normal(display, .5f, .25f, -.75f, &display_error));
    list_state(display, "before_repeat");
    CHECK(mc_client_display_list_call(display, id, &display_error));
    list_state(display, "after_repeat");
}

static void array_normal_controls(mc_client_display *display,
                                  mc_client_display_context_lease **lease) {
    setup(display, true);
    CHECK(mc_client_display_context_acquire(display, lease, &display_error));
    /* Observation only: use the bound canonical VBO and its queried attribute
     * layout. These procedures do not prepare or replace any submitted value. */
#define ARRAY_OBSERVATION_PROC(type, name, symbol)                                                 \
    type name = NULL;                                                                              \
    do {                                                                                           \
        SDL_FunctionPointer address = SDL_GL_GetProcAddress(symbol);                               \
        _Static_assert(sizeof(name) == sizeof(address), "GL observation pointer representation");  \
        CHECK(address != NULL);                                                                    \
        memcpy(&name, &address, sizeof(name));                                                     \
    } while (0)
    ARRAY_OBSERVATION_PROC(mc_client_gl_get_integer_fn, get_integer, "glGetIntegerv");
    ARRAY_OBSERVATION_PROC(mc_client_gl_get_error_fn, get_error, "glGetError");
    ARRAY_OBSERVATION_PROC(PFNGLGETVERTEXATTRIBIVPROC, get_attribute, "glGetVertexAttribiv");
    ARRAY_OBSERVATION_PROC(PFNGLGETVERTEXATTRIBPOINTERVPROC, get_pointer,
                           "glGetVertexAttribPointerv");
    ARRAY_OBSERVATION_PROC(PFNGLGETBUFFERPARAMETERIVPROC, get_buffer, "glGetBufferParameteriv");
    ARRAY_OBSERVATION_PROC(PFNGLGETBUFFERSUBDATAPROC, read_buffer, "glGetBufferSubData");
#undef ARRAY_OBSERVATION_PROC
    CHECK(get_error() == GL_NO_ERROR);
    uint8_t bytes[192];
    for (int value = -128; value <= 127; ++value) {
        CHECK(geometry(bytes, false, false, false) == 3);
        constant_bytes(bytes, 3, (int8_t)value, (int8_t)value, (int8_t)value);
        arrays(display, *lease, bytes, true, false, true);
        CHECK(mc_client_display_source_batch(*lease, bytes, bytes, 96, 3, GL_TRIANGLES,
                                             &display_error));
        mc_client_source_fog_state state;
        CHECK(mc_client_display_source_fog_inspect(display, &state) && !state.normal_known);
        GLint binding = 0, capacity = 0;
        get_integer(GL_ARRAY_BUFFER_BINDING, &binding);
        get_buffer(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &capacity);
        CHECK(binding > 0 && capacity > 0 && capacity <= 4096);
        static const GLuint locations[3] = {0, 1, 3};
        static const GLint components[3] = {3, 2, 3};
        GLint strides[3];
        size_t offsets[3];
        for (size_t attribute = 0; attribute < 3; ++attribute) {
            GLint enabled = 0, type = 0, size = 0, normalized = 0, buffer = 0;
            void *pointer = NULL;
            get_attribute(locations[attribute], GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
            get_attribute(locations[attribute], GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
            get_attribute(locations[attribute], GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
            get_attribute(locations[attribute], GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
            get_attribute(locations[attribute], GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
            get_attribute(locations[attribute], GL_VERTEX_ATTRIB_ARRAY_STRIDE, &strides[attribute]);
            get_pointer(locations[attribute], GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
            CHECK(enabled == GL_TRUE && type == GL_FLOAT && size == components[attribute] &&
                  normalized == GL_FALSE && buffer == binding);
            offsets[attribute] = (size_t)(uintptr_t)pointer;
            CHECK(strides[attribute] >= components[attribute] * (GLint)sizeof(float) &&
                  strides[attribute] <= 4096 && offsets[attribute] <= 4096);
            CHECK(offsets[attribute] + 2u * (size_t)strides[attribute] +
                      (size_t)components[attribute] * sizeof(float) <=
                  (size_t)capacity);
        }
        CHECK(get_error() == GL_NO_ERROR);
        uint8_t uploaded[4096];
        memset(uploaded, 0xa5, sizeof(uploaded));
        read_buffer(GL_ARRAY_BUFFER, 0, (GLsizeiptr)capacity, uploaded);
        CHECK(get_error() == GL_NO_ERROR);
        if (value == -128)
            printf("ARRAY_NORMAL_LAYOUT attribute=3 size=3 type=%u stride=%d offset=%zu "
                   "normalized=0 buffer_is_bound=1 vertices=3\n",
                   (unsigned int)GL_FLOAT, strides[2], offsets[2]);
        for (size_t vertex = 0; vertex < 3; ++vertex) {
            CHECK(memcmp(uploaded + offsets[0] + vertex * (size_t)strides[0], bytes + vertex * 32,
                         3u * sizeof(float)) == 0);
            CHECK(memcmp(uploaded + offsets[1] + vertex * (size_t)strides[1],
                         bytes + vertex * 32 + 12, 2u * sizeof(float)) == 0);
        }
        printf("NATIVE_ARRAY_NORMAL %d", value);
        for (size_t vertex = 0; vertex < 3; ++vertex) {
            float normal[3];
            memcpy(normal, uploaded + offsets[2] + vertex * (size_t)strides[2], sizeof(normal));
            putchar(' ');
            words(normal, 3);
        }
        printf(" %u\n", state.normal_known ? 1u : 0u);
    }
}

static void color_input_controls(mc_client_display *display,
                                 mc_client_display_context_lease **lease,
                                 mc_client_display_list_range **range) {
    setup(display, true);
    CHECK(mc_client_display_source_color_material_enable(display, true, &display_error));
    CHECK(mc_client_display_source_normal(display, .25f, -.5f, .75f, &display_error));
    CHECK(mc_client_display_context_acquire(display, lease, &display_error));
    int32_t id = 0;
    CHECK(mc_client_display_lists_generate(display, 1, 8, range, &id, &display_error));
    uint8_t bytes[192];
    for (int value = 0; value < 256; ++value) {
        CHECK(geometry(bytes, false, false, false) == 3);
        for (size_t vertex = 0; vertex < 3; ++vertex) {
            memset(bytes + vertex * 32 + 12, 0, 20);
            memset(bytes + vertex * 32 + 20, value, 4);
        }
        arrays(display, *lease, bytes, false, true, false);
        CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .625f, &display_error));
        CHECK(mc_client_display_source_batch(*lease, bytes, bytes, 96, 3, GL_TRIANGLES,
                                             &display_error));
        float direct[12];
        submitted_attribute(2, 4, direct);
        printf("NATIVE_COLOR_INPUT %d direct ", value);
        words(direct, 12);
        putchar('\n');
        /* Current color after a direct array draw is not an input oracle. */
        CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .625f, &display_error));
        uint8_t prepared[96];
        memcpy(prepared, bytes, sizeof(prepared));
        CHECK(mc_client_display_lists_begin(display, id, &display_error));
        CHECK(mc_client_display_source_color(display, .25f, .5f, .75f, .375f, &display_error));
        CHECK(mc_client_display_source_batch(*lease, bytes, bytes, 96, 3, GL_TRIANGLES,
                                             &display_error));
        CHECK(mc_client_display_lists_end(display, &display_error));
        CHECK(memcmp(prepared, bytes, sizeof(prepared)) == 0);
        for (size_t vertex = 0; vertex < 3; ++vertex)
            memset(bytes + vertex * 32 + 20, value ^ 255, 4);
        CHECK(mc_client_display_source_array(display, MC_CLIENT_SOURCE_COLOR_ARRAY, false,
                                             &display_error));
        CHECK(mc_client_display_source_color(display, .125f, .25f, .5f, .875f, &display_error));
        CHECK(mc_client_display_list_call(display, id, &display_error));
        float compiled[12];
        submitted_attribute(2, 4, compiled);
        printf("NATIVE_COLOR_INPUT %d list ", value);
        words(compiled, 12);
        putchar('\n');
        for (size_t vertex = 0; vertex < 3; ++vertex)
            memset(bytes + vertex * 32 + 20, value ^ 0x55, 4);
        arrays(display, *lease, bytes, false, true, false);
        CHECK(mc_client_display_source_color(display, .875f, .625f, .375f, .125f, &display_error));
        CHECK(mc_client_display_list_call(display, id, &display_error));
        float repeated[12];
        submitted_attribute(2, 4, repeated);
        CHECK(memcmp(compiled, repeated, sizeof(compiled)) == 0);
    }
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    const unsigned int original_controls = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    const bool materials = strcmp(argv[1], "--material") == 0;
    const bool lists = strcmp(argv[1], "--list-state") == 0;
    const bool array_normals = strcmp(argv[1], "--array-normals") == 0;
    const bool color_inputs = strcmp(argv[1], "--color-inputs") == 0;
    const bool material_sequence = strcmp(argv[1], "--material-sequence") == 0;
    static const char prefix_option[] = "--material-prefix=";
    const size_t prefix_length = sizeof(prefix_option) - 1u;
    const bool material_prefix = strncmp(argv[1], prefix_option, prefix_length) == 0;
    CHECK(!material_prefix || (strlen(argv[1]) == prefix_length + 1u &&
                               argv[1][prefix_length] >= '0' && argv[1][prefix_length] <= '8'));
    const int prefix = material_prefix ? argv[1][prefix_length] - '0' : -1;
    const int index = materials || lists || array_normals || color_inputs || material_prefix ? -1
                      : material_sequence                                                    ? 32
                                          : atoi(argv[1]);
    CHECK(materials || lists || array_normals || color_inputs || material_prefix ||
          (index >= 0 && (size_t)index < sizeof(names) / sizeof(names[0])));
    mc_client_display *display = create();
    mc_client_display_context_lease *lease = NULL;
    mc_client_display_list_range *range = NULL;
    mc_client_texture_resource *texture_resource = NULL;
    mc_client_render_engine_classes *classes = NULL;
    if (materials) {
        material_controls(display);
    } else if (material_prefix) {
        material_prefix_controls(display, prefix);
    } else if (lists) {
        list_controls(display, &lease, &range);
    } else if (array_normals) {
        array_normal_controls(display, &lease);
    } else if (color_inputs) {
        color_input_controls(display, &lease, &range);
    } else {
        const char *name = names[index];
        if (index >= 56) {
            actual_quad(display, name, index == 56, &classes);
        } else if (index >= 38) {
            extended_scene(display, index - 38, &lease, &texture_resource);
        } else {
            setup(display, index <= 20 || (index >= 25 && index <= 27));
            if (index >= 36) {
                converted(display, index == 36, &classes);
                pixels(display, name, 0);
            } else {
                const bool fan = index == 23 || index == 24 || index == 34 || index == 35;
                const bool controls = index >= 13 && index <= 20;
                uint8_t bytes[192];
                const int count = geometry(bytes, fan, controls, index == 35);
                const bool normal_array = index < 10 || (index >= 17 && index <= 24) || index >= 33;
                const bool color_array = (index >= 21 && index <= 24) || index == 33;
                mc_client_texture texture_id = 0;
                if (index < 8) {
                    static const int8_t values[8] = {-128, -127, -1, 0, 1, 64, 126, 127};
                    constant_bytes(bytes, count, 0, 0, values[index]);
                    if (values[index] < 0)
                        light(display, 0, GL_POSITION, 0, 0, -1, 0);
                } else if (index == 8) {
                    constant_bytes(bytes, count, 0, 0, 0);
                    light(display, 0, GL_POSITION, 1, 0, 0, 0);
                } else if (index == 9) {
                    constant_bytes(bytes, count, 127, -127, 0);
                    CHECK(mc_client_display_source_light_enable(display, 1, true, &display_error));
                    light(display, 0, GL_POSITION, 1, 0, 0, 0);
                    light(display, 0, GL_DIFFUSE, 1, 0, 0, 1);
                    light(display, 1, GL_POSITION, 0, -1, 0, 0);
                    light(display, 1, GL_DIFFUSE, 0, 1, 0, 1);
                } else if (index <= 12) {
                    static const float values[3] = {0, .5f, 1};
                    CHECK(mc_client_display_source_normal(display, 0, 0, values[index - 10],
                                                          &display_error));
                } else if (controls) {
                    CHECK(mc_client_display_source_scale(display, 2, 3, 4, &display_error));
                    normal_controls(display, (unsigned int)(index - (index < 17 ? 13 : 17)));
                    CHECK(mc_client_display_source_normal(display, 0, 0, .5f, &display_error));
                    if (normal_array)
                        constant_bytes(bytes, count, 16, 64, 64);
                } else if (index <= 24) {
                    CHECK(mc_client_display_source_color_material_enable(display, true,
                                                                         &display_error));
                    CHECK(mc_client_display_source_shade_model(
                        display, index % 2 == 1 ? GL_FLAT : GL_SMOOTH, &display_error));
                } else if (index == 25 || index == 26) {
                    CHECK(mc_client_display_source_normal(display, 1, 0, 0, &display_error));
                    if (index == 25)
                        light(display, 0, GL_POSITION, 1, 0, 0, 0);
                    CHECK(mc_client_display_source_rotate(display, 90, 0, 0, 1, &display_error));
                    if (index == 26)
                        light(display, 0, GL_POSITION, 1, 0, 0, 0);
                } else if (index == 27) {
                    light(display, 0, GL_POSITION, 1, 0, 0, 0);
                    CHECK(mc_client_display_source_normal(display, 1, 0, 0, &display_error));
                    CHECK(mc_client_display_source_scale(display, -1, 1, 1, &display_error));
                } else if (index >= 28 && index <= 31) {
                    static const int32_t modes[4] = {GL_AMBIENT, GL_DIFFUSE, GL_EMISSION,
                                                     GL_SPECULAR};
                    CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .625f,
                                                         &display_error));
                    CHECK(mc_client_display_source_color_material(
                        display, GL_FRONT_AND_BACK, modes[index - 28], &display_error));
                    CHECK(mc_client_display_source_color_material_enable(display, true,
                                                                         &display_error));
                    CHECK(mc_client_display_source_normal(display, 0, 0, 1, &display_error));
                } else if (index == 32) {
                    CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK,
                                                                  GL_AMBIENT, &display_error));
                    CHECK(mc_client_display_source_color_material_enable(display, true,
                                                                         &display_error));
                    CHECK(mc_client_display_source_color(display, .25f, .5f, .75f, .5f,
                                                         &display_error));
                    CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK,
                                                                  GL_DIFFUSE, &display_error));
                    CHECK(mc_client_display_source_color(display, .75f, .25f, .5f, .75f,
                                                         &display_error));
                    CHECK(mc_client_display_source_color_material(display, GL_FRONT_AND_BACK,
                                                                  GL_EMISSION, &display_error));
                    CHECK(mc_client_display_source_color(display, .125f, .25f, .125f, 1,
                                                         &display_error));
                    CHECK(mc_client_display_source_color_material_enable(display, false,
                                                                         &display_error));
                    CHECK(mc_client_display_source_color(display, .875f, .5f, .25f, .125f,
                                                         &display_error));
                } else if (index >= 33) {
                    CHECK(mc_client_display_source_color_material_enable(display, true,
                                                                         &display_error));
                    if (index >= 34) {
                        CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .625f,
                                                             &display_error));
                        texture_id = texture(display, &texture_resource);
                    }
                    if (index == 35)
                        CHECK(mc_client_display_source_fog_enable(display, true, &display_error));
                }
                CHECK(mc_client_display_context_acquire(display, &lease, &display_error));
                arrays(display, lease, bytes, normal_array, color_array, texture_id != 0);
                if (index == 33) {
                    int32_t id = 0;
                    CHECK(mc_client_display_lists_generate(display, 1, 8, &range, &id,
                                                           &display_error));
                    CHECK(mc_client_display_lists_begin(display, id, &display_error));
                    CHECK(mc_client_display_source_color(display, .75f, .5f, .25f, .625f,
                                                         &display_error));
                    CHECK(mc_client_display_source_push(display, &display_error));
                    CHECK(mc_client_display_source_scale(display, 1, 1, .5, &display_error));
                    CHECK(mc_client_display_source_batch(lease, bytes, bytes, (size_t)count * 32,
                                                         count, GL_TRIANGLES, &display_error));
                    CHECK(mc_client_display_source_pop(display, &display_error));
                    CHECK(mc_client_display_lists_end(display, &display_error));
                    memset(bytes, 0, sizeof(bytes));
                    CHECK(mc_client_display_list_call(display, id, &display_error));
                    pixels(display, name, 0);
                    CHECK(mc_client_display_source_clear(
                        display, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, &display_error));
                    CHECK(mc_client_display_source_load_identity(display, &display_error));
                    CHECK(mc_client_display_source_rotate(display, 90, 0, 0, 1, &display_error));
                    CHECK(mc_client_display_source_scale(display, .5, .5, 2, &display_error));
                    CHECK(mc_client_display_list_call(display, id, &display_error));
                    pixels(display, name, 1);
                } else {
                    CHECK(mc_client_display_source_batch(
                        lease, bytes, bytes, (size_t)count * 32, count,
                        fan ? GL_TRIANGLE_FAN : GL_TRIANGLES, &display_error));
                    pixels(display, name, 0);
                }
            }
        }
    }
    if (material_sequence)
        material_state(display, "after_contiguous_draw");
    CHECK(mc_client_display_lists_retire(&range, &display_error));
    CHECK(mc_client_display_context_release(&lease, &display_error));
    CHECK(mc_client_display_retire_texture(&texture_resource, &display_error));
    if (classes != NULL)
        CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_destroy(display, &display_error));
    _mm_setcsr(original_controls);
    if (materials)
        printf("Source lighting material: 16 rows; %u checks\n", checks);
    else if (material_prefix)
        printf("Source lighting material prefix: %d; %u checks\n", prefix, checks);
    else if (material_sequence)
        printf("Source lighting material sequence: 3 rows; %u checks\n", checks);
    else if (lists)
        printf("Source lighting list state: 262 rows; %u checks\n", checks);
    else if (array_normals)
        printf("Source lighting array normals: 256 rows; %u checks\n", checks);
    else if (color_inputs)
        printf("Source lighting color inputs: 512 rows; %u checks\n", checks);
    else
        printf("Source lighting raster: %s; %u checks\n", names[index], checks);
    return 0;
}
