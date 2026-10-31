#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
/* Genuine saved-World helpers; earlier picking main is not executed. */
#define MC_PICKING_BODY_MAIN_NAME mc_recenter_picking_fixture_main
int mc_recenter_picking_fixture_main(int argc, char **argv);
#include "test_picking.c"
#undef main

#include "client/display/source_surface.h"
#include "client/render_engine/internal.h"
#include "client/world/render_global_jo.h"
#include "client/world/render_global_recenter.h"
#include "client/world/render_global_update_sort.h"
#include "client/world/source_chunk_renderer_internal.h"
#include "entity/entity_internal.h"

enum { CAPACITY = 128, TRACE_CAPACITY = 8192 };

typedef struct observation {
    mc_render_global_jo fields;
    mc_client_source_dk *actors[CAPACITY];
    size_t actor_count, calls;

    struct {
        int left, right, result;
        bool returned;
    } trace[TRACE_CAPACITY];

    mc_client_render_global_error *error;
} observation;

static int identity(const observation *c, const mc_client_source_dk *d) {
    if (d == NULL)
        return 0;
    for (size_t i = 0; i < c->actor_count; ++i)
        if (d == c->actors[i])
            return (int)i + 1;
    return -1;
}

static bool compare(void *owner, mc_client_source_dk *left, mc_client_source_dk *right, int *out) {
    observation *c = owner;
    if (c->calls == TRACE_CAPACITY)
        return false;
    const size_t i = c->calls++;
    c->trace[i].left = identity(c, left);
    c->trace[i].right = identity(c, right);
    c->trace[i].returned = mc_render_global_jo_compare(&c->fields, left, right, out, c->error);
    if (c->trace[i].returned)
        c->trace[i].result = *out;
    return c->trace[i].returned;
}

static void ids(const observation *c, mc_client_source_dk *const *array, size_t length) {
    if (array == NULL)
        fputs("N", stdout);
    else if (length == 0)
        putchar('-');
    else
        for (size_t i = 0; i < length; ++i)
            printf("%s%d", i == 0 ? "" : ",", identity(c, array[i]));
}

static double from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool list_new(mc_client_render_global *n, mc_client_render_list **out,
                     mc_client_render_global_error *e) {
    *out = mc_render_global_allocate(n, sizeof(**out), (mc_client_graphics_site){0}, e);
    return *out != NULL;
}

static bool rows(picking_fixture *f, const char *path) {
    FILE *input = fopen(path, "r");
    CHECK(input != NULL);
    unsigned int observed = 0;
    for (;;) {
        char kind[8], name[64];
        unsigned int index;
        int present, count, length, queued;
        int32_t p, q, r, x, y, z;
        uint64_t raw[3];
        const int parsed = fscanf(input,
                                  "%7s%u%63s%d%" SCNx64 "%" SCNx64 "%" SCNx64 "%" SCNd32 "%" SCNd32
                                  "%" SCNd32 "%" SCNd32 "%" SCNd32 "%" SCNd32 "%d%d%d",
                                  kind, &index, name, &present, &raw[0], &raw[1], &raw[2], &p, &q,
                                  &r, &x, &y, &z, &count, &length, &queued);
        if (parsed == EOF)
            break;
        CHECK(parsed == 16 && index == observed++ && count >= 0 && count <= CAPACITY &&
              length >= -1 && length <= CAPACITY && queued >= 0 && queued <= CAPACITY);
        mc_client_render_global *n = NULL;
        mc_client_render_global_error error = {0};
        CHECK(mc_client_render_global_allocate(f->graphics, NULL, &n, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
        n->fields.t = f->app;
        observation *c = calloc(1, sizeof(*c));
        CHECK(c != NULL);
        c->error = &error;
        c->actor_count = (size_t)count;
        for (int i = 0; i < count; ++i) {
            int dirty;
            c->actors[i] = mc_source_dk_allocate(n, &error);
            CHECK(c->actors[i] != NULL);
            mc_client_source_dk_fields *d = &c->actors[i]->fields;
            CHECK(fscanf(input,
                         "%" SCNd32 "%" SCNd32 "%" SCNd32 "%" SCNd32 "%" SCNd32 "%" SCNd32 "%d",
                         &d->c, &d->d, &d->e, &d->q, &d->r, &d->s, &dirty) == 7);
            d->u = dirty != 0;
        }
        mc_client_source_dk **array = NULL;
        int order[CAPACITY] = {0};
        if (length >= 0) {
            array =
                mc_render_global_allocate(n, (size_t)(length == 0 ? 1 : length) * sizeof(*array),
                                          (mc_client_graphics_site){0}, &error);
            CHECK(array != NULL);
        }
        for (int i = 0; i < length; ++i) {
            CHECK(fscanf(input, "%d", &order[i]) == 1 && order[i] >= 0 && order[i] <= count);
            array[i] = order[i] == 0 ? NULL : c->actors[order[i] - 1];
        }
        CHECK(list_new(n, &n->fields.m, &error));
        for (int i = 0; i < queued; ++i) {
            int id;
            CHECK(fscanf(input, "%d", &id) == 1 && id >= 0 && id <= count);
            CHECK(mc_render_global_pointer_append(n, n->fields.m,
                                                  id == 0 ? NULL : c->actors[id - 1],
                                                  (mc_client_graphics_site){0}, &error));
        }
        n->active = true;
        if (strcmp(kind, "JO") == 0) {
            mc_entity *camera = NULL;
            if (present) {
                CHECK(mc_local_player_allocate(f->base.entities, &camera, NULL) == MC_ENTITY_OK);
                camera->base.x = from_bits(raw[0]);
                camera->base.y = from_bits(raw[1]);
                camera->base.z = from_bits(raw[2]);
            }
            const bool constructed = mc_render_global_jo_construct(&c->fields, camera, &error);
            bool ok = false;
            if (constructed && array != NULL)
                ok = mc_render_global_update_sort(array, (size_t)length, c, compare, &error);
            const char *exception =
                ok                                                                ? "NONE"
                : error.status == MC_CLIENT_RENDER_GLOBAL_SOURCE_ILLEGAL_ARGUMENT ? "IAE"
                                                                                  : "NPE";
            CHECK(ok || (constructed && array == NULL) ||
                  error.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE ||
                  error.status == MC_CLIENT_RENDER_GLOBAL_SOURCE_ILLEGAL_ARGUMENT);
            printf("JO %u %s %s ", index, name, exception);
            if (constructed)
                printf("%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64, picking_bits(c->fields.a),
                       picking_bits(c->fields.b), picking_bits(c->fields.c));
            else
                putchar('N');
            putchar(' ');
            ids(c, array, length < 0 ? 0 : (size_t)length);
            putchar(' ');
            if (c->calls == 0)
                putchar('-');
            for (size_t i = 0; i < c->calls; ++i) {
                printf("%s%d:%d:", i == 0 ? "" : ",", c->trace[i].left, c->trace[i].right);
                if (c->trace[i].returned)
                    printf("%d", c->trace[i].result);
                else
                    putchar('X');
            }
            putchar('\n');
            mc_client_source_dk *final[CAPACITY];
            if (length > 0)
                memcpy(final, array, (size_t)length * sizeof(*final));
            for (int i = 0; i < length; ++i)
                array[i] = order[i] == 0 ? NULL : c->actors[order[i] - 1];
            n->fields.n = array;
            n->n_length = length < 0 ? 0 : (size_t)length;
            error = (mc_client_render_global_error){0};
            CHECK(mc_render_global_sort_entered(n, camera, &error) == ok);
            if (length > 0)
                CHECK(memcmp(final, array, (size_t)length * sizeof(*final)) == 0);
            if (!ok) {
                CHECK(!error.native_policy);
                if (!constructed)
                    CHECK(error.source_pc == 6 && error.parent.pc == 296);
                else if (array == NULL)
                    CHECK(error.source_pc == 299);
                else
                    CHECK((error.source_pc == 1 || error.source_pc == 36) &&
                          error.parent.pc == 299);
            }
            mc_entity_release(camera);
        } else {
            CHECK(strcmp(kind, "REC") == 0);
            n->fields.p = p;
            n->fields.q = q;
            n->fields.r = r;
            n->fields.o = array;
            n->o_length = length < 0 ? 0 : (size_t)length;
            const bool ok = mc_render_global_recenter_entered(n, x, y, z, &error);
            const char *exception =
                ok                                                          ? "NONE"
                : error.status == MC_CLIENT_RENDER_GLOBAL_SOURCE_ARITHMETIC ? "ARITH"
                : error.graphics.status == MC_CLIENT_GRAPHICS_ARRAY_INDEX   ? "AIOOBE"
                                                                            : "NPE";
            CHECK(ok ||
                  (!error.native_policy &&
                   (error.source_pc == 106 || error.source_pc == 309 || error.source_pc == 314)));
            printf("REC %u %s %s %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
                   " %" PRId32 " ",
                   index, name, exception, n->fields.B, n->fields.C, n->fields.D, n->fields.E,
                   n->fields.F, n->fields.G);
            ids(c, array, length < 0 ? 0 : (size_t)length);
            putchar(' ');
            if (n->fields.m->count == 0)
                putchar('-');
            else
                ids(c, (mc_client_source_dk **)(void *)n->fields.m->entries, n->fields.m->count);
            putchar(' ');
            if (count == 0)
                putchar('-');
            for (int i = 0; i < count; ++i) {
                const mc_client_source_dk_fields *d = &c->actors[i]->fields;
                printf("%s%" PRId32 ":%" PRId32 ":%" PRId32 ":%d", i == 0 ? "" : ",", d->c, d->d,
                       d->e, d->u);
            }
            putchar('\n');
        }
        CHECK(n->active);
        n->active = false;
        CHECK(mc_client_render_global_retire(&n, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              n == NULL);
        free(c);
    }
    CHECK(observed == 31 && fclose(input) == 0);
    return true;
}

static bool device(picking_fixture *f) {
    mc_client_display *display = NULL;
    mc_client_display_error gl;
    CHECK(mc_client_display_create("Source recenter and rebuild", 64, 64, true, &display, &gl));
    mc_client_display_list_range *range = NULL;
    int32_t base;
    /* The command quota covers the WHOLE range, not each name. Two hundred
     * real bounding-list draws plus one replacement prefix must fit. */
    CHECK(mc_client_display_lists_generate(display, 600, 600, &range, &base, &gl));
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state opts;
    CHECK(mc_game_settings_read_state(settings, &opts));
    opts.values.render_distance = 3;
    CHECK(mc_game_settings_set_values(settings, &opts.values, NULL) == MC_GAME_SETTINGS_OK);
    f->app->settings = settings;
    f->app->source_math_classes = f->math;
    f->app->i = f->actors[0];
    mc_client_render_global *n = NULL;
    mc_client_render_global_error error = {0};
    CHECK(mc_client_render_global_allocate(f->graphics, NULL, &n, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    n->fields.t = f->app;
    n->fields.k = f->base.world; /* Declared borrowed field; DK takes independent leases. */
    n->display = display;
    n->fields.s = base;
    CHECK(list_new(n, &n->fields.a, &error) && list_new(n, &n->fields.m, &error));
    const size_t bindings = f->base.world->persistent_bindings;
    mc_client_render_global_world_state state = {0};
    n->active = true;
    const bool rebuilt = mc_render_global_rebuild_entered(n, f->math, &state, &error);
    if (!rebuilt) {
        fprintf(stderr,
                "Rebuild status%d %s.%s%s:%u native%d operation=%s; progress%u xyz=%" PRId32
                ",%" PRId32 ",%" PRId32 " constructed=%zu published=%zu arena=%zu; "
                "registry%d %s; graphics%d %s.%s%s:%" PRId32 " %s; "
                "Display%d %s [%s]; graphics.Display%d %s [%s]; Math%d %s; "
                "Entities%d Bounds%d %s\n",
                (int)error.status, error.source_class == NULL ? "?" : error.source_class,
                error.source_method == NULL ? "?" : error.source_method,
                error.source_descriptor == NULL ? "?" : error.source_descriptor, error.source_pc,
                error.native_policy, error.operation == NULL ? "?" : error.operation,
                state.rebuild_pc, state.x, state.y, state.z, state.constructed_chunks,
                state.published_chunks, n->arena.bytes, (int)error.registry.status,
                error.registry.message == NULL ? "?" : error.registry.message,
                (int)error.graphics.status,
                error.graphics.site.class_name == NULL ? "?" : error.graphics.site.class_name,
                error.graphics.site.method == NULL ? "?" : error.graphics.site.method,
                error.graphics.site.descriptor == NULL ? "?" : error.graphics.site.descriptor,
                error.graphics.site.pc,
                error.graphics.operation == NULL ? "?" : error.graphics.operation,
                (int)error.display.status,
                error.display.operation == NULL ? "?" : error.display.operation,
                error.display.detail, (int)error.graphics.display.status,
                error.graphics.display.operation == NULL ? "?" : error.graphics.display.operation,
                error.graphics.display.detail, (int)error.math.status,
                error.math.operation == NULL ? "?" : error.math.operation, (int)error.entity.status,
                (int)error.bounds.status,
                error.bounds.operation == NULL ? "?" : error.bounds.operation);
    }
    CHECK(rebuilt);
    CHECK(state.rebuild_returned && state.rebuild_pc == 699 && state.last_store_pc == 696 &&
          state.constructed_chunks == 200 && state.published_chunks == 200 &&
          state.queued_chunks == 200 && n->fields.I == 2 && n->fields.H == 3 &&
          n->o_length == 200 && n->n_length == 200 && n->fields.m->count == 200 &&
          f->base.world->persistent_bindings == bindings + 200);
    mc_render_global_jo comparator;
    CHECK(mc_render_global_jo_construct(&comparator, f->actors[0], &error));
    for (size_t i = 1; i < n->n_length; ++i) {
        int order;
        CHECK(mc_render_global_jo_compare(&comparator, n->fields.n[i - 1], n->fields.n[i], &order,
                                          &error) &&
              order <= 0);
    }
    for (size_t i = 0; i < n->o_length; ++i)
        n->fields.o[i]->fields.u = false;
    n->fields.m->count = 0;
    CHECK(mc_render_global_recenter_entered(n, 160, 77, -160, &error));
    CHECK(n->fields.m->count == 200 && n->fields.B == 112 && n->fields.E == 176 &&
          n->fields.C == 0 && n->fields.F == 112 && n->fields.D == -208 && n->fields.G == -144);
    size_t appended = 0;
    for (int32_t x = 0; x < 5; ++x)
        for (int32_t z = 0; z < 5; ++z)
            for (int32_t y = 0; y < 8; ++y) {
                mc_client_source_dk *d = n->fields.o[((z * 8) + y) * 5 + x];
                CHECK(n->fields.m->entries[appended++] == d && d->fields.u && !d->fields.E &&
                      !d->fields.o);
            }
    for (size_t i = 0; i < n->o_length; ++i)
        n->fields.o[i]->fields.u = false;
    n->fields.m->count = 0;
    mc_client_source_dk *third = n->fields.o[10];
    bool *passes = third->fields.p;
    third->fields.p = NULL;
    CHECK(!mc_render_global_recenter_entered(n, 320, 0, 320, &error));
    CHECK(!error.native_policy && error.source_pc == 13 && strcmp(error.source_class, "dk") == 0 &&
          strcmp(error.source_method, "b") == 0 && error.parent.pc == 26 &&
          strcmp(error.parent.class_name, "dk") == 0 && strcmp(error.parent.method, "a") == 0 &&
          strcmp(error.parent.descriptor, "(III)V") == 0 && error.ancestor.pc == 327 &&
          strcmp(error.ancestor.class_name, "n") == 0 && strcmp(error.ancestor.method, "b") == 0 &&
          strcmp(error.ancestor.descriptor, "(III)V") == 0 && n->fields.m->count == 2 &&
          n->fields.m->entries[0] == n->fields.o[0] && n->fields.m->entries[1] == n->fields.o[5] &&
          n->fields.o[0]->fields.c == 320 && n->fields.o[5]->fields.c == 320 &&
          third->fields.c == 160 && !third->fields.u && n->fields.C == 0 && n->fields.F == 32);
    third->fields.p = passes; /* End declared corruption after observing its source prefix. */
    n->active = false;
    CHECK(mc_client_render_global_retire(&n, &error) == MC_CLIENT_RENDER_GLOBAL_OK && n == NULL);
    CHECK(f->base.world->persistent_bindings == bindings);
    CHECK(mc_client_display_lists_retire(&range, &gl));
    CHECK(mc_client_display_destroy(display, &gl));
    f->app->settings = NULL;
    f->app->i = NULL;
    f->app->source_math_classes = NULL;
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    return true;
}

int main(int argc, char **argv) {
    const bool cpu_only = argc == 5 && strcmp(argv[4], "--cpu-only") == 0;
    if (argc != 4 && !cpu_only)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]))
        return 1;
    f->app->entities = f->base.entities;
    f->app->registry = f->base.registry;
    if (!rows(f, argv[3]) ||
        (!cpu_only && (!picking_case_open(f, argv[2]) || !device(f) || !picking_case_close(f))) ||
        !picking_close(f))
        return 1;
    free(f);
    printf("Source recenter/JO: %u checks; 31 rows; device=%d\n", checks, !cpu_only);
    return 0;
}
