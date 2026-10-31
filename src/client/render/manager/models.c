/* Direct C source constructors derived from the retained complete Code.
 * Offline authoring metadata is absent from the client. All geometry, vectors,
 * UVs, mirrors and allocations use the canonical model graph kernel. */
#include "internal.h"
#include "mc/core/math/trig_double.h"
#include <stdint.h>
#include <string.h>

static bool model_ea_void(mc_client_model_graph *g, manager_model *model);
static bool model_eh_void(mc_client_model_graph *g, manager_model *model);
static bool model_eh_F(mc_client_model_graph *g, manager_model *model, float arg1);
static bool model_ej_void(mc_client_model_graph *g, manager_model *model);
static bool model_ez_void(mc_client_model_graph *g, manager_model *model);
static bool model_hh_void(mc_client_model_graph *g, manager_model *model);
static bool model_il_void(mc_client_model_graph *g, manager_model *model);
static bool model_il_F(mc_client_model_graph *g, manager_model *model, float arg1);
static bool model_lc_void(mc_client_model_graph *g, manager_model *model);
static bool model_lo_void(mc_client_model_graph *g, manager_model *model);
static bool model_mw_void(mc_client_model_graph *g, manager_model *model);
static bool model_nn_IF(mc_client_model_graph *g, manager_model *model, int32_t arg1, float arg2);
static bool model_no_I(mc_client_model_graph *g, manager_model *model, int32_t arg1);
static bool model_nq_void(mc_client_model_graph *g, manager_model *model);
static bool model_o_void(mc_client_model_graph *g, manager_model *model);
static bool model_sc_void(mc_client_model_graph *g, manager_model *model);
static bool model_td_void(mc_client_model_graph *g, manager_model *model);
static bool model_wn_void(mc_client_model_graph *g, manager_model *model);

static bool model_ea_void(mc_client_model_graph *g, manager_model *model) {
    mc_client_model_part *part1 = NULL;
    mc_client_model_part *part2 = NULL;
    float local1 = 0;
    mc_client_model_part *part3 = NULL;
    mc_client_model_part *part4 = NULL;
    mc_client_model_part *part5 = NULL;
    mc_client_model_part *part6 = NULL;
    if (!model_nn_IF(g, model, 12, 0.0f))
        return false;
    part1 = mc_client_model_graph_part_allocate(g, "ea", 8);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 0, 0);
    (model)->fields.d = part1; /* ea:17 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.8000000000000p+1f,
                                   -0x1.0000000000000p+2f, -0x1.0000000000000p+2f, 6, 6, 6,
                                   0x1.3333340000000p-1f))
        return false;
    mc_client_model_graph_pivot((model)->fields.d, 0.0f, 0x1.8000000000000p+2f,
                                -0x1.0000000000000p+3f);
    part2 = mc_client_model_graph_part_allocate(g, "ea", 54);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 28, 8);
    (model)->fields.e = part2; /* ea:65 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, -0x1.0000000000000p+2f,
                                   -0x1.4000000000000p+3f, -0x1.c000000000000p+2f, 8, 16, 6,
                                   0x1.c000000000000p+0f))
        return false;
    mc_client_model_graph_pivot((model)->fields.e, 0.0f, 0x1.4000000000000p+2f, 2.0f);
    local1 = 0x1.0000000000000p-1f;
    part3 = mc_client_model_graph_part_allocate(g, "ea", 104);
    if (part3 == NULL)
        return false;
    mc_client_model_graph_part_construct(part3, 0, 16);
    (model)->fields.f = part3; /* ea:114 */
    if (!mc_client_model_graph_box(g, (model)->fields.f, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, 6, 4, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.f, -0x1.8000000000000p+1f, 0x1.8000000000000p+3f,
                                0x1.c000000000000p+2f);
    part4 = mc_client_model_graph_part_allocate(g, "ea", 148);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 0, 16);
    (model)->fields.g = part4; /* ea:158 */
    if (!mc_client_model_graph_box(g, (model)->fields.g, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, 6, 4, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.g, 0x1.8000000000000p+1f, 0x1.8000000000000p+3f,
                                0x1.c000000000000p+2f);
    part5 = mc_client_model_graph_part_allocate(g, "ea", 192);
    if (part5 == NULL)
        return false;
    mc_client_model_graph_part_construct(part5, 0, 16);
    (model)->fields.h = part5; /* ea:202 */
    if (!mc_client_model_graph_box(g, (model)->fields.h, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, 6, 4, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.h, -0x1.8000000000000p+1f, 0x1.8000000000000p+3f,
                                -0x1.4000000000000p+2f);
    part6 = mc_client_model_graph_part_allocate(g, "ea", 236);
    if (part6 == NULL)
        return false;
    mc_client_model_graph_part_construct(part6, 0, 16);
    (model)->fields.i = part6; /* ea:246 */
    if (!mc_client_model_graph_box(g, (model)->fields.i, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, 6, 4, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.i, 0x1.8000000000000p+1f, 0x1.8000000000000p+3f,
                                -0x1.4000000000000p+2f);
    return true;
}

static bool model_eh_void(mc_client_model_graph *g, manager_model *model) {
    if (!model_nn_IF(g, model, 6, 0.0f))
        return false;
    return true;
}

static bool model_eh_F(mc_client_model_graph *g, manager_model *model, float arg1) {
    if (!model_nn_IF(g, model, 6, arg1))
        return false;
    return true;
}

static bool model_ej_void(mc_client_model_graph *g, manager_model *model) {
    if (!mc_client_model_graph_fill_biped(g, &model->fields, 0.0f, 0.0f))
        return false;
    return true;
}

static bool model_ez_void(mc_client_model_graph *g, manager_model *model) {
    manager_part_array *array1 = NULL;
    mc_client_model_part *part2 = NULL;
    mc_client_model_part *part3 = NULL;
    mc_client_model_part *part4 = NULL;
    mc_client_model_part *part5 = NULL;
    mc_client_model_part *part6 = NULL;
    int32_t local1 = 0;
    int32_t local2 = 0;
    int32_t local3 = 0;
    int32_t local4 = 0;
    int32_t value7 = 0;
    int32_t value8 = 0;
    float value9 = 0;
    int32_t value10 = 0;
    int32_t value11 = 0;
    int32_t value12 = 0;
    float value13 = 0;
    int32_t value14 = 0;
    int32_t value15 = 0;
    float value16 = 0;
    int32_t value17 = 0;
    int32_t value18 = 0;
    int32_t value19 = 0;
    float value20 = 0;
    int32_t value21 = 0;
    int32_t value22 = 0;
    float value23 = 0;
    int32_t value24 = 0;
    int32_t value25 = 0;
    int32_t value26 = 0;
    int32_t value27 = 0;
    float value28 = 0;
    int32_t value29 = 0;
    float value30 = 0;
    int32_t value31 = 0;
    int32_t value32 = 0;
    int32_t value33 = 0;
    float value34 = 0;
    int32_t value35 = 0;
    int32_t value36 = 0;
    float value37 = 0;
    int32_t value38 = 0;
    int32_t value39 = 0;
    int32_t value40 = 0;
    float value41 = 0;
    int32_t value42 = 0;
    float value43 = 0;
    int32_t value44 = 0;
    int32_t value45 = 0;
    int32_t value46 = 0;
    float value47 = 0;
    int32_t value48 = 0;
    int32_t value49 = 0;
    float value50 = 0;
    int32_t value51 = 0;
    int32_t value52 = 0;
    float value53 = 0;
    int32_t value54 = 0;
    int32_t value55 = 0;
    int32_t value56 = 0;
    float value57 = 0;
    int32_t value58 = 0;
    int32_t value59 = 0;
    int32_t value60 = 0;
    float value61 = 0;
    int32_t value62 = 0;
    int32_t value63 = 0;
    float value64 = 0;
    int32_t value65 = 0;
    int32_t value66 = 0;
    float value67 = 0;
    int32_t value68 = 0;
    int32_t value69 = 0;
    float value70 = 0;
    model->fields.n = false; /* KO:6, no FH construction. */
    array1 = manager_part_array_new(g, 5, "ez", 6);
    if (array1 == NULL)
        return false;
    (model)->array_a = array1; /* ez:9 */
    part2 = mc_client_model_graph_part_allocate(g, "ez", 17);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 0, 8);
    ((model)->array_a)->elements[0] = part2; /* ez:27 */
    part3 = mc_client_model_graph_part_allocate(g, "ez", 33);
    if (part3 == NULL)
        return false;
    mc_client_model_graph_part_construct(part3, 0, 0);
    ((model)->array_a)->elements[1] = part3; /* ez:42 */
    part4 = mc_client_model_graph_part_allocate(g, "ez", 48);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 0, 0);
    ((model)->array_a)->elements[2] = part4; /* ez:57 */
    part5 = mc_client_model_graph_part_allocate(g, "ez", 63);
    if (part5 == NULL)
        return false;
    mc_client_model_graph_part_construct(part5, 0, 0);
    ((model)->array_a)->elements[3] = part5; /* ez:72 */
    part6 = mc_client_model_graph_part_allocate(g, "ez", 78);
    if (part6 == NULL)
        return false;
    mc_client_model_graph_part_construct(part6, 0, 0);
    ((model)->array_a)->elements[4] = part6; /* ez:87 */
    local1 = 24;
    local2 = 6;
    local3 = 20;
    local4 = 4;
    value7 = mc_java_i32_neg(local1);
    value8 = (value7) / (2);
    value9 = (float)(value8);
    value10 = mc_java_i32_neg(local3);
    value11 = (value10) / (2);
    value12 = mc_java_i32_add(value11, 2);
    value13 = (float)(value12);
    value14 = mc_java_i32_sub(local3, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[0], value9, value13,
                                   -0x1.8000000000000p+1f, local1, value14, 4, 0.0f))
        return false;
    value15 = mc_java_i32_add(0, local4);
    value16 = (float)(value15);
    mc_client_model_graph_pivot(((model)->array_a)->elements[0], 0.0f, value16, 0.0f);
    value17 = mc_java_i32_neg(local1);
    value18 = (value17) / (2);
    value19 = mc_java_i32_add(value18, 2);
    value20 = (float)(value19);
    value21 = mc_java_i32_neg(local2);
    value22 = mc_java_i32_sub(value21, 1);
    value23 = (float)(value22);
    value24 = mc_java_i32_sub(local1, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[1], value20, value23,
                                   -0x1.0000000000000p+0f, value24, local2, 2, 0.0f))
        return false;
    value25 = mc_java_i32_neg(local1);
    value26 = (value25) / (2);
    value27 = mc_java_i32_add(value26, 1);
    value28 = (float)(value27);
    value29 = mc_java_i32_add(0, local4);
    value30 = (float)(value29);
    mc_client_model_graph_pivot(((model)->array_a)->elements[1], value28, value30, 0.0f);
    value31 = mc_java_i32_neg(local1);
    value32 = (value31) / (2);
    value33 = mc_java_i32_add(value32, 2);
    value34 = (float)(value33);
    value35 = mc_java_i32_neg(local2);
    value36 = mc_java_i32_sub(value35, 1);
    value37 = (float)(value36);
    value38 = mc_java_i32_sub(local1, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[2], value34, value37,
                                   -0x1.0000000000000p+0f, value38, local2, 2, 0.0f))
        return false;
    value39 = (local1) / (2);
    value40 = mc_java_i32_sub(value39, 1);
    value41 = (float)(value40);
    value42 = mc_java_i32_add(0, local4);
    value43 = (float)(value42);
    mc_client_model_graph_pivot(((model)->array_a)->elements[2], value41, value43, 0.0f);
    value44 = mc_java_i32_neg(local1);
    value45 = (value44) / (2);
    value46 = mc_java_i32_add(value45, 2);
    value47 = (float)(value46);
    value48 = mc_java_i32_neg(local2);
    value49 = mc_java_i32_sub(value48, 1);
    value50 = (float)(value49);
    value51 = mc_java_i32_sub(local1, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[3], value47, value50,
                                   -0x1.0000000000000p+0f, value51, local2, 2, 0.0f))
        return false;
    value52 = mc_java_i32_add(0, local4);
    value53 = (float)(value52);
    value54 = mc_java_i32_neg(local3);
    value55 = (value54) / (2);
    value56 = mc_java_i32_add(value55, 1);
    value57 = (float)(value56);
    mc_client_model_graph_pivot(((model)->array_a)->elements[3], 0.0f, value53, value57);
    value58 = mc_java_i32_neg(local1);
    value59 = (value58) / (2);
    value60 = mc_java_i32_add(value59, 2);
    value61 = (float)(value60);
    value62 = mc_java_i32_neg(local2);
    value63 = mc_java_i32_sub(value62, 1);
    value64 = (float)(value63);
    value65 = mc_java_i32_sub(local1, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[4], value61, value64,
                                   -0x1.0000000000000p+0f, value65, local2, 2, 0.0f))
        return false;
    value66 = mc_java_i32_add(0, local4);
    value67 = (float)(value66);
    value68 = (local3) / (2);
    value69 = mc_java_i32_sub(value68, 1);
    value70 = (float)(value69);
    mc_client_model_graph_pivot(((model)->array_a)->elements[4], 0.0f, value67, value70);
    (((model)->array_a)->elements[0])->d = 0x1.921fb60000000p+0f; /* ez:355 */
    (((model)->array_a)->elements[1])->e = 0x1.2d97c80000000p+2f; /* ez:366 */
    (((model)->array_a)->elements[2])->e = 0x1.921fb60000000p+0f; /* ez:377 */
    (((model)->array_a)->elements[3])->e = 0x1.921fb60000000p+1f; /* ez:388 */
    return true;
}

static bool model_hh_void(mc_client_model_graph *g, manager_model *model) {
    mc_client_model_part *part1 = NULL;
    mc_client_model_part *part2 = NULL;
    mc_client_model_part *part3 = NULL;
    mc_client_model_part *part4 = NULL;
    mc_client_model_part *part5 = NULL;
    float value6 = 0;
    float value7 = 0;
    float value8 = 0;
    float value9 = 0;
    float value10 = 0;
    float value11 = 0;
    float value12 = 0;
    float value13 = 0;
    if (!model_nn_IF(g, model, 12, 0.0f))
        return false;
    part1 = mc_client_model_graph_part_allocate(g, "hh", 8);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 0, 0);
    (model)->fields.d = part1; /* hh:17 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.0000000000000p+2f,
                                   -0x1.0000000000000p+2f, -0x1.8000000000000p+2f, 8, 8, 6, 0.0f))
        return false;
    mc_client_model_graph_pivot((model)->fields.d, 0.0f, 0x1.0000000000000p+2f,
                                -0x1.0000000000000p+3f);
    part2 = mc_client_model_graph_part_allocate(g, "hh", 53);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 22, 0);
    (model)->fields.b = part2; /* hh:63 */
    if (!mc_client_model_graph_box(g, (model)->fields.b, -0x1.0000000000000p+2f,
                                   -0x1.4000000000000p+2f, -0x1.0000000000000p+2f, 1, 3, 1, 0.0f))
        return false;
    mc_client_model_graph_pivot((model)->fields.b, 0.0f, 0x1.8000000000000p+1f,
                                -0x1.c000000000000p+2f);
    part3 = mc_client_model_graph_part_allocate(g, "hh", 96);
    if (part3 == NULL)
        return false;
    mc_client_model_graph_part_construct(part3, 22, 0);
    (model)->fields.c = part3; /* hh:106 */
    if (!mc_client_model_graph_box(g, (model)->fields.c, 0x1.8000000000000p+1f,
                                   -0x1.4000000000000p+2f, -0x1.0000000000000p+2f, 1, 3, 1, 0.0f))
        return false;
    mc_client_model_graph_pivot((model)->fields.c, 0.0f, 0x1.8000000000000p+1f,
                                -0x1.c000000000000p+2f);
    part4 = mc_client_model_graph_part_allocate(g, "hh", 139);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 52, 0);
    (model)->fields.a = part4; /* hh:149 */
    if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.0000000000000p+1f,
                                   -0x1.8000000000000p+1f, 0.0f, 4, 6, 2, 0.0f))
        return false;
    mc_client_model_graph_pivot((model)->fields.a, 0.0f, 0x1.c000000000000p+3f,
                                0x1.8000000000000p+2f);
    ((model)->fields.a)->d = 0x1.921fb60000000p+0f; /* hh:187 */
    part5 = mc_client_model_graph_part_allocate(g, "hh", 191);
    if (part5 == NULL)
        return false;
    mc_client_model_graph_part_construct(part5, 18, 4);
    (model)->fields.e = part5; /* hh:201 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, -0x1.8000000000000p+2f,
                                   -0x1.4000000000000p+3f, -0x1.c000000000000p+2f, 12, 18, 10,
                                   0.0f))
        return false;
    mc_client_model_graph_pivot((model)->fields.e, 0.0f, 0x1.4000000000000p+2f, 2.0f);
    value6 = (((model)->fields.f)->a) - (1.0f);
    ((model)->fields.f)->a = value6; /* hh:245 */
    value7 = (((model)->fields.g)->a) + (1.0f);
    ((model)->fields.g)->a = value7; /* hh:258 */
    value8 = (((model)->fields.f)->c) + (0.0f);
    ((model)->fields.f)->c = value8; /* hh:271 */
    value9 = (((model)->fields.g)->c) + (0.0f);
    ((model)->fields.g)->c = value9; /* hh:284 */
    value10 = (((model)->fields.h)->a) - (1.0f);
    ((model)->fields.h)->a = value10; /* hh:297 */
    value11 = (((model)->fields.i)->a) + (1.0f);
    ((model)->fields.i)->a = value11; /* hh:310 */
    value12 = (((model)->fields.h)->c) - (1.0f);
    ((model)->fields.h)->c = value12; /* hh:323 */
    value13 = (((model)->fields.i)->c) - (1.0f);
    ((model)->fields.i)->c = value13; /* hh:336 */
    return true;
}

static bool model_il_void(mc_client_model_graph *g, manager_model *model) {
    if (!model_il_F(g, model, 0.0f))
        return false;
    return true;
}

static bool model_il_F(mc_client_model_graph *g, manager_model *model, float arg1) {
    int32_t local2 = 0;
    mc_client_model_part *part1 = NULL;
    float value2 = 0;
    mc_client_model_part *part3 = NULL;
    float value4 = 0;
    float value5 = 0;
    mc_client_model_part *part6 = NULL;
    float value7 = 0;
    mc_client_model_part *part8 = NULL;
    int32_t value9 = 0;
    float value10 = 0;
    mc_client_model_part *part11 = NULL;
    int32_t value12 = 0;
    float value13 = 0;
    mc_client_model_part *part14 = NULL;
    int32_t value15 = 0;
    float value16 = 0;
    mc_client_model_part *part17 = NULL;
    int32_t value18 = 0;
    float value19 = 0;
    model->fields.n = false; /* KO:6, no FH construction. */
    local2 = 4;
    part1 = mc_client_model_graph_part_allocate(g, "il", 7);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 0, 0);
    (model)->fields.a = part1; /* il:16 */
    if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.0000000000000p+2f,
                                   -0x1.0000000000000p+3f, -0x1.0000000000000p+2f, 8, 8, 8, arg1))
        return false;
    value2 = (float)(local2);
    mc_client_model_graph_pivot((model)->fields.a, 0.0f, value2, 0.0f);
    part3 = mc_client_model_graph_part_allocate(g, "il", 51);
    if (part3 == NULL)
        return false;
    mc_client_model_graph_part_construct(part3, 32, 0);
    (model)->fields.b = part3; /* il:61 */
    value4 = (arg1) + (0x1.0000000000000p-1f);
    if (!mc_client_model_graph_box(g, (model)->fields.b, -0x1.0000000000000p+2f,
                                   -0x1.0000000000000p+3f, -0x1.0000000000000p+2f, 8, 8, 8, value4))
        return false;
    value5 = (float)(local2);
    mc_client_model_graph_pivot((model)->fields.b, 0.0f, value5, 0.0f);
    part6 = mc_client_model_graph_part_allocate(g, "il", 99);
    if (part6 == NULL)
        return false;
    mc_client_model_graph_part_construct(part6, 16, 16);
    (model)->fields.c = part6; /* il:110 */
    if (!mc_client_model_graph_box(g, (model)->fields.c, -0x1.0000000000000p+2f, 0.0f,
                                   -0x1.0000000000000p+1f, 8, 12, 4, arg1))
        return false;
    value7 = (float)(local2);
    mc_client_model_graph_pivot((model)->fields.c, 0.0f, value7, 0.0f);
    part8 = mc_client_model_graph_part_allocate(g, "il", 143);
    if (part8 == NULL)
        return false;
    mc_client_model_graph_part_construct(part8, 0, 16);
    (model)->fields.d = part8; /* il:153 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, 6, 4, arg1))
        return false;
    value9 = mc_java_i32_add(12, local2);
    value10 = (float)(value9);
    mc_client_model_graph_pivot((model)->fields.d, -0x1.0000000000000p+1f, value10,
                                0x1.0000000000000p+2f);
    part11 = mc_client_model_graph_part_allocate(g, "il", 190);
    if (part11 == NULL)
        return false;
    mc_client_model_graph_part_construct(part11, 0, 16);
    (model)->fields.e = part11; /* il:200 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, 6, 4, arg1))
        return false;
    value12 = mc_java_i32_add(12, local2);
    value13 = (float)(value12);
    mc_client_model_graph_pivot((model)->fields.e, 2.0f, value13, 0x1.0000000000000p+2f);
    part14 = mc_client_model_graph_part_allocate(g, "il", 236);
    if (part14 == NULL)
        return false;
    mc_client_model_graph_part_construct(part14, 0, 16);
    (model)->fields.f = part14; /* il:246 */
    if (!mc_client_model_graph_box(g, (model)->fields.f, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, 6, 4, arg1))
        return false;
    value15 = mc_java_i32_add(12, local2);
    value16 = (float)(value15);
    mc_client_model_graph_pivot((model)->fields.f, -0x1.0000000000000p+1f, value16,
                                -0x1.0000000000000p+2f);
    part17 = mc_client_model_graph_part_allocate(g, "il", 283);
    if (part17 == NULL)
        return false;
    mc_client_model_graph_part_construct(part17, 0, 16);
    (model)->fields.g = part17; /* il:293 */
    if (!mc_client_model_graph_box(g, (model)->fields.g, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, 6, 4, arg1))
        return false;
    value18 = mc_java_i32_add(12, local2);
    value19 = (float)(value18);
    mc_client_model_graph_pivot((model)->fields.g, 2.0f, value19, -0x1.0000000000000p+2f);
    return true;
}

static bool model_lc_void(mc_client_model_graph *g, manager_model *model) {
    float local1 = 0;
    mc_client_model_part *part1 = NULL;
    mc_client_model_part *part2 = NULL;
    mc_client_model_part *part3 = NULL;
    mc_client_model_part *part4 = NULL;
    if (!model_ej_void(g, model))
        return false;
    local1 = 0.0f;
    part1 = mc_client_model_graph_part_allocate(g, "lc", 7);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 40, 16);
    (model)->fields.d = part1; /* lc:18 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.0000000000000p+0f,
                                   -0x1.0000000000000p+1f, -0x1.0000000000000p+0f, 2, 12, 2,
                                   local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.d, -0x1.4000000000000p+2f, 2.0f, 0.0f);
    part2 = mc_client_model_graph_part_allocate(g, "lc", 51);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 40, 16);
    (model)->fields.e = part2;  /* lc:62 */
    ((model)->fields.e)->g = 1; /* lc:70 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, -0x1.0000000000000p+0f,
                                   -0x1.0000000000000p+1f, -0x1.0000000000000p+0f, 2, 12, 2,
                                   local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.e, 0x1.4000000000000p+2f, 2.0f, 0.0f);
    part3 = mc_client_model_graph_part_allocate(g, "lc", 103);
    if (part3 == NULL)
        return false;
    mc_client_model_graph_part_construct(part3, 0, 16);
    (model)->fields.f = part3; /* lc:113 */
    if (!mc_client_model_graph_box(g, (model)->fields.f, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.0000000000000p+0f, 2, 12, 2, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.f, -0x1.0000000000000p+1f, 0x1.8000000000000p+3f,
                                0.0f);
    part4 = mc_client_model_graph_part_allocate(g, "lc", 146);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 0, 16);
    (model)->fields.g = part4;  /* lc:156 */
    ((model)->fields.g)->g = 1; /* lc:164 */
    if (!mc_client_model_graph_box(g, (model)->fields.g, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.0000000000000p+0f, 2, 12, 2, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.g, 2.0f, 0x1.8000000000000p+3f, 0.0f);
    return true;
}

static bool model_lo_void(mc_client_model_graph *g, manager_model *model) {
    manager_part_array *array1 = NULL;
    int32_t local1 = 0;
    mc_client_model_part *part2 = NULL;
    int32_t value3 = 0;
    float value4 = 0;
    float value5 = 0;
    mc_java_random *random6 = NULL;
    mc_java_random *local2 = NULL;
    int32_t local3 = 0;
    mc_client_model_part *part7 = NULL;
    int32_t value8 = 0;
    float value9 = 0;
    int32_t value10 = 0;
    int32_t value11 = 0;
    float value12 = 0;
    float value13 = 0;
    float value14 = 0;
    float value15 = 0;
    float value16 = 0;
    float value17 = 0;
    float value18 = 0;
    float value19 = 0;
    float local4 = 0;
    int32_t value20 = 0;
    float value21 = 0;
    float value22 = 0;
    float value23 = 0;
    float value24 = 0;
    float value25 = 0;
    float local5 = 0;
    int32_t draw26 = 0;
    int32_t value27 = 0;
    int32_t local6 = 0;
    int32_t value28 = 0;
    float value29 = 0;
    model->fields.n = false; /* KO:6, no FH construction. */
    array1 = manager_part_array_new(g, 9, "lo", 7);
    if (array1 == NULL)
        return false;
    (model)->array_b = array1; /* lo:10 */
    local1 = -16;
    part2 = mc_client_model_graph_part_allocate(g, "lo", 17);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 0, 0);
    (model)->fields.a = part2; /* lo:26 */
    if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.0000000000000p+3f,
                                   -0x1.0000000000000p+3f, -0x1.0000000000000p+3f, 16, 16, 16,
                                   0.0f))
        return false;
    value3 = mc_java_i32_add(24, local1);
    value4 = (float)(value3);
    value5 = (((model)->fields.a)->b) + (value4);
    ((model)->fields.a)->b = value5; /* lo:62 */
    random6 = mc_client_model_graph_allocate(g, sizeof(mc_java_random), "lo", 65);
    if (random6 == NULL)
        return false;
    mc_java_random_seed(random6, 1660);
    local2 = random6;
    local3 = 0;
    while (local3 < ((model)->array_b)->length) {
        part7 = mc_client_model_graph_part_allocate(g, "lo", 92);
        if (part7 == NULL)
            return false;
        mc_client_model_graph_part_construct(part7, 0, 0);
        ((model)->array_b)->elements[local3] = part7; /* lo:101 */
        value8 = (local3) % (3);
        value9 = (float)(value8);
        value10 = (local3) / (3);
        value11 = (value10) % (2);
        value12 = (float)(value11);
        value13 = (value12) * (0x1.0000000000000p-1f);
        value14 = (value9) - (value13);
        value15 = (value14) + (0x1.0000000000000p-2f);
        value16 = (value15) / (2.0f);
        value17 = (value16) * (2.0f);
        value18 = (value17) - (1.0f);
        value19 = (value18) * (0x1.4000000000000p+2f);
        local4 = value19;
        value20 = (local3) / (3);
        value21 = (float)(value20);
        value22 = (value21) / (2.0f);
        value23 = (value22) * (2.0f);
        value24 = (value23) - (1.0f);
        value25 = (value24) * (0x1.4000000000000p+2f);
        local5 = value25;
        draw26 = 0;
        if (!mc_java_random_next_bounded(local2, 7, &draw26))
            return false;
        value27 = mc_java_i32_add(draw26, 8);
        local6 = value27;
        if (!mc_client_model_graph_box(g, ((model)->array_b)->elements[local3],
                                       -0x1.0000000000000p+0f, 0.0f, -0x1.0000000000000p+0f, 2,
                                       local6, 2, 0.0f))
            return false;
        (((model)->array_b)->elements[local3])->a = local4; /* lo:182 */
        (((model)->array_b)->elements[local3])->c = local5; /* lo:193 */
        value28 = mc_java_i32_add(31, local1);
        value29 = (float)(value28);
        (((model)->array_b)->elements[local3])->b = value29; /* lo:207 */
        local3 = mc_java_i32_add(local3, 1);
    }
    return true;
}

static bool model_mw_void(mc_client_model_graph *g, manager_model *model) {
    mc_client_model_part *part1 = NULL;
    mc_client_model_part *part2 = NULL;
    if (!model_nn_IF(g, model, 12, 0.0f))
        return false;
    part1 = mc_client_model_graph_part_allocate(g, "mw", 8);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 0, 0);
    (model)->fields.d = part1; /* mw:17 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.8000000000000p+1f,
                                   -0x1.0000000000000p+2f, -0x1.8000000000000p+2f, 6, 6, 8, 0.0f))
        return false;
    mc_client_model_graph_pivot((model)->fields.d, 0.0f, 0x1.8000000000000p+2f,
                                -0x1.0000000000000p+3f);
    part2 = mc_client_model_graph_part_allocate(g, "mw", 53);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 28, 8);
    (model)->fields.e = part2; /* mw:64 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, -0x1.0000000000000p+2f,
                                   -0x1.4000000000000p+3f, -0x1.c000000000000p+2f, 8, 16, 6, 0.0f))
        return false;
    mc_client_model_graph_pivot((model)->fields.e, 0.0f, 0x1.4000000000000p+2f, 2.0f);
    return true;
}

static bool model_nn_IF(mc_client_model_graph *g, manager_model *model, int32_t arg1, float arg2) {
    mc_client_model_part *part1 = NULL;
    int32_t value2 = 0;
    float value3 = 0;
    mc_client_model_part *part4 = NULL;
    int32_t value5 = 0;
    float value6 = 0;
    mc_client_model_part *part7 = NULL;
    int32_t value8 = 0;
    float value9 = 0;
    mc_client_model_part *part10 = NULL;
    int32_t value11 = 0;
    float value12 = 0;
    mc_client_model_part *part13 = NULL;
    int32_t value14 = 0;
    float value15 = 0;
    mc_client_model_part *part16 = NULL;
    int32_t value17 = 0;
    float value18 = 0;
    model->fields.n = false; /* KO:6, no FH construction. */
    part1 = mc_client_model_graph_part_allocate(g, "nn", 5);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 0, 0);
    (model)->fields.d = part1; /* nn:14 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.0000000000000p+2f,
                                   -0x1.0000000000000p+2f, -0x1.0000000000000p+3f, 8, 8, 8, arg2))
        return false;
    value2 = mc_java_i32_sub(18, arg1);
    value3 = (float)(value2);
    mc_client_model_graph_pivot((model)->fields.d, 0.0f, value3, -0x1.8000000000000p+2f);
    part4 = mc_client_model_graph_part_allocate(g, "nn", 53);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 28, 8);
    (model)->fields.e = part4; /* nn:64 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, -0x1.4000000000000p+2f,
                                   -0x1.4000000000000p+3f, -0x1.c000000000000p+2f, 10, 16, 8, arg2))
        return false;
    value5 = mc_java_i32_sub(17, arg1);
    value6 = (float)(value5);
    mc_client_model_graph_pivot((model)->fields.e, 0.0f, value6, 2.0f);
    part7 = mc_client_model_graph_part_allocate(g, "nn", 102);
    if (part7 == NULL)
        return false;
    mc_client_model_graph_part_construct(part7, 0, 16);
    (model)->fields.f = part7; /* nn:112 */
    if (!mc_client_model_graph_box(g, (model)->fields.f, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, arg1, 4, arg2))
        return false;
    value8 = mc_java_i32_sub(24, arg1);
    value9 = (float)(value8);
    mc_client_model_graph_pivot((model)->fields.f, -0x1.8000000000000p+1f, value9,
                                0x1.c000000000000p+2f);
    part10 = mc_client_model_graph_part_allocate(g, "nn", 148);
    if (part10 == NULL)
        return false;
    mc_client_model_graph_part_construct(part10, 0, 16);
    (model)->fields.g = part10; /* nn:158 */
    if (!mc_client_model_graph_box(g, (model)->fields.g, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, arg1, 4, arg2))
        return false;
    value11 = mc_java_i32_sub(24, arg1);
    value12 = (float)(value11);
    mc_client_model_graph_pivot((model)->fields.g, 0x1.8000000000000p+1f, value12,
                                0x1.c000000000000p+2f);
    part13 = mc_client_model_graph_part_allocate(g, "nn", 194);
    if (part13 == NULL)
        return false;
    mc_client_model_graph_part_construct(part13, 0, 16);
    (model)->fields.h = part13; /* nn:204 */
    if (!mc_client_model_graph_box(g, (model)->fields.h, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, arg1, 4, arg2))
        return false;
    value14 = mc_java_i32_sub(24, arg1);
    value15 = (float)(value14);
    mc_client_model_graph_pivot((model)->fields.h, -0x1.8000000000000p+1f, value15,
                                -0x1.4000000000000p+2f);
    part16 = mc_client_model_graph_part_allocate(g, "nn", 240);
    if (part16 == NULL)
        return false;
    mc_client_model_graph_part_construct(part16, 0, 16);
    (model)->fields.i = part16; /* nn:250 */
    if (!mc_client_model_graph_box(g, (model)->fields.i, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.0000000000000p+1f, 4, arg1, 4, arg2))
        return false;
    value17 = mc_java_i32_sub(24, arg1);
    value18 = (float)(value17);
    mc_client_model_graph_pivot((model)->fields.i, 0x1.8000000000000p+1f, value18,
                                -0x1.4000000000000p+2f);
    return true;
}

static bool model_no_I(mc_client_model_graph *g, manager_model *model, int32_t arg1) {
    mc_client_model_part *part1 = NULL;
    mc_client_model_part *part2 = NULL;
    mc_client_model_part *part3 = NULL;
    mc_client_model_part *part4 = NULL;
    mc_client_model_part *part5 = NULL;
    model->fields.n = false; /* KO:6, no FH construction. */
    part1 = mc_client_model_graph_part_allocate(g, "no", 5);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 0, arg1);
    (model)->fields.a = part1; /* no:14 */
    if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.0000000000000p+2f,
                                   0x1.0000000000000p+4f, -0x1.0000000000000p+2f, 8, 8, 8, 0.0f))
        return false;
    if (arg1 > 0) {
        part2 = mc_client_model_graph_part_allocate(g, "no", 41);
        if (part2 == NULL)
            return false;
        mc_client_model_graph_part_construct(part2, 0, arg1);
        (model)->fields.a = part2; /* no:50 */
        if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.8000000000000p+1f,
                                       0x1.1000000000000p+4f, -0x1.8000000000000p+1f, 6, 6, 6,
                                       0.0f))
            return false;
        part3 = mc_client_model_graph_part_allocate(g, "no", 73);
        if (part3 == NULL)
            return false;
        mc_client_model_graph_part_construct(part3, 32, 0);
        (model)->fields.b = part3; /* no:83 */
        if (!mc_client_model_graph_box(g, (model)->fields.b, -0x1.a000000000000p+1f,
                                       0x1.2000000000000p+4f, -0x1.c000000000000p+1f, 2, 2, 2,
                                       0.0f))
            return false;
        part4 = mc_client_model_graph_part_allocate(g, "no", 103);
        if (part4 == NULL)
            return false;
        mc_client_model_graph_part_construct(part4, 32, 4);
        (model)->fields.c = part4; /* no:113 */
        if (!mc_client_model_graph_box(g, (model)->fields.c, 0x1.4000000000000p+0f,
                                       0x1.2000000000000p+4f, -0x1.c000000000000p+1f, 2, 2, 2,
                                       0.0f))
            return false;
        part5 = mc_client_model_graph_part_allocate(g, "no", 133);
        if (part5 == NULL)
            return false;
        mc_client_model_graph_part_construct(part5, 32, 8);
        (model)->fields.d = part5; /* no:144 */
        if (!mc_client_model_graph_box(g, (model)->fields.d, 0.0f, 0x1.5000000000000p+4f,
                                       -0x1.c000000000000p+1f, 1, 1, 1, 0.0f))
            return false;
    }
    return true;
}

static bool model_nq_void(mc_client_model_graph *g, manager_model *model) {
    manager_part_array *array1 = NULL;
    mc_client_model_part *part2 = NULL;
    mc_client_model_part *part3 = NULL;
    mc_client_model_part *part4 = NULL;
    mc_client_model_part *part5 = NULL;
    mc_client_model_part *part6 = NULL;
    mc_client_model_part *part7 = NULL;
    int32_t local1 = 0;
    int32_t local2 = 0;
    int32_t local3 = 0;
    int32_t local4 = 0;
    int32_t value8 = 0;
    int32_t value9 = 0;
    float value10 = 0;
    int32_t value11 = 0;
    int32_t value12 = 0;
    float value13 = 0;
    int32_t value14 = 0;
    float value15 = 0;
    int32_t value16 = 0;
    int32_t value17 = 0;
    int32_t value18 = 0;
    float value19 = 0;
    int32_t value20 = 0;
    int32_t value21 = 0;
    int32_t value22 = 0;
    float value23 = 0;
    int32_t value24 = 0;
    int32_t value25 = 0;
    int32_t value26 = 0;
    float value27 = 0;
    int32_t value28 = 0;
    int32_t value29 = 0;
    int32_t value30 = 0;
    float value31 = 0;
    int32_t value32 = 0;
    int32_t value33 = 0;
    float value34 = 0;
    int32_t value35 = 0;
    int32_t value36 = 0;
    int32_t value37 = 0;
    int32_t value38 = 0;
    float value39 = 0;
    int32_t value40 = 0;
    float value41 = 0;
    int32_t value42 = 0;
    int32_t value43 = 0;
    int32_t value44 = 0;
    float value45 = 0;
    int32_t value46 = 0;
    int32_t value47 = 0;
    float value48 = 0;
    int32_t value49 = 0;
    int32_t value50 = 0;
    int32_t value51 = 0;
    float value52 = 0;
    int32_t value53 = 0;
    float value54 = 0;
    int32_t value55 = 0;
    int32_t value56 = 0;
    int32_t value57 = 0;
    float value58 = 0;
    int32_t value59 = 0;
    int32_t value60 = 0;
    float value61 = 0;
    int32_t value62 = 0;
    int32_t value63 = 0;
    float value64 = 0;
    int32_t value65 = 0;
    int32_t value66 = 0;
    int32_t value67 = 0;
    float value68 = 0;
    int32_t value69 = 0;
    int32_t value70 = 0;
    int32_t value71 = 0;
    float value72 = 0;
    int32_t value73 = 0;
    int32_t value74 = 0;
    float value75 = 0;
    int32_t value76 = 0;
    int32_t value77 = 0;
    float value78 = 0;
    int32_t value79 = 0;
    int32_t value80 = 0;
    float value81 = 0;
    model->fields.n = false; /* KO:6, no FH construction. */
    array1 = manager_part_array_new(g, 7, "nq", 7);
    if (array1 == NULL)
        return false;
    (model)->array_a = array1; /* nq:10 */
    part2 = mc_client_model_graph_part_allocate(g, "nq", 18);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 0, 10);
    ((model)->array_a)->elements[0] = part2; /* nq:28 */
    part3 = mc_client_model_graph_part_allocate(g, "nq", 34);
    if (part3 == NULL)
        return false;
    mc_client_model_graph_part_construct(part3, 0, 0);
    ((model)->array_a)->elements[1] = part3; /* nq:43 */
    part4 = mc_client_model_graph_part_allocate(g, "nq", 49);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 0, 0);
    ((model)->array_a)->elements[2] = part4; /* nq:58 */
    part5 = mc_client_model_graph_part_allocate(g, "nq", 64);
    if (part5 == NULL)
        return false;
    mc_client_model_graph_part_construct(part5, 0, 0);
    ((model)->array_a)->elements[3] = part5; /* nq:73 */
    part6 = mc_client_model_graph_part_allocate(g, "nq", 79);
    if (part6 == NULL)
        return false;
    mc_client_model_graph_part_construct(part6, 0, 0);
    ((model)->array_a)->elements[4] = part6; /* nq:88 */
    part7 = mc_client_model_graph_part_allocate(g, "nq", 94);
    if (part7 == NULL)
        return false;
    mc_client_model_graph_part_construct(part7, 44, 10);
    ((model)->array_a)->elements[5] = part7; /* nq:105 */
    local1 = 20;
    local2 = 8;
    local3 = 16;
    local4 = 4;
    value8 = mc_java_i32_neg(local1);
    value9 = (value8) / (2);
    value10 = (float)(value9);
    value11 = mc_java_i32_neg(local3);
    value12 = (value11) / (2);
    value13 = (float)(value12);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[0], value10, value13,
                                   -0x1.0000000000000p+0f, local1, local3, 2, 0.0f))
        return false;
    value14 = mc_java_i32_add(0, local4);
    value15 = (float)(value14);
    mc_client_model_graph_pivot(((model)->array_a)->elements[0], 0.0f, value15, 0.0f);
    value16 = mc_java_i32_neg(local1);
    value17 = (value16) / (2);
    value18 = mc_java_i32_add(value17, 1);
    value19 = (float)(value18);
    value20 = mc_java_i32_neg(local3);
    value21 = (value20) / (2);
    value22 = mc_java_i32_add(value21, 1);
    value23 = (float)(value22);
    value24 = mc_java_i32_sub(local1, 2);
    value25 = mc_java_i32_sub(local3, 2);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[5], value19, value23,
                                   -0x1.0000000000000p+0f, value24, value25, 1, 0.0f))
        return false;
    value26 = mc_java_i32_add(0, local4);
    value27 = (float)(value26);
    mc_client_model_graph_pivot(((model)->array_a)->elements[5], 0.0f, value27, 0.0f);
    value28 = mc_java_i32_neg(local1);
    value29 = (value28) / (2);
    value30 = mc_java_i32_add(value29, 2);
    value31 = (float)(value30);
    value32 = mc_java_i32_neg(local2);
    value33 = mc_java_i32_sub(value32, 1);
    value34 = (float)(value33);
    value35 = mc_java_i32_sub(local1, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[1], value31, value34,
                                   -0x1.0000000000000p+0f, value35, local2, 2, 0.0f))
        return false;
    value36 = mc_java_i32_neg(local1);
    value37 = (value36) / (2);
    value38 = mc_java_i32_add(value37, 1);
    value39 = (float)(value38);
    value40 = mc_java_i32_add(0, local4);
    value41 = (float)(value40);
    mc_client_model_graph_pivot(((model)->array_a)->elements[1], value39, value41, 0.0f);
    value42 = mc_java_i32_neg(local1);
    value43 = (value42) / (2);
    value44 = mc_java_i32_add(value43, 2);
    value45 = (float)(value44);
    value46 = mc_java_i32_neg(local2);
    value47 = mc_java_i32_sub(value46, 1);
    value48 = (float)(value47);
    value49 = mc_java_i32_sub(local1, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[2], value45, value48,
                                   -0x1.0000000000000p+0f, value49, local2, 2, 0.0f))
        return false;
    value50 = (local1) / (2);
    value51 = mc_java_i32_sub(value50, 1);
    value52 = (float)(value51);
    value53 = mc_java_i32_add(0, local4);
    value54 = (float)(value53);
    mc_client_model_graph_pivot(((model)->array_a)->elements[2], value52, value54, 0.0f);
    value55 = mc_java_i32_neg(local1);
    value56 = (value55) / (2);
    value57 = mc_java_i32_add(value56, 2);
    value58 = (float)(value57);
    value59 = mc_java_i32_neg(local2);
    value60 = mc_java_i32_sub(value59, 1);
    value61 = (float)(value60);
    value62 = mc_java_i32_sub(local1, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[3], value58, value61,
                                   -0x1.0000000000000p+0f, value62, local2, 2, 0.0f))
        return false;
    value63 = mc_java_i32_add(0, local4);
    value64 = (float)(value63);
    value65 = mc_java_i32_neg(local3);
    value66 = (value65) / (2);
    value67 = mc_java_i32_add(value66, 1);
    value68 = (float)(value67);
    mc_client_model_graph_pivot(((model)->array_a)->elements[3], 0.0f, value64, value68);
    value69 = mc_java_i32_neg(local1);
    value70 = (value69) / (2);
    value71 = mc_java_i32_add(value70, 2);
    value72 = (float)(value71);
    value73 = mc_java_i32_neg(local2);
    value74 = mc_java_i32_sub(value73, 1);
    value75 = (float)(value74);
    value76 = mc_java_i32_sub(local1, 4);
    if (!mc_client_model_graph_box(g, ((model)->array_a)->elements[4], value72, value75,
                                   -0x1.0000000000000p+0f, value76, local2, 2, 0.0f))
        return false;
    value77 = mc_java_i32_add(0, local4);
    value78 = (float)(value77);
    value79 = (local3) / (2);
    value80 = mc_java_i32_sub(value79, 1);
    value81 = (float)(value80);
    mc_client_model_graph_pivot(((model)->array_a)->elements[4], 0.0f, value78, value81);
    (((model)->array_a)->elements[0])->d = 0x1.921fb60000000p+0f;  /* nq:418 */
    (((model)->array_a)->elements[1])->e = 0x1.2d97c80000000p+2f;  /* nq:429 */
    (((model)->array_a)->elements[2])->e = 0x1.921fb60000000p+0f;  /* nq:440 */
    (((model)->array_a)->elements[3])->e = 0x1.921fb60000000p+1f;  /* nq:451 */
    (((model)->array_a)->elements[5])->d = -0x1.921fb60000000p+0f; /* nq:462 */
    return true;
}

static bool model_o_void(mc_client_model_graph *g, manager_model *model) {
    float local1 = 0;
    float local2 = 0;
    mc_client_model_part *part1 = NULL;
    mc_client_model_part *part2 = NULL;
    mc_client_model_part *part3 = NULL;
    mc_client_model_part *part4 = NULL;
    mc_client_model_part *part5 = NULL;
    mc_client_model_part *part6 = NULL;
    mc_client_model_part *part7 = NULL;
    mc_client_model_part *part8 = NULL;
    mc_client_model_part *part9 = NULL;
    mc_client_model_part *part10 = NULL;
    mc_client_model_part *part11 = NULL;
    model->fields.n = false; /* KO:6, no FH construction. */
    local1 = 0.0f;
    local2 = 0x1.b000000000000p+3f;
    part1 = mc_client_model_graph_part_allocate(g, "o", 10);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 0, 0);
    (model)->fields.a = part1; /* o:19 */
    if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.8000000000000p+1f,
                                   -0x1.8000000000000p+1f, -0x1.0000000000000p+1f, 6, 6, 4, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.a, -0x1.0000000000000p+0f, local2,
                                -0x1.c000000000000p+2f);
    part2 = mc_client_model_graph_part_allocate(g, "o", 54);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 18, 14);
    (model)->fields.b = part2; /* o:65 */
    if (!mc_client_model_graph_box(g, (model)->fields.b, -0x1.0000000000000p+2f,
                                   -0x1.0000000000000p+1f, -0x1.8000000000000p+1f, 6, 9, 6, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.b, 0.0f, 0x1.c000000000000p+3f, 2.0f);
    part3 = mc_client_model_graph_part_allocate(g, "o", 100);
    if (part3 == NULL)
        return false;
    mc_client_model_graph_part_construct(part3, 21, 0);
    (model)->extra_k = part3; /* o:110 */
    if (!mc_client_model_graph_box(g, (model)->extra_k, -0x1.0000000000000p+2f,
                                   -0x1.8000000000000p+1f, -0x1.8000000000000p+1f, 8, 6, 7, local1))
        return false;
    mc_client_model_graph_pivot((model)->extra_k, -0x1.0000000000000p+0f, 0x1.c000000000000p+3f,
                                2.0f);
    part4 = mc_client_model_graph_part_allocate(g, "o", 146);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 0, 18);
    (model)->fields.c = part4; /* o:156 */
    if (!mc_client_model_graph_box(g, (model)->fields.c, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.0000000000000p+0f, 2, 8, 2, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.c, -0x1.4000000000000p+1f, 0x1.0000000000000p+4f,
                                0x1.c000000000000p+2f);
    part5 = mc_client_model_graph_part_allocate(g, "o", 190);
    if (part5 == NULL)
        return false;
    mc_client_model_graph_part_construct(part5, 0, 18);
    (model)->fields.d = part5; /* o:200 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.0000000000000p+0f, 2, 8, 2, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.d, 0x1.0000000000000p-1f, 0x1.0000000000000p+4f,
                                0x1.c000000000000p+2f);
    part6 = mc_client_model_graph_part_allocate(g, "o", 234);
    if (part6 == NULL)
        return false;
    mc_client_model_graph_part_construct(part6, 0, 18);
    (model)->fields.e = part6; /* o:244 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.0000000000000p+0f, 2, 8, 2, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.e, -0x1.4000000000000p+1f, 0x1.0000000000000p+4f,
                                -0x1.0000000000000p+2f);
    part7 = mc_client_model_graph_part_allocate(g, "o", 278);
    if (part7 == NULL)
        return false;
    mc_client_model_graph_part_construct(part7, 0, 18);
    (model)->fields.f = part7; /* o:288 */
    if (!mc_client_model_graph_box(g, (model)->fields.f, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.0000000000000p+0f, 2, 8, 2, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.f, 0x1.0000000000000p-1f, 0x1.0000000000000p+4f,
                                -0x1.0000000000000p+2f);
    part8 = mc_client_model_graph_part_allocate(g, "o", 322);
    if (part8 == NULL)
        return false;
    mc_client_model_graph_part_construct(part8, 9, 18);
    (model)->extra_j = part8; /* o:333 */
    if (!mc_client_model_graph_box(g, (model)->extra_j, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.0000000000000p+0f, 2, 8, 2, local1))
        return false;
    mc_client_model_graph_pivot((model)->extra_j, -0x1.0000000000000p+0f, 0x1.8000000000000p+3f,
                                0x1.0000000000000p+3f);
    part9 = mc_client_model_graph_part_allocate(g, "o", 367);
    if (part9 == NULL)
        return false;
    mc_client_model_graph_part_construct(part9, 16, 14);
    (model)->fields.g = part9; /* o:378 */
    if (!mc_client_model_graph_box(g, (model)->fields.g, -0x1.8000000000000p+1f,
                                   -0x1.4000000000000p+2f, 0.0f, 2, 2, 1, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.g, -0x1.0000000000000p+0f, local2,
                                -0x1.c000000000000p+2f);
    part10 = mc_client_model_graph_part_allocate(g, "o", 410);
    if (part10 == NULL)
        return false;
    mc_client_model_graph_part_construct(part10, 16, 14);
    (model)->fields.h = part10; /* o:421 */
    if (!mc_client_model_graph_box(g, (model)->fields.h, 1.0f, -0x1.4000000000000p+2f, 0.0f, 2, 2,
                                   1, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.h, -0x1.0000000000000p+0f, local2,
                                -0x1.c000000000000p+2f);
    part11 = mc_client_model_graph_part_allocate(g, "o", 452);
    if (part11 == NULL)
        return false;
    mc_client_model_graph_part_construct(part11, 0, 10);
    (model)->fields.i = part11; /* o:462 */
    if (!mc_client_model_graph_box(g, (model)->fields.i, -0x1.0000000000000p+1f, 0.0f,
                                   -0x1.4000000000000p+2f, 3, 3, 4, local1))
        return false;
    mc_client_model_graph_pivot((model)->fields.i, -0x1.0000000000000p-1f, local2,
                                -0x1.c000000000000p+2f);
    return true;
}

static bool model_sc_void(mc_client_model_graph *g, manager_model *model) {
    float local1 = 0;
    int32_t local2 = 0;
    mc_client_model_part *part1 = NULL;
    int32_t value2 = 0;
    float value3 = 0;
    mc_client_model_part *part4 = NULL;
    float value5 = 0;
    mc_client_model_part *part6 = NULL;
    int32_t value7 = 0;
    float value8 = 0;
    mc_client_model_part *part9 = NULL;
    int32_t value10 = 0;
    float value11 = 0;
    mc_client_model_part *part12 = NULL;
    int32_t value13 = 0;
    float value14 = 0;
    mc_client_model_part *part15 = NULL;
    int32_t value16 = 0;
    float value17 = 0;
    mc_client_model_part *part18 = NULL;
    int32_t value19 = 0;
    float value20 = 0;
    mc_client_model_part *part21 = NULL;
    int32_t value22 = 0;
    float value23 = 0;
    mc_client_model_part *part24 = NULL;
    int32_t value25 = 0;
    float value26 = 0;
    mc_client_model_part *part27 = NULL;
    int32_t value28 = 0;
    float value29 = 0;
    mc_client_model_part *part30 = NULL;
    int32_t value31 = 0;
    float value32 = 0;
    model->fields.n = false; /* KO:6, no FH construction. */
    local1 = 0.0f;
    local2 = 15;
    part1 = mc_client_model_graph_part_allocate(g, "sc", 10);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 32, 4);
    (model)->fields.a = part1; /* sc:20 */
    if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.0000000000000p+2f,
                                   -0x1.0000000000000p+2f, -0x1.0000000000000p+3f, 8, 8, 8, local1))
        return false;
    value2 = mc_java_i32_add(0, local2);
    value3 = (float)(value2);
    mc_client_model_graph_pivot((model)->fields.a, 0.0f, value3, -0x1.8000000000000p+1f);
    part4 = mc_client_model_graph_part_allocate(g, "sc", 58);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 0, 0);
    (model)->fields.b = part4; /* sc:67 */
    if (!mc_client_model_graph_box(g, (model)->fields.b, -0x1.8000000000000p+1f,
                                   -0x1.8000000000000p+1f, -0x1.8000000000000p+1f, 6, 6, 6, local1))
        return false;
    value5 = (float)(local2);
    mc_client_model_graph_pivot((model)->fields.b, 0.0f, value5, 0.0f);
    part6 = mc_client_model_graph_part_allocate(g, "sc", 102);
    if (part6 == NULL)
        return false;
    mc_client_model_graph_part_construct(part6, 0, 12);
    (model)->fields.c = part6; /* sc:112 */
    if (!mc_client_model_graph_box(g, (model)->fields.c, -0x1.4000000000000p+2f,
                                   -0x1.0000000000000p+2f, -0x1.8000000000000p+2f, 10, 8, 12,
                                   local1))
        return false;
    value7 = mc_java_i32_add(0, local2);
    value8 = (float)(value7);
    mc_client_model_graph_pivot((model)->fields.c, 0.0f, value8, 0x1.2000000000000p+3f);
    part9 = mc_client_model_graph_part_allocate(g, "sc", 150);
    if (part9 == NULL)
        return false;
    mc_client_model_graph_part_construct(part9, 18, 0);
    (model)->fields.d = part9; /* sc:160 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.e000000000000p+3f,
                                   -0x1.0000000000000p+0f, -0x1.0000000000000p+0f, 16, 2, 2,
                                   local1))
        return false;
    value10 = mc_java_i32_add(0, local2);
    value11 = (float)(value10);
    mc_client_model_graph_pivot((model)->fields.d, -0x1.0000000000000p+2f, value11, 2.0f);
    part12 = mc_client_model_graph_part_allocate(g, "sc", 196);
    if (part12 == NULL)
        return false;
    mc_client_model_graph_part_construct(part12, 18, 0);
    (model)->fields.e = part12; /* sc:206 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, -0x1.0000000000000p+0f,
                                   -0x1.0000000000000p+0f, -0x1.0000000000000p+0f, 16, 2, 2,
                                   local1))
        return false;
    value13 = mc_java_i32_add(0, local2);
    value14 = (float)(value13);
    mc_client_model_graph_pivot((model)->fields.e, 0x1.0000000000000p+2f, value14, 2.0f);
    part15 = mc_client_model_graph_part_allocate(g, "sc", 242);
    if (part15 == NULL)
        return false;
    mc_client_model_graph_part_construct(part15, 18, 0);
    (model)->fields.f = part15; /* sc:252 */
    if (!mc_client_model_graph_box(g, (model)->fields.f, -0x1.e000000000000p+3f,
                                   -0x1.0000000000000p+0f, -0x1.0000000000000p+0f, 16, 2, 2,
                                   local1))
        return false;
    value16 = mc_java_i32_add(0, local2);
    value17 = (float)(value16);
    mc_client_model_graph_pivot((model)->fields.f, -0x1.0000000000000p+2f, value17, 1.0f);
    part18 = mc_client_model_graph_part_allocate(g, "sc", 288);
    if (part18 == NULL)
        return false;
    mc_client_model_graph_part_construct(part18, 18, 0);
    (model)->fields.g = part18; /* sc:298 */
    if (!mc_client_model_graph_box(g, (model)->fields.g, -0x1.0000000000000p+0f,
                                   -0x1.0000000000000p+0f, -0x1.0000000000000p+0f, 16, 2, 2,
                                   local1))
        return false;
    value19 = mc_java_i32_add(0, local2);
    value20 = (float)(value19);
    mc_client_model_graph_pivot((model)->fields.g, 0x1.0000000000000p+2f, value20, 1.0f);
    part21 = mc_client_model_graph_part_allocate(g, "sc", 334);
    if (part21 == NULL)
        return false;
    mc_client_model_graph_part_construct(part21, 18, 0);
    (model)->fields.h = part21; /* sc:344 */
    if (!mc_client_model_graph_box(g, (model)->fields.h, -0x1.e000000000000p+3f,
                                   -0x1.0000000000000p+0f, -0x1.0000000000000p+0f, 16, 2, 2,
                                   local1))
        return false;
    value22 = mc_java_i32_add(0, local2);
    value23 = (float)(value22);
    mc_client_model_graph_pivot((model)->fields.h, -0x1.0000000000000p+2f, value23, 0.0f);
    part24 = mc_client_model_graph_part_allocate(g, "sc", 380);
    if (part24 == NULL)
        return false;
    mc_client_model_graph_part_construct(part24, 18, 0);
    (model)->fields.i = part24; /* sc:390 */
    if (!mc_client_model_graph_box(g, (model)->fields.i, -0x1.0000000000000p+0f,
                                   -0x1.0000000000000p+0f, -0x1.0000000000000p+0f, 16, 2, 2,
                                   local1))
        return false;
    value25 = mc_java_i32_add(0, local2);
    value26 = (float)(value25);
    mc_client_model_graph_pivot((model)->fields.i, 0x1.0000000000000p+2f, value26, 0.0f);
    part27 = mc_client_model_graph_part_allocate(g, "sc", 426);
    if (part27 == NULL)
        return false;
    mc_client_model_graph_part_construct(part27, 18, 0);
    (model)->extra_j = part27; /* sc:436 */
    if (!mc_client_model_graph_box(g, (model)->extra_j, -0x1.e000000000000p+3f,
                                   -0x1.0000000000000p+0f, -0x1.0000000000000p+0f, 16, 2, 2,
                                   local1))
        return false;
    value28 = mc_java_i32_add(0, local2);
    value29 = (float)(value28);
    mc_client_model_graph_pivot((model)->extra_j, -0x1.0000000000000p+2f, value29,
                                -0x1.0000000000000p+0f);
    part30 = mc_client_model_graph_part_allocate(g, "sc", 473);
    if (part30 == NULL)
        return false;
    mc_client_model_graph_part_construct(part30, 18, 0);
    (model)->extra_k = part30; /* sc:483 */
    if (!mc_client_model_graph_box(g, (model)->extra_k, -0x1.0000000000000p+0f,
                                   -0x1.0000000000000p+0f, -0x1.0000000000000p+0f, 16, 2, 2,
                                   local1))
        return false;
    value31 = mc_java_i32_add(0, local2);
    value32 = (float)(value31);
    mc_client_model_graph_pivot((model)->extra_k, 0x1.0000000000000p+2f, value32,
                                -0x1.0000000000000p+0f);
    return true;
}

static bool model_td_void(mc_client_model_graph *g, manager_model *model) {
    int32_t local1 = 0;
    mc_client_model_part *part1 = NULL;
    int32_t value2 = 0;
    float value3 = 0;
    mc_client_model_part *part4 = NULL;
    int32_t value5 = 0;
    float value6 = 0;
    mc_client_model_part *part7 = NULL;
    int32_t value8 = 0;
    float value9 = 0;
    mc_client_model_part *part10 = NULL;
    int32_t value11 = 0;
    float value12 = 0;
    mc_client_model_part *part13 = NULL;
    int32_t value14 = 0;
    float value15 = 0;
    mc_client_model_part *part16 = NULL;
    int32_t value17 = 0;
    float value18 = 0;
    mc_client_model_part *part19 = NULL;
    int32_t value20 = 0;
    float value21 = 0;
    mc_client_model_part *part22 = NULL;
    int32_t value23 = 0;
    float value24 = 0;
    model->fields.n = false; /* KO:6, no FH construction. */
    local1 = 16;
    part1 = mc_client_model_graph_part_allocate(g, "td", 8);
    if (part1 == NULL)
        return false;
    mc_client_model_graph_part_construct(part1, 0, 0);
    (model)->fields.a = part1; /* td:17 */
    if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.0000000000000p+1f,
                                   -0x1.8000000000000p+2f, -0x1.0000000000000p+1f, 4, 6, 3, 0.0f))
        return false;
    value2 = mc_java_i32_add(-1, local1);
    value3 = (float)(value2);
    mc_client_model_graph_pivot((model)->fields.a, 0.0f, value3, -0x1.0000000000000p+2f);
    part4 = mc_client_model_graph_part_allocate(g, "td", 53);
    if (part4 == NULL)
        return false;
    mc_client_model_graph_part_construct(part4, 14, 0);
    (model)->fields.g = part4; /* td:63 */
    if (!mc_client_model_graph_box(g, (model)->fields.g, -0x1.0000000000000p+1f,
                                   -0x1.0000000000000p+2f, -0x1.0000000000000p+2f, 4, 2, 2, 0.0f))
        return false;
    value5 = mc_java_i32_add(-1, local1);
    value6 = (float)(value5);
    mc_client_model_graph_pivot((model)->fields.g, 0.0f, value6, -0x1.0000000000000p+2f);
    part7 = mc_client_model_graph_part_allocate(g, "td", 98);
    if (part7 == NULL)
        return false;
    mc_client_model_graph_part_construct(part7, 14, 4);
    (model)->fields.h = part7; /* td:108 */
    if (!mc_client_model_graph_box(g, (model)->fields.h, -0x1.0000000000000p+0f,
                                   -0x1.0000000000000p+1f, -0x1.8000000000000p+1f, 2, 2, 2, 0.0f))
        return false;
    value8 = mc_java_i32_add(-1, local1);
    value9 = (float)(value8);
    mc_client_model_graph_pivot((model)->fields.h, 0.0f, value9, -0x1.0000000000000p+2f);
    part10 = mc_client_model_graph_part_allocate(g, "td", 143);
    if (part10 == NULL)
        return false;
    mc_client_model_graph_part_construct(part10, 0, 9);
    (model)->fields.b = part10; /* td:153 */
    if (!mc_client_model_graph_box(g, (model)->fields.b, -0x1.8000000000000p+1f,
                                   -0x1.0000000000000p+2f, -0x1.8000000000000p+1f, 6, 8, 6, 0.0f))
        return false;
    value11 = mc_java_i32_add(0, local1);
    value12 = (float)(value11);
    mc_client_model_graph_pivot((model)->fields.b, 0.0f, value12, 0.0f);
    part13 = mc_client_model_graph_part_allocate(g, "td", 190);
    if (part13 == NULL)
        return false;
    mc_client_model_graph_part_construct(part13, 26, 0);
    (model)->fields.c = part13; /* td:200 */
    if (!mc_client_model_graph_box(g, (model)->fields.c, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.8000000000000p+1f, 3, 5, 3, 0.0f))
        return false;
    value14 = mc_java_i32_add(3, local1);
    value15 = (float)(value14);
    mc_client_model_graph_pivot((model)->fields.c, -0x1.0000000000000p+1f, value15, 1.0f);
    part16 = mc_client_model_graph_part_allocate(g, "td", 233);
    if (part16 == NULL)
        return false;
    mc_client_model_graph_part_construct(part16, 26, 0);
    (model)->fields.d = part16; /* td:243 */
    if (!mc_client_model_graph_box(g, (model)->fields.d, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.8000000000000p+1f, 3, 5, 3, 0.0f))
        return false;
    value17 = mc_java_i32_add(3, local1);
    value18 = (float)(value17);
    mc_client_model_graph_pivot((model)->fields.d, 1.0f, value18, 1.0f);
    part19 = mc_client_model_graph_part_allocate(g, "td", 275);
    if (part19 == NULL)
        return false;
    mc_client_model_graph_part_construct(part19, 24, 13);
    (model)->fields.e = part19; /* td:286 */
    if (!mc_client_model_graph_box(g, (model)->fields.e, 0.0f, 0.0f, -0x1.8000000000000p+1f, 1, 4,
                                   6, 0.0f))
        return false;
    value20 = mc_java_i32_add(-3, local1);
    value21 = (float)(value20);
    mc_client_model_graph_pivot((model)->fields.e, -0x1.0000000000000p+2f, value21, 0.0f);
    part22 = mc_client_model_graph_part_allocate(g, "td", 320);
    if (part22 == NULL)
        return false;
    mc_client_model_graph_part_construct(part22, 24, 13);
    (model)->fields.f = part22; /* td:331 */
    if (!mc_client_model_graph_box(g, (model)->fields.f, -0x1.0000000000000p+0f, 0.0f,
                                   -0x1.8000000000000p+1f, 1, 4, 6, 0.0f))
        return false;
    value23 = mc_java_i32_add(-3, local1);
    value24 = (float)(value23);
    mc_client_model_graph_pivot((model)->fields.f, 0x1.0000000000000p+2f, value24, 0.0f);
    return true;
}

static bool model_wn_void(mc_client_model_graph *g, manager_model *model) {
    manager_part_array *array1 = NULL;
    int32_t local1 = 0;
    mc_client_model_part *part2 = NULL;
    int32_t value3 = 0;
    float value4 = 0;
    float value5 = 0;
    int32_t local2 = 0;
    mc_client_model_part *part6 = NULL;
    double value7 = 0;
    double value8 = 0;
    double value9 = 0;
    double value10 = 0;
    double value11 = 0;
    double local3 = 0;
    double value12 = 0;
    float value13 = 0;
    float value14 = 0;
    float local5 = 0;
    double value15 = 0;
    float value16 = 0;
    float value17 = 0;
    float local6 = 0;
    int32_t value18 = 0;
    float value19 = 0;
    double value20 = 0;
    double value21 = 0;
    double value22 = 0;
    double value23 = 0;
    double value24 = 0;
    double value25 = 0;
    float value26 = 0;
    model->fields.n = false; /* KO:6, no FH construction. */
    array1 = manager_part_array_new(g, 8, "wn", 7);
    if (array1 == NULL)
        return false;
    (model)->array_b = array1; /* wn:10 */
    local1 = -16;
    part2 = mc_client_model_graph_part_allocate(g, "wn", 17);
    if (part2 == NULL)
        return false;
    mc_client_model_graph_part_construct(part2, 0, 0);
    (model)->fields.a = part2; /* wn:26 */
    if (!mc_client_model_graph_box(g, (model)->fields.a, -0x1.8000000000000p+2f,
                                   -0x1.0000000000000p+3f, -0x1.8000000000000p+2f, 12, 16, 12,
                                   0.0f))
        return false;
    value3 = mc_java_i32_add(24, local1);
    value4 = (float)(value3);
    value5 = (((model)->fields.a)->b) + (value4);
    ((model)->fields.a)->b = value5; /* wn:62 */
    local2 = 0;
    while (local2 < ((model)->array_b)->length) {
        part6 = mc_client_model_graph_part_allocate(g, "wn", 81);
        if (part6 == NULL)
            return false;
        mc_client_model_graph_part_construct(part6, 48, 0);
        ((model)->array_b)->elements[local2] = part6; /* wn:91 */
        value7 = (double)(local2);
        value8 = (value7) * (0x1.921fb54442d18p+1);
        value9 = (value8) * (0x1.0000000000000p+1);
        value10 = (double)(((model)->array_b)->length);
        value11 = (value9) / (value10);
        local3 = value11;
        value12 = mc_math_native_double_cos(local3);
        value13 = (float)(value12);
        value14 = (value13) * (0x1.4000000000000p+2f);
        local5 = value14;
        value15 = mc_math_native_double_sin(local3);
        value16 = (float)(value15);
        value17 = (value16) * (0x1.4000000000000p+2f);
        local6 = value17;
        if (!mc_client_model_graph_box(g, ((model)->array_b)->elements[local2],
                                       -0x1.0000000000000p+0f, 0.0f, -0x1.0000000000000p+0f, 2, 18,
                                       2, 0.0f))
            return false;
        (((model)->array_b)->elements[local2])->a = local5; /* wn:156 */
        (((model)->array_b)->elements[local2])->c = local6; /* wn:167 */
        value18 = mc_java_i32_add(31, local1);
        value19 = (float)(value18);
        (((model)->array_b)->elements[local2])->b = value19; /* wn:181 */
        value20 = (double)(local2);
        value21 = (value20) * (0x1.921fb54442d18p+1);
        value22 = (value21) * (-0x1.0000000000000p+1);
        value23 = (double)(((model)->array_b)->length);
        value24 = (value22) / (value23);
        value25 = (value24) + (0x1.921fb54442d18p+0);
        local3 = value25;
        value26 = (float)(local3);
        (((model)->array_b)->elements[local2])->e = value26; /* wn:214 */
        local2 = mc_java_i32_add(local2, 1);
    }
    return true;
}

manager_part_array *manager_part_array_new(mc_client_model_graph *g, int32_t length,
                                           const char *source, int32_t pc) {
    if (length < 0 ||
        (size_t)length > (SIZE_MAX - sizeof(manager_part_array)) / sizeof(mc_client_model_part *)) {
        *g->failure = (mc_client_model_error){
            .status = MC_CLIENT_MODEL_LIMIT, .source_class = source, .source_pc = pc};
        return NULL;
    }
    manager_part_array *p = mc_client_model_graph_allocate(
        g, sizeof(*p) + (size_t)length * sizeof(*p->elements), source, pc);
    if (p != NULL)
        p->length = length;
    return p;
}

manager_model *manager_model_allocate(mc_client_model_graph *g, const char *source_class,
                                      const char *descriptor, const char *caller, int32_t pc) {
    manager_model *p = mc_client_model_graph_allocate(g, sizeof(*p), caller, pc);
    if (p != NULL) {
        p->source_class = source_class;
        p->descriptor = descriptor;
    }
    return p;
}

bool manager_model_construct(mc_client_model_graph *g, manager_model *m, float f, int32_t i) {
    if (m == NULL || m->phase != MC_CLIENT_MODEL_RAW)
        return false;
    m->phase = MC_CLIENT_MODEL_CONSTRUCTING;
    bool ok = false;
    if (strcmp(m->source_class, "ea") == 0)
        ok = model_ea_void(g, m);
    else if (strcmp(m->source_class, "eh") == 0)
        ok = (strcmp(m->descriptor, "()V") == 0 ? model_eh_void(g, m) : model_eh_F(g, m, f));
    else if (strcmp(m->source_class, "ej") == 0)
        ok = model_ej_void(g, m);
    else if (strcmp(m->source_class, "ez") == 0)
        ok = model_ez_void(g, m);
    else if (strcmp(m->source_class, "hh") == 0)
        ok = model_hh_void(g, m);
    else if (strcmp(m->source_class, "il") == 0)
        ok = (strcmp(m->descriptor, "()V") == 0 ? model_il_void(g, m) : model_il_F(g, m, f));
    else if (strcmp(m->source_class, "lc") == 0)
        ok = model_lc_void(g, m);
    else if (strcmp(m->source_class, "lo") == 0)
        ok = model_lo_void(g, m);
    else if (strcmp(m->source_class, "mw") == 0)
        ok = model_mw_void(g, m);
    else if (strcmp(m->source_class, "nn") == 0)
        ok = model_nn_IF(g, m, i, f);
    else if (strcmp(m->source_class, "no") == 0)
        ok = model_no_I(g, m, i);
    else if (strcmp(m->source_class, "nq") == 0)
        ok = model_nq_void(g, m);
    else if (strcmp(m->source_class, "o") == 0)
        ok = model_o_void(g, m);
    else if (strcmp(m->source_class, "sc") == 0)
        ok = model_sc_void(g, m);
    else if (strcmp(m->source_class, "td") == 0)
        ok = model_td_void(g, m);
    else if (strcmp(m->source_class, "wn") == 0)
        ok = model_wn_void(g, m);
    else if (strcmp(m->source_class, "fh") == 0)
        ok = mc_client_model_graph_fill_biped(g, &m->fields, f, 0.0f);
    else
        *g->failure = (mc_client_model_error){.status = MC_CLIENT_MODEL_INVALID_ARGUMENT,
                                              .source_class = m->source_class,
                                              .source_pc = 0};
    m->phase = ok ? MC_CLIENT_MODEL_READY : MC_CLIENT_MODEL_FAILED;
    return ok;
}
