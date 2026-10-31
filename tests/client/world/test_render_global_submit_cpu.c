#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
/* Actual owner factories; earlier picking main and World cases are not run. */
#define MC_PICKING_BODY_MAIN_NAME mc_submit_cpu_picking_fixture_main
int mc_submit_cpu_picking_fixture_main(int argc, char **argv);
#include "test_picking.c"
#undef main

#include "client/render_engine/internal.h"
#include "client/render_engine/lighting.h"
#include "client/world/source_chunk_renderer_internal.h"
#include "mc/client/world/render_global_submit.h"

enum { CAPACITY = 16 };

static bool list_new(mc_client_render_global *n, mc_client_render_list **out,
                     mc_client_render_global_error *error) {
    *out = mc_render_global_allocate(n, sizeof(**out), (mc_client_graphics_site){0}, error);
    return *out != NULL;
}

static int identity(mc_client_source_dk *const *actors, size_t count, const void *value) {
    if (value == NULL)
        return 0;
    for (size_t i = 0; i < count; ++i)
        if (value == actors[i])
            return (int)i + 1;
    return -1;
}

static bool run(picking_fixture *f, const char *path) {
    FILE *input = fopen(path, "r");
    CHECK(input != NULL);
    unsigned int seen = 0;
    for (;;) {
        unsigned int index;
        char name[64];
        int32_t cursor, distance, old_distance, pass;
        int app_present, settings_present, count, length, queued;
        const int parsed =
            fscanf(input, "%u%63s%" SCNd32 "%d%d%" SCNd32 "%" SCNd32 "%" SCNd32 "%d%d%d", &index,
                   name, &cursor, &app_present, &settings_present, &distance, &old_distance, &pass,
                   &count, &length, &queued);
        if (parsed == EOF)
            break;
        CHECK(parsed == 11 && index == seen++ && count >= 0 && count <= CAPACITY && length >= -1 &&
              length <= CAPACITY && queued >= -1 && queued <= CAPACITY);
        mc_client_render_engine_class_state classes_before, classes_after;
        mc_math_class_state math_before, math_after;
        CHECK(mc_client_render_engine_classes_inspect(f->graphics, &classes_before));
        CHECK(mc_math_classes_state(f->math, &math_before, NULL) == MC_MATH_CLASS_OK);
        mc_client_render_global *n = NULL;
        mc_client_render_global_error error = {0};
        CHECK(mc_client_render_global_allocate(f->graphics, NULL, &n, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
        mc_client_source_dk *actors[CAPACITY] = {0};
        for (int i = 0; i < count; ++i) {
            int dirty;
            CHECK(fscanf(input, "%d", &dirty) == 1);
            actors[i] = mc_source_dk_allocate(n, &error);
            CHECK(actors[i] != NULL && actors[i]->fields.p == NULL);
            actors[i]->fields.u = dirty != 0;
        }
        mc_client_source_dk **array = NULL;
        if (length >= 0) {
            array =
                mc_render_global_allocate(n, (size_t)(length == 0 ? 1 : length) * sizeof(*array),
                                          (mc_client_graphics_site){0}, &error);
            CHECK(array != NULL);
        }
        for (int i = 0; i < length; ++i) {
            int id;
            CHECK(fscanf(input, "%d", &id) == 1 && id >= 0 && id <= count);
            array[i] = id == 0 ? NULL : actors[id - 1];
        }
        mc_client_render_list *queue = NULL;
        if (queued >= 0)
            CHECK(list_new(n, &queue, &error));
        for (int i = 0; i < queued; ++i) {
            int id;
            CHECK(fscanf(input, "%d", &id) == 1 && id >= 0 && id <= count);
            CHECK(mc_render_global_pointer_append(n, queue, id == 0 ? NULL : actors[id - 1],
                                                  (mc_client_graphics_site){0}, &error));
        }
        mc_game_settings *settings = NULL;
        if (settings_present) {
            CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
            mc_game_settings_state fields;
            CHECK(mc_game_settings_read_state(settings, &fields));
            fields.values.render_distance = distance;
            fields.values.fancy_graphics = true;
            CHECK(mc_game_settings_set_values(settings, &fields.values, NULL) ==
                  MC_GAME_SETTINGS_OK);
        }
        f->app->settings = settings;
        n->fields.t = app_present ? f->app : NULL;
        n->fields.o = array;
        n->o_length = length < 0 ? 0 : (size_t)length;
        n->fields.m = queue;
        n->fields.R = cursor;
        n->fields.H = old_distance;
        n->fields.M = 11;
        n->fields.N = 22;
        n->fields.O = 33;
        n->fields.P = 44;
        n->fields.Q = 55;
        mc_client_render_global_submit_call *call = NULL;
        const mc_client_render_global_status status =
            mc_client_render_global_submit_begin(n, NULL, pass, .5, f->math, &call, &error);
        CHECK(call != NULL && status != MC_CLIENT_RENDER_GLOBAL_OK && !error.native_policy &&
              n->submit_call == call && !n->active && n->holds == 2);
        mc_client_render_global_submit_state state;
        CHECK(mc_client_render_global_submit_inspect(call, &state) && !state.active &&
              !state.returned && !state.retiring && state.failure != NULL &&
              state.cleanup != NULL && state.failure != &error && state.failure->status == status &&
              !state.failure->source_returned && state.owner == n && state.camera == NULL &&
              state.batch_camera == NULL && state.math == f->math && state.frames != NULL &&
              state.frame_count >= 1 &&
              state.failure->source_pc == (unsigned int)state.frames[0].pc &&
              strcmp(state.failure->source_class, "n") == 0 &&
              strcmp(state.failure->source_method, "a") == 0 &&
              strcmp(state.failure->source_descriptor, "(Lls;ID)I") == 0 && n->fields.m == queue &&
              n->fields.o == array);
        const char *exception = status == MC_CLIENT_RENDER_GLOBAL_SOURCE_ARITHMETIC ? "ARITH"
                                : status == MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX    ? "AIOOBE"
                                                                                    : "NPE";
        CHECK(status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE ||
              status == MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX ||
              status == MC_CLIENT_RENDER_GLOBAL_SOURCE_ARITHMETIC);
        if (strcmp(name, "rebuild_old_dk_null_passes") == 0)
            CHECK(state.rebuild != NULL && state.rebuild->rebuild_pc == 54 &&
                  !state.rebuild->rebuild_returned && state.frame_count == 4 &&
                  state.frames[1].pc == 54 && state.frames[2].pc == 1 && state.frames[3].pc == 13);
        else
            CHECK(state.rebuild == NULL && state.frame_count == 1);
        mc_client_lighting_state lighting;
        CHECK(mc_client_lighting_inspect(f->graphics, &lighting) &&
              lighting.phase == MC_CLIENT_RENDER_ENGINE_RAW);
        printf("SUBMIT %u %s %s %" PRId32 " ", index, name, exception, n->fields.R);
        if (queue == NULL)
            putchar('N');
        else if (queue->count == 0)
            putchar('-');
        else
            for (size_t i = 0; i < queue->count; ++i) {
                const int id = identity(actors, (size_t)count, queue->entries[i]);
                CHECK(id >= 0);
                printf("%s%d", i == 0 ? "" : ",", id);
            }
        printf(" %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %zu ",
               n->fields.M, n->fields.N, n->fields.O, n->fields.P, n->fields.Q, n->fields.H,
               state.dirty_polls);
        for (size_t i = 0; i < state.frame_count; ++i) {
            const mc_client_graphics_site *frame = &state.frames[i];
            CHECK(frame->class_name != NULL && frame->method != NULL && frame->descriptor != NULL);
            printf("%s%s.%s%s@%" PRId32, i == 0 ? "" : ">", frame->class_name, frame->method,
                   frame->descriptor, frame->pc);
        }
        putchar('\n');
        const mc_client_render_global_error frozen = *state.failure;
        mc_client_render_global_submit_call *other = NULL;
        mc_client_render_global_error busy;
        const int32_t r_before = n->fields.R;
        CHECK(mc_client_render_global_submit_begin(n, NULL, pass, .5, f->math, &other, &busy) ==
                  MC_CLIENT_RENDER_GLOBAL_BUSY &&
              other == NULL && n->fields.R == r_before);
        mc_client_render_global *attempt = n;
        CHECK(mc_client_render_global_retire(&attempt, &busy) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
              attempt == n && n->holds == 2 && memcmp(state.failure, &frozen, sizeof(frozen)) == 0);
        CHECK(mc_client_render_global_submit_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              call == NULL && n->submit_call == NULL && n->holds == 1);
        CHECK(mc_client_render_global_submit_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
        CHECK(mc_math_classes_state(f->math, &math_after, NULL) == MC_MATH_CLASS_OK &&
              math_after.holds == math_before.holds);
        CHECK(mc_client_render_global_retire(&n, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              n == NULL);
        CHECK(mc_client_render_engine_classes_inspect(f->graphics, &classes_after) &&
              classes_after.native_holds == classes_before.native_holds);
        f->app->settings = NULL;
        CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    }
    CHECK(seen == 12 && fclose(input) == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]))
        return 1;
    f->app->registry = f->base.registry;
    f->app->entities = f->base.entities;
    f->app->source_math_classes = f->math;
    if (!run(f, argv[2]) || !picking_close(f))
        return 1;
    free(f);
    printf("Source submission CPU: %u checks; 12 pre-U prefixes\n", checks);
    return 0;
}
