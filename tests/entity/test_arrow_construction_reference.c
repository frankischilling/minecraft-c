/* Authored finite transport over the actual component. No original default
 * entropy, Gaussian return, field result or constructor output is supplied. */
#define MC_ARROW_CONSTRUCTION_MAIN mc_arrow_component_unused_main
#include "test_arrow_construction.c"
#undef MC_ARROW_CONSTRUCTION_MAIN

#include "entity/living/fields_internal.h"
#include "entity/watcher_payload_internal.h"
#include <errno.h>
#include <inttypes.h>

enum { AR_ROWS = 40, AR_COLUMNS = 12 };

typedef struct ar_profile {
    int32_t v[AR_COLUMNS];
} ar_profile;

typedef struct ar_other_world {
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *provider;
    mc_world_info_name *name;
    mc_world_runtime *world;
} ar_other_world;

static mc_world_runtime *ar_world_a, *ar_world_b;

static const char *ar_bool(bool value) {
    return value ? "true" : "false";
}

static double ar_double(uint64_t bits) {
    double value;
    memcpy(&value, &bits, 8);
    return value;
}

static float ar_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

static void ar_d(double value) {
    printf("\"%016" PRIx64 "\"", ac_dbits(value));
}

static void ar_f(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    printf("\"%08" PRIx32 "\"", bits);
}

static void ar_random(const mc_java_random *value) {
    if (value == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("[\"%016" PRIx64 "\",%s,", value->state, ar_bool(value->have_next_next_gaussian));
    ar_d(value->next_next_gaussian);
    putchar(']');
}

static void ar_math_random(const mc_java_random_profile *profile) {
    if (profile == NULL || profile->math == NULL) {
        fputs("null", stdout);
        return;
    }
    const mc_java_random value = {.state = atomic_load(&profile->math->state),
                                  .next_next_gaussian = profile->math->next_next_gaussian,
                                  .have_next_next_gaussian =
                                      profile->math->have_next_next_gaussian};
    ar_random(&value);
}

static void ar_doubles(const double *values, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i) {
        if (i != 0)
            putchar(',');
        ar_d(values[i]);
    }
    putchar(']');
}

static void ar_floats(const float *values, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i) {
        if (i != 0)
            putchar(',');
        ar_f(values[i]);
    }
    putchar(']');
}

static bool ar_watch(const mc_entity_watcher *watcher) {
    if (watcher == NULL) {
        fputs("null", stdout);
        return true;
    }
    printf("{\"map_null\":%s,\"dirty\":%s,\"records\":[", ar_bool(watcher->map == NULL),
           ar_bool(watcher->dirty));
    const mc_entity_watch_node *nodes[64];
    size_t count = 0;
    if (watcher->map != NULL) {
        for (const mc_entity_watch_node *node = watcher->map->head; node != NULL;
             node = node->next) {
            CHECK(count < sizeof(nodes) / sizeof(*nodes) && node->record != NULL);
            size_t at = count++;
            while (at != 0 && nodes[at - 1]->key > node->key) {
                nodes[at] = nodes[at - 1];
                --at;
            }
            nodes[at] = node;
        }
        CHECK(count == watcher->map->size);
    }
    for (size_t i = 0; i < count; ++i) {
        const mc_entity_watch_record *r = nodes[i]->record;
        printf("%s{\"key\":%" PRId32 ",\"type\":%" PRId32 ",\"id\":%" PRId32
               ",\"value\":{\"kind\":",
               i == 0 ? "" : ",", nodes[i]->key, r->type, r->index);
        switch (mc_entity_watch_payload_kind_of(r)) {
            case MC_ENTITY_WATCH_PAYLOAD_NULL:
                fputs("\"Null\",\"value\":null", stdout);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_BYTE:
                printf("\"Byte\",\"value\":%d", (int)r->byte_value);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_INTEGER:
                printf("\"Integer\",\"value\":%" PRId32, r->integer_value);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_STRING: {
                mc_player_string_view text;
                CHECK(mc_player_string_read(r->string_value, &text));
                fputs("\"String\",\"value\":[", stdout);
                for (size_t j = 0; j < text.length; ++j)
                    printf("%s%u", j == 0 ? "" : ",", (unsigned int)text.units[j]);
                putchar(']');
                break;
            }
            default:
                CHECK(false);
        }
        printf("},\"dirty\":%s}", ar_bool(r->dirty));
    }
    fputs("]}", stdout);
    return true;
}

static bool ar_snapshot(const mc_entity *p, const mc_entity *shooter) {
    if (p == NULL) {
        fputs("null", stdout);
        return true;
    }
    mc_entity_class_literal_view kind;
    CHECK(mc_entity_class_literal_inspect(p->class_literal, &kind) == MC_ENTITY_OK);
    printf("{\"class\":\"%s\",\"id\":%" PRId32 ",\"world\":", kind.client_name, p->base.id);
    CHECK(p->base.world == NULL || p->base.world == ar_world_a || p->base.world == ar_world_b);
    fputs(p->base.world == NULL         ? "null"
          : p->base.world == ar_world_a ? "\"world-a\""
                                        : "\"world-b\"",
          stdout);
    fputs(",\"arrow\":", stdout);
    if (p->arrow == NULL)
        fputs("null", stdout);
    else {
        const mc_arrow_fields *a = &p->arrow->fields;
        CHECK(a->c == NULL || a->c == shooter);
        printf("{\"a\":%s,\"b\":%" PRId32 ",\"c\":%s,\"d\":%" PRId32 ",\"e\":%" PRId32
               ",\"f\":%" PRId32 ",\"g\":%" PRId32 ",\"h\":%" PRId32 ",\"i\":%s,\"j\":%" PRId32
               ",\"k\":%" PRId32 "}",
               ar_bool(a->a), a->b, a->c == NULL ? "null" : "\"shooter\"", a->d, a->e, a->f, a->g,
               a->h, ar_bool(a->i), a->j, a->k);
    }
    const mc_entity_base_fields *b = &p->base;
    const double position[] = {b->x, b->y, b->z},
                 motion[] = {b->velocity_x, b->velocity_y, b->velocity_z},
                 history[] = {b->aJ, b->aK, b->aL, b->bl, b->bm, b->bn};
    const float angles[] = {b->yaw, b->aT, b->aU, b->aV},
                dimensions[] = {b->width, b->height, b->offset};
    fputs(",\"position\":", stdout);
    ar_doubles(position, 3);
    fputs(",\"motion\":", stdout);
    ar_doubles(motion, 3);
    fputs(",\"history\":", stdout);
    ar_doubles(history, 6);
    fputs(",\"angles\":", stdout);
    ar_floats(angles, 4);
    fputs(",\"dimensions\":", stdout);
    ar_floats(dimensions, 3);
    fputs(",\"bounds\":", stdout);
    if (p->bounds == NULL)
        fputs("null", stdout);
    else {
        const double bounds[] = {p->bounds->min_x, p->bounds->min_y, p->bounds->min_z,
                                 p->bounds->max_x, p->bounds->max_y, p->bounds->max_z};
        ar_doubles(bounds, 6);
    }
    printf(",\"dead\":%s,\"random\":", ar_bool(b->be));
    ar_random(p->random);
    fputs(",\"watcher\":", stdout);
    CHECK(ar_watch(p->watcher));
    putchar('}');
    return true;
}

static bool ar_context(mc_entity_classes *classes, mc_java_random_profile *profile,
                       mc_world_runtime *world, mc_entity *shooter) {
    printf("{\"counter\":%" PRId32 ",\"world_random\":", classes->next_id);
    ar_random(world == NULL ? NULL : world->fields.r);
    fputs(",\"math_random\":", stdout);
    ar_math_random(profile);
    fputs(",\"shooter\":", stdout);
    CHECK(ar_snapshot(shooter, shooter));
    putchar('}');
    return true;
}

static void ar_site(const mc_arrow_site *s) {
    if (s->class_name == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%" PRId32 "}",
           s->class_name, s->method, s->descriptor, s->pc);
}

static void ar_receipt(const mc_arrow_error *e) {
    fputs("{\"site\":", stdout);
    ar_site(&e->site);
    fputs(",\"heading_site\":", stdout);
    ar_site(&e->heading_site);
    printf(",\"heading_entered\":%s,\"heading_returned\":%s,\"gaussian_calls\":%u,\"random_"
           "captured\":%s,\"random_before\":",
           ar_bool(e->heading_entered), ar_bool(e->heading_returned), e->gaussian_calls,
           ar_bool(e->random_captured));
    ar_random(e->random_captured ? &e->random_before : NULL);
    fputs(",\"random_after\":", stdout);
    ar_random(e->random_captured ? &e->random_after : NULL);
    fputs(",\"gaussian_values\":", stdout);
    ar_doubles(e->gaussian_values, 3);
    putchar('}');
}

static bool ar_profiles(const char *path, ar_profile rows[AR_ROWS]) {
    FILE *file = NULL;
#ifdef _MSC_VER
    if (fopen_s(&file, path, "rb") != 0)
        return false;
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL)
        return false;
    char bytes[8193];
    const size_t size = fread(bytes, 1, sizeof(bytes), file);
    const bool valid = size < sizeof(bytes) && ferror(file) == 0;
    const int closed = fclose(file);
    CHECK(valid && closed == 0);
    bytes[size] = '\0';
    char *cursor = bytes;
    for (size_t i = 0; i < AR_ROWS; ++i) {
        for (size_t j = 0; j < AR_COLUMNS; ++j) {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
                ++cursor;
            CHECK((*cursor >= '0' && *cursor <= '9') || *cursor == '-');
            errno = 0;
            char *end = NULL;
            const intmax_t value = strtoimax(cursor, &end, 10);
            CHECK(end != cursor && errno == 0 && value >= INT32_MIN && value <= INT32_MAX);
            CHECK(*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' || *end == '\0');
            rows[i].v[j] = (int32_t)value;
            cursor = end;
        }
        const int32_t *v = rows[i].v;
        CHECK(v[0] == (int32_t)i && v[1] >= 0 && v[1] <= 3 && v[2] >= 0 && v[2] <= 3 && v[3] >= 0 &&
              v[3] <= 4 && v[4] >= 0 && v[4] <= 3 && v[5] >= 0 && v[5] <= 10 && v[6] >= 0 &&
              v[6] <= 4 && v[7] >= 0 && v[7] <= 4 && (v[9] == 0 || v[9] == 1) &&
              (v[10] == -1 || (i == 18 && v[10] == 16)) && v[11] >= 0 && v[11] <= 3);
    }
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
        ++cursor;
    CHECK(cursor == bytes + size);
    return true;
}

static void ar_arguments(const ar_profile *p, double position[3], double direction[3], float *speed,
                         float *spread) {
    const double tiny = ar_double(1), nan = ar_double(UINT64_C(0x7ff8000000001234));
    const double inf = ar_double(UINT64_C(0x7ff0000000000000));
    const double geometries[][3] = {
        {7, -3, 11}, {-0.0, 0.0, -0.0}, {tiny, -tiny, -17.25}, {nan, inf, -inf}};
    const double directions[][3] = {
        {1, 2, 3},   {0, 0, 0},   {-0.0, 0.0, -0.0}, {tiny, -tiny, tiny}, {1e300, -1e300, 1e300},
        {nan, 2, 3}, {1, nan, 3}, {1, 2, nan},       {inf, 2, 3},         {1, -inf, 3},
        {1, 2, inf}};
    const float speeds[] = {1.5f, 0, -1.5f, ar_float(UINT32_C(0x7fc01234)),
                            ar_float(UINT32_C(0x7f800000))};
    const float spreads[] = {1, 0, -1, ar_float(UINT32_C(0x7fc01234)),
                             ar_float(UINT32_C(0x7f800000))};
    memcpy(position, geometries[p->v[4]], 3 * sizeof(double));
    memcpy(direction, directions[p->v[5]], 3 * sizeof(double));
    *speed = speeds[p->v[6]];
    *spread = spreads[p->v[7]];
}

static bool ar_row(const ar_profile *p, mc_entity_classes *classes, mc_java_random_profile *profile,
                   mc_math_classes *math, mc_world_runtime *world, mc_entity *shooter,
                   mc_entity *arrow) {
    double position[3], direction[3];
    float speed, spread;
    ar_arguments(p, position, direction, &speed, &spread);
    printf("ARROW_CONSTRUCTION {\"schema\":\"ARROW_CONSTRUCTION_RAW_V1\",\"case\":%" PRId32
           ",\"input\":[",
           p->v[0]);
    for (size_t i = 0; i < AR_COLUMNS; ++i)
        printf("%s%" PRId32, i == 0 ? "" : ",", p->v[i]);
    fputs("],\"arguments\":{\"position\":", stdout);
    ar_doubles(position, 3);
    fputs(",\"direction\":", stdout);
    ar_doubles(direction, 3);
    fputs(",\"speed\":", stdout);
    ar_f(speed);
    fputs(",\"spread\":", stdout);
    ar_f(spread);
    fputs("},\"before\":", stdout);
    CHECK(ar_context(classes, profile, world, shooter));
    fputs(",\"arrow_before\":", stdout);
    CHECK(ar_snapshot(p->v[1] == 3 ? arrow : NULL, shooter));
    mc_arrow_error e;
    mc_entity_status status;
    switch (p->v[1]) {
        case 0:
            status = mc_arrow_construct_world(arrow, world, &e);
            break;
        case 1:
            status = mc_arrow_construct_position(arrow, world, position[0], position[1],
                                                 position[2], &e);
            break;
        case 2:
            status = mc_arrow_construct_living(arrow, world, shooter, math, &e);
            break;
        default:
            status = mc_arrow_heading(arrow, math, direction[0], direction[1], direction[2], speed,
                                      spread, &e);
            break;
    }
    CHECK((status == MC_ENTITY_OK) == (p->v[0] != 14 && p->v[0] != 15 && p->v[0] != 38));
    CHECK(status == MC_ENTITY_OK ||
          (e.source_exception && e.exception_class != NULL && !e.native_policy));
    printf(",\"returned\":%s,\"exception\":\"%s\",\"after\":", ar_bool(status == MC_ENTITY_OK),
           status == MC_ENTITY_OK ? "NONE" : e.exception_class);
    CHECK(ar_context(classes, profile, world, shooter));
    fputs(",\"arrow_after\":", stdout);
    CHECK(ar_snapshot(arrow, shooter));
    fputs(",\"exception_message\":null,\"exception_frames\":[],\"native\":", stdout);
    ar_receipt(&e);
    fputs("}\n", stdout);
    CHECK(ferror(stdout) == 0 && fflush(stdout) == 0);
    return true;
}

static bool ar_world_create(arrow_fixture *f, ar_other_world *other) {
    CHECK(mc_world_regions_create(NULL, &other->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t blocks[256] = {0};
    mc_registry_block_view view;
    CHECK(mc_registry_classes_inspect_blocks(f->base.app.registry, &view, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        blocks[i] = view.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options so = {.session_token = 6382,
                                       .block_map = blocks,
                                       .block_map_length = 256,
                                       .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open("arrow-reference-b/world", other->regions, &so, &other->store,
                              NULL) == MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'A', 'r', 'r', 'o', 'w', 'B'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 6}, SIZE_MAX, &other->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &other->provider, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->base.app.world_classes, &limits, &other->world,
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = other->store, .name = other->name, .seed = 23, .dimension = other->provider};
    CHECK(mc_world_runtime_construct_world(other->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool ar_world_finish(ar_other_world *other) {
    CHECK(mc_world_runtime_retire_entity_containers(other->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_unbind(other->provider);
    CHECK(mc_world_runtime_retire_world(other->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(other->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(other->provider);
    CHECK(mc_world_store_free_checked(&other->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(other->regions);
    mc_world_info_name_release(other->name);
    return true;
}

static bool ar_cold(const ar_profile *p) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_math_classes *math = NULL;
    mc_entity *a = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK &&
          mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_arrow_allocate(classes, &a, NULL) == MC_ENTITY_OK);
    /* Declared native fixture work, outside the observed constructor. The
     * public route materializes the actual shared holder and draws once;
     * reflection in the original prepares its holder without that draw.
     * Independent raw seeds/prefixes are unpaired. Only the Arrow call's
     * before/after invariants and cold IN owner are compared here. */
    double unused;
    CHECK(mc_java_math_random(profile, &unused) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(ar_row(p, classes, profile, math, NULL, NULL, a));
    mc_math_class_state state;
    CHECK(mc_math_classes_state(math, &state, NULL) == MC_MATH_CLASS_OK &&
          state.phase == MC_MATH_CLASS_COLD && profile->math != NULL);
    mc_entity_release(a);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK &&
          mc_entity_classes_free(classes) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    return true;
}

static bool ar_run(const char *resources, const ar_profile rows[AR_ROWS]) {
    CHECK(ar_cold(&rows[0]));
    arrow_fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && ac_setup(f, resources, "arrow-reference-a") && ac_prepare_shooters(f));
    ar_other_world other = {0};
    CHECK(ar_world_create(f, &other));
    ar_world_a = f->base.world;
    ar_world_b = other.world;
    CHECK(mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    mc_entity *retained = NULL;
    for (size_t i = 1; i < AR_ROWS; ++i) {
        const ar_profile *p = &rows[i];
        const int32_t *v = p->v;
        f->base.world->fields.B = v[2] == 2;
        mc_world_runtime *world = v[2] == 0 ? NULL : v[2] == 3 ? other.world : f->base.world;
        mc_entity *shooter = v[3] == 0   ? NULL
                             : v[3] == 2 ? f->base.player
                                         : f->shooters[v[3] == 1 ? 0 : v[3] - 2];
        if (shooter != NULL) {
            mc_entity_position_rotation_error posed;
            CHECK(mc_entity_source_position_rotation(shooter, 8.5, 64, 8.5, 30, 10, &posed) ==
                  MC_ENTITY_OK);
            if (v[11] == 2)
                mc_entity_living_fields(shooter)->Y = 0;
        }
        mc_entity *a = NULL;
        if (v[10] == 16) {
            CHECK(retained != NULL);
            a = retained;
            retained = NULL;
        } else {
            CHECK(ac_allocate(f, &a));
            if (v[1] == 3) {
                CHECK(mc_arrow_construct_world(a, world, NULL) == MC_ENTITY_OK);
                a->base.velocity_x = .25;
                a->base.velocity_y = -.375;
                a->base.velocity_z = .5;
                a->base.yaw = 11;
                a->base.aT = -12;
                a->base.aU = 13;
                a->base.aV = -14;
                a->arrow->fields.j = 37;
                a->arrow->fields.k = 75;
                mc_java_random_reseed(a->random, v[8]);
                if (v[9] != 0)
                    (void)mc_java_random_next_gaussian(a->random);
            }
        }
        mc_java_random *saved = a->random;
        if (v[11] == 1)
            a->random = NULL;
        if (v[11] == 3)
            a->base.be = true;
        CHECK(ar_row(p, f->base.app.entities, f->base.app.profile, f->math, world, shooter, a));
        if (v[11] == 1)
            a->random = saved;
        if (i == 16)
            retained = a;
        else
            mc_entity_release(a);
    }
    CHECK(retained == NULL && ar_world_finish(&other) && ac_finish(f));
    free(f);
    ar_world_a = ar_world_b = NULL;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    ar_profile rows[AR_ROWS];
    if (!ar_profiles(argv[3], rows) || !ar_run(argv[1], rows))
        return 1;
    printf("Arrow construction reference: %zu checks;40 genuine profiles\n", checks);
    return ferror(stdout) == 0 && fflush(stdout) == 0 ? 0 : 1;
}
