#include "../../../item/stack/source_internal.h"
#include "../../player/player_internal.h"
#include "internal.h"
#include "mc/item/registry/runtime.h"
#include <string.h>

static bool owner_player(mc_mob_tick_scope *s, const mc_player_string *name, mc_entity **out) {
    mc_world_runtime *w;
    if (!mt_world(s, &w))
        return false;
    if (mc_world_runtime_call_enter(w, &s->error->world) != MC_WORLD_STORAGE_OK)
        return mt_fail(s, MC_ENTITY_WORLD_FAILED, s->error->world.operation);
    bool ok = false;
    *out = NULL;
    for (int32_t i = 0;; i = mc_java_i32_add(i, 1)) {
        s->error->child_site = (mc_mob_site){"fd", "a", "(Ljava/lang/String;)Lgs;", 7};
        mc_world_reference_list *list = w->fields.d;
        const mc_world_entity_list_ledger *ledger = &w->player_membership;
        if (list == NULL) {
            mt_null(s, "Wolf owner lookup null Player List");
            break;
        }
        if (list != ledger->list || list->count != ledger->count) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf owner lookup actual Player List ownership");
            break;
        }
        if (i >= 0 && (size_t)(uint32_t)i >= list->count) {
            ok = true;
            break;
        }
        s->error->child_site.pc = 21;
        if (!mt_work(s))
            break;
        if (i < 0 || list->items == NULL || list->items[i] != ledger->entries[i]) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf owner lookup actual Player occurrence");
            break;
        }
        mc_entity *p = ledger->entries[i];
        if (p == NULL) {
            s->error->child_site.pc = 29;
            mt_null(s, "Wolf owner lookup null Player name receiver");
            break;
        }
        if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
            s->error->child_site.pc = 26;
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf owner lookup actual Player cast");
            break;
        }
        const mc_player_string *other = p->local_player->fields.player.l;
        s->error->child_site.pc = 32;
        if (name == NULL) {
            mt_null(s, "Wolf owner lookup null String equals receiver");
            break;
        }
        mc_player_string_view a = {0}, b = {0};
        if (!mc_player_string_read(name, &a) ||
            (other != NULL && !mc_player_string_read(other, &b))) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf owner lookup actual immutable String");
            break;
        }
        if (other != NULL && a.length == b.length &&
            (a.length == 0 || memcmp(a.units, b.units, a.length * sizeof(*a.units)) == 0)) {
            if (mc_entity_retain(p) != MC_ENTITY_OK) {
                mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf owner Player local hold");
                break;
            }
            *out = p;
            ok = true;
            break;
        }
    }
    mc_world_runtime_call_leave(w);
    if (ok)
        s->error->child_site = (mc_mob_site){0};
    return ok;
}

static bool flag(mc_mob_tick_scope *s, int mask, bool *out) {
    int8_t value;
    if (!mt_watch_byte(s, 16, &value))
        return false;
    *out = (value & mask) != 0;
    return true;
}

static bool sitting(mc_mob_tick_scope *s, bool value) {
    int8_t old;
    if (!mt_watch_byte(s, 16, &old))
        return false;
    return mt_watch_byte_set(s, 16, mc_java_i8_narrow(value ? old | 1 : old & -2));
}

static bool hunt(mc_mob_tick_scope *s) {
    mc_world_runtime *w;
    mt_at(s, "gi", "f_", "()V", 115);
    if (!mt_world(s, &w))
        return false;
    if (w->fields.r == NULL)
        return mt_null(s, "Wolf null World Random receiver");
    int32_t draw;
    if (!mc_java_random_next_bounded(w->fields.r, 100, &draw))
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf actual World Random bound");
    ++s->error->state.concrete_world_random_calls;
    if (draw != 0)
        return true;
    const mc_entity_class_literal *sheep = NULL;
    if (mc_entity_class_literal_get(s->entity->classes, MC_ENTITY_CLASS_KIND_SHEEP, &sheep) !=
        MC_ENTITY_OK)
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf actual Sheep Class literal");
    mc_entity_bounds *first = NULL, *expanded = NULL;
    mc_world_class_entity_list *list = NULL;
    bool ok = false;
    mt_at(s, "gi", "f_", "()V", 157);
    if (!mt_fp(s))
        return false;
    s->error->child = MC_MOB_TICK_CHILD_BOUNDS;
    const double x = s->entity->base.x, y = s->entity->base.y, z = s->entity->base.z;
    mc_entity_status status =
        mc_entity_bounds_pool_get(s->entity->classes, x, y, z, at_add(x, 1.0), at_add(y, 1.0),
                                  at_add(z, 1.0), &first, &s->error->detail.bounds);
    if (status != MC_ENTITY_OK) {
        mt_fail(s, status, "Wolf actual pooled hunt bounds");
        goto done;
    }
    mt_at(s, "gi", "f_", "()V", 169);
    status = mc_entity_bounds_expand(s->entity->classes, first, 16.0, 4.0, 16.0, &expanded,
                                     &s->error->detail.bounds);
    if (status != MC_ENTITY_OK) {
        mt_fail(s, status, "Wolf actual hunt bounds expansion");
        goto done;
    }
    mt_at(s, "gi", "f_", "()V", 172);
    s->error->child = MC_MOB_TICK_CHILD_CLASS_LIST;
    if (mc_world_runtime_class_entities(w, sheep, expanded, &list, &s->error->detail.class_list) !=
        MC_WORLD_STORAGE_OK) {
        mt_fail(s, MC_ENTITY_WORLD_FAILED, "Wolf actual Sheep Class List query");
        goto done;
    }
    size_t count;
    mt_at(s, "gi", "f_", "()V", 177);
    if (mc_world_class_entity_list_count(list, &count) != MC_WORLD_STORAGE_OK) {
        mt_fail(s, MC_ENTITY_WORLD_FAILED, "Wolf actual Sheep List isEmpty");
        goto done;
    }
    if (count != 0) {
        mt_at(s, "gi", "f_", "()V", 200);
        if (!mt_world(s, &w))
            goto done;
        if (w->fields.r == NULL) {
            mt_null(s, "Wolf selection null World Random receiver");
            goto done;
        }
        if (count > INT32_MAX || !mc_java_random_next_bounded(w->fields.r, (int32_t)count, &draw)) {
            mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf actual Sheep List Random bound");
            goto done;
        }
        ++s->error->state.concrete_world_random_calls;
        mc_entity *selected = NULL;
        mt_at(s, "gi", "f_", "()V", 203);
        if (mc_world_class_entity_list_get(list, (size_t)(uint32_t)draw, &selected) !=
                MC_WORLD_STORAGE_OK ||
            !mt_target_store(s, true, selected)) {
            if (s->error->status == MC_ENTITY_OK)
                mt_fail(s, MC_ENTITY_WORLD_FAILED, "Wolf actual Sheep List get");
            goto done;
        }
    }
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    ok = true;
done:
    mc_world_class_entity_list_release(list);
    if (expanded != NULL)
        mc_entity_bounds_release(s->entity->classes, expanded);
    if (first != NULL)
        mc_entity_bounds_release(s->entity->classes, first);
    return ok;
}

bool mt_wolf_behavior(mc_mob_tick_scope *s) {
    if (!mt_creature(s))
        return false;
    mc_mob_entity_state *m = s->entity->mob;
    bool tame;
    bool owner_branch = false;
    bool present = false;
    if (!m->creature_attacking) {
        mt_at(s, "gi", "f_", "()V", 12);
        if (!mt_path_present(s, &present))
            return false;
    }
    if (!m->creature_attacking && !present) {
        mt_at(s, "gi", "f_", "()V", 19);
        if (!flag(s, 4, &tame))
            return false;
        if (tame && s->entity->base.aH == NULL) {
            mt_at(s, "gi", "f_", "()V", 37);
            const mc_player_string *name;
            if (!mt_watch_string(s, 17, &name))
                return false;
            mc_entity *owner = NULL;
            mt_at(s, "gi", "f_", "()V", 40);
            if (!owner_player(s, name, &owner))
                return false;
            owner_branch = true;
            if (owner != NULL) {
                const double dx = at_sub(owner->base.x, s->entity->base.x);
                const double dy = at_sub(owner->base.y, s->entity->base.y);
                const double dz = at_sub(owner->base.z, s->entity->base.z);
                mt_at(s, "gi", "f_", "()V", 50);
                if (!mt_math(s) || !mt_fp(s)) {
                    mc_entity_release(owner);
                    return false;
                }
                const float distance =
                    at_root(at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz)));
                mc_entity_release(owner);
                if (distance > 5.0f) {
                    mt_at(s, "gi", "f_", "()V", 64);
                    s->error->child_site = (mc_mob_site){"gi", "c", "(Lsn;F)V", 8};
                    return mt_fail(s, MC_ENTITY_STATE_ERROR,
                                   "Wolf actual owner-follow World path child");
                }
            } else {
                bool in_water;
                mt_at(s, "gi", "f_", "()V", 71);
                if (!mt_water(s, &in_water) || (!in_water && !sitting(s, true)))
                    return false;
            }
        }
    }
    if (!owner_branch) {
        mc_entity *target;
        mt_at(s, "gi", "f_", "()V", 86);
        if (!mt_target_get(s, true, &target))
            return false;
        if (target == NULL) {
            mt_at(s, "gi", "f_", "()V", 93);
            if (!mt_path_present(s, &present))
                return false;
        }
        if (target == NULL && !present) {
            mt_at(s, "gi", "f_", "()V", 100);
            if (!flag(s, 4, &tame))
                return false;
            if (!tame && !hunt(s))
                return false;
        }
    }
    bool water;
    mt_at(s, "gi", "f_", "()V", 215);
    if (!mt_water(s, &water))
        return false;
    if (water && !sitting(s, false))
        return false;
    mc_world_runtime *w;
    mt_at(s, "gi", "f_", "()V", 230);
    if (!mt_world(s, &w))
        return false;
    if (!w->fields.B) {
        mt_at(s, "gi", "f_", "()V", 249);
        if (!mt_watch_int_set(s, 18, mt_fields(s)->Y))
            return false;
    }
    mt_at(s, "gi", "f_", "()V", 252);
    return true;
}

/* Each helper consumes exactly the Source operands selected by GI.o. The
 * enclosing real tick retains Registry/Math and the Wolf; this local owns its
 * actual selected Player and returned nonnull Stack through all later uses. */
static bool interest_registry(mc_mob_tick_scope *s, mc_registry_status status) {
    if (status == MC_REGISTRY_OK)
        return true;
    const mc_entity_status entity_status = status == MC_REGISTRY_LIMIT ? MC_ENTITY_LIMIT
                                           : status == MC_REGISTRY_ALLOCATION_FAILED
                                               ? MC_ENTITY_ALLOCATION_FAILED
                                           : status == MC_REGISTRY_BUSY ? MC_ENTITY_BUSY
                                                                        : MC_ENTITY_STATE_ERROR;
    (void)mt_fail(s, entity_status, "Wolf interest actual Item source operation");
    mt_registry_classify(s, &s->error->registry);
    if (s->error->registry.active_initializer_class != NULL)
        s->error->child_site =
            (mc_mob_site){s->error->registry.active_initializer_class, "<clinit>", "()V",
                          s->error->registry.active_initializer_pc};
    return false;
}

static bool interest_id(mc_mob_tick_scope *s, const mc_item_stack *stack, unsigned int pc,
                        int32_t *out) {
    mt_at(s, "gi", "o", "()V", pc);
    s->error->child_site = (mc_mob_site){"gi", "o", "()V", (int32_t)pc};
    const mc_item_stack_status status = mc_item_stack_source_id_get(stack, out);
    if (status != MC_ITEM_STACK_OK) {
        s->error->reference = (mc_entity_error){.status = MC_ENTITY_STATE_ERROR,
                                                .operation = "Wolf interest actual Stack id field",
                                                .item = status};
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf interest actual Stack id field");
    }
    mc_mob_tick_wolf_interest_state *r = &s->error->state.wolf_interest;
    r->stack_ids[r->id_reads++] = *out;
    return true;
}

static bool interest_tame(mc_mob_tick_scope *s, unsigned int pc, bool *out) {
    mt_at(s, "gi", "o", "()V", pc);
    s->error->child_site = (mc_mob_site){"gi", "D", "()Z", 6};
    ++s->error->state.wolf_interest.tame_calls;
    return flag(s, 4, out);
}

static bool player_interest(mc_mob_tick_scope *s, mc_entity *player) {
    mc_mob_tick_wolf_interest_state *r = &s->error->state.wolf_interest;
    r->entered = true;
    r->player_identity = (uintptr_t)player;
    bool held_player = false, held_stack = false, ok = false;
    mc_item_stack *stack = NULL;
    /* The Source instanceof/checkcast has already selected an actual gs.
     * Only canonical dc storage is represented here; no fake Player view. */
    mt_at(s, "gi", "o", "()V", 43);
    if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        player->local_player == NULL)
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf interest concrete Player storage");
    const mc_entity_status hold = mc_entity_retain(player);
    if (hold != MC_ENTITY_OK) {
        s->error->reference =
            (mc_entity_error){.status = hold, .operation = "Wolf interest selected Player hold"};
        return mt_fail(s, hold, "Wolf interest selected Player hold");
    }
    held_player = true;
    mc_inventory_player *inventory = player->local_player->fields.player.c;
    r->inventory_identity = (uintptr_t)inventory;
    mt_at(s, "gi", "o", "()V", 51);
    s->error->child_site = (mc_mob_site){"gi", "o", "()V", 51};
    if (inventory == NULL) {
        mt_null(s, "Wolf interest null Player Inventory receiver");
        goto done;
    }
    s->error->child = MC_MOB_TICK_CHILD_INVENTORY;
    const mc_item_stack *borrowed = NULL;
    const mc_inventory_status current =
        mc_inventory_player_current(inventory, &borrowed, &s->error->detail.inventory);
    const mc_inventory_current_error *cause = &s->error->detail.inventory;
    r->selected_read = cause->source_entered;
    if (r->selected_read)
        r->selected = cause->selected;
    if (current != MC_INVENTORY_OK) {
        s->error->child_site = (mc_mob_site){cause->site.class_name, cause->site.method,
                                             cause->site.descriptor, cause->site.pc};
        if (current == MC_INVENTORY_NULL_REFERENCE && !cause->native_policy)
            mt_null(s, "Wolf interest actual Inventory current Stack child");
        else
            mt_fail(s,
                    current == MC_INVENTORY_LIMIT               ? MC_ENTITY_LIMIT
                    : current == MC_INVENTORY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                    : current == MC_INVENTORY_BUSY              ? MC_ENTITY_BUSY
                                                                : MC_ENTITY_STATE_ERROR,
                    "Wolf interest actual Inventory current Stack child");
        goto done;
    }
    r->inventory_returned = true;
    r->stack_identity = (uintptr_t)borrowed;
    r->stack_nonnull = borrowed != NULL;
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    if (borrowed == NULL) {
        ok = true; /* Actual null Stack branch55/56 ->126, no D/Item read. */
        goto done;
    }
    stack = (mc_item_stack *)borrowed;
    mt_at(s, "gi", "o", "()V", 55);
    const mc_item_stack_status stack_hold = mc_item_stack_retain(stack);
    if (stack_hold != MC_ITEM_STACK_OK) {
        const mc_entity_status status =
            stack_hold == MC_ITEM_STACK_LIMIT ? MC_ENTITY_LIMIT : MC_ENTITY_STATE_ERROR;
        s->error->reference = (mc_entity_error){
            .status = status, .operation = "Wolf interest captured Stack hold", .item = stack_hold};
        mt_fail(s, status, "Wolf interest captured Stack hold");
        goto done;
    }
    held_stack = true;
    bool tame;
    if (!interest_tame(s, 60, &tame))
        goto done;
    if (!tame) {
        int32_t id;
        if (!interest_id(s, stack, 67, &id))
            goto done;
        mt_at(s, "gi", "o", "()V", 70);
        s->error->child = MC_MOB_TICK_CHILD_ITEM;
        s->error->child_site = (mc_mob_site){"gi", "o", "()V", 70};
        const mc_item *bone = NULL;
        if (!interest_registry(
                s, mc_registry_item_static_get(s->registry, "aV", &bone, &s->error->registry)))
            goto done;
        r->bone_identity = (uintptr_t)bone;
        mt_at(s, "gi", "o", "()V", 73);
        s->error->child_site.pc = 73;
        int32_t bone_id;
        if (!interest_registry(
                s, mc_registry_item_source_id(s->registry, bone, &bone_id, &s->error->registry)))
            goto done;
        r->bone_id_read = true;
        r->bone_id = bone_id;
        s->error->child = MC_MOB_TICK_CHILD_NONE;
        if (id == bone_id) {
            s->entity->mob->wolf.a = true; /* Actual putfield81. */
            mt_at(s, "gi", "o", "()V", 81);
            ok = true;
            goto done;
        }
    }
    if (!interest_tame(s, 88, &tame))
        goto done;
    if (tame) {
        const mc_registry_item_array *array = NULL;
        const mc_item *item = NULL;
        mt_at(s, "gi", "o", "()V", 94);
        s->error->child = MC_MOB_TICK_CHILD_ITEM;
        s->error->child_site = (mc_mob_site){"gi", "o", "()V", 94};
        if (!interest_registry(s, mc_registry_item_c_get(s->registry, &array, &s->error->registry)))
            goto done;
        r->arrays[r->array_reads++] = (uintptr_t)array;
        int32_t id;
        if (!interest_id(s, stack, 98, &id))
            goto done;
        mt_at(s, "gi", "o", "()V", 101);
        s->error->child_site.pc = 101;
        if (!interest_registry(
                s, mc_registry_item_c_at(s->registry, array, id, &item, &s->error->registry)))
            goto done;
        r->items[0] = (uintptr_t)item;
        mt_at(s, "gi", "o", "()V", 102);
        s->error->child_site.pc = 102;
        bool is_food;
        if (!interest_registry(
                s, mc_registry_item_food_type(s->registry, item, &is_food, &s->error->registry)))
            goto done;
        r->food_type = is_food;
        if (is_food) {
            /* Actual second getstatic and fresh Stack id/aaload. Never reuse
             * the earlier descriptor, index or Item as a getter substitute. */
            mt_at(s, "gi", "o", "()V", 109);
            s->error->child_site.pc = 109;
            if (!interest_registry(
                    s, mc_registry_item_c_get(s->registry, &array, &s->error->registry)))
                goto done;
            r->arrays[r->array_reads++] = (uintptr_t)array;
            if (!interest_id(s, stack, 113, &id))
                goto done;
            mt_at(s, "gi", "o", "()V", 116);
            s->error->child_site.pc = 116;
            if (!interest_registry(
                    s, mc_registry_item_c_at(s->registry, array, id, &item, &s->error->registry)))
                goto done;
            r->items[1] = (uintptr_t)item;
            mt_at(s, "gi", "o", "()V", 117);
            s->error->child_site.pc = 117;
            if (!interest_registry(s, mc_registry_item_food_type(s->registry, item, &is_food,
                                                                 &s->error->registry)))
                goto done;
            if (item != NULL && !is_food) {
                mt_fail(s, MC_ENTITY_STATE_ERROR, "Wolf interest actual Food checkcast");
                s->error->native_policy = false;
                s->error->source_exception = true;
                s->error->exception_class = "java/lang/ClassCastException";
                goto done;
            }
            mt_at(s, "gi", "o", "()V", 120);
            s->error->child_site.pc = 120;
            bool preferred;
            if (!interest_registry(s, mc_registry_item_food_wolf_preferred(
                                          s->registry, item, &preferred, &s->error->registry)))
                goto done;
            r->food_returned = true;
            s->entity->mob->wolf.a = preferred; /* Actual putfield123. */
            mt_at(s, "gi", "o", "()V", 123);
        }
    }
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    ok = true;
done:
    if (held_stack)
        mc_item_stack_release(stack);
    if (held_player)
        mc_entity_release(player);
    if (ok) {
        r->returned = true;
        r->interested = s->entity->mob->wolf.a;
        s->error->child = MC_MOB_TICK_CHILD_NONE;
        s->error->child_site = (mc_mob_site){0};
    }
    return ok;
}

bool mt_wolf_action_tail(mc_mob_tick_scope *s) {
    mc_mob_entity_state *m = s->entity->mob;
    m->wolf.a = false;
    mc_entity *look;
    mt_at(s, "gi", "o", "()V", 10);
    if (!mt_target_get(s, false, &look))
        return false;
    bool present = false;
    if (look != NULL) {
        mt_at(s, "gi", "o", "()V", 17);
        if (!mt_path_present(s, &present))
            return false;
    }
    if (look != NULL && !present) {
        bool angry;
        mt_at(s, "gi", "o", "()V", 24);
        if (!flag(s, 2, &angry))
            return false;
        if (!angry) {
            /* Y10 and Z31 are distinct actual source reads. The intervening
             * C24 predicate must complete before this Z field occurrence. */
            mt_at(s, "gi", "o", "()V", 31);
            if (!mt_target_get(s, false, &look))
                return false;
            bool is_player = false;
            if (look != NULL) {
                mt_at(s, "gi", "o", "()V", 36);
                const mc_entity_class_literal *player_class = NULL;
                if (mc_entity_class_literal_get(s->entity->classes, MC_ENTITY_CLASS_KIND_PLAYER,
                                                &player_class) != MC_ENTITY_OK ||
                    mc_entity_class_literal_is_assignable_from(player_class, look->class_literal,
                                                               &is_player) != MC_ENTITY_OK)
                    return mt_fail(s, MC_ENTITY_STATE_ERROR,
                                   "Wolf interest actual Player instanceof Class identity");
            }
            if (is_player && !player_interest(s, look))
                return false;
        }
    }
    if (!mt_fields(s)->V && m->wolf.f && !m->wolf.g) {
        mt_at(s, "gi", "o", "()V", 148);
        if (!mt_path_present(s, &present))
            return false;
    } else
        present = true; /* Earlier source guards skip the remaining branch. */
    if (!present && s->entity->base.aX) {
        m->wolf.g = true;
        m->wolf.h = 0.0f;
        m->wolf.i = 0.0f;
        mt_at(s, "gi", "o", "()V", 183);
        mc_world_runtime *w;
        if (!mt_world(s, &w))
            return false;
        (void)w; /* Actual client FD.a(sn,byte) return0. */
    }
    mt_at(s, "gi", "o", "()V", 186);
    return true;
}

bool mt_wolf_tail(mc_mob_tick_scope *s) {
    mc_entity *p = s->entity;
    mc_mob_wolf_fields *v = &p->mob->wolf;
    v->c = v->b;
    mt_at(s, "gi", "w_", "()V", 32);
    if (!mt_fp(s))
        return false;
    v->b = at_fadd(v->b, at_fmul(at_fsub(v->a ? 1.0f : 0.0f, v->b), .4f));
    if (v->a)
        mt_fields(s)->aC = 10;
    bool wet;
    mt_at(s, "gi", "w_", "()V", 72);
    s->error->child = MC_MOB_TICK_CHILD_EFFECT;
    const mc_entity_status status = mc_entity_effect_wet(p, &wet, &s->error->detail.effect);
    if (status != MC_ENTITY_OK)
        return mt_fail(s, status, "Wolf actual wet child");
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    if (wet) {
        v->f = true;
        v->g = false;
        v->h = v->i = 0.0f;
    } else if ((v->f || v->g) && v->g) {
        if (v->h == 0.0f) {
            float a, b;
            mc_world_runtime *sound_world = p->base.world;
            const mc_registry_string *sound;
            mt_at(s, "gi", "w_", "()V", 136);
            if (!mt_literal(s, "mob.wolf.shake", &sound))
                return false;
            mt_at(s, "gi", "w_", "()V", 146);
            if (!mt_float(s, &a))
                return false;
            mt_at(s, "gi", "w_", "()V", 153);
            if (!mt_float(s, &b))
                return false;
            mt_at(s, "gi", "w_", "()V", 162);
            if (!mt_sound_prepared(s, sound_world, sound, .4f,
                                   at_fadd(at_fmul(at_fsub(a, b), .2f), 1.0f)))
                return false;
        }
        v->i = v->h;
        v->h = at_fadd(v->h, .05f);
        if (v->i >= 2.0f) {
            v->f = false;
            v->g = false;
            v->i = v->h = 0.0f;
        }
        if (v->h > .4f) {
            mt_at(s, "gi", "w_", "()V", 227);
            if (p->bounds == NULL)
                return mt_null(s, "Wolf shake null permanent bounds");
            const float y = at_narrow(p->bounds->min_y);
            mt_at(s, "gi", "w_", "()V", 242);
            if (!mt_math(s))
                return false;
            const int32_t count =
                mc_java_float_to_i32(at_fmul(mc_math_sin(mc_math_classes_table(s->math),
                                                         at_fmul(at_fsub(v->h, .4f), 3.1415927f)),
                                             7.0f));
            for (int32_t i = 0; i < count; ++i) {
                mt_at(s, "gi", "w_", "()V", 261);
                float a, b;
                if (!mt_work(s) || !mt_float(s, &a))
                    return false;
                const float x =
                    at_fmul(at_fmul(at_fsub(at_fmul(a, 2.0f), 1.0f), p->base.width), .5f);
                mt_at(s, "gi", "w_", "()V", 282);
                if (!mt_float(s, &b))
                    return false;
                const float z =
                    at_fmul(at_fmul(at_fsub(at_fmul(b, 2.0f), 1.0f), p->base.width), .5f);
                mc_world_runtime *particle_world = p->base.world;
                const mc_registry_string *particle;
                mt_at(s, "gi", "w_", "()V", 303);
                if (!mt_literal(s, "splash", &particle))
                    return false;
                mt_at(s, "gi", "w_", "()V", 338);
                if (!mt_particle_prepared(s, particle_world, particle,
                                          at_add(p->base.x, at_widen(x)), at_widen(at_fadd(y, .8f)),
                                          at_add(p->base.z, at_widen(z)), p->base.velocity_x,
                                          p->base.velocity_y, p->base.velocity_z))
                    return false;
            }
        }
    }
    mt_at(s, "gi", "w_", "()V", 347);
    return true;
}
