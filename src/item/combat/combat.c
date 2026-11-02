#include "mc/item/combat/combat.h"

#include "../../block/registry/registry_internal.h"
#include "../../entity/entity_internal.h"
#include "../../entity/player/statistics_status_internal.h"
#include "../render/internal.h"
#include "../stack/source_internal.h"
#include "mc/core/java/java.h"

#include <string.h>

typedef enum combat_body { BODY_NONE = 0, BODY_GM, BODY_DQ, BODY_QD, BODY_SA } combat_body;

/* Complete supplied-client declaration audit, including DQ/CP intermediates.
 * Every represented kind has its own entry. A future kind has BODY_NONE until
 * its actual method resolution is independently established. */
static const combat_body bodies[MC_ITEM_KIND_COUNT] = {[MC_ITEM_BASE] = BODY_GM,
                                                       [MC_ITEM_SHOVEL] = BODY_DQ,
                                                       [MC_ITEM_PICKAXE] = BODY_DQ,
                                                       [MC_ITEM_AXE] = BODY_DQ,
                                                       [MC_ITEM_FLINT_AND_STEEL] = BODY_GM,
                                                       [MC_ITEM_FOOD] = BODY_GM,
                                                       [MC_ITEM_BOW] = BODY_GM,
                                                       [MC_ITEM_COAL] = BODY_GM,
                                                       [MC_ITEM_SWORD] = BODY_QD,
                                                       [MC_ITEM_HOE] = BODY_GM,
                                                       [MC_ITEM_SEEDS] = BODY_GM,
                                                       [MC_ITEM_ARMOR] = BODY_GM,
                                                       [MC_ITEM_PAINTING] = BODY_GM,
                                                       [MC_ITEM_SIGN] = BODY_GM,
                                                       [MC_ITEM_DOOR] = BODY_GM,
                                                       [MC_ITEM_BUCKET] = BODY_GM,
                                                       [MC_ITEM_MINECART] = BODY_GM,
                                                       [MC_ITEM_SADDLE] = BODY_SA,
                                                       [MC_ITEM_REDSTONE] = BODY_GM,
                                                       [MC_ITEM_SNOWBALL] = BODY_GM,
                                                       [MC_ITEM_BOAT] = BODY_GM,
                                                       [MC_ITEM_BLOCK_PLACER] = BODY_GM,
                                                       [MC_ITEM_EGG] = BODY_GM,
                                                       [MC_ITEM_FISHING_ROD] = BODY_GM,
                                                       [MC_ITEM_DYE] = BODY_GM,
                                                       [MC_ITEM_BED] = BODY_GM,
                                                       [MC_ITEM_SHEARS] = BODY_GM,
                                                       [MC_ITEM_RECORD] = BODY_GM,
                                                       [MC_ITEM_SOUP] = BODY_GM,
                                                       [MC_ITEM_COOKIE] = BODY_GM,
                                                       [MC_ITEM_MAP] = BODY_GM,
                                                       [MC_ITEM_BLOCK] = BODY_GM,
                                                       [MC_ITEM_WOOL_BLOCK] = BODY_GM,
                                                       [MC_ITEM_LOG_BLOCK] = BODY_GM,
                                                       [MC_ITEM_SLAB_BLOCK] = BODY_GM,
                                                       [MC_ITEM_SAPLING_BLOCK] = BODY_GM,
                                                       [MC_ITEM_LEAVES_BLOCK] = BODY_GM,
                                                       [MC_ITEM_PISTON_BLOCK] = BODY_GM};

static mc_item_combat_site site(const char *owner, const char *method, const char *descriptor,
                                int32_t pc) {
    return (mc_item_combat_site){owner, method, descriptor, pc};
}

void mc_item_combat_error_init(mc_item_combat_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->native_policy = true;
    e->site.pc = e->child.pc = e->dependency.pc = e->watcher.pc = -1;
    e->registry.initializer_pc = e->registry.constructor_pc = e->registry.active_initializer_pc =
        e->registry.block_id = e->registry.item_argument = -1;
    e->statistic.writer.index = e->statistic.writer.text_offset = SIZE_MAX;
}

static bool fail(mc_item_combat_error *e, mc_entity_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    e->native_policy = !e->source_exception && !e->source_error;
    return false;
}

static bool source(mc_item_combat_error *e, const char *name, const char *operation) {
    e->source_exception = true;
    e->exception_class = name;
    return fail(e, MC_ENTITY_STATE_ERROR, operation);
}

static bool null_reference(mc_item_combat_error *e, const char *operation) {
    return source(e, "java/lang/NullPointerException", operation);
}

static mc_entity_status registry_status(mc_registry_status status) {
    return status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
           : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                     : MC_ENTITY_STATE_ERROR;
}

static bool registry_failed(mc_registry_classes *classes, mc_item_combat_error *e) {
    mc_item_render_throwable throwable;
    mc_item_render_registry_classification(classes, &e->registry, &throwable, &e->exception_class);
    e->source_exception = throwable == MC_ITEM_RENDER_SOURCE_EXCEPTION;
    e->source_error = throwable == MC_ITEM_RENDER_SOURCE_ERROR;
    return fail(e, registry_status(e->registry.status), e->registry.message);
}

static bool registry_admit(mc_registry_classes *classes, mc_item_combat_error *e) {
    if (classes == NULL || classes->options.side != MC_REGISTRY_CLIENT)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item combat actual client code Registry");
    if (classes->entered != 0)
        return fail(e, MC_ENTITY_BUSY, "Item combat quiescent Registry");
    return true;
}

static bool item_admit(mc_registry_classes *classes, const mc_item *item, mc_item_combat_error *e) {
    if (item == NULL)
        return null_reference(e, "Item combat null invocation receiver");
    if (!registry_admit(classes, e))
        return false;
    if (item->classes != classes || (unsigned int)item->kind >= (unsigned int)MC_ITEM_KIND_COUNT)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item combat actual receiver storage namespace");
    if (bodies[item->kind] == BODY_NONE)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item combat unproved concrete virtual body");
    return true;
}

static bool stack_result(mc_item_combat_error *e, mc_item_stack_status status,
                         const char *operation) {
    e->stack = status;
    return status == MC_ITEM_STACK_OK ||
           fail(e, status == MC_ITEM_STACK_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR,
                operation);
}

static bool instance(mc_registry_classes *classes, mc_entity *value, mc_entity_class_kind kind,
                     bool *out, mc_item_combat_error *e) {
    *out = false;
    if (value == NULL)
        return true;
    const mc_entity_class_literal *literal = NULL;
    if (mc_entity_class_literal_get(classes->entities, kind, &literal) != MC_ENTITY_OK ||
        mc_entity_class_literal_is_assignable_from(literal, value->class_literal, out) !=
            MC_ENTITY_OK)
        return fail(e, MC_ENTITY_STATE_ERROR, "Item combat actual Class identity");
    return true;
}

mc_entity_status mc_item_combat_damage(mc_registry_classes *classes, const mc_item *item,
                                       mc_entity *target, int32_t *out,
                                       mc_item_combat_error *error) {
    (void)target; /* None of the actual38 resolved bodies reads this argument. */
    mc_item_combat_error e;
    mc_item_combat_error_init(&e);
    e.site = site("gm", "a", "(Lsn;)I", 0);
    if (out == NULL)
        (void)fail(&e, MC_ENTITY_INVALID_ARGUMENT, "Item combat damage native output");
    else if (item_admit(classes, item, &e)) {
        const combat_body body = bodies[item->kind];
        if (body == BODY_DQ) {
            e.site = site("dq", "a", "(Lsn;)I", 1);
            *out = item->dq.bm;
            e.site.pc = 4;
        } else if (body == BODY_QD) {
            e.site = site("qd", "a", "(Lsn;)I", 1);
            *out = item->subtype.qd.a;
            e.site.pc = 4;
        } else {
            *out = 1;
            e.site.pc = 1;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static void durability_at(mc_item_combat_error *e, bool nested, int32_t pc) {
    mc_item_combat_site *s = nested ? &e->child : &e->site;
    *s = site("iz", "a", "(ILsn;)V", pc);
    e->dependency = (mc_item_combat_site){.pc = -1};
}

/* IZ.e and IZ.j each execute their own getstatic0 before Stack.id4 and
 * aaload7. Neither is GM.g: subtype flags do not change maximum>0 here. */
static bool maximum(mc_registry_classes *classes, mc_item_stack *stack, const char *method,
                    int32_t *out, mc_item_combat_error *e) {
    e->dependency = site("iz", method, method[0] == 'e' ? "()Z" : "()I", 0);
    if (!registry_admit(classes, e))
        return false;
    const mc_registry_item_array *array = NULL;
    if (mc_registry_item_c_get(classes, &array, &e->registry) != MC_REGISTRY_OK)
        return registry_failed(classes, e);
    e->dependency.pc = 4;
    int32_t id;
    if (!stack_result(e, mc_item_stack_source_id_get(stack, &id),
                      "Item durability actual Stack id field"))
        return false;
    e->dependency.pc = 7;
    const mc_item *item = NULL;
    if (mc_registry_item_c_at(classes, array, id, &item, &e->registry) != MC_REGISTRY_OK)
        return registry_failed(classes, e);
    e->dependency.pc = 8;
    if (item == NULL)
        return null_reference(e, "Item durability null maximum Item invocation receiver");
    if (!item_admit(classes, item, e))
        return false;
    /* Every supplied descendant inherits GM.f: field1/return4. */
    *out = item->fields.a;
    return true;
}

static bool statistic_failed(mc_item_combat_error *e) {
    if (e->statistic.status == MC_PLAYER_STATISTIC_NULL_REFERENCE ||
        (e->statistic.status == MC_PLAYER_STATISTIC_WRITER_FAILED &&
         e->statistic.writer.status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
         e->statistic.writer.exception == MC_CLIENT_STATS_NULL_REFERENCE)) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    }
    e->dependency = site(e->statistic.site.class_name, e->statistic.site.method,
                         e->statistic.site.descriptor, (int32_t)e->statistic.site.pc);
    return fail(e, mc_player_statistic_entity_status(&e->statistic), e->statistic.operation);
}

static bool durability(mc_registry_classes *classes, mc_item_stack *stack, int32_t amount,
                       mc_entity *attacker, bool nested, mc_item_combat_error *e) {
    e->durability_entered = true;
    e->amount = amount;
    durability_at(e, nested, 1);
    if (stack == NULL)
        return null_reference(e, "Item durability null Stack e invocation receiver");
    if (!maximum(classes, stack, "e", &e->damageability_maximum, e))
        return false;
    if (e->damageability_maximum <= 0) {
        durability_at(e, nested, 7);
        return true;
    }
    durability_at(e, nested, 10);
    if (!stack_result(e, mc_item_stack_source_damage_get(stack, &e->damage_before),
                      "Item durability actual damage field"))
        return false;
    const int32_t next = mc_java_i32_add(e->damage_before, amount);
    durability_at(e, nested, 15);
    if (!stack_result(e, mc_item_stack_source_damage_set(stack, next),
                      "Item durability wrapped damage store"))
        return false;
    e->damage_stored = true;
    e->damage_after = next;
    durability_at(e, nested, 19);
    int32_t comparison_damage;
    if (!stack_result(e, mc_item_stack_source_damage_get(stack, &comparison_damage),
                      "Item durability comparison damage reload"))
        return false;
    durability_at(e, nested, 23);
    if (!maximum(classes, stack, "j", &e->maximum, e))
        return false;
    if (comparison_damage <= e->maximum) {
        durability_at(e, nested, 79);
        return true;
    }
    e->break_selected = true;
    durability_at(e, nested, 30);
    bool player;
    if (!instance(classes, attacker, MC_ENTITY_CLASS_KIND_PLAYER, &player, e))
        return false;
    if (player) {
        durability_at(e, nested, 40);
        if (mc_registry_classes_initialize_statistics(classes, &e->registry) != MC_REGISTRY_OK)
            return registry_failed(classes, e);
        mc_statistic *const *array = classes->statistics.F;
        durability_at(e, nested, 44);
        int32_t id;
        if (!stack_result(e, mc_item_stack_source_id_get(stack, &id),
                          "Item durability broken statistic late Stack id"))
            return false;
        durability_at(e, nested, 47);
        if (array == NULL)
            return null_reference(e, "Item durability null captured broken-statistic array");
        if (id < 0 || id >= 32000)
            return source(e, "java/lang/ArrayIndexOutOfBoundsException",
                          "Item durability broken-statistic signed array index");
        const mc_statistic *statistic = array[id];
        durability_at(e, nested, 49);
        e->statistic_entered = true;
        if (mc_local_player_add_statistic(attacker, statistic, 1, &e->statistic) !=
            MC_PLAYER_STATISTIC_OK)
            return statistic_failed(e);
        e->statistic_returned = true;
    }
    durability_at(e, nested, 54);
    if (!stack_result(e, mc_item_stack_source_count_get(stack, &e->count_before),
                      "Item durability actual count reload"))
        return false;
    e->count_read = true;
    e->count_after = mc_java_i32_sub(e->count_before, 1);
    durability_at(e, nested, 59);
    if (!stack_result(e, mc_item_stack_source_count_set(stack, e->count_after),
                      "Item durability wrapped count decrement store"))
        return false;
    e->count_stored = true;
    durability_at(e, nested, 63);
    int32_t current;
    if (!stack_result(e, mc_item_stack_source_count_get(stack, &current),
                      "Item durability negative count reload"))
        return false;
    if (current < 0) {
        durability_at(e, nested, 71);
        if (!stack_result(e, mc_item_stack_source_count_set(stack, 0),
                          "Item durability negative count clamp store"))
            return false;
        e->count_clamped = true;
        e->count_after = 0;
    }
    durability_at(e, nested, 76);
    if (!stack_result(e, mc_item_stack_source_damage_set(stack, 0),
                      "Item durability broken damage reset store"))
        return false;
    e->damage_reset = true;
    e->damage_after = 0;
    durability_at(e, nested, 79);
    return true;
}

mc_entity_status mc_item_stack_combat_damage(mc_registry_classes *classes, mc_item_stack *stack,
                                             int32_t amount, mc_entity *attacker,
                                             mc_item_combat_error *error) {
    mc_item_combat_error e;
    mc_item_combat_error_init(&e);
    (void)durability(classes, stack, amount, attacker, false, &e);
    if (error != NULL)
        *error = e;
    return e.status;
}

static mc_entity_watch_record *pig_watch(mc_entity *pig, bool setter, mc_item_combat_error *e) {
    e->dependency = site("wh", setter ? "a" : "v", setter ? "(Z)V" : "()Z", setter ? 14 : 6);
    if (pig->watcher == NULL) {
        (void)null_reference(e, "Saddle null Pig watcher invocation receiver");
        return NULL;
    }
    e->watcher = site("ud", setter ? "b" : "a", setter ? "(ILjava/lang/Object;)V" : "(I)B", 8);
    mc_entity_watch_map *map = pig->watcher->map;
    if (map == NULL) {
        (void)null_reference(e, "Saddle Pig watcher null Map invocation receiver");
        return NULL;
    }
    mc_entity_watch_record *record = NULL;
    size_t seen = 0;
    for (mc_entity_watch_node *node = map->head; node != NULL; node = node->next) {
        if (seen++ >= map->size) {
            (void)fail(e, MC_ENTITY_STATE_ERROR, "Saddle Pig watcher Map native ownership");
            return NULL;
        }
        if (node->key == 16) {
            record = node->record;
            break;
        }
    }
    e->watcher.pc = setter ? 19 : 16;
    if (record == NULL) {
        (void)null_reference(e, "Saddle Pig watcher null record receiver");
        return NULL;
    }
    if (record->type != 0) {
        e->watcher.pc = setter ? 22 : 19;
        if (setter)
            (void)fail(e, MC_ENTITY_STATE_ERROR, "Saddle Pig setter boxed native representation");
        else if (record->type == 4 && record->string_value == NULL) {
            e->watcher.pc = 22;
            (void)null_reference(e, "Saddle Pig watcher null Byte unbox receiver");
        } else
            (void)source(e, "java/lang/ClassCastException", "Saddle Pig watcher boxed Byte cast");
        return NULL;
    }
    return record;
}

static bool saddle(mc_registry_classes *classes, mc_item_stack *stack, mc_entity *target,
                   mc_item_combat_error *e) {
    e->site.pc = 3;
    e->child = site("sa", "a", "(Liz;Lls;)V", 1);
    bool pig;
    if (!instance(classes, target, MC_ENTITY_CLASS_KIND_PIG, &pig, e))
        return false;
    if (!pig) {
        e->child.pc = 34;
        return true;
    }
    e->site.pc = 3;
    e->child = site("sa", "a", "(Liz;Lls;)V", 13);
    mc_entity_watch_record *record = pig_watch(target, false, e);
    if (record == NULL)
        return false;
    if ((record->byte_value & 1) != 0) {
        e->child = site("sa", "a", "(Liz;Lls;)V", 34);
        return true;
    }
    e->child = site("sa", "a", "(Liz;Lls;)V", 21);
    record = pig_watch(target, true, e);
    if (record == NULL)
        return false;
    if (record->byte_value != 1) {
        record->byte_value = 1; /* WH.a(true) replaces the complete Byte. */
        record->dirty = true;
        target->watcher->dirty = true;
    }
    e->saddle_stored = true;
    e->child = site("sa", "a", "(Liz;Lls;)V", 26);
    e->dependency = (mc_item_combat_site){.pc = -1};
    e->watcher = (mc_item_combat_site){.pc = -1};
    if (stack == NULL)
        return null_reference(e, "Saddle null Stack count field receiver after Pig store");
    if (!stack_result(e, mc_item_stack_source_count_get(stack, &e->count_before),
                      "Saddle actual Stack count reload"))
        return false;
    e->count_read = true;
    e->count_after = mc_java_i32_sub(e->count_before, 1);
    e->child.pc = 31;
    if (!stack_result(e, mc_item_stack_source_count_set(stack, e->count_after),
                      "Saddle wrapped Stack count store"))
        return false;
    e->count_stored = true;
    e->child.pc = 34;
    return true;
}

mc_entity_status mc_item_combat_hit(mc_registry_classes *classes, const mc_item *item,
                                    mc_item_stack *stack, mc_entity *target, mc_entity *attacker,
                                    bool *out, mc_item_combat_error *error) {
    mc_item_combat_error e;
    mc_item_combat_error_init(&e);
    e.site = site("gm", "a", "(Liz;Lls;Lls;)Z", 0);
    if (out == NULL)
        (void)fail(&e, MC_ENTITY_INVALID_ARGUMENT, "Item combat hit native output");
    else if (item_admit(classes, item, &e)) {
        const combat_body body = bodies[item->kind];
        if (body == BODY_GM) {
            *out = false;
            e.site.pc = 1;
        } else if (body == BODY_DQ || body == BODY_QD) {
            e.site = site(body == BODY_DQ ? "dq" : "qd", "a", "(Liz;Lls;Lls;)Z", 3);
            if (durability(classes, stack, body == BODY_DQ ? 2 : 1, attacker, true, &e)) {
                *out = true;
                e.site.pc = 7;
            }
        } else if (body == BODY_SA) {
            e.site = site("sa", "a", "(Liz;Lls;Lls;)Z", 3);
            if (saddle(classes, stack, target, &e)) {
                *out = true;
                e.site.pc = 7;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
