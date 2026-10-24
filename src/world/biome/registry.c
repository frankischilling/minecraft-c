#include "mc/world/biome/registry.h"

#include "../../block/registry/registry_internal.h"
#include "mc/core/java/java.h"

#include <float.h>
#include <limits.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Biome registry arithmetic requires the captured x86-64 profile"
#endif
#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
#error "Biome registry arithmetic requires strict floating-point compilation"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0,
               "Biome registry requires binary32/binary64 evaluation");
_Static_assert(sizeof(float) == 4 && sizeof(double) == 8, "Biome registry floating-point storage");

struct mc_biome_spawn_list {
    mc_registry_list data;
};

struct mc_biome_spawn {
    mc_biome_spawn_fields fields;
};

typedef enum biome_construction_phase {
    BIOME_RAW,
    BIOME_CONSTRUCTING,
    BIOME_CONSTRUCTED,
    BIOME_FAILED
} biome_construction_phase;

struct mc_biome {
    mc_registry_classes *classes;
    mc_biome_kind kind;
    biome_construction_phase phase;
    const char *constructor_class;
    int32_t constructor_pc;
    mc_biome_fields fields;
};

static const char *const client_classes[MC_BIOME_KIND_COUNT] = {"kd", "fs", "tf", "t",
                                                                "ry", "yj", "rb", "g"};
static const char *const server_classes[MC_BIOME_KIND_COUNT] = {"gs", "dw", "ma", "m",
                                                                "le", "pp", "kk", "f"};

static mc_registry_error native_error(mc_registry_status status, const char *message) {
    return (mc_registry_error){.status = status,
                               .message = message,
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_registry_status result(mc_registry_error *error, mc_registry_status status,
                                 const char *message) {
    if (error != NULL)
        *error = native_error(status, message);
    return status;
}

static void stage(mc_biome *biome, mc_biome_kind declaring_kind, int32_t pc) {
    biome->constructor_class = biome->classes->options.side == MC_REGISTRY_CLIENT
                                   ? client_classes[declaring_kind]
                                   : server_classes[declaring_kind];
    biome->constructor_pc = pc;
}

static mc_registry_status own_error(mc_biome *biome) {
    biome->classes->last_error.constructor_class = biome->constructor_class;
    biome->classes->last_error.constructor_pc = biome->constructor_pc;
    return biome->classes->last_error.status;
}

static mc_registry_status fail(mc_biome *biome, mc_registry_status status, const char *message) {
    (void)mc_registry_fail(biome->classes, status, message, NULL);
    return own_error(biome);
}

static mc_registry_status method_fail(mc_registry_classes *classes, int32_t pc,
                                      mc_registry_status status, const char *message) {
    (void)mc_registry_fail(classes, status, message, NULL);
    classes->last_error.constructor_class =
        classes->options.side == MC_REGISTRY_CLIENT ? "kd" : "gs";
    classes->last_error.constructor_pc = pc;
    return status;
}

static bool strict_fp(void) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u;
}

static mc_registry_status admitted(mc_registry_classes *classes) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Biome operation requires an admitted flow", NULL);
    return MC_REGISTRY_OK;
}

#define TRY(expression)                                                                            \
    do {                                                                                           \
        const mc_registry_status status_ = (expression);                                           \
        if (status_ != MC_REGISTRY_OK)                                                             \
            return status_;                                                                        \
    } while (0)

static mc_registry_status class_initialize(mc_registry_classes *classes, mc_biome_kind kind);

mc_registry_status mc_registry_biome_allocate(mc_registry_classes *classes, mc_biome_kind kind,
                                              mc_biome **out) {
    TRY(admitted(classes));
    if (out == NULL || (unsigned int)kind >= (unsigned int)MC_BIOME_KIND_COUNT)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Biome allocation arguments",
                                NULL);
    TRY(class_initialize(classes, kind));
    mc_biome *biome = mc_registry_allocate(classes, sizeof(*biome));
    if (biome == NULL)
        return classes->last_error.status;
    *biome = (mc_biome){.classes = classes, .kind = kind, .phase = BIOME_RAW, .constructor_pc = -1};
    *out = biome;
    return MC_REGISTRY_OK;
}

static mc_registry_status list_new(mc_biome *biome, int32_t allocation_pc, int32_t constructor_pc,
                                   int32_t store_pc, const mc_biome_spawn_list **destination) {
    stage(biome, MC_BIOME_KIND_KD_GS, allocation_pc);
    mc_biome_spawn_list *list = mc_registry_allocate(biome->classes, sizeof(*list));
    if (list == NULL)
        return own_error(biome);
    stage(biome, MC_BIOME_KIND_KD_GS, constructor_pc);
    *list = (mc_biome_spawn_list){0};
    stage(biome, MC_BIOME_KIND_KD_GS, store_pc);
    *destination = list;
    return MC_REGISTRY_OK;
}

static mc_registry_status list_add(mc_biome *biome, const mc_biome_spawn_list *receiver,
                                   mc_biome_spawn *spawn) {
    if (receiver == NULL)
        return fail(biome, MC_REGISTRY_NULL_REFERENCE, "Biome spawn List.add receiver");
    mc_registry_list *list = &((mc_biome_spawn_list *)receiver)->data;
    if (list->count == (size_t)INT32_MAX)
        return fail(biome, MC_REGISTRY_LIMIT, "Biome spawn list count");
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 10u : list->capacity;
        if (list->capacity != 0) {
            size_t growth = capacity / 2u;
            if (growth == 0)
                growth = 1;
            capacity =
                growth > (size_t)INT32_MAX - capacity ? (size_t)INT32_MAX : capacity + growth;
        }
        if (capacity > SIZE_MAX / sizeof(void *))
            return fail(biome, MC_REGISTRY_LIMIT, "Biome spawn list allocation size");
        void **elements = mc_registry_allocate(biome->classes, capacity * sizeof(*elements));
        if (elements == NULL)
            return own_error(biome);
        for (size_t i = 0; i < list->count; ++i)
            elements[i] = list->elements[i];
        list->elements = elements;
        list->capacity = capacity;
    }
    list->elements[list->count] = spawn;
    ++list->count;
    return MC_REGISTRY_OK;
}

static mc_registry_status list_clear(mc_biome *biome, const mc_biome_spawn_list *receiver) {
    if (receiver == NULL)
        return fail(biome, MC_REGISTRY_NULL_REFERENCE, "Biome spawn List.clear receiver");
    mc_registry_list *list = &((mc_biome_spawn_list *)receiver)->data;
    for (size_t i = 0; i < list->count; ++i)
        list->elements[i] = NULL;
    list->count = 0;
    return MC_REGISTRY_OK;
}

static const mc_biome_entity_class *resolve_class(mc_registry_classes *classes, size_t index) {
    static const char *const clients[13] = {"cn", "uz", "fr", "gb", "uw", "dl", "wh",
                                            "ww", "bx", "xt", "gi", "bp", "ya"};
    static const char *const servers[13] = {"bn", "ne", "dv", "ec", "nb", "ce", "oc",
                                            "or", "bd", "pf", "eh", "aw", "pi"};
    mc_biome_entity_class *identity = &classes->biomes.entity_classes[index];
    identity->client_name = clients[index];
    identity->server_name = servers[index];
    return identity;
}

static mc_registry_status append_spawn(mc_biome *biome, mc_biome_kind declaring_kind,
                                       const mc_biome_spawn_list *list, int32_t new_pc,
                                       int32_t class_pc, size_t entity_index,
                                       int32_t constructor_pc, int32_t add_pc, int32_t weight) {
    stage(biome, declaring_kind, new_pc);
    mc_biome_spawn *spawn = mc_registry_allocate(biome->classes, sizeof(*spawn));
    if (spawn == NULL)
        return own_error(biome);
    *spawn = (mc_biome_spawn){0};
    stage(biome, declaring_kind, class_pc);
    const mc_biome_entity_class *entity_class = resolve_class(biome->classes, entity_index);
    stage(biome, declaring_kind, constructor_pc);
    spawn->fields.a = entity_class;
    spawn->fields.b = weight;
    stage(biome, declaring_kind, add_pc);
    return list_add(biome, list, spawn);
}

static mc_registry_status base_construct(mc_biome *biome) {
    mc_block *block = NULL;
    stage(biome, MC_BIOME_KIND_KD_GS, 5);
    TRY(mc_registry_block_static_load(biome->classes, "v", &block));
    stage(biome, MC_BIOME_KIND_KD_GS, 8);
    if (block == NULL)
        return fail(biome, MC_REGISTRY_NULL_REFERENCE, "Biome Block.v.bn receiver");
    const int8_t top = mc_java_i8_narrow(block->fields.bn);
    stage(biome, MC_BIOME_KIND_KD_GS, 12);
    biome->fields.p = top;
    stage(biome, MC_BIOME_KIND_KD_GS, 16);
    TRY(mc_registry_block_static_load(biome->classes, "w", &block));
    stage(biome, MC_BIOME_KIND_KD_GS, 19);
    if (block == NULL)
        return fail(biome, MC_REGISTRY_NULL_REFERENCE, "Biome Block.w.bn receiver");
    const int8_t filler = mc_java_i8_narrow(block->fields.bn);
    stage(biome, MC_BIOME_KIND_KD_GS, 23);
    biome->fields.q = filler;
    stage(biome, MC_BIOME_KIND_KD_GS, 29);
    biome->fields.r = 5169201;
    TRY(list_new(biome, 33, 37, 40, &biome->fields.s));
    TRY(list_new(biome, 44, 48, 51, &biome->fields.t));
    TRY(list_new(biome, 55, 59, 62, &biome->fields.u));
    stage(biome, MC_BIOME_KIND_KD_GS, 67);
    biome->fields.w = true;
#define SPAWN(field, allocation, literal, index, constructor, append, weight)                      \
    TRY(append_spawn(biome, MC_BIOME_KIND_KD_GS, biome->fields.field, allocation, literal, index,  \
                     constructor, append, weight))
    SPAWN(s, 74, 78, 0u, 82, 85, 10);
    SPAWN(s, 95, 99, 1u, 103, 106, 10);
    SPAWN(s, 116, 120, 2u, 124, 127, 10);
    SPAWN(s, 137, 141, 3u, 145, 148, 10);
    SPAWN(s, 158, 162, 4u, 166, 169, 10);
    SPAWN(t, 179, 183, 5u, 187, 190, 12);
    SPAWN(t, 200, 204, 6u, 208, 211, 10);
    SPAWN(t, 221, 225, 7u, 229, 232, 10);
    SPAWN(t, 242, 246, 8u, 250, 253, 8);
    SPAWN(u, 263, 267, 9u, 271, 274, 10);
#undef SPAWN
    stage(biome, MC_BIOME_KIND_KD_GS, 280);
    return MC_REGISTRY_OK;
}

static mc_registry_status construct_body(mc_biome *biome) {
    TRY(base_construct(biome));
    switch (biome->kind) {
        case MC_BIOME_KIND_RB_KK:
        case MC_BIOME_KIND_G_F:
            TRY(append_spawn(biome, biome->kind, biome->fields.t, 8, 12, 10u, 15, 18, 2));
            stage(biome, biome->kind, 24);
            break;
        case MC_BIOME_KIND_T_M:
        case MC_BIOME_KIND_RY_LE:
            stage(biome, biome->kind, 8);
            TRY(list_clear(biome, biome->fields.s));
            stage(biome, biome->kind, 17);
            TRY(list_clear(biome, biome->fields.t));
            stage(biome, biome->kind, 26);
            TRY(list_clear(biome, biome->fields.u));
            if (biome->kind == MC_BIOME_KIND_T_M) {
                TRY(append_spawn(biome, biome->kind, biome->fields.s, 35, 39, 11u, 43, 46, 10));
                TRY(append_spawn(biome, biome->kind, biome->fields.s, 56, 60, 12u, 64, 67, 10));
                stage(biome, biome->kind, 73);
            } else {
                TRY(append_spawn(biome, biome->kind, biome->fields.t, 35, 39, 7u, 43, 46, 10));
                stage(biome, biome->kind, 52);
            }
            break;
        case MC_BIOME_KIND_KD_GS:
            break;
        default:
            stage(biome, biome->kind, 4);
            break;
    }
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_biome_construct(mc_biome *biome) {
    if (biome == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    mc_registry_classes *classes = biome->classes;
    TRY(admitted(classes));
    if (biome->phase != BIOME_RAW)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Biome constructor requires its RAW owner", NULL);
    biome->phase = BIOME_CONSTRUCTING;
    const mc_registry_status status = construct_body(biome);
    biome->phase = status == MC_REGISTRY_OK ? BIOME_CONSTRUCTED : BIOME_FAILED;
    return status;
}

mc_registry_status mc_registry_biome_byte_store(mc_registry_classes *classes, mc_biome *biome,
                                                char field, int8_t value) {
    TRY(admitted(classes));
    if (biome == NULL || biome->classes != classes || (field != 'p' && field != 'q'))
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Biome byte-store arguments",
                                NULL);
    if (field == 'p')
        biome->fields.p = value;
    else
        biome->fields.q = value;
    return MC_REGISTRY_OK;
}

static mc_registry_status classify(mc_registry_classes *classes, float temperature, float rainfall,
                                   const mc_biome **out) {
    TRY(mc_registry_biomes_active_use(classes));
    if (!strict_fp())
        return method_fail(classes, 2, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                           "Biome strict floating-point environment");
    volatile float moisture = rainfall * temperature;
    char field;
    if (temperature < 0.1f)
        field = 'k';
    else if (moisture < 0.2f) {
        if (temperature < 0.5f)
            field = 'k';
        else if (temperature < 0.95f)
            field = 'e';
        else
            field = 'h';
    } else if (moisture > 0.5f && temperature < 0.7f)
        field = 'b';
    else if (temperature < 0.5f)
        field = 'g';
    else if (temperature < 0.97f)
        field = moisture < 0.35f ? 'f' : 'd';
    else if (moisture < 0.45f)
        field = 'i';
    else
        field = moisture < 0.9f ? 'c' : 'a';
    *out = classes->biomes.named[(size_t)(field - 'a')];
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_biome_lookup(mc_registry_classes *classes, double temperature,
                                            double rainfall, const mc_biome **out) {
    TRY(admitted(classes));
    if (out == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Biome lookup output", NULL);
    TRY(mc_registry_biomes_active_use(classes));
    if (!strict_fp())
        return method_fail(classes, 4, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                           "Biome strict floating-point environment");
    volatile double product = temperature * 63.0;
    const int32_t t = mc_java_double_to_i32(product);
    product = rainfall * 63.0;
    const int32_t r = mc_java_double_to_i32(product);
    const mc_biome *const *array = classes->biomes.x;
    const int32_t index = mc_java_i32_add(t, mc_java_i32_mul(r, 64));
    if (array == NULL)
        return method_fail(classes, 27, MC_REGISTRY_NULL_REFERENCE, "Biome lookup array");
    if (index < 0 || (size_t)index >= classes->biomes.x_length)
        return method_fail(classes, 27, MC_REGISTRY_INDEX_OUT_OF_RANGE, "Biome lookup index");
    *out = array[(size_t)index];
    return MC_REGISTRY_OK;
}

static mc_registry_status sand_fields(mc_registry_classes *classes, size_t index, int32_t field_pc,
                                      int32_t q_pc, int32_t p_pc) {
    /* Both evaluated receivers stay captured across the real Block active use. */
    mc_biome *first = classes->biomes.named[index];
    mc_biome *second = classes->biomes.named[index];
    mc_block *sand = NULL;
    TRY(mc_registry_block_static_load(classes, "F", &sand));
    if (sand == NULL)
        return method_fail(classes, field_pc, MC_REGISTRY_NULL_REFERENCE,
                           "Biome Block.F.bn receiver");
    const int8_t value = mc_java_i8_narrow(sand->fields.bn);
    if (second == NULL)
        return method_fail(classes, q_pc, MC_REGISTRY_NULL_REFERENCE, "Biome sand q receiver");
    second->fields.q = value;
    if (first == NULL)
        return method_fail(classes, p_pc, MC_REGISTRY_NULL_REFERENCE, "Biome sand p receiver");
    first->fields.p = value;
    return MC_REGISTRY_OK;
}

static mc_registry_status table_body(mc_registry_classes *classes) {
    for (int32_t t = 0; t < 64; ++t) {
        for (int32_t r = 0; r < 64; ++r) {
            const mc_biome **array = classes->biomes.x;
            const size_t index = (size_t)t + (size_t)r * 64u;
            if (!strict_fp())
                return method_fail(classes, 29, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                   "Biome strict floating-point environment");
            volatile float temperature = (float)t / 63.0f;
            volatile float rainfall = (float)r / 63.0f;
            const mc_biome *value = NULL;
            TRY(classify(classes, temperature, rainfall, &value));
            if (array == NULL)
                return method_fail(classes, 38, MC_REGISTRY_NULL_REFERENCE, "Biome table array");
            if (index >= classes->biomes.x_length)
                return method_fail(classes, 38, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                                   "Biome table index");
            array[index] = value;
        }
    }
    /* h51/h54/F57, then j71/j74/F77; q is stored before p. */
    TRY(sand_fields(classes, 7u, 60, 65, 68));
    TRY(sand_fields(classes, 9u, 80, 85, 88));
    return MC_REGISTRY_OK;
}

static mc_registry_status initialize_body(mc_registry_classes *classes) {
    mc_biome *value = NULL;
    const mc_registry_string *name = NULL;
#define AT(pc) mc_registry_initializer_stage(classes, (pc))
#define NEW(kind, allocation_pc, constructor_pc)                                                   \
    do {                                                                                           \
        AT(allocation_pc);                                                                         \
        TRY(mc_registry_biome_allocate(classes, kind, &value));                                    \
        AT(constructor_pc);                                                                        \
        TRY(mc_registry_biome_construct(value));                                                   \
    } while (0)
#define O(number, literal_pc, call_pc)                                                             \
    do {                                                                                           \
        AT(literal_pc);                                                                            \
        const int32_t argument_ = (number);                                                        \
        AT(call_pc);                                                                               \
        value->fields.o = argument_;                                                               \
    } while (0)
#define R(number, literal_pc, call_pc)                                                             \
    do {                                                                                           \
        AT(literal_pc);                                                                            \
        const int32_t argument_ = (number);                                                        \
        AT(call_pc);                                                                               \
        value->fields.r = argument_;                                                               \
    } while (0)
#define N(text, literal_pc, call_pc)                                                               \
    do {                                                                                           \
        AT(literal_pc);                                                                            \
        TRY(mc_registry_literal_ascii(classes, (text), &name));                                    \
        AT(call_pc);                                                                               \
        value->fields.n = name;                                                                    \
    } while (0)
#define V(pc)                                                                                      \
    do {                                                                                           \
        AT(pc);                                                                                    \
        value->fields.v = true;                                                                    \
    } while (0)
#define W(pc)                                                                                      \
    do {                                                                                           \
        AT(pc);                                                                                    \
        value->fields.w = false;                                                                   \
    } while (0)
#define P(index, pc)                                                                               \
    do {                                                                                           \
        AT(pc);                                                                                    \
        classes->biomes.named[index] = value;                                                      \
    } while (0)
    NEW(MC_BIOME_KIND_YJ_PP, 0, 4);
    O(588342, 7, 9);
    N("Rainforest", 12, 14);
    R(2094168, 17, 19);
    P(0, 22);
    NEW(MC_BIOME_KIND_TF_MA, 25, 29);
    O(522674, 32, 34);
    N("Swampland", 37, 39);
    R(9154376, 42, 44);
    P(1, 47);
    NEW(MC_BIOME_KIND_KD_GS, 50, 54);
    O(10215459, 57, 59);
    N("Seasonal Forest", 62, 64);
    P(2, 67);
    NEW(MC_BIOME_KIND_RB_KK, 70, 74);
    O(353825, 77, 79);
    N("Forest", 82, 84);
    R(5159473, 87, 89);
    P(3, 92);
    NEW(MC_BIOME_KIND_FS_DW, 95, 99);
    O(14278691, 102, 104);
    N("Savanna", 107, 109);
    P(4, 112);
    NEW(MC_BIOME_KIND_KD_GS, 115, 119);
    O(10595616, 122, 124);
    N("Shrubland", 127, 129);
    P(5, 132);
    NEW(MC_BIOME_KIND_G_F, 135, 139);
    O(3060051, 142, 144);
    N("Taiga", 147, 149);
    V(152);
    R(8107825, 155, 157);
    P(6, 160);
    NEW(MC_BIOME_KIND_FS_DW, 163, 167);
    O(16421912, 170, 172);
    N("Desert", 175, 177);
    W(180);
    P(7, 183);
    NEW(MC_BIOME_KIND_FS_DW, 186, 190);
    O(16767248, 193, 195);
    N("Plains", 198, 200);
    P(8, 203);
    NEW(MC_BIOME_KIND_FS_DW, 206, 210);
    O(16772499, 213, 215);
    N("Ice Desert", 218, 220);
    V(223);
    W(226);
    R(12899129, 229, 231);
    P(9, 234);
    NEW(MC_BIOME_KIND_KD_GS, 237, 241);
    O(5762041, 244, 246);
    N("Tundra", 249, 251);
    V(254);
    R(12899129, 257, 259);
    P(10, 262);
    NEW(MC_BIOME_KIND_T_M, 265, 269);
    O(16711680, 272, 274);
    N("Hell", 277, 279);
    W(282);
    P(11, 285);
    NEW(MC_BIOME_KIND_RY_LE, 288, 292);
    O(8421631, 295, 297);
    N("Sky", 300, 302);
    W(305);
    P(12, 308);
    AT(314);
    const mc_biome **array = mc_registry_allocate(classes, 4096u * sizeof(*array));
    if (array == NULL)
        return classes->last_error.status;
    AT(317);
    classes->biomes.x = array;
    classes->biomes.x_length = 4096u;
    AT(320);
    TRY(table_body(classes));
    AT(323);
#undef P
#undef W
#undef V
#undef N
#undef R
#undef O
#undef NEW
#undef AT
    return MC_REGISTRY_OK;
}

static mc_registry_status class_initialize(mc_registry_classes *classes, mc_biome_kind kind) {
    if ((unsigned int)kind >= (unsigned int)MC_BIOME_KIND_COUNT)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Biome class kind", NULL);
    mc_registry_class_state *state = &classes->biome_states[kind];
    if (state->phase == MC_REGISTRY_READY || state->phase == MC_REGISTRY_INITIALIZING)
        return MC_REGISTRY_OK;
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = -1;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, client_classes[kind], server_classes[kind],
                                  &state->pc);
    const mc_registry_status status = kind == MC_BIOME_KIND_KD_GS
                                          ? initialize_body(classes)
                                          : class_initialize(classes, MC_BIOME_KIND_KD_GS);
    state->phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    if (status != MC_REGISTRY_OK)
        state->failure = classes->last_error;
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_biomes_active_use(mc_registry_classes *classes) {
    TRY(admitted(classes));
    return class_initialize(classes, MC_BIOME_KIND_KD_GS);
}

mc_registry_status mc_registry_biome_static_load(mc_registry_classes *classes, char field,
                                                 const mc_biome **out) {
    TRY(admitted(classes));
    if (out == NULL || field < 'a' || field > 'm')
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Biome static operand",
                                NULL);
    TRY(mc_registry_biomes_active_use(classes));
    *out = classes->biomes.named[(size_t)(field - 'a')];
    return MC_REGISTRY_OK;
}

static mc_registry_status public_enter(mc_registry_classes *classes, mc_registry_error *error) {
    if (classes == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Biome class owner");
    if (classes->entered != 0)
        return result(error, MC_REGISTRY_BUSY, "Registry flow entered");
    ++classes->entered;
    return MC_REGISTRY_OK;
}

static mc_registry_status public_leave(mc_registry_classes *classes, mc_registry_status status,
                                       mc_registry_error *error) {
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return result(error, status, NULL);
    if (error != NULL) {
        *error = classes->last_error;
        error->status = status;
    }
    return status;
}

mc_registry_status mc_registry_classes_initialize_biomes(mc_registry_classes *classes,
                                                         mc_registry_error *error) {
    TRY(public_enter(classes, error));
    const mc_registry_status status = mc_registry_biomes_active_use(classes);
    return public_leave(classes, status, error);
}

mc_registry_status mc_biome_registry_inspect(const mc_registry_classes *classes,
                                             mc_biome_registry_view *out,
                                             mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Biome inspect arguments");
    const mc_registry_biome_storage *b = &classes->biomes;
    *out = (mc_biome_registry_view){.phase = classes->biome_states[MC_BIOME_KIND_KD_GS].phase,
                                    .initializer_pc = classes->biome_states[MC_BIOME_KIND_KD_GS].pc,
                                    .a = b->named[0],
                                    .b = b->named[1],
                                    .c = b->named[2],
                                    .d = b->named[3],
                                    .e = b->named[4],
                                    .f = b->named[5],
                                    .g = b->named[6],
                                    .h = b->named[7],
                                    .i = b->named[8],
                                    .j = b->named[9],
                                    .k = b->named[10],
                                    .l = b->named[11],
                                    .m = b->named[12],
                                    .x = b->x,
                                    .x_length = b->x_length};
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_biome_static_get(mc_registry_classes *classes, char field,
                                       const mc_biome **out, mc_registry_error *error) {
    if (out == NULL || field < 'a' || field > 'm')
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Biome static arguments");
    TRY(public_enter(classes, error));
    const mc_registry_status status = mc_registry_biome_static_load(classes, field, out);
    return public_leave(classes, status, error);
}

mc_registry_status mc_biome_registry_classify_float(mc_registry_classes *classes, float temperature,
                                                    float rainfall, const mc_biome **out,
                                                    mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Biome float output");
    TRY(public_enter(classes, error));
    const mc_registry_status status = classify(classes, temperature, rainfall, out);
    return public_leave(classes, status, error);
}

mc_registry_status mc_biome_registry_lookup(mc_registry_classes *classes, double temperature,
                                            double rainfall, const mc_biome **out,
                                            mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Biome lookup output");
    TRY(public_enter(classes, error));
    const mc_registry_status status = mc_registry_biome_lookup(classes, temperature, rainfall, out);
    return public_leave(classes, status, error);
}

mc_registry_status mc_biome_table_rebuild(mc_registry_classes *classes, mc_registry_error *error) {
    TRY(public_enter(classes, error));
    mc_registry_status status = mc_registry_biomes_active_use(classes);
    if (status == MC_REGISTRY_OK)
        status = table_body(classes);
    return public_leave(classes, status, error);
}

mc_biome_kind mc_biome_kind_get(const mc_biome *biome) {
    return biome == NULL ? MC_BIOME_KIND_NONE : biome->kind;
}

const mc_biome_fields *mc_biome_fields_get(const mc_biome *biome) {
    return biome == NULL ? NULL : &biome->fields;
}

size_t mc_biome_spawn_list_size(const mc_biome_spawn_list *list) {
    return list == NULL ? 0 : list->data.count;
}

const mc_biome_spawn *mc_biome_spawn_list_at(const mc_biome_spawn_list *list, size_t index) {
    return list == NULL || index >= list->data.count ? NULL : list->data.elements[index];
}

const mc_biome_spawn_fields *mc_biome_spawn_fields_get(const mc_biome_spawn *spawn) {
    return spawn == NULL ? NULL : &spawn->fields;
}

const char *mc_biome_entity_class_name(const mc_biome_entity_class *entity_class,
                                       mc_registry_side side) {
    if (entity_class == NULL)
        return NULL;
    return side == MC_REGISTRY_CLIENT   ? entity_class->client_name
           : side == MC_REGISTRY_SERVER ? entity_class->server_name
                                        : NULL;
}

mc_registry_status mc_biome_entity_class_canonical(const mc_registry_classes *classes,
                                                   const mc_biome_entity_class *logical,
                                                   const mc_entity_class_literal **out,
                                                   mc_registry_error *error) {
    static const mc_entity_class_kind kinds[13] = {
        MC_ENTITY_CLASS_KIND_SPIDER,    MC_ENTITY_CLASS_KIND_ZOMBIE,  MC_ENTITY_CLASS_KIND_SKELETON,
        MC_ENTITY_CLASS_KIND_CREEPER,   MC_ENTITY_CLASS_KIND_SLIME,   MC_ENTITY_CLASS_KIND_SHEEP,
        MC_ENTITY_CLASS_KIND_PIG,       MC_ENTITY_CLASS_KIND_CHICKEN, MC_ENTITY_CLASS_KIND_COW,
        MC_ENTITY_CLASS_KIND_SQUID,     MC_ENTITY_CLASS_KIND_WOLF,    MC_ENTITY_CLASS_KIND_GHAST,
        MC_ENTITY_CLASS_KIND_PIG_ZOMBIE};
    if (classes == NULL || logical == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Biome Class canonical arguments");
    size_t index = 0;
    while (index < 13u && logical != &classes->biomes.entity_classes[index])
        ++index;
    if (index == 13u || logical->client_name == NULL || logical->server_name == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "foreign/unresolved Biome Class handle");
    if (classes->entities == NULL)
        return result(error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                      "Biome canonical Entity Class owner unbound");
    const mc_entity_class_literal *literal = NULL;
    if (mc_entity_class_literal_get(classes->entities, kinds[index], &literal) != MC_ENTITY_OK)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Biome canonical Entity Class owner");
    *out = literal;
    return result(error, MC_REGISTRY_OK, NULL);
}

#undef TRY
