/* Actual Arrow NBT calls over the genuine fixture; values are emitted live.
 * No original result, entropy or exception prefix is supplied to production. */
#define MC_ARROW_NBT_MAIN mc_arrow_nbt_reference_unused_component_main
#include "test_arrow_nbt.c"
#undef MC_ARROW_NBT_MAIN
#include "entity/living/fields_internal.h"
#include "entity/watcher_payload_internal.h"
#include <errno.h>
#include <inttypes.h>

static FILE *nr_out;
static mc_world_runtime *ar_world_a, *ar_world_b;

static const char *ar_bool(bool value) {
    return value ? "true" : "false";
}

static void ar_d(double value) {
    fprintf(nr_out, "\"%016" PRIx64 "\"", ac_dbits(value));
}

static void ar_f(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    fprintf(nr_out, "\"%08" PRIx32 "\"", bits);
}

static void ar_random(const mc_java_random *value) {
    if (value == NULL) {
        fputs("null", nr_out);
        return;
    }
    fprintf(nr_out, "[\"%016" PRIx64 "\",%s,", value->state,
            ar_bool(value->have_next_next_gaussian));
    ar_d(value->next_next_gaussian);
    fputc(']', nr_out);
}

static void ar_math_random(const mc_java_random_profile *profile) {
    if (profile == NULL || profile->math == NULL) {
        fputs("null", nr_out);
        return;
    }
    const mc_java_random value = {.state = atomic_load(&profile->math->state),
                                  .next_next_gaussian = profile->math->next_next_gaussian,
                                  .have_next_next_gaussian =
                                      profile->math->have_next_next_gaussian};
    ar_random(&value);
}

static void ar_doubles(const double *values, size_t count) {
    fputc('[', nr_out);
    for (size_t i = 0; i < count; ++i) {
        if (i != 0)
            fputc(',', nr_out);
        ar_d(values[i]);
    }
    fputc(']', nr_out);
}

static void ar_floats(const float *values, size_t count) {
    fputc('[', nr_out);
    for (size_t i = 0; i < count; ++i) {
        if (i != 0)
            fputc(',', nr_out);
        ar_f(values[i]);
    }
    fputc(']', nr_out);
}

static bool ar_watch(const mc_entity_watcher *watcher) {
    if (watcher == NULL) {
        fputs("null", nr_out);
        return true;
    }
    fprintf(nr_out, "{\"map_null\":%s,\"dirty\":%s,\"records\":[", ar_bool(watcher->map == NULL),
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
        fprintf(nr_out,
                "%s{\"key\":%" PRId32 ",\"type\":%" PRId32 ",\"id\":%" PRId32
                ",\"value\":{\"kind\":",
                i == 0 ? "" : ",", nodes[i]->key, r->type, r->index);
        switch (mc_entity_watch_payload_kind_of(r)) {
            case MC_ENTITY_WATCH_PAYLOAD_NULL:
                fputs("\"Null\",\"value\":null", nr_out);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_BYTE:
                fprintf(nr_out, "\"Byte\",\"value\":%d", (int)r->byte_value);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_INTEGER:
                fprintf(nr_out, "\"Integer\",\"value\":%" PRId32, r->integer_value);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_STRING: {
                mc_player_string_view text;
                CHECK(mc_player_string_read(r->string_value, &text));
                fputs("\"String\",\"value\":[", nr_out);
                for (size_t j = 0; j < text.length; ++j)
                    fprintf(nr_out, "%s%u", j == 0 ? "" : ",", (unsigned int)text.units[j]);
                fputc(']', nr_out);
                break;
            }
            default:
                CHECK(false);
        }
        fprintf(nr_out, "},\"dirty\":%s}", ar_bool(r->dirty));
    }
    fputs("]}", nr_out);
    return true;
}

static bool ar_snapshot(const mc_entity *p, const mc_entity *shooter) {
    if (p == NULL) {
        fputs("null", nr_out);
        return true;
    }
    mc_entity_class_literal_view kind;
    CHECK(mc_entity_class_literal_inspect(p->class_literal, &kind) == MC_ENTITY_OK);
    fprintf(nr_out, "{\"class\":\"%s\",\"id\":%" PRId32 ",\"world\":", kind.client_name,
            p->base.id);
    CHECK(p->base.world == NULL || p->base.world == ar_world_a || p->base.world == ar_world_b);
    fputs(p->base.world == NULL         ? "null"
          : p->base.world == ar_world_a ? "\"world-a\""
                                        : "\"world-b\"",
          nr_out);
    fputs(",\"arrow\":", nr_out);
    if (p->arrow == NULL)
        fputs("null", nr_out);
    else {
        const mc_arrow_fields *a = &p->arrow->fields;
        CHECK(a->c == NULL || a->c == shooter);
        fprintf(nr_out,
                "{\"a\":%s,\"b\":%" PRId32 ",\"c\":%s,\"d\":%" PRId32 ",\"e\":%" PRId32
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
    fputs(",\"position\":", nr_out);
    ar_doubles(position, 3);
    fputs(",\"motion\":", nr_out);
    ar_doubles(motion, 3);
    fputs(",\"history\":", nr_out);
    ar_doubles(history, 6);
    fputs(",\"angles\":", nr_out);
    ar_floats(angles, 4);
    fputs(",\"dimensions\":", nr_out);
    ar_floats(dimensions, 3);
    fputs(",\"bounds\":", nr_out);
    if (p->bounds == NULL)
        fputs("null", nr_out);
    else {
        const double bounds[] = {p->bounds->min_x, p->bounds->min_y, p->bounds->min_z,
                                 p->bounds->max_x, p->bounds->max_y, p->bounds->max_z};
        ar_doubles(bounds, 6);
    }
    fprintf(nr_out, ",\"dead\":%s,\"random\":", ar_bool(b->be));
    ar_random(p->random);
    fputs(",\"watcher\":", nr_out);
    CHECK(ar_watch(p->watcher));
    fputc('}', nr_out);
    return true;
}

static bool ar_context(mc_entity_classes *classes, mc_java_random_profile *profile,
                       mc_world_runtime *world, mc_entity *shooter) {
    fprintf(nr_out, "{\"counter\":%" PRId32 ",\"world_random\":", classes->next_id);
    ar_random(world == NULL ? NULL : world->fields.r);
    fputs(",\"math_random\":", nr_out);
    ar_math_random(profile);
    fputs(",\"shooter\":", nr_out);
    CHECK(ar_snapshot(shooter, shooter));
    fputc('}', nr_out);
    return true;
}

static bool nr_snapshot(const mc_entity *p, const mc_entity *shooter,
                        const mc_entity_bounds *original) {
    if (p == NULL) {
        fputs("null", nr_out);
        return true;
    }
    fputs("{\"entity\":", nr_out);
    CHECK(ar_snapshot(p, shooter));
    fputs(",\"base\":{\"fall\":", nr_out);
    ar_f(p->base.bk);
    fprintf(nr_out,
            ",\"fire\":%" PRId32 ",\"air\":%" PRId32 ",\"on_ground\":%s,\"y_size\":", p->base.bv,
            p->base.bz, ar_bool(p->base.aX));
    ar_f(p->base.y_size);
    fprintf(nr_out, "},\"bounds_same\":%s,\"original_bounds\":", ar_bool(p->bounds == original));
    if (original == NULL)
        fputs("null", nr_out);
    else {
        const double values[] = {original->min_x, original->min_y, original->min_z,
                                 original->max_x, original->max_y, original->max_z};
        ar_doubles(values, 6);
    }
    fputc('}', nr_out);
    return true;
}

static bool nr_name(mc_nbt_string name) {
    fputc('"', nr_out);
    for (size_t i = 0; i < name.length; ++i) {
        CHECK(name.units[i] >= 32 && name.units[i] < 127 && name.units[i] != '"' &&
              name.units[i] != '\\');
        fputc((int)name.units[i], nr_out);
    }
    fputc('"', nr_out);
    return true;
}

static void nr_utf16(mc_nbt_string name) {
    fputs("{\"utf16\":[", nr_out);
    for (size_t i = 0; i < name.length; ++i)
        fprintf(nr_out, "%s%u", i == 0 ? "" : ",", (unsigned int)name.units[i]);
    fputs("]}", nr_out);
}

static bool nr_tag(const mc_nbt_tag *tag, unsigned int depth) {
    CHECK(depth < 16);
    if (tag == NULL) {
        fputs("null", nr_out);
        return true;
    }
    fprintf(nr_out, "{\"kind\":%u", (unsigned int)tag->type);
    if (tag->type == MC_NBT_COMPOUND) {
        fputs(",\"map_null\":false,\"values\":{", nr_out);
        for (size_t i = 0; i < tag->value.compound.length; ++i) {
            const mc_nbt_tag *field = tag->value.compound.items[i];
            CHECK(field != NULL);
            if (i != 0)
                fputc(',', nr_out);
            CHECK(nr_name(field->name));
            fputc(':', nr_out);
            CHECK(nr_tag(field, depth + 1));
        }
        fputs("},\"source_order\":[", nr_out);
        for (size_t i = 0; i < tag->value.compound.length; ++i) {
            const mc_nbt_tag *field = tag->value.compound.items[i];
            fprintf(nr_out, "%s{\"key\":", i == 0 ? "" : ",");
            nr_utf16(field->name);
            fputs(",\"tag\":", nr_out);
            CHECK(nr_tag(field, depth + 1));
            fputc('}', nr_out);
        }
        fputc(']', nr_out);
    } else if (tag->type == MC_NBT_LIST) {
        fprintf(nr_out, ",\"element_type\":%u,\"values\":[",
                (unsigned int)tag->value.list.element_type);
        for (size_t i = 0; i < tag->value.list.length; ++i) {
            if (i != 0)
                fputc(',', nr_out);
            CHECK(nr_tag(tag->value.list.items[i], depth + 1));
        }
        fputc(']', nr_out);
    } else {
        fputs(",\"value\":", nr_out);
        switch (tag->type) {
            case MC_NBT_BYTE:
                fprintf(nr_out, "%d", (int)tag->value.byte_value);
                break;
            case MC_NBT_SHORT:
                fprintf(nr_out, "%d", (int)tag->value.short_value);
                break;
            case MC_NBT_INT:
                fprintf(nr_out, "%" PRId32, tag->value.int_value);
                break;
            case MC_NBT_LONG:
                fprintf(nr_out, "{\"J\":\"%016" PRIx64 "\"}", (uint64_t)tag->value.long_value);
                break;
            case MC_NBT_FLOAT:
                fprintf(nr_out, "{\"F\":\"%08" PRIx32 "\"}", tag->value.float_bits);
                break;
            case MC_NBT_DOUBLE:
                fprintf(nr_out, "{\"D\":\"%016" PRIx64 "\"}", tag->value.double_bits);
                break;
            case MC_NBT_STRING:
                nr_utf16(tag->value.string);
                break;
            default:
                CHECK(false);
        }
    }
    fputc('}', nr_out);
    return true;
}

static void nr_own(arrow_nbt_input *in, int32_t mode) {
    an_input_init(in, false, false);
    in->root.value.compound.items = in->items + 7;
    in->root.value.compound.length = 8;
    const int32_t coordinates[3][3] = {{17, -19, 32767}, {-32768, -1, 0}, {32767, 0, 1}};
    const size_t chosen = mode == 1 ? 1 : mode == 2 ? 2 : 0;
    for (size_t i = 0; i < 3; ++i)
        in->fields[7 + i].value.short_value = mc_java_i16_narrow(coordinates[chosen][i]);
    in->fields[10].value.byte_value = mode == 3 ? -1 : -128;
    in->fields[11].value.byte_value = -1;
    in->fields[12].value.byte_value = -2;
    in->fields[13].value.byte_value = in->fields[14].value.byte_value = mode == 3   ? -1
                                                                        : mode == 4 ? 2
                                                                                    : 1;
    if (mode == 5)
        in->root.value.compound.length = 0;
    if (mode == 6) {
        in->root.value.compound.items = in->items + 8;
        in->root.value.compound.length = 7;
    }
}

static void nr_parent(arrow_nbt_input *in, int32_t mode) {
    nr_own(in, 0);
    in->root.value.compound.items = in->items;
    in->root.value.compound.length = 15;
    const double position[] = {8.5, 64.9375, -8.25}, motion[] = {.1, .2, .3};
    for (size_t i = 0; i < 3; ++i) {
        in->pos[i].value.double_bits = ac_dbits(position[i]);
        in->motion[i].value.double_bits = ac_dbits(motion[i]);
    }
    in->rotation[0].value.float_bits = an_fbits(450);
    in->rotation[1].value.float_bits = an_fbits(-450);
    in->fields[3].value.float_bits = an_fbits(3.5f);
    in->fields[4].value.short_value = -19;
    in->fields[5].value.short_value = 298;
    in->fields[6].value.byte_value = 1;
    if (mode == 1) {
        in->fields[2].type = MC_NBT_BYTE;
        in->fields[2].value.byte_value = 7;
    } else if (mode == 2) {
        for (size_t i = 2; i + 1 < in->root.value.compound.length; ++i)
            in->items[i] = in->items[i + 1];
        --in->root.value.compound.length;
    } else if (mode == 3)
        in->fields[1].value.list.length = 1;
    else if (mode == 4) {
        in->motion[1].type = MC_NBT_FLOAT;
        in->motion[1].value.float_bits = an_fbits(.2f);
    } else if (mode == 5)
        in->motion_items[1] = NULL;
    else if (mode == 6) {
        in->pos[0].type = MC_NBT_FLOAT;
        in->pos[0].value.float_bits = an_fbits(8.5f);
        in->fields[0].value.list.length = 1;
        in->fields[0].value.list.element_type = MC_NBT_FLOAT;
    } else if (mode == 7) {
        in->fields[14].type = MC_NBT_SHORT;
        in->fields[14].value.short_value = 1;
    } else if (mode == 8) {
        in->motion[0].value.double_bits = UINT64_C(0x7ff0000000000000);
        in->motion[1].value.double_bits = UINT64_C(0x7ff8000000001234);
        in->motion[2].value.double_bits = UINT64_C(0x8000000000000000);
        in->rotation[0].value.float_bits = an_fbits(810);
        in->rotation[1].value.float_bits = an_fbits(-810);
    } else if (mode == 9) {
        in->motion[0].value.double_bits = ac_dbits(-10.000000000000002);
        in->motion[1].value.double_bits = ac_dbits(10);
        in->motion[2].value.double_bits = ac_dbits(-10);
        in->rotation[0].value.float_bits = UINT32_C(0x80000000);
        in->rotation[1].value.float_bits = UINT32_C(0x7fc01234);
    }
}

static bool nr_prepare(arrow_nbt_fixture *f, const int32_t p[5], mc_entity **out,
                       arrow_nbt_input *in, const mc_nbt_tag **compound) {
    *out = NULL;
    mc_world_runtime *world = p[4] == 0 ? NULL : f->construction.base.world;
    if (p[1] != 5) {
        CHECK(ac_allocate(&f->construction, out) &&
              mc_arrow_construct_world(*out, world, NULL) == MC_ENTITY_OK);
        mc_entity *a = *out;
        mc_entity_position_rotation_error pose;
        CHECK(mc_entity_source_position_rotation(a, 7.25, 70.5, -3.75, 11.25f, -12.5f, &pose) ==
                  MC_ENTITY_OK &&
              mc_arrow_shooter_store(a, f->construction.shooters[0], NULL) == MC_ENTITY_OK);
        a->base.velocity_x = .25;
        a->base.velocity_y = -.375;
        a->base.velocity_z = .5;
        a->base.bk = 2.25f;
        a->base.bv = 65537;
        a->base.bz = -65537;
        a->base.aX = true;
        a->base.y_size = p[1] == 3 && p[2] == 1 ? .3125f : 0;
        static const int32_t values[8][6] = {
            {101, -202, 303, 404, 505, 222},
            {65535, 65536, -65537, 256, 257, -1},
            {-65536, -65535, 65537, -256, -257, 255},
            {INT32_MIN, INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN, INT32_MAX},
            {7, 8, 9, 10, 11, 12},
            {7, 8, 9, 10, 11, 12},
            {7, 8, 9, 10, 11, 12},
            {7, 8, 9, 10, 11, 12}};
        const int32_t *v = values[p[1] == 1 ? p[2] : 0];
        mc_arrow_fields *own = &a->arrow->fields;
        own->d = v[0];
        own->e = v[1];
        own->f = v[2];
        own->g = v[3];
        own->h = v[4];
        own->b = v[5];
        own->j = 37;
        own->k = 75;
        own->a = p[1] != 1 || p[2] < 4 || (p[2] & 1) != 0;
        own->i = p[1] != 1 || p[2] < 4 || (p[2] & 2) != 0;
        if (p[1] == 4 && p[2] == 1)
            a->base.be = true;
    }
    if (p[1] == 0)
        nr_own(in, p[2]);
    else if (p[1] == 2 || p[1] == 5)
        nr_parent(in, p[1] == 2 ? p[2] : 0);
    else {
        an_input_init(in, false, false);
        in->fields[16].type = MC_NBT_INT;
        in->fields[16].name = an_ascii("retained_unknown", in->names[16]);
        in->fields[16].value.int_value = 1234567;
        in->items[0] = in->fields + 16;
        in->root.value.compound.length = 1;
    }
    if (p[1] == 5) {
        static const uint16_t unknown[] = {'N', 'o', 'S', 'u', 'c', 'h', 'A', 'r',
                                           'r', 'o', 'w', 'T', 'y', 'p', 'e'};
        in->items[in->root.value.compound.length++] = in->fields + 15;
        if (p[2] == 1) {
            for (size_t i = 0; i < 7; ++i)
                in->items[i] = in->fields + i;
            in->items[7] = in->fields + 15;
            in->root.value.compound.length = 8;
        } else if (p[2] == 2) {
            in->fields[14].type = MC_NBT_SHORT;
            in->fields[14].value.short_value = 1;
        } else if (p[2] == 3)
            in->fields[15].value.string =
                (mc_nbt_string){unknown, sizeof(unknown) / sizeof(*unknown)};
    }
    if (p[3] >= 1 && p[3] <= 8) {
        in->fields[6 + p[3]].type = MC_NBT_INT;
        in->fields[6 + p[3]].value.int_value = 123;
    }
    *compound = p[3] == 9 ? NULL : &in->root;
    return true;
}

static void nr_site(const mc_arrow_site *site) {
    if (site == NULL || site->class_name == NULL) {
        fputs("null", nr_out);
        return;
    }
    fprintf(nr_out, "{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%d}",
            site->class_name, site->method, site->descriptor, site->pc);
}

static bool nr_row(arrow_nbt_fixture *f, const int32_t p[5]) {
    mc_entity *arrow = NULL, *result = NULL;
    arrow_nbt_input in;
    const mc_nbt_tag *input;
    CHECK(nr_prepare(f, p, &arrow, &in, &input));
    const mc_entity_bounds *original = arrow == NULL ? NULL : arrow->bounds;
    mc_entity *shooter = f->construction.shooters[0];
    mc_entity_classes *classes = f->construction.base.app.entities;
    mc_java_random_profile *profile = f->construction.base.app.profile;
    mc_world_runtime *world = p[4] == 0 ? NULL : f->construction.base.world;
    /* Buffer only transport bytes so the factory's real source println remains
     * a separate complete stdout line. No native source stream is replaced. */
#ifdef _MSC_VER
    CHECK(tmpfile_s(&nr_out) == 0);
#else
    nr_out = tmpfile();
#endif
    CHECK(nr_out != NULL);
    fprintf(nr_out, "ARROW_NBT {\"schema\":\"ARROW_NBT_RAW_V1\",\"case\":%" PRId32 ",\"input\":[",
            p[0]);
    for (size_t i = 0; i < 5; ++i)
        fprintf(nr_out, "%s%" PRId32, i == 0 ? "" : ",", p[i]);
    fputs("],\"domain\":\"paired\",\"before\":", nr_out);
    CHECK(ar_context(classes, profile, world, shooter));
    fputs(",\"arrow_before\":", nr_out);
    CHECK(nr_snapshot(arrow, shooter, original));
    fputs(",\"tag_before\":", nr_out);
    CHECK(nr_tag(input, 0));
    mc_entity_nbt_error error = mc_entity_nbt_empty_error();
    mc_nbt_tree *tree = NULL;
    mc_entity_nbt_status status;
    bool saved = false;
    switch (p[1]) {
        case 0:
            status = mc_arrow_nbt_read_held(arrow, input, &error) ? MC_ENTITY_NBT_OK : error.status;
            break;
        case 1:
            status = mc_arrow_nbt_build_fields_held(arrow, NULL, &tree, &error) ? MC_ENTITY_NBT_OK
                                                                                : error.status;
            break;
        case 2:
            status = mc_entity_read_nbt(arrow, input, &error);
            break;
        case 3:
            status = mc_entity_build_nbt(arrow, NULL, &tree, &error);
            break;
        case 4:
            status = mc_entity_save_nbt(arrow, NULL, &saved, &tree, &error);
            break;
        default:
            status = mc_entity_factory_nbt(classes, world, input, &result, &error);
            break;
    }
    CHECK(status == MC_ENTITY_NBT_OK || status == MC_ENTITY_NBT_WRONG_TAG ||
          status == MC_ENTITY_NBT_NULL_REFERENCE || status == MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS);
    const char *exception = status == MC_ENTITY_NBT_OK          ? "NONE"
                            : status == MC_ENTITY_NBT_WRONG_TAG ? "java/lang/ClassCastException"
                            : status == MC_ENTITY_NBT_NULL_REFERENCE
                                ? "java/lang/NullPointerException"
                                : "java/lang/IndexOutOfBoundsException";
    fprintf(nr_out, ",\"returned\":%s,\"exception\":\"%s\",\"saved\":%s,\"after\":",
            ar_bool(status == MC_ENTITY_NBT_OK), exception, p[1] == 4 ? ar_bool(saved) : "null");
    CHECK(ar_context(classes, profile, world, shooter));
    fputs(",\"arrow_after\":", nr_out);
    CHECK(nr_snapshot(arrow, shooter, original));
    fputs(",\"result\":", nr_out);
    CHECK(nr_snapshot(result, shooter, original));
    fputs(",\"tag_after\":", nr_out);
    CHECK(nr_tag(tree == NULL ? input : mc_nbt_tree_root(tree), 0));
    fprintf(nr_out,
            ",\"exception_message\":null,\"exception_frames\":[],\"native\":{\"status\":%u,\"base_"
            "pc\":%u,\"factory_pc\":%u,\"wrapper_pc\":%u,\"arrow_site\":",
            (unsigned int)status, error.base_bytecode, error.factory_bytecode,
            error.wrapper_bytecode);
    const bool own = error.late_hook == MC_ENTITY_NBT_LATE_ARROW;
    nr_site(own ? &error.arrow.site : NULL);
    fputs(",\"arrow_child\":", nr_out);
    nr_site(own ? &error.arrow.child : NULL);
    fprintf(nr_out, ",\"arrow_returned\":%s}}\n", ar_bool(own && error.arrow.returned));
    CHECK(ferror(nr_out) == 0 && fflush(nr_out) == 0 && fseek(nr_out, 0, SEEK_SET) == 0);
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), nr_out)) != 0)
        CHECK(fwrite(buffer, 1, bytes, stdout) == bytes);
    CHECK(ferror(nr_out) == 0 && fclose(nr_out) == 0 && fflush(stdout) == 0);
    nr_out = NULL;
    mc_nbt_tree_free(tree);
    mc_entity_release(result);
    mc_entity_release(arrow);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    int32_t profiles[48][5];
    static const int32_t declared[48][5] = {
        {0, 0, 0, 0, 1},   {1, 0, 1, 0, 1},  {2, 0, 2, 0, 1},  {3, 0, 3, 0, 1},  {4, 0, 4, 0, 1},
        {5, 0, 5, 0, 1},   {6, 0, 6, 0, 1},  {7, 0, 0, 1, 1},  {8, 0, 0, 2, 1},  {9, 0, 0, 3, 1},
        {10, 0, 0, 4, 1},  {11, 0, 0, 5, 1}, {12, 0, 0, 6, 1}, {13, 0, 0, 7, 1}, {14, 0, 0, 8, 1},
        {15, 0, 0, 9, 1},  {16, 1, 0, 0, 1}, {17, 1, 1, 0, 1}, {18, 1, 2, 0, 1}, {19, 1, 3, 0, 1},
        {20, 1, 4, 0, 1},  {21, 1, 5, 0, 1}, {22, 1, 6, 0, 1}, {23, 1, 7, 0, 1}, {24, 2, 0, 0, 1},
        {25, 2, 1, 0, 1},  {26, 2, 2, 0, 1}, {27, 2, 3, 0, 1}, {28, 2, 4, 0, 1}, {29, 2, 5, 0, 1},
        {30, 2, 6, 0, 1},  {31, 2, 7, 0, 1}, {32, 2, 8, 0, 1}, {33, 2, 9, 0, 1}, {34, 3, 0, 0, 1},
        {35, 3, 1, 0, 1},  {36, 4, 0, 0, 1}, {37, 4, 1, 0, 1}, {38, 4, 0, 0, 0}, {39, 5, 0, 0, 1},
        {40, 5, 0, 0, 0},  {41, 5, 1, 0, 1}, {42, 5, 2, 0, 1}, {43, 5, 3, 0, 1}, {44, 0, 0, 10, 1},
        {45, 0, 0, 11, 1}, {46, 1, 0, 9, 1}, {47, 1, 0, 11, 1}};
    FILE *input = fopen(argv[3], "rb");
    if (input == NULL)
        return 1;
    char bytes[8193];
    const size_t size = fread(bytes, 1, sizeof(bytes), input);
    const bool complete = size < sizeof(bytes) && ferror(input) == 0;
    const int closed = fclose(input);
    if (!complete || closed != 0)
        return 1;
    bytes[size] = '\0';
    char *cursor = bytes;
    for (size_t row = 0; row < 48; ++row) {
        for (size_t column = 0; column < 5; ++column) {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
                ++cursor;
            if (!((*cursor >= '0' && *cursor <= '9') || *cursor == '-'))
                return 1;
            errno = 0;
            char *end = NULL;
            const intmax_t value = strtoimax(cursor, &end, 10);
            if (end == cursor || errno != 0 || value < INT32_MIN || value > INT32_MAX ||
                !(*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' || *end == '\0') ||
                value != declared[row][column])
                return 1;
            profiles[row][column] = (int32_t)value;
            cursor = end;
        }
    }
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
        ++cursor;
    if (cursor != bytes + size)
        return 1;
    arrow_nbt_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !an_setup(f, argv[1], "arrow-nbt-reference") ||
        !ac_prepare_shooters(&f->construction))
        return 1;
    ar_world_a = f->construction.base.world;
    for (size_t row = 0; row < 44; ++row)
        if (!nr_row(f, profiles[row]))
            return 1;
    if (!an_finish(f))
        return 1;
    free(f);
    return printf("Arrow NBT reference: %zu checks;44 genuine profiles\n", checks) < 0 ||
                   fflush(stdout) != 0
               ? 1
               : 0;
}
