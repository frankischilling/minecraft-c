#include "lookup_observation.h"

static bool helpers(observation *o) {
    mc_client_argb_image *image = NULL;
    OBS_NEED(mc_client_argb_image_create(3, 2, 4096, &image) == MC_CLIENT_RENDER_ENGINE_OK);
    for (int32_t y = 0; y < 2; ++y)
        for (int32_t x = 0; x < 3; ++x) {
            uint32_t bits = ((x + y) % 2 == 0 ? 0u : UINT32_C(0xff000000)) |
                            ((uint32_t)(x * 31 + 1) << 16) | ((uint32_t)(y * 67 + 1) << 8) |
                            (uint32_t)(x + y + 1);
            OBS_NEED(mc_client_argb_image_set_pixel(image, x, y, bits) ==
                     MC_CLIENT_RENDER_ENGINE_OK);
        }
    const int32_t sizes[] = {-1, 6, 8, 0, 1, 5, -2, -3, -4};
    const char *labels[] = {"new",    "fill-6",    "fill-8",         "fill-0",         "fill-1",
                            "fill-5", "fill-null", "null-image-new", "null-image-fill"};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        mc_client_color_array *destination = NULL, *result = NULL;
        mc_client_color_array_error error = {0};
        if (sizes[i] >= 0 || sizes[i] == -4) {
            const int32_t length = sizes[i] == -4 ? 1 : sizes[i];
            OBS_NEED(mc_client_color_array_create(
                         length, NULL, (mc_client_graphics_site){NULL, NULL, NULL, -1},
                         &destination, &error) == MC_CLIENT_COLOR_ARRAY_OK);
            if (sizes[i] >= 0)
                for (int32_t at = 0; at < length; ++at)
                    OBS_NEED(mc_client_color_array_write(destination, (size_t)at,
                                                         UINT32_C(0x11223344)) ==
                             MC_CLIENT_COLOR_ARRAY_OK);
        }
        const bool fill = sizes[i] != -1 && sizes[i] != -3;
        const mc_client_color_array_status status =
            fill ? mc_client_color_array_fill(sizes[i] < -2 ? NULL : image, destination, &error)
                 : mc_client_color_array_from_image(sizes[i] == -3 ? NULL : image, NULL, &result,
                                                    &error);
        if (fill && status == MC_CLIENT_COLOR_ARRAY_OK)
            result = destination;
        obs_event(o, labels[i]);
        fputs(",\"outcome\":", stdout);
        obs_string(status == MC_CLIENT_COLOR_ARRAY_OK               ? "return"
                   : status == MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE ? "throw"
                                                                    : "frontier");
        fputs(",\"exception\":", stdout);
        obs_string(status == MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE ? "java.lang.NullPointerException"
                                                                  : NULL);
        fputs(",\"array\":", stdout);
        obs_array(o, result);
        fputs(",\"destination\":", stdout);
        obs_array(o, destination);
        if (sizes[i] == -1 && result != NULL) {
            mc_client_color_array_state state;
            OBS_NEED(mc_client_color_array_inspect(result, &state) && state.length != 0);
            const uint32_t saved = state.bits[0], pixel = image->pixels[0];
            OBS_NEED(mc_client_color_array_write(result, 0, saved ^ UINT32_C(0x00123456)) ==
                     MC_CLIENT_COLOR_ARRAY_OK);
            printf(",\"image_plane_independent\":%s", image->pixels[0] == pixel ? "true" : "false");
            OBS_NEED(mc_client_color_array_write(result, 0, saved) == MC_CLIENT_COLOR_ARRAY_OK);
        }
        printf(",\"native\":{\"status\":%d,\"site\":", (int)status);
        obs_site(error.site);
        fputs("}}", stdout);
        if (!fill && result != NULL)
            OBS_NEED(mc_client_color_array_release(&result, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
        if (destination != NULL)
            OBS_NEED(mc_client_color_array_release(&destination, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    }
    mc_client_argb_image_release(image);
    return true;
}

static bool statics(observation *o, bool first_setter) {
    uint16_t units[128];
    mc_client_color_array *array = NULL;
    OBS_NEED(mc_client_render_engine_colors(o->engine, NULL, obs_key("/misc/watercolor.png", units),
                                            &array, o->error) == MC_CLIENT_LOOKUP_OK);
    const char *names[] = {"hw", "ia", "jh"};
    for (int i = 0; i < MC_CLIENT_COLORIZER_COUNT; ++i) {
        mc_client_colorizer_error error = {0};
        mc_client_colorizer_state state;
        char label[32];
        OBS_NEED(mc_client_colorizer_inspect(o->classes, (mc_client_colorizer_kind)i, &state));
        if (state.phase != MC_CLIENT_COLORIZER_RAW) {
            fprintf(stderr, "actual %s phase%d before observed first activation\n", names[i],
                    (int)state.phase);
            OBS_NEED(false); /* Never reset a real warm owner to manufacture cold state. */
        }
        if (!first_setter) {
            OBS_NEED(mc_client_colorizer_initialize(o->classes, (mc_client_colorizer_kind)i, NULL,
                                                    &error) == MC_CLIENT_COLORIZER_OK);
            OBS_NEED(mc_client_colorizer_inspect(o->classes, (mc_client_colorizer_kind)i, &state));
            snprintf(label, sizeof(label), "%s-initial", names[i]);
            obs_event(o, label);
            fputs(",\"outcome\":\"return\",\"array\":", stdout);
            obs_array(o, (mc_client_color_array *)state.a);
            putchar('}');
        }
        OBS_NEED(mc_client_colorizer_set(o->classes, (mc_client_colorizer_kind)i, array, NULL,
                                         &error) == MC_CLIENT_COLORIZER_OK);
        OBS_NEED(mc_client_colorizer_inspect(o->classes, (mc_client_colorizer_kind)i, &state));
        snprintf(label, sizeof(label), "%s-set", names[i]);
        obs_event(o, label);
        fputs(",\"outcome\":\"return\",\"array\":", stdout);
        obs_array(o, (mc_client_color_array *)state.a);
        printf(",\"same\":%s}", state.a == array ? "true" : "false");
    }
    mc_client_color_array_state values;
    OBS_NEED(mc_client_color_array_inspect(array, &values) && values.length != 0);
    OBS_NEED(mc_client_color_array_write(array, 0, values.bits[0] ^ UINT32_C(0x00123456)) ==
             MC_CLIENT_COLOR_ARRAY_OK);
    for (int i = 0; i < MC_CLIENT_COLORIZER_COUNT; ++i) {
        mc_client_colorizer_state state;
        char label[32];
        OBS_NEED(mc_client_colorizer_inspect(o->classes, (mc_client_colorizer_kind)i, &state));
        snprintf(label, sizeof(label), "%s-mutated", names[i]);
        obs_event(o, label);
        fputs(",\"outcome\":\"return\",\"array\":", stdout);
        obs_array(o, (mc_client_color_array *)state.a);
        putchar('}');
    }
    OBS_NEED(mc_client_color_array_release(&array, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    return obs_color(o, "cache-mutated", "/misc/watercolor.png");
}

static bool calls(observation *o, const char *profile) {
    if (strcmp(profile, "helpers") == 0)
        return helpers(o);
    if (strcmp(profile, "statics") == 0 || strcmp(profile, "first-setter") == 0)
        return statics(o, strcmp(profile, "first-setter") == 0);
    if (strcmp(profile, "default") == 0 || strcmp(profile, "zip") == 0) {
        OBS_NEED(strcmp(o->pack_class, strcmp(profile, "zip") == 0 ? "vp" : "pa") == 0);
        const char *labels[] = {"water", "cache", "grass", "foliage", "missing", "clamp", "blur"};
        const char *keys[] = {"/misc/watercolor.png",      "/misc/watercolor.png",
                              "/misc/grasscolor.png",      "/misc/foliagecolor.png",
                              "/lookup-absent.png",        "%clamp%/misc/watercolor.png",
                              "%blur%/misc/watercolor.png"};
        for (size_t i = 0; i < 7; ++i)
            OBS_NEED(obs_color(o, labels[i], keys[i]));
    } else if (strcmp(profile, "frontiers") == 0) {
        const char *labels[] = {"nonimage", "strip", "corrupt", "null-key"};
        const char *keys[] = {"/font.txt", "##/misc/watercolor.png", "%blur%/lookup-truncated.png",
                              NULL};
        for (size_t i = 0; i < 4; ++i)
            OBS_NEED(obs_color(o, labels[i], keys[i]));
    } else if (strcmp(profile, "priority") == 0) {
        OBS_NEED(obs_color(o, "water", "/misc/watercolor.png"));
        o->engine->fields.l = NULL;
        OBS_NEED(obs_color(o, "null-list-cache", "/misc/watercolor.png"));
        o->engine->fields.l = o->packs;
        mc_client_texture_pack *saved = o->packs->selected;
        o->packs->selected = NULL;
        const bool ok = obs_color(o, "null-pack-cache", "/misc/watercolor.png") &&
                        obs_color(o, "null-pack-miss", "/different-absent.png");
        o->packs->selected = saved;
        OBS_NEED(ok);
    } else
        OBS_NEED(false);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    observation *o = calloc(1, sizeof(*o));
    if (o == NULL)
        return 1;
    bool ok = obs_open(o, argv[1], argv[2], true);
    if (ok) {
        obs_begin(o, "color", argv[3], NULL);
        ok = calls(o, argv[3]);
        fputs("]}\n", stdout);
    }
    const bool output_ok = fflush(stdout) == 0 && !ferror(stdout);
    const bool cleanup = obs_close(o);
    free(o);
    return ok && output_ok && cleanup ? 0 : 1;
}
