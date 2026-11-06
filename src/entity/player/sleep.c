#include "mc/entity/player/sleep.h"
#include "../classes/classes_internal.h"
#include "mc/world/runtime/construction.h"
#include "player_internal.h"
#include "tick_math_internal.h"
#include <string.h>

static bool fail(mc_player_sleep_error *error, mc_player_sleep_status status, const char *operation,
                 unsigned int pc, bool native) {
    error->status = status;
    error->operation = operation;
    error->site = (mc_player_site){"gs", "b", "(III)Lcw;", pc};
    error->native_policy = native;
    return false;
}

static bool null_reference(mc_player_sleep_error *error, const char *operation, unsigned int pc) {
    error->source_exception = true;
    error->exception_class = "java/lang/NullPointerException";
    return fail(error, MC_PLAYER_SLEEP_SOURCE_NULL, operation, pc, false);
}

static bool world_fields(mc_entity *player, mc_world_runtime_fields *out,
                         mc_player_sleep_error *error, unsigned int pc) {
    mc_world_runtime *world = player->base.world;
    if (world == NULL)
        return null_reference(error, "GS sleep actual World null receiver", pc);
    const mc_world_storage_status status = mc_world_runtime_fields_get(world, out);
    if (status != MC_WORLD_STORAGE_OK) {
        error->world = (mc_world_storage_error){status, "GS sleep actual World fields", 0};
        return fail(error, MC_PLAYER_SLEEP_CHILD_FAILED, error->world.operation, pc, true);
    }
    return true;
}

static bool result(mc_entity *player, const char *field, unsigned int pc, unsigned int returned_pc,
                   const mc_player_sleep_result **out, mc_player_sleep_error *error) {
    const mc_player_sleep_result *value = NULL;
    const mc_player_sleep_result_status status =
        mc_player_sleep_result_static_get(player->classes, field, &value, &error->result);
    if (status != MC_PLAYER_SLEEP_RESULT_OK) {
        error->child = error->result.site;
        const mc_player_sleep_status mapped =
            status == MC_PLAYER_SLEEP_RESULT_LIMIT               ? MC_PLAYER_SLEEP_LIMIT
            : status == MC_PLAYER_SLEEP_RESULT_ALLOCATION_FAILED ? MC_PLAYER_SLEEP_ALLOCATION_FAILED
            : status == MC_PLAYER_SLEEP_RESULT_CLASS_FAILED      ? MC_PLAYER_SLEEP_CLASS_FAILED
            : status == MC_PLAYER_SLEEP_RESULT_BUSY              ? MC_PLAYER_SLEEP_BUSY
                                                                 : MC_PLAYER_SLEEP_CHILD_FAILED;
        return fail(error, mapped, error->result.operation, pc, error->result.native_policy);
    }
    *out = value;
    error->site = (mc_player_site){"gs", "b", "(III)Lcw;", returned_pc};
    error->returned = true;
    return true;
}

static bool too_far(double position, int32_t coordinate, double limit) {
    double distance = at_sub(position, (double)coordinate);
    uint64_t bits;
    memcpy(&bits, &distance, sizeof(bits));
    bits &= UINT64_C(0x7fffffffffffffff); /* Actual Math.abs, including -0 and NaN. */
    memcpy(&distance, &bits, sizeof(distance));
    /* Java dcmpl returns -1 for NaN: these source branches continue. */
    return distance > limit;
}

static void run(mc_entity *player, int32_t x, int32_t y, int32_t z,
                const mc_player_sleep_result **out, mc_player_sleep_error *error) {
    mc_world_runtime_fields world;
    if (!world_fields(player, &world, error, 4))
        return;
    if (!world.B) {
        bool sleeping;
        if (mc_player_is_sleeping(player, &sleeping, &error->sleeping) != MC_PLAYER_OK) {
            error->child = error->sleeping.site;
            (void)fail(error, MC_PLAYER_SLEEP_CHILD_FAILED, error->sleeping.operation, 11, true);
            return;
        }
        if (sleeping) {
            (void)result(player, "e", 24, 27, out, error);
            return;
        }
        /* DC inherits LS.W: be1 before Y8; no World/FP child is added. */
        if (player->base.be || player->local_player->fields.living.Y <= 0) {
            (void)result(player, "e", 24, 27, out, error);
            return;
        }
        if (!world_fields(player, &world, error, 32))
            return;
        if (world.t == NULL) {
            (void)null_reference(error, "GS sleep actual provider null receiver", 35);
            return;
        }
        mc_world_provider_fields provider;
        const mc_world_provider_status provider_status =
            mc_world_provider_fields_get(world.t, &provider);
        if (provider_status != MC_WORLD_PROVIDER_OK) {
            error->provider = (mc_world_provider_error){
                .status = provider_status, .operation = "GS sleep actual provider fields"};
            (void)fail(error, MC_PLAYER_SLEEP_CHILD_FAILED, error->provider.operation, 35, true);
            return;
        }
        if (provider.c) {
            (void)result(player, "b", 41, 44, out, error);
            return;
        }
        if (!world_fields(player, &world, error, 49))
            return;
        if (world.f < 4) {
            (void)result(player, "c", 55, 58, out, error);
            return;
        }
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            error->entity.status = MC_ENTITY_UNSUPPORTED_FP;
            (void)fail(error, MC_PLAYER_SLEEP_CHILD_FAILED, "GS sleep strict distance FP controls",
                       65, true);
            return;
        }
        if (too_far(player->base.x, x, 3.0) || too_far(player->base.y, y, 2.0) ||
            too_far(player->base.z, z, 3.0)) {
            (void)result(player, "d", 110, 113, out, error);
            return;
        }
    }
    (void)fail(error, MC_PLAYER_SLEEP_UNIMPLEMENTED, "GS sleep actual resize descendant", 119,
               true);
}

mc_player_sleep_status mc_player_try_sleep(mc_entity *player, int32_t x, int32_t y, int32_t z,
                                           const mc_player_sleep_result **out,
                                           mc_player_sleep_error *out_error) {
    mc_player_sleep_error error = {0};
    if (out == NULL)
        (void)fail(&error, MC_PLAYER_SLEEP_ARGUMENT, "GS sleep native output", 0, true);
    else if (player == NULL)
        (void)null_reference(&error, "GS sleep null invocation receiver", 0);
    else if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
             player->classes->source != MC_ENTITY_CLIENT)
        (void)fail(&error, MC_PLAYER_SLEEP_ARGUMENT, "GS sleep canonical client LocalPlayer", 0,
                   true);
    else {
        const mc_entity_status status = mc_entity_retain(player);
        if (status != MC_ENTITY_OK) {
            error.entity.status = status;
            (void)fail(&error,
                       status == MC_ENTITY_LIMIT ? MC_PLAYER_SLEEP_LIMIT : MC_PLAYER_SLEEP_BUSY,
                       "GS sleep native Player occurrence", 0, true);
        } else {
            run(player, x, y, z, out, &error);
            mc_entity_release(player);
        }
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}
