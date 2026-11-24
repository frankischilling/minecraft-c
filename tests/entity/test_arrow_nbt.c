/* Finite native semantic/persistence controls on genuine Arrow storage.
 * Java Map-null payloads and mutable writer/OOM prefixes have no authored
 * native surrogate here. Construction clocks remain actual independent clocks. */
#define MC_ARROW_CONSTRUCTION_MAIN mc_arrow_nbt_unused_construction_main
#include "test_arrow_construction.c"
#undef MC_ARROW_CONSTRUCTION_MAIN

#include "entity/arrow/nbt_internal.h"
#include "mc/entity/nbt/storage.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/saving/saving.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/storage/objects_internal.h"

typedef struct arrow_nbt_input {
    mc_nbt_tag root, fields[17], pos[3], motion[3], rotation[2];
    const mc_nbt_tag *items[17], *pos_items[3], *motion_items[3], *rotation_items[2];
    uint16_t names[17][20];
} arrow_nbt_input;

typedef struct arrow_nbt_fixture {
    arrow_fixture construction;
    mc_chunk_provider_fields provider;
    mc_chunk_state *loaded;
    mc_entity *loaded_arrow; /* Borrowed from the actual adopted Chunk/World. */
} arrow_nbt_fixture;

static mc_nbt_string an_ascii(const char *text, uint16_t *storage) {
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        storage[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){storage, length};
}

static uint32_t an_fbits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const mc_nbt_tag *an_field(const mc_nbt_tag *root, const char *key) {
    uint16_t units[32];
    if (strlen(key) > sizeof(units) / sizeof(*units))
        return NULL;
    const mc_nbt_string name = an_ascii(key, units);
    return mc_nbt_compound_get(root, name.units, name.length);
}

/* Reusable authored readable views. No parser, factory or source observation is
 * substituted by these inputs. semantic fields[0..14], id[15], unknown[16]. */
static void an_input_init(arrow_nbt_input *in, bool named, bool unknown) {
    static const char *const names[] = {"Pos",    "Motion",   "Rotation", "FallDistance", "Fire",
                                        "Air",    "OnGround", "xTile",    "yTile",        "zTile",
                                        "inTile", "inData",   "shake",    "inGround",     "player",
                                        "id",     "Shooter"};
    static const uint16_t arrow_name[] = {'A', 'r', 'r', 'o', 'w'};
    static const uint16_t root_name[] = {'S', 'a', 'v', 'e', 'd', 'A', 'r', 'r', 'o', 'w'};
    memset(in, 0, sizeof(*in));
    in->root.type = MC_NBT_COMPOUND;
    in->root.name = (mc_nbt_string){root_name, sizeof(root_name) / sizeof(*root_name)};
    in->root.value.compound.items = in->items;
    in->root.value.compound.length = 15;
    for (size_t i = 0; i < 17; ++i) {
        in->fields[i].name = an_ascii(names[i], in->names[i]);
        in->items[i] = in->fields + i;
    }
    const double position[] = {8.5, 64, 8.5}, motion[] = {.5, -0.0, -.25};
    const float rotation[] = {450, -725};
    for (size_t i = 0; i < 3; ++i) {
        in->pos[i].type = in->motion[i].type = MC_NBT_DOUBLE;
        in->pos[i].value.double_bits = ac_dbits(position[i]);
        in->motion[i].value.double_bits = ac_dbits(motion[i]);
        in->pos_items[i] = in->pos + i;
        in->motion_items[i] = in->motion + i;
    }
    for (size_t i = 0; i < 2; ++i) {
        in->rotation[i].type = MC_NBT_FLOAT;
        in->rotation[i].value.float_bits = an_fbits(rotation[i]);
        in->rotation_items[i] = in->rotation + i;
    }
    in->fields[0].type = in->fields[1].type = in->fields[2].type = MC_NBT_LIST;
    in->fields[0].value.list.element_type = in->fields[1].value.list.element_type = MC_NBT_DOUBLE;
    in->fields[2].value.list.element_type = MC_NBT_FLOAT;
    in->fields[0].value.list.items = in->pos_items;
    in->fields[1].value.list.items = in->motion_items;
    in->fields[2].value.list.items = in->rotation_items;
    in->fields[0].value.list.length = in->fields[1].value.list.length = 3;
    in->fields[2].value.list.length = 2;
    in->fields[3].type = MC_NBT_FLOAT;
    in->fields[3].value.float_bits = an_fbits(1.25f);
    in->fields[4].type = in->fields[5].type = MC_NBT_SHORT;
    in->fields[4].value.short_value = -123;
    in->fields[5].value.short_value = 300;
    in->fields[6].type = MC_NBT_BYTE;
    in->fields[6].value.byte_value = 2;
    for (size_t i = 7; i < 10; ++i)
        in->fields[i].type = MC_NBT_SHORT;
    in->fields[7].value.short_value = INT16_MIN;
    in->fields[8].value.short_value = INT16_MAX;
    in->fields[9].value.short_value = -123;
    for (size_t i = 10; i < 15; ++i)
        in->fields[i].type = MC_NBT_BYTE;
    in->fields[10].value.byte_value = -1;
    in->fields[11].value.byte_value = INT8_MIN;
    in->fields[12].value.byte_value = INT8_MAX;
    in->fields[13].value.byte_value = 2;
    in->fields[14].value.byte_value = -1;
    in->fields[15].type = MC_NBT_STRING;
    in->fields[15].value.string = (mc_nbt_string){arrow_name, 5};
    in->fields[16].type = MC_NBT_LONG;
    in->fields[16].value.long_value = INT64_C(0x123456789abcdef);
    if (named)
        ++in->root.value.compound.length;
    if (unknown) {
        in->items[in->root.value.compound.length] = in->fields + 16;
        ++in->root.value.compound.length;
    }
}

/* Named seams reused by the independent native reference emitter. */
static bool an_setup(arrow_nbt_fixture *f, const char *resources, const char *directory) {
    CHECK(ac_setup(&f->construction, resources, directory) &&
          mc_chunk_provider_bind_fields(f->construction.base.world->fields.v, &f->provider, NULL) ==
              MC_WORLD_STORAGE_OK);
    return true;
}

static bool an_new_arrow(arrow_nbt_fixture *f, mc_entity **out) {
    CHECK(ac_allocate(&f->construction, out) &&
          mc_arrow_construct_world(*out, f->construction.base.world, NULL) == MC_ENTITY_OK);
    return true;
}

static bool an_finish(arrow_nbt_fixture *f) {
    mc_chunk_state *retiring[16];
    const size_t count = f->provider.cache->insertion_count;
    CHECK(count <= sizeof(retiring) / sizeof(*retiring));
    if (count != 0)
        memcpy(retiring, f->provider.cache->insertions, count * sizeof(*retiring));
    if (count != 0) {
        CHECK(mc_world_runtime_retire_entity_containers(f->construction.base.world, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK &&
              mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                                  f->provider.cache->capacity, f->provider.cache->insertions,
                                  f->provider.cache->insertion_capacity) &&
              mc_world_runtime_retire_world(f->construction.base.world, NULL) ==
                  MC_WORLD_STORAGE_OK);
        for (size_t i = 0; i < count; ++i) {
            bool duplicate = false;
            for (size_t j = 0; j < i; ++j)
                duplicate = duplicate || retiring[j] == retiring[i];
            if (!duplicate)
                CHECK(mc_world_store_retire_chunk(f->construction.base.store, retiring[i], NULL) ==
                      MC_WORLD_STORAGE_OK);
        }
    }
    f->loaded = NULL;
    f->loaded_arrow = NULL;
    CHECK(ac_finish(&f->construction));
    return true;
}

static bool an_own_values(const mc_arrow_fields *v) {
    CHECK(v->d == INT16_MIN && v->e == INT16_MAX && v->f == -123 && v->g == 255 && v->h == 128 &&
          v->b == 127 && !v->i && v->a);
    return true;
}

static void an_sentinels(mc_arrow_fields *v) {
    v->d = 1001;
    v->e = 1002;
    v->f = 1003;
    v->g = 1004;
    v->h = 1005;
    v->b = 1006;
    v->i = true;
    v->a = false;
}

static bool an_own_read(arrow_nbt_fixture *f) {
    mc_entity *a = NULL;
    arrow_nbt_input input;
    an_input_init(&input, false, false);
    CHECK(an_new_arrow(f, &a));
    const mc_java_random random = *a->random;
    mc_entity_nbt_error error = mc_entity_nbt_empty_error();
    CHECK(mc_arrow_nbt_read_held(a, &input.root, &error) && an_own_values(&a->arrow->fields) &&
          error.late_hook == MC_ENTITY_NBT_LATE_ARROW && error.arrow.returned &&
          error.arrow.site.pc == 101 && ac_random_equal(a->random, &random));
    const int8_t flags[] = {0, 1, 2, -1, INT8_MIN, INT8_MAX};
    for (size_t i = 0; i < sizeof(flags) / sizeof(*flags); ++i) {
        input.fields[13].value.byte_value = input.fields[14].value.byte_value = flags[i];
        CHECK(mc_arrow_nbt_read_held(a, &input.root, &error) &&
              a->arrow->fields.i == (flags[i] == 1) && a->arrow->fields.a == (flags[i] != 0));
    }
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    CHECK(mc_arrow_nbt_read_held(a, &empty, NULL) && a->arrow->fields.d == 0 &&
          a->arrow->fields.e == 0 && a->arrow->fields.f == 0 && a->arrow->fields.g == 0 &&
          a->arrow->fields.h == 0 && a->arrow->fields.b == 0 && !a->arrow->fields.i &&
          !a->arrow->fields.a && ac_random_equal(a->random, &random));
    mc_entity_release(a);
    return true;
}

static bool an_prefixes(arrow_nbt_fixture *f) {
    CHECK(ac_prepare_shooters(&f->construction));
    mc_entity *a = NULL;
    CHECK(an_new_arrow(f, &a) &&
          mc_arrow_shooter_store(a, f->construction.shooters[0], NULL) == MC_ENTITY_OK);
    const size_t held = f->construction.shooters[0]->holds;
    a->arrow->fields.j = 123456;
    a->arrow->fields.k = -789;
    const mc_java_random random = *a->random;
    static const int32_t pcs[] = {4, 14, 24, 34, 48, 62, 76, 95};
    for (size_t failed = 0; failed < 8; ++failed) {
        arrow_nbt_input input;
        an_input_init(&input, false, false);
        an_sentinels(&a->arrow->fields);
        input.fields[failed + 7].type = MC_NBT_INT;
        input.fields[failed + 7].value.int_value = 91;
        mc_entity_nbt_error error = mc_entity_nbt_empty_error();
        CHECK(!mc_arrow_nbt_read_held(a, &input.root, &error) &&
              error.status == MC_ENTITY_NBT_WRONG_TAG &&
              error.late_hook == MC_ENTITY_NBT_LATE_ARROW && !error.arrow.returned &&
              error.arrow.site.pc == pcs[failed] && error.arrow.child.pc == 25);
        const mc_arrow_fields *v = &a->arrow->fields;
        CHECK(v->d == (failed > 0 ? INT16_MIN : 1001) && v->e == (failed > 1 ? INT16_MAX : 1002) &&
              v->f == (failed > 2 ? -123 : 1003) && v->g == (failed > 3 ? 255 : 1004) &&
              v->h == (failed > 4 ? 128 : 1005) && v->b == (failed > 5 ? 127 : 1006) &&
              v->i == (failed <= 6) && !v->a && v->c == f->construction.shooters[0] &&
              a->arrow->shooter_hold == v->c && v->j == 123456 && v->k == -789 &&
              v->c->holds == held && ac_random_equal(a->random, &random));
    }
    mc_entity_nbt_error error = mc_entity_nbt_empty_error();
    an_sentinels(&a->arrow->fields);
    CHECK(!mc_arrow_nbt_read_held(a, NULL, &error) &&
          error.status == MC_ENTITY_NBT_NULL_REFERENCE && error.arrow.site.pc == 4 &&
          a->arrow->fields.d == 1001 && a->arrow->fields.j == 123456 &&
          f->construction.shooters[0]->holds == held);
    mc_entity_release(a);
    CHECK(f->construction.shooters[0]->holds == held - 1);
    return true;
}

static bool an_base_and_build(arrow_nbt_fixture *f) {
    mc_entity *a = NULL;
    arrow_nbt_input input;
    an_input_init(&input, false, false);
    input.motion[0].value.double_bits = ac_dbits(10.5);
    input.motion[1].value.double_bits = ac_dbits(-10.5);
    CHECK(an_new_arrow(f, &a));
    const mc_entity_bounds *bounds = a->bounds;
    const mc_java_random random = *a->random;
    mc_entity_nbt_error error;
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_entity_read_nbt(a, &input.root, &error) == MC_ENTITY_NBT_OK &&
          error.status == MC_ENTITY_NBT_OK && error.late_hook == MC_ENTITY_NBT_LATE_NONE &&
          a->base.x == 8.5 && a->base.y == 64 && a->base.z == 8.5 && a->base.bl == 8.5 &&
          a->base.bm == 64 && a->base.bn == 8.5 && a->base.aJ == 8.5 && a->base.aK == 64 &&
          a->base.aL == 8.5 && a->base.velocity_x == 0 && a->base.velocity_y == 0 &&
          a->base.velocity_z == -.25 && a->base.yaw == 90 && a->base.aT == -5 &&
          a->base.aU == 450 && a->base.aV == -725 && a->base.bk == 1.25f && a->base.bv == -123 &&
          a->base.bz == 300 && a->base.aX && a->bounds == bounds && a->bounds->min_x == 8.25 &&
          a->bounds->min_y == 64 && a->bounds->max_y == 64.5 && an_own_values(&a->arrow->fields) &&
          ac_random_equal(a->random, &random));
    mc_nbt_tree *tree = NULL;
    CHECK(mc_entity_build_nbt(a, NULL, &tree, &error) == MC_ENTITY_NBT_OK && tree != NULL);
    const mc_nbt_tag *root = mc_nbt_tree_root(tree);
    CHECK(root->value.compound.length == 15 && an_field(root, "id") == NULL &&
          an_field(root, "xTile")->type == MC_NBT_SHORT &&
          an_field(root, "xTile")->value.short_value == INT16_MIN &&
          an_field(root, "inTile")->type == MC_NBT_BYTE &&
          an_field(root, "inTile")->value.byte_value == -1 &&
          an_field(root, "inData")->value.byte_value == INT8_MIN &&
          an_field(root, "inGround")->value.byte_value == 0 &&
          an_field(root, "player")->value.byte_value == 1 && ac_random_equal(a->random, &random));
    mc_nbt_tree_free(tree);
    /* A late own fault follows the whole parent pose/bounds prefix. */
    input.fields[14].type = MC_NBT_INT;
    input.fields[14].value.int_value = 1;
    input.pos[0].value.double_bits = ac_dbits(12.5);
    CHECK(mc_entity_read_nbt(a, &input.root, &error) == MC_ENTITY_NBT_WRONG_TAG &&
          error.base_bytecode == 311 && error.arrow.site.pc == 95 && a->base.x == 12.5 &&
          a->bounds->min_x == 12.25 && !a->arrow->fields.i && a->arrow->fields.a);
    /* Initial named-list capture precedes every motion store. */
    an_input_init(&input, false, false);
    input.fields[2].type = MC_NBT_INT;
    input.fields[2].value.int_value = 0;
    a->base.velocity_x = 7;
    CHECK(mc_entity_read_nbt(a, &input.root, &error) == MC_ENTITY_NBT_WRONG_TAG &&
          error.base_bytecode == 17 && a->base.velocity_x == 7 && a->base.x == 12.5);
    mc_entity_release(a);
    return true;
}

static bool an_save_capacity(arrow_nbt_fixture *f) {
    mc_entity *a = NULL;
    CHECK(an_new_arrow(f, &a));
    a->arrow->fields.d = 98305; /* Java i2s -> -32767. */
    a->arrow->fields.g = 511;   /* Java i2b -> -1. */
    a->arrow->fields.h = -129;  /* Java i2b -> 127. */
    a->arrow->fields.b = -257;  /* Java i2b -> -1. */
    a->arrow->fields.i = a->arrow->fields.a = true;
    a->base.y = 63.25;
    a->base.y_size = .125f;
    mc_nbt_tree *tree = NULL;
    mc_entity_nbt_error error;
    mc_entity_nbt_error_init(&error);
    CHECK(mc_arrow_nbt_build_fields_held(a, NULL, &tree, &error) &&
          error.late_hook == MC_ENTITY_NBT_LATE_ARROW && error.arrow.site.pc == 95 &&
          error.arrow.returned && mc_nbt_tree_root(tree)->value.compound.length == 8 &&
          an_field(mc_nbt_tree_root(tree), "xTile")->value.short_value == -32767 &&
          an_field(mc_nbt_tree_root(tree), "inData")->value.byte_value == 127);
    mc_nbt_tree_free(tree);
    tree = NULL;
    bool serialized = false;
    CHECK(mc_entity_save_nbt(a, NULL, &serialized, &tree, &error) == MC_ENTITY_NBT_OK &&
          serialized && tree != NULL);
    const mc_nbt_tag *root = mc_nbt_tree_root(tree), *pos = an_field(root, "Pos");
    CHECK(root->value.compound.length == 16 && an_field(root, "id")->type == MC_NBT_STRING &&
          an_field(root, "id")->value.string.length == 5 &&
          an_field(root, "xTile")->value.short_value == -32767 &&
          an_field(root, "inTile")->value.byte_value == -1 &&
          an_field(root, "inData")->value.byte_value == 127 &&
          an_field(root, "shake")->value.byte_value == -1 &&
          an_field(root, "inGround")->value.byte_value == 1 &&
          an_field(root, "player")->value.byte_value == 1 &&
          pos->value.list.items[1]->value.double_bits == ac_dbits(63.375));
    mc_nbt_limits limits = mc_nbt_default_limits();
    mc_nbt_tree *out = tree;
    limits.max_elements = 15;
    serialized = false;
    CHECK(mc_entity_save_nbt(a, &limits, &serialized, &out, &error) != MC_ENTITY_NBT_OK &&
          out == tree && !serialized);
    mc_nbt_tree *direct = NULL;
    CHECK(mc_entity_build_nbt(a, &limits, &direct, &error) == MC_ENTITY_NBT_OK &&
          mc_nbt_tree_root(direct)->value.compound.length == 15);
    mc_nbt_tree_free(direct);
    limits.max_elements = 16;
    out = NULL;
    CHECK(mc_entity_save_nbt(a, &limits, &serialized, &out, &error) == MC_ENTITY_NBT_OK &&
          serialized && mc_nbt_tree_root(out)->value.compound.length == 16);
    mc_nbt_tree_free(out);
    a->base.be = true;
    limits.max_allocation_bytes = 0;
    serialized = true;
    out = tree;
    CHECK(mc_entity_save_nbt(a, &limits, &serialized, &out, &error) == MC_ENTITY_NBT_OK &&
          !serialized && out == tree);
    a->base.be = false;
    mc_nbt_tree_free(tree);
    mc_entity_release(a);
    return true;
}

static bool an_same_site(mc_arrow_site a, mc_arrow_site b) {
    return a.class_name == b.class_name && a.method == b.method && a.descriptor == b.descriptor &&
           a.pc == b.pc;
}

static bool an_same_cart_site(mc_minecart_site a, mc_minecart_site b) {
    return a.selected == b.selected && a.entered == b.entered &&
           a.declaring_class == b.declaring_class && a.method == b.method &&
           a.bytecode == b.bytecode;
}

static bool an_same_entity_error(const mc_entity_error *a, const mc_entity_error *b) {
    return a->status == b->status && a->operation == b->operation && a->bytecode == b->bytecode &&
           a->profile == b->profile && a->item == b->item && a->world == b->world &&
           an_same_cart_site(a->minecart_world_constructor, b->minecart_world_constructor) &&
           an_same_cart_site(a->minecart_constructor, b->minecart_constructor) &&
           an_same_cart_site(a->minecart_initializer, b->minecart_initializer) &&
           an_same_cart_site(a->minecart_initializer_cause, b->minecart_initializer_cause);
}

static bool an_same_position_error(const mc_arrow_position_error *a,
                                   const mc_arrow_position_error *b) {
    return a->status == b->status && a->operation == b->operation && a->bytecode == b->bytecode &&
           a->position_bytecode == b->position_bytecode && a->source_null == b->source_null &&
           a->native_policy == b->native_policy;
}

static bool an_same_arrow_error(const mc_arrow_error *a, const mc_arrow_error *b) {
    if (a->status != b->status || a->operation != b->operation ||
        a->exception_class != b->exception_class || a->native_policy != b->native_policy ||
        a->source_exception != b->source_exception || !an_same_site(a->site, b->site) ||
        !an_same_site(a->child, b->child) || !an_same_site(a->heading_site, b->heading_site) ||
        !an_same_entity_error(&a->entity, &b->entity) || a->math.status != b->math.status ||
        a->math.operation != b->math.operation || a->math.source_pc != b->math.source_pc ||
        a->math.initial_cause != b->math.initial_cause ||
        !an_same_position_error(&a->pose, &b->pose) ||
        !an_same_position_error(&a->position, &b->position) ||
        a->heading_entered != b->heading_entered || a->heading_returned != b->heading_returned ||
        a->gaussian_calls != b->gaussian_calls || a->random_captured != b->random_captured ||
        !ac_random_equal(&a->random_before, &b->random_before) ||
        !ac_random_equal(&a->random_after, &b->random_after))
        return false;
    for (size_t i = 0; i < 3; ++i)
        if (ac_dbits(a->gaussian_values[i]) != ac_dbits(b->gaussian_values[i]))
            return false;
    return true;
}

static bool an_selectors(arrow_nbt_fixture *f) {
    mc_entity *raw = NULL;
    mc_arrow_error call, copy;
    CHECK(ac_allocate(&f->construction, &raw) &&
          mc_arrow_construct_living(raw, NULL, NULL, NULL, &call) == MC_ENTITY_STATE_ERROR &&
          call.source_exception && call.site.pc == 73);
    mc_entity_nbt_error selected = mc_entity_nbt_empty_error();
    selected.status = MC_ENTITY_NBT_ENTITY_FAILED;
    selected.operation = "authored passive selector control";
    selected.field = "caller field";
    selected.factory_bytecode = 45;
    selected.base_bytecode = 311;
    selected.index = 3;
    CHECK(mc_entity_nbt_select_arrow_call_detail(&selected, &call) &&
          selected.late_hook == MC_ENTITY_NBT_LATE_ARROW_CALL &&
          selected.status == MC_ENTITY_NBT_ENTITY_FAILED && selected.factory_bytecode == 45 &&
          selected.base_bytecode == 311 && selected.index == 3 &&
          mc_entity_nbt_arrow_call_error(&selected, &copy) && an_same_arrow_error(&copy, &call));
    mc_entity_nbt_select_arrow_detail(&selected);
    CHECK(selected.late_hook == MC_ENTITY_NBT_LATE_ARROW && selected.arrow.site.pc == -1 &&
          selected.arrow.child.pc == -1 && !selected.arrow.returned &&
          an_same_entity_error(&selected.construction, &call.entity) &&
          selected.stack.status == MC_ITEM_STACK_OK && selected.nbt.status == MC_NBT_OK &&
          selected.registry.status == MC_ENTITY_REGISTRY_OK && selected.factory_bytecode == 45 &&
          selected.base_bytecode == 311 && selected.index == 3 &&
          strcmp(selected.field, "caller field") == 0);
    copy = call;
    CHECK(!mc_entity_nbt_arrow_call_error(&selected, &copy) && an_same_arrow_error(&copy, &call));
    selected = mc_entity_nbt_empty_error();
    CHECK(!mc_entity_nbt_select_arrow_call_detail(NULL, &call) &&
          !mc_entity_nbt_select_arrow_call_detail(&selected, NULL) &&
          !mc_entity_nbt_arrow_call_error(NULL, &copy) &&
          !mc_entity_nbt_arrow_call_error(&selected, NULL));
    mc_entity_release(raw);
    return true;
}

static bool an_factory(arrow_nbt_fixture *f) {
    arrow_nbt_input input;
    an_input_init(&input, true, true);
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->construction.base.app.entities, &before) ==
          MC_ENTITY_OK);
    const uint64_t unique = atomic_load(&f->construction.base.app.profile->uniquifier);
    mc_entity *a = NULL;
    mc_entity_nbt_error error;
    CHECK(mc_entity_factory_nbt(f->construction.base.app.entities, f->construction.base.world,
                                &input.root, &a, &error) == MC_ENTITY_NBT_OK &&
          a != NULL && mc_entity_has_arrow(a) && a->phase == MC_ENTITY_READY && a->holds == 1 &&
          a->arrow->constructor_pc == 58 && a->arrow->fields.c == NULL &&
          a->arrow->shooter_hold == NULL && a->arrow->fields.j == 0 && a->arrow->fields.k == 0 &&
          a->storage_source == NULL && a->base.world == f->construction.base.world &&
          a->base.x == 8.5 && an_own_values(&a->arrow->fields) &&
          atomic_load(&f->construction.base.app.profile->uniquifier) ==
              unique * UINT64_C(1181783497276652981));
    CHECK(mc_entity_classes_get_snapshot(f->construction.base.app.entities, &after) ==
              MC_ENTITY_OK &&
          after.live_entities == before.live_entities + 1 && after.next_id == before.next_id + 1);
    const mc_java_random first_random = *a->random;
    mc_entity *b = NULL;
    CHECK(mc_entity_factory_nbt(f->construction.base.app.entities, NULL, &input.root, &b, NULL) ==
              MC_ENTITY_NBT_OK &&
          b != NULL && b != a && b->random != a->random && b->base.world == NULL &&
          b->arrow->fields.c == NULL && b->arrow->fields.j == 0 && b->arrow->fields.k == 0 &&
          ac_random_equal(a->random, &first_random));
    mc_entity_release(b);
    CHECK(mc_entity_classes_get_snapshot(f->construction.base.app.entities, &before) ==
          MC_ENTITY_OK);
    input.fields[14].type = MC_NBT_SHORT;
    input.fields[14].value.short_value = 3;
    b = a;
    CHECK(mc_entity_factory_nbt(f->construction.base.app.entities, f->construction.base.world,
                                &input.root, &b, &error) == MC_ENTITY_NBT_WRONG_TAG &&
          b == a && error.factory_bytecode == 66 && error.base_bytecode == 311 &&
          error.arrow.site.pc == 95 && error.arrow.child.pc == 25 &&
          mc_entity_classes_get_snapshot(f->construction.base.app.entities, &after) ==
              MC_ENTITY_OK &&
          after.live_entities == before.live_entities && after.next_id == before.next_id + 1 &&
          after.allocation_bytes == before.allocation_bytes &&
          ac_random_equal(a->random, &first_random));
    mc_entity_release(a);
    return an_selectors(f);
}

static bool an_storage(arrow_nbt_fixture *f) {
    arrow_nbt_input input;
    an_input_init(&input, true, true);
    mc_entity *a = NULL;
    CHECK(an_new_arrow(f, &a) && mc_entity_read_nbt(a, &input.root, NULL) == MC_ENTITY_NBT_OK &&
          mc_arrow_shooter_store(a, f->construction.shooters[1], NULL) == MC_ENTITY_OK);
    a->arrow->fields.j = 91;
    a->arrow->fields.k = -92;
    const size_t holds = f->construction.shooters[1]->holds;
    const mc_java_random random = *a->random;
    mc_entity_nbt_error error;
    size_t direct = 0, retained = 0;
    CHECK(mc_entity_read_nbt(a, &input.root, NULL) == MC_ENTITY_NBT_OK &&
          a->arrow->fields.c == f->construction.shooters[1] &&
          a->arrow->shooter_hold == a->arrow->fields.c && a->arrow->fields.c->holds == holds &&
          a->arrow->fields.j == 91 && a->arrow->fields.k == -92 &&
          mc_entity_storage_native_bytes(a, &direct, &error) == MC_ENTITY_NBT_OK &&
          direct == mc_entity_native_bytes(a) &&
          mc_entity_preserve_source(a, &input.root, NULL, &error) == MC_ENTITY_NBT_OK &&
          mc_entity_storage_native_bytes(a, &retained, &error) == MC_ENTITY_NBT_OK &&
          retained > direct);
    const mc_nbt_tag *old = mc_entity_storage_source(a);
    CHECK(old != NULL && old != &input.root && old->name.length == input.root.name.length &&
          old->value.compound.length == 17 &&
          an_field(old, "Shooter")->value.long_value == INT64_C(0x123456789abcdef));
    input.fields[16].value.long_value = -1;
    CHECK(an_field(old, "Shooter")->value.long_value == INT64_C(0x123456789abcdef));
    a->arrow->fields.d = -12;
    a->arrow->fields.g = 300;
    a->arrow->fields.i = true;
    mc_nbt_tree *tree = NULL;
    bool serialized = false;
    CHECK(mc_entity_save_storage_nbt(a, NULL, &serialized, &tree, &error) == MC_ENTITY_NBT_OK &&
          serialized && tree != NULL);
    const mc_nbt_tag *root = mc_nbt_tree_root(tree);
    CHECK(root->value.compound.length == 17 && root->name.length == old->name.length &&
          an_field(root, "Shooter")->value.long_value == INT64_C(0x123456789abcdef) &&
          an_field(root, "xTile")->value.short_value == -12 &&
          an_field(root, "inTile")->value.byte_value == 44 &&
          an_field(root, "inGround")->value.byte_value == 1 &&
          root->value.compound.items[16] == an_field(root, "Shooter") &&
          mc_entity_storage_source(a) == old && a->arrow->fields.c == f->construction.shooters[1] &&
          a->arrow->shooter_hold == a->arrow->fields.c && a->arrow->fields.j == 91 &&
          a->arrow->fields.k == -92 && a->arrow->fields.c->holds == holds &&
          ac_random_equal(a->random, &random));
    mc_nbt_tree *fresh = NULL;
    CHECK(mc_entity_save_nbt(a, NULL, &serialized, &fresh, NULL) == MC_ENTITY_NBT_OK &&
          serialized && mc_nbt_tree_root(fresh)->value.compound.length == 16 &&
          an_field(mc_nbt_tree_root(fresh), "Shooter") == NULL);
    mc_nbt_tree_free(fresh);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = retained;
    CHECK(mc_entity_preserve_source(a, &input.root, &limits, &error) != MC_ENTITY_NBT_OK &&
          mc_entity_storage_source(a) == old &&
          an_field(old, "Shooter")->value.long_value == INT64_C(0x123456789abcdef));
    mc_nbt_tree *out = tree;
    serialized = false;
    CHECK(mc_entity_save_storage_nbt(a, &limits, &serialized, &out, &error) != MC_ENTITY_NBT_OK &&
          !serialized && out == tree && mc_entity_storage_source(a) == old);
    /* Replacing from a view borrowed from the old source is explicitly supported. */
    CHECK(mc_entity_preserve_source(a, old, NULL, NULL) == MC_ENTITY_NBT_OK &&
          mc_entity_storage_source(a) != old &&
          an_field(mc_entity_storage_source(a), "Shooter")->value.long_value ==
              INT64_C(0x123456789abcdef));
    a->base.be = true;
    limits.max_allocation_bytes = 0;
    serialized = true;
    CHECK(mc_entity_save_storage_nbt(a, &limits, &serialized, &out, NULL) == MC_ENTITY_NBT_OK &&
          !serialized && out == tree && a->arrow->fields.c->holds == holds &&
          ac_random_equal(a->random, &random));
    a->base.be = false;
    mc_nbt_tree_free(tree);
    mc_entity_release(a);
    CHECK(f->construction.shooters[1]->holds == holds - 1);
    return true;
}

/* Author a complete actual region record, then use the real GAME coordinator,
 * concrete factory, Chunk adoption, provenance copy and activation. */
static bool an_load_scene(arrow_nbt_fixture *f) {
    fixture *base = &f->construction.base;
    static const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    static const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                        MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                        MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                        MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    const size_t array_bytes =
        MC_CHUNK_BLOCKS + 3u * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES;
    uint8_t *arrays = calloc(1, array_bytes);
    CHECK(arrays != NULL && f->provider.cache->insertion_count == 0);
    mc_nbt_tag root = {.type = MC_NBT_COMPOUND}, level = {.type = MC_NBT_COMPOUND};
    mc_nbt_tag fields[11] = {0};
    const mc_nbt_tag *root_items[] = {&level}, *level_items[11];
    uint16_t encoded[12][20];
    level.name = an_ascii("Level", encoded[11]);
    root.value.compound.items = root_items;
    root.value.compound.length = 1;
    level.value.compound.items = level_items;
    level.value.compound.length = 11;
    for (size_t i = 0; i < 11; ++i) {
        fields[i].type = types[i];
        fields[i].name = an_ascii(names[i], encoded[i]);
        level_items[i] = fields + i;
    }
    size_t offset = 0;
    for (size_t i = 0; i < 5; ++i) {
        fields[i + 3].value.byte_array.data = arrays + offset;
        fields[i + 3].value.byte_array.length = lengths[i];
        offset += lengths[i];
    }
    for (size_t i = 0; i < 256; ++i)
        arrays[i * 128u + 63u] = 1;
    memset(arrays + array_bytes - MC_CHUNK_HEIGHTMAP_BYTES, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    fields[8].value.byte_value = 1;
    arrow_nbt_input input;
    an_input_init(&input, true, true);
    const mc_nbt_tag *entities[] = {&input.root};
    fields[9].value.list.element_type = fields[10].value.list.element_type = MC_NBT_COMPOUND;
    fields[9].value.list.items = entities;
    fields[9].value.list.length = 1;
    size_t length = 0, written = 0;
    CHECK(mc_nbt_measure(&root, NULL, &length, NULL) == MC_NBT_OK && length > 0);
    uint8_t *wire = malloc(length);
    uint32_t growth = 0;
    CHECK(wire != NULL && mc_nbt_write(&root, NULL, wire, length, &written, NULL) == MC_NBT_OK &&
          written == length &&
          mc_world_regions_write(base->regions, mc_world_store_root(base->store), 0, 0, wire,
                                 length, 73, &growth, NULL) == MC_WORLD_STORAGE_OK);
    free(wire);
    free(arrays);
    CHECK(mc_world_runtime_get(base->world, 0, 0, &f->loaded) == MC_CHUNK_ACQUIRE_OK &&
          f->loaded != NULL && f->loaded->active && !f->loaded->blank && f->loaded->has_entities &&
          f->provider.cache->insertion_count == 1);
    mc_world_chunk *chunk = mc_world_store_chunk_object(base->store, f->loaded);
    mc_world_chunk_fields actual;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &actual) == MC_WORLD_STORAGE_OK &&
          actual.q && actual.d == base->world && actual.m != NULL);
    size_t found = 0;
    for (size_t slice = 0; slice < 8; ++slice) {
        const mc_world_chunk_entity_list *list = NULL;
        size_t count = 0;
        CHECK(mc_world_chunk_slice_at(actual.m, slice, &list) == MC_WORLD_STORAGE_OK &&
              mc_world_chunk_entity_count(list, &count) == MC_WORLD_STORAGE_OK);
        for (size_t i = 0; i < count; ++i) {
            mc_entity *entity = NULL;
            CHECK(mc_world_chunk_entity_at(list, i, &entity) == MC_WORLD_STORAGE_OK &&
                  entity != NULL && mc_entity_has_arrow(entity) && found == 0);
            f->loaded_arrow = entity;
            ++found;
        }
    }
    mc_entity *a = f->loaded_arrow;
    CHECK(found == 1 && a != NULL && a->phase == MC_ENTITY_READY && a->base.world == base->world &&
          a->base.bF && a->base.bG == 0 && a->base.bH == 4 && a->base.bI == 0 &&
          a->arrow->constructor_pc == 58 && a->arrow->fields.c == NULL &&
          a->arrow->shooter_hold == NULL && a->arrow->fields.j == 0 && a->arrow->fields.k == 0 &&
          an_own_values(&a->arrow->fields) && a->holds > 0 && mc_entity_storage_source(a) != NULL &&
          an_field(mc_entity_storage_source(a), "Shooter")->value.long_value ==
              INT64_C(0x123456789abcdef));
    return true;
}

static bool an_actual_save(arrow_nbt_fixture *f) {
    fixture *base = &f->construction.base;
    mc_entity *a = f->loaded_arrow;
    CHECK(a != NULL && f->loaded != NULL);
    const int32_t id = a->base.id;
    const mc_java_random random = *a->random;
    const mc_nbt_tag *source = mc_entity_storage_source(a);
    a->arrow->fields.d = -211;
    a->arrow->fields.g = 303;
    a->arrow->fields.b = 258;
    a->arrow->fields.i = true;
    a->arrow->fields.a = false;
    a->arrow->fields.j = 37;
    a->arrow->fields.k = 41;
    mc_world_game_save_error *error = calloc(1, sizeof(*error));
    uint32_t growth = 0;
    CHECK(error != NULL &&
          mc_world_runtime_write_game_chunk(base->world, f->loaded, 74, &growth, error) ==
              MC_WORLD_STORAGE_OK &&
          a->base.id == id && a->arrow->fields.j == 37 && a->arrow->fields.k == 41 &&
          a->arrow->fields.c == NULL && a->arrow->shooter_hold == NULL &&
          mc_entity_storage_source(a) == source && ac_random_equal(a->random, &random));
    free(error);
    mc_compression_buffer bytes = {0};
    mc_nbt_document *document = NULL;
    size_t consumed = 0;
    CHECK(mc_world_regions_read(base->regions, mc_world_store_root(base->store), 0, 0, &bytes,
                                NULL) == MC_WORLD_STORAGE_OK &&
          mc_nbt_read_compound(bytes.data, bytes.length, NULL, &document, &consumed, NULL) ==
              MC_NBT_OK &&
          consumed == bytes.length);
    const mc_nbt_tag *level = an_field(mc_nbt_document_root(document), "Level");
    const mc_nbt_tag *entities = an_field(level, "Entities");
    CHECK(entities != NULL && entities->type == MC_NBT_LIST && entities->value.list.length == 1);
    const mc_nbt_tag *saved = entities->value.list.items[0];
    CHECK(saved != NULL && saved->type == MC_NBT_COMPOUND && saved->value.compound.length == 17 &&
          an_field(saved, "id")->type == MC_NBT_STRING &&
          an_field(saved, "xTile")->value.short_value == -211 &&
          an_field(saved, "inTile")->value.byte_value == 47 &&
          an_field(saved, "shake")->value.byte_value == 2 &&
          an_field(saved, "inGround")->value.byte_value == 1 &&
          an_field(saved, "player")->value.byte_value == 0 &&
          an_field(saved, "Shooter")->type == MC_NBT_LONG &&
          an_field(saved, "Shooter")->value.long_value == INT64_C(0x123456789abcdef) &&
          an_field(saved, "j") == NULL && an_field(saved, "k") == NULL);
    mc_nbt_document_free(document);
    mc_compression_buffer_free(&bytes);
    return true;
}

#ifndef MC_ARROW_NBT_MAIN
#define MC_ARROW_NBT_MAIN main
#endif
int MC_ARROW_NBT_MAIN(int argc, char **argv);

int MC_ARROW_NBT_MAIN(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    arrow_nbt_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !an_setup(f, argv[1], "arrow-nbt"))
        return 1;
    static bool (*const cases[])(arrow_nbt_fixture *) = {
        an_own_read, an_prefixes, an_base_and_build, an_save_capacity,
        an_factory,  an_storage,  an_load_scene,     an_actual_save};
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i)
        if (!cases[i](f) || printf("ARROW_NBT_CASE id=%zu\n", i) < 0 || fflush(stdout) != 0)
            return 1;
    if (!an_finish(f))
        return 1;
    free(f);
    if (printf("Arrow NBT: %zu checks;8 native semantic and storage controls\n", checks) < 0 ||
        fflush(stdout) != 0)
        return 1;
    return 0;
}
