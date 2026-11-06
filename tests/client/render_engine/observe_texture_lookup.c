#include "lookup_observation.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

typedef void(GLAPIENTRY *obs_integer_fn)(GLenum, GLint *);
typedef void(GLAPIENTRY *obs_bind_fn)(GLenum, GLuint);
typedef void(GLAPIENTRY *obs_store_fn)(GLenum, GLint);
typedef void(GLAPIENTRY *obs_level_fn)(GLenum, GLint, GLenum, GLint *);
typedef void(GLAPIENTRY *obs_image_fn)(GLenum, GLint, GLenum, GLenum, GLvoid *);
typedef GLboolean(GLAPIENTRY *obs_is_fn)(GLuint);
typedef GLenum(GLAPIENTRY *obs_error_fn)(void);
typedef const GLubyte *(GLAPIENTRY *obs_string_fn)(GLenum);

typedef struct queries {
    obs_integer_fn integer;
    obs_bind_fn bind, bind_buffer;
    obs_store_fn store;
    obs_level_fn level;
    obs_image_fn image;
    obs_is_fn is_texture;
    obs_error_fn error;
    obs_string_fn string;
} queries;

static bool query_load(queries *q) {
    q->integer = (obs_integer_fn)SDL_GL_GetProcAddress("glGetIntegerv");
    q->bind = (obs_bind_fn)SDL_GL_GetProcAddress("glBindTexture");
    q->bind_buffer = (obs_bind_fn)SDL_GL_GetProcAddress("glBindBuffer");
    q->store = (obs_store_fn)SDL_GL_GetProcAddress("glPixelStorei");
    q->level = (obs_level_fn)SDL_GL_GetProcAddress("glGetTexLevelParameteriv");
    q->image = (obs_image_fn)SDL_GL_GetProcAddress("glGetTexImage");
    q->is_texture = (obs_is_fn)SDL_GL_GetProcAddress("glIsTexture");
    q->error = (obs_error_fn)SDL_GL_GetProcAddress("glGetError");
    q->string = (obs_string_fn)SDL_GL_GetProcAddress("glGetString");
    OBS_NEED(q->integer && q->bind && q->bind_buffer && q->store && q->level && q->image &&
             q->is_texture && q->error && q->string);
    return true;
}

static bool device_json(queries *q, char *buffer, size_t capacity) {
    const GLenum names[] = {GL_VENDOR, GL_RENDERER, GL_VERSION};
    const char *keys[] = {"vendor", "renderer", "version"};
    size_t at = 0;
    buffer[at++] = '{';
    for (size_t i = 0; i < 3; ++i) {
        const unsigned char *value = q->string(names[i]);
        OBS_NEED(value != NULL);
        if (i != 0)
            buffer[at++] = ',';
        const int count = snprintf(buffer + at, capacity - at, "\"%s\":\"", keys[i]);
        OBS_NEED(count > 0 && (size_t)count < capacity - at);
        at += (size_t)count;
        size_t length = 0;
        while (*value != 0) {
            OBS_NEED(++length <= 4096 && at + 7 < capacity);
            if (*value < 32 || *value > 126) {
                const int n = snprintf(buffer + at, capacity - at, "\\u%04x", (unsigned)*value);
                OBS_NEED(n == 6);
                at += 6;
            } else {
                if (*value == '"' || *value == '\\')
                    buffer[at++] = '\\';
                buffer[at++] = (char)*value;
            }
            ++value;
        }
        buffer[at++] = '"';
    }
    OBS_NEED(at + 2 < capacity);
    buffer[at++] = '}';
    buffer[at] = 0;
    return true;
}

static bool texture(observation *o, queries *q, const char *label, const char *name) {
    uint16_t units[128];
    mc_client_texture id = UINT32_C(0xdeadbeef);
    const mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        o->engine, o->display, NULL, obs_key(name, units), &id, o->error);
    obs_event(o, label);
    fputs(",\"outcome\":", stdout);
    obs_string(status == MC_CLIENT_LOOKUP_OK               ? "return"
               : status == MC_CLIENT_LOOKUP_NULL_REFERENCE ? "throw"
                                                           : "frontier");
    fputs(",\"exception\":", stdout);
    obs_string(status == MC_CLIENT_LOOKUP_NULL_REFERENCE ? "java.lang.NullPointerException" : NULL);
    fputs(",\"id\":", stdout);
    if (status == MC_CLIENT_LOOKUP_OK)
        printf("%" PRIu32, id);
    else
        fputs("null", stdout);
    fputs(",\"state\":", stdout);
    obs_state(o);
    GLint before = 0;
    q->integer(GL_TEXTURE_BINDING_2D, &before);
    uint8_t *pixels = NULL;
    GLint width = 0, height = 0;
    if (status == MC_CLIENT_LOOKUP_OK && q->is_texture(id)) {
        const GLenum names[] = {GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_PIXELS,
                                GL_PACK_SKIP_ROWS};
        GLint saved[4], pbo = 0;
        for (size_t i = 0; i < 4; ++i)
            q->integer(names[i], &saved[i]);
        q->integer(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
        q->bind(GL_TEXTURE_2D, id);
        q->bind_buffer(GL_PIXEL_PACK_BUFFER, 0);
        for (size_t i = 0; i < 4; ++i)
            q->store(names[i], i == 0 ? 1 : 0);
        q->level(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        q->level(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        bool valid = width > 0 && height > 0 &&
                     (uint64_t)(uint32_t)width * (uint32_t)height * 4u <= 1048576u;
        if (valid)
            pixels = malloc((size_t)(uint32_t)width * (uint32_t)height * 4u);
        if (pixels != NULL)
            q->image(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        for (size_t i = 0; i < 4; ++i)
            q->store(names[i], saved[i]);
        q->bind_buffer(GL_PIXEL_PACK_BUFFER, (GLuint)pbo);
        q->bind(GL_TEXTURE_2D, (GLuint)before);
        bool restored = true;
        for (size_t i = 0; i < 4; ++i) {
            GLint current = 0;
            q->integer(names[i], &current);
            if (current != saved[i])
                restored = false;
        }
        GLint current_pbo = 0;
        q->integer(GL_PIXEL_PACK_BUFFER_BINDING, &current_pbo);
        if (current_pbo != pbo)
            restored = false;
        if (!valid || pixels == NULL || !restored) {
            free(pixels);
            OBS_NEED(false);
        }
    }
    GLint after = 0;
    q->integer(GL_TEXTURE_BINDING_2D, &after);
    if (before != after) {
        free(pixels);
        OBS_NEED(false);
    }
    printf(",\"bound_before\":%d,\"bound_after\":%d,\"width\":", before, after);
    if (pixels != NULL)
        printf("%d,\"height\":%d,\"gpu\":", width, height);
    else
        fputs("null,\"height\":null,\"gpu\":", stdout);
    if (pixels != NULL)
        obs_hex(pixels, (size_t)(uint32_t)width * (uint32_t)height * 4u);
    else
        fputs("null", stdout);
    free(pixels);
    fputs(",\"transfer\":", stdout);
    if (status == MC_CLIENT_LOOKUP_OK)
        obs_hex(o->engine->fields.f->data, o->engine->fields.f->limit);
    else
        fputs("null", stdout);
    printf(",\"gl_error\":%u,\"native\":", (unsigned)q->error());
    obs_error(o->error);
    putchar('}');
    OBS_NEED(mc_client_render_engine_retire_lookup(o->engine, o->error) == MC_CLIENT_LOOKUP_OK);
    return true;
}

static bool calls(observation *o, queries *q, const char *profile) {
    if (strcmp(profile, "default") == 0 || strcmp(profile, "zip") == 0) {
        OBS_NEED(strcmp(o->pack_class, strcmp(profile, "zip") == 0 ? "vp" : "pa") == 0);
        OBS_NEED(texture(o, q, "ordinary", "/gui/background.png"));
        OBS_NEED(texture(o, q, "cache", "/gui/background.png"));
        OBS_NEED(texture(o, q, "missing", "/lookup-absent.png"));
    } else if (strcmp(profile, "frontiers") == 0) {
        OBS_NEED(texture(o, q, "strip", "##/gui/background.png"));
        OBS_NEED(texture(o, q, "clamp", "%clamp%/gui/background.png"));
        OBS_NEED(texture(o, q, "corrupt", "%blur%/lookup-truncated.png"));
        OBS_NEED(texture(o, q, "null-key", NULL));
    } else if (strcmp(profile, "priority") == 0) {
        OBS_NEED(texture(o, q, "ordinary", "/gui/background.png"));
        o->engine->fields.l = NULL;
        OBS_NEED(texture(o, q, "null-list-cache", "/gui/background.png"));
        o->engine->fields.l = o->packs;
        mc_client_texture_pack *saved = o->packs->selected;
        o->packs->selected = NULL;
        const bool ok = texture(o, q, "null-pack-cache", "/gui/background.png") &&
                        texture(o, q, "null-pack-miss", "/different-absent.png");
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
    char *device = malloc(90000);
    queries q;
    if (o == NULL || device == NULL) {
        free(o);
        free(device);
        return 1;
    }
    bool ok = obs_open(o, argv[1], argv[2], false);
    if (ok)
        ok = mc_client_display_create("Actual String lookup observation", 64, 64, true, &o->display,
                                      NULL);
    if (ok)
        ok = query_load(&q) && device_json(&q, device, 90000);
    if (ok) {
        obs_begin(o, "texture", argv[3], device);
        ok = calls(o, &q, argv[3]);
        fputs("]}\n", stdout);
    }
    const bool output_ok = fflush(stdout) == 0 && !ferror(stdout);
    const bool cleanup = obs_close(o);
    free(device);
    free(o);
    return ok && output_ok && cleanup ? 0 : 1;
}
