#include "mc/client/world/held_item_tick.h"

#include "../../application_state.h"
#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Held-item tick arithmetic requires the qualified x86-64 profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Held-item tick requires strict floating transformations"
#endif

struct mc_client_held_item_tick_call {
    mc_client_held_item_tick_state state;
    mc_client_held_item_tick_error error;
    mc_item_stack *selected_occurrence, *previous_occurrence;
};

static mc_client_held_item_tick_status result(mc_client_held_item_tick_error *out,
                                              mc_client_held_item_tick_status status,
                                              const char *operation, bool native_policy) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->status = status;
        out->operation = operation;
        out->native_policy = native_policy;
        out->site.pc = out->child.pc = -1;
    }
    return status;
}

static bool fail(mc_client_held_item_tick_call *call, mc_client_held_item_tick_status status,
                 const char *operation, int32_t pc, bool native_policy) {
    call->state.status = status;
    call->state.source_pc = (unsigned int)pc;
    call->error.status = status;
    call->error.operation = operation;
    call->error.native_policy = native_policy;
    call->error.site = (mc_client_graphics_site){"ra", "a", "()V", pc};
    return false;
}

static bool hold(mc_client_held_item_tick_call *call, mc_item_stack *stack,
                 mc_item_stack **occurrence, int32_t pc) {
    if (stack == NULL)
        return true;
    call->error.stack = mc_item_stack_retain(stack);
    if (call->error.stack != MC_ITEM_STACK_OK)
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_STACK,
                    "held-item tick observed Stack occurrence", pc, true);
    *occurrence = stack;
    return true;
}

static bool publish(mc_client_held_item_tick_call *call, mc_client_source_ra *ra,
                    mc_item_stack *value, int32_t pc) {
    if (ra->b_occurrence != value) {
        if (value != NULL) {
            call->error.stack = mc_item_stack_retain(value);
            if (call->error.stack != MC_ITEM_STACK_OK)
                return fail(call, MC_CLIENT_HELD_ITEM_TICK_STACK,
                            "held-item published Stack native occurrence", pc, true);
        }
        mc_item_stack *old = ra->b_occurrence;
        ra->b = value;
        ra->b_occurrence = value;
        mc_item_stack_release(old);
    } else
        ra->b = value;
    call->state.b_stored = true;
    return true;
}

static bool player(mc_client_held_item_tick_call *call, mc_entity *receiver,
                   mc_local_player_snapshot *out, int32_t pc) {
    if (receiver == NULL)
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL, "actual ra.a null Player receiver",
                    pc, false);
    call->error.player = mc_local_player_get_snapshot(receiver, out);
    if (call->error.player != MC_ENTITY_OK)
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_PLAYER, "held-item tick actual Player fields",
                    pc, true);
    return true;
}

static bool selected_field(mc_client_held_item_tick_call *call, mc_entity *receiver, int32_t *out,
                           int32_t player_pc, int32_t inventory_pc) {
    mc_local_player_snapshot value;
    if (!player(call, receiver, &value, player_pc))
        return false;
    if (value.player.c == NULL)
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL,
                    "actual ra.a null Inventory selected-field receiver", inventory_pc, false);
    mc_inventory_player_state inventory;
    if (!mc_inventory_player_read_state(value.player.c, &inventory))
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_INVENTORY,
                    "held-item actual selected-field access", inventory_pc, true);
    *out = inventory.selected;
    return true;
}

static bool body(mc_client_held_item_tick_call *call) {
    mc_client_source_ra *ra = call->state.owner->source;
    memcpy(&call->state.before_c, &ra->c, sizeof(ra->c));
    memcpy(&ra->d, &ra->c, sizeof(ra->d)); /* Actual putfield5. */
    memcpy(&call->state.after_d, &ra->d, sizeof(ra->d));
    call->state.d_stored = true;
    call->state.source_pc = 5;
    call->state.app = ra->a; /* Actual getfield9. */
    if (ra->a == NULL)
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL, "actual ra.a null App.h receiver",
                    12, false);
    call->state.player = ra->a->h; /* Actual getfield12. */
    mc_local_player_snapshot fields;
    if (!player(call, call->state.player, &fields, 17))
        return false;
    if (fields.player.c == NULL)
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL,
                    "actual ra.a null Inventory invocation receiver", 20, false);
    const mc_item_stack *selected = NULL;
    if (mc_inventory_player_current(fields.player.c, &selected, &call->error.inventory) !=
        MC_INVENTORY_OK) {
        call->error.child = (mc_client_graphics_site){
            call->error.inventory.site.class_name, call->error.inventory.site.method,
            call->error.inventory.site.descriptor, call->error.inventory.site.pc};
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_INVENTORY, call->error.inventory.operation, 20,
                    call->error.inventory.native_policy);
    }
    call->state.current_returned = true;
    call->state.selected = selected;
    if (!hold(call, (mc_item_stack *)selected, &call->selected_occurrence, 20))
        return false;
    int32_t slot;
    if (!selected_field(call, call->state.player, &slot, 31, 34))
        return false;
    call->state.selected_slot = slot;
    bool same = ra->g == slot && selected == ra->b;
    call->state.previous = ra->b;
    if (!hold(call, ra->b, &call->previous_occurrence, 56))
        return false;
    if (ra->b == NULL && selected == NULL)
        same = true;
    if (selected != NULL && ra->b != NULL && selected != ra->b) {
        mc_item_stack_fields current, previous;
        call->error.stack = mc_item_stack_fields_get(selected, &current);
        if (call->error.stack != MC_ITEM_STACK_OK)
            return fail(call, MC_CLIENT_HELD_ITEM_TICK_STACK, "held-item current Stack fields", 89,
                        true);
        call->error.stack = mc_item_stack_fields_get(ra->b, &previous);
        if (call->error.stack != MC_ITEM_STACK_OK)
            return fail(call, MC_CLIENT_HELD_ITEM_TICK_STACK, "held-item previous Stack fields", 96,
                        true);
        if (current.id == previous.id && current.damage == previous.damage) {
            if (!publish(call, ra, (mc_item_stack *)selected, 118))
                return false;
            call->state.equivalent_replaced = true;
            same = true;
        }
    }
    call->state.same = same;
    call->state.source_pc = 146;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(call, MC_CLIENT_HELD_ITEM_TICK_FLOATING,
                    "held-item tick native floating environment", 146, true);
    float delta = _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(same ? 1.0f : 0.0f), _mm_set_ss(ra->c)));
    if (delta < -0.4f)
        delta = -0.4f;
    if (delta > 0.4f)
        delta = 0.4f;
    memcpy(&call->state.delta, &delta, sizeof(delta));
    const float after = _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(ra->c), _mm_set_ss(delta)));
    memcpy(&ra->c, &after, sizeof(after)); /* Actual putfield183. */
    memcpy(&call->state.after_c, &ra->c, sizeof(ra->c));
    call->state.c_stored = true;
    call->state.source_pc = 183;
    if (ra->c < 0.1f) {
        if (!publish(call, ra, (mc_item_stack *)selected, 198))
            return false;
        if (!selected_field(call, call->state.player, &slot, 203, 206))
            return false;
        ra->g = slot;
        call->state.g_stored = true;
    }
    call->state.after_g = ra->g;
    call->state.source_pc = 212;
    call->state.source_returned = true;
    call->error.site = (mc_client_graphics_site){"ra", "a", "()V", 212};
    return true;
}

mc_client_held_item_tick_status
mc_client_held_item_tick_begin(mc_client_held_item *owner, mc_client_held_item_tick_call **out,
                               mc_client_held_item_tick_error *error) {
    if (owner == NULL || owner->source == NULL || out == NULL || *out != NULL)
        return result(error, MC_CLIENT_HELD_ITEM_TICK_ARGUMENT,
                      "held-item tick native owner/output", true);
    if (owner->active || owner->source->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING)
        return result(error, MC_CLIENT_HELD_ITEM_TICK_BUSY, "held-item tick active owner", true);
    if (owner->source->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING)
        return result(error, MC_CLIENT_HELD_ITEM_TICK_PHASE, "held-item tick terminal owner", true);
    mc_client_held_item_tick_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return result(error, MC_CLIENT_HELD_ITEM_TICK_ALLOCATION,
                      "held-item tick native call allocation", true);
    result(&call->error, MC_CLIENT_HELD_ITEM_TICK_OK, NULL, false);
    if (mc_client_held_item_retain(owner, &call->error.owner) != MC_CLIENT_ENTITY_RENDERER_OK) {
        if (error != NULL) {
            result(error, MC_CLIENT_HELD_ITEM_TICK_OWNER, "held-item tick owner occurrence", true);
            error->owner = call->error.owner;
        }
        free(call);
        return MC_CLIENT_HELD_ITEM_TICK_OWNER;
    }
    call->state.owner = owner;
    call->state.receiver = owner->source;
    call->state.error = &call->error;
    call->state.active = owner->active = true;
    *out = call;
    (void)body(call);
    call->state.active = owner->active = false;
    if (error != NULL)
        *error = call->error;
    return call->state.status;
}

bool mc_client_held_item_tick_inspect(const mc_client_held_item_tick_call *call,
                                      mc_client_held_item_tick_state *out) {
    if (call == NULL || out == NULL)
        return false;
    *out = call->state;
    return true;
}

mc_client_held_item_tick_status
mc_client_held_item_tick_retire(mc_client_held_item_tick_call **slot,
                                mc_client_held_item_tick_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_HELD_ITEM_TICK_ARGUMENT,
                      "held-item tick native retirement slot", true);
    mc_client_held_item_tick_call *call = *slot;
    if (call == NULL)
        return result(error, MC_CLIENT_HELD_ITEM_TICK_OK, NULL, false);
    if (call->state.active || call->state.owner->active)
        return result(error, MC_CLIENT_HELD_ITEM_TICK_BUSY, "held-item tick active retirement",
                      true);
    mc_client_entity_renderer_error owner_error;
    if (mc_client_held_item_release(&call->state.owner, &owner_error) !=
        MC_CLIENT_ENTITY_RENDERER_OK) {
        if (error != NULL) {
            result(error, MC_CLIENT_HELD_ITEM_TICK_OWNER, "held-item tick occurrence retirement",
                   true);
            error->owner = owner_error;
        }
        return MC_CLIENT_HELD_ITEM_TICK_OWNER;
    }
    mc_item_stack_release(call->selected_occurrence);
    mc_item_stack_release(call->previous_occurrence);
    free(call);
    *slot = NULL;
    return result(error, MC_CLIENT_HELD_ITEM_TICK_OK, NULL, false);
}
