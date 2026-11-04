/* Actual App/statistics/Player fixture. Its entry point is unused. */
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define main mc_food_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "item/stack/source_internal.h"
#include "mc/client/controller/use.h"
#include "mc/core/java/java.h"
#include "mc/item/use/use.h"
#include "world/runtime/runtime_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>

static void food_exception(char out[128], bool source, const char *name, bool ok) {
    const char *text = ok ? "NONE" : source && name != NULL ? name : "NATIVE_FAILURE";
    size_t i = 0;
    while (text[i] != 0 && i + 1u < 128u) {
        out[i] = text[i] == '/' ? '.' : text[i];
        ++i;
    }
    out[i] = 0;
}

static bool food_profiles(fixture *f, const char *path) {
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(f->app.entities, f->app.profile, 0,
                                                     &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(controller, &f->app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    for (int mode = 0; mode < 2; ++mode) {
        FILE *input = fopen(path, "rb");
        CHECK(input != NULL);
        char line[512];
        size_t index = 0;
        while (fgets(line, sizeof line, input) != NULL) {
            int32_t p[7];
            CHECK(strchr(line, '\n') != NULL);
            char *cursor = line;
            for (size_t n = 0; n < 7; ++n) {
                char *end = NULL;
                errno = 0;
                const intmax_t value = strtoimax(cursor, &end, 10);
                CHECK(end != cursor && errno == 0 && value >= INT32_MIN && value <= INT32_MAX);
                p[n] = (int32_t)value;
                if (n < 6)
                    CHECK(*end == '\t');
                else
                    CHECK(end[strspn(end, "\r\n")] == 0);
                cursor = end + 1;
            }
            CHECK(p[0] >= 0 && p[0] < 32000 && (p[5] == 0 || p[5] == 1) &&
                  (p[6] == 0 || p[6] == 1));
            mc_item_stack *stack = NULL;
            CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
                  mc_item_stack_construct(stack, p[0], p[1], INT32_MIN) == MC_ITEM_STACK_OK &&
                  mc_item_stack_source_animation_set(stack, 9) == MC_ITEM_STACK_OK);
            mc_inventory_player *inventory = f->player->local_player->fields.player.c;
            if (mode == 1)
                CHECK(mc_inventory_player_set(inventory, 2, stack, NULL) == MC_INVENTORY_OK &&
                      mc_inventory_player_select_hotbar(inventory, 2, NULL) == MC_INVENTORY_OK);
            f->player->local_player->fields.living.Y = p[2];
            f->player->local_player->fields.living.E = p[3];
            f->player->base.by = p[4];
            f->world->fields.B = p[6] != 0;
            const mc_statistic *used = f->app.registry->statistics.E[p[0]];
            int32_t before, after;
            CHECK(used != NULL && mc_client_stats_writer_value(f->app.stats_writer, used, &before,
                                                               NULL) == MC_CLIENT_STATS_OK);
            mc_item_use_call *call = NULL;
            mc_singleplayer_use_call *controller_call = NULL;
            mc_item_use_state state = {0};
            mc_singleplayer_use_state controller_state = {0};
            const mc_item_stack *returned = NULL;
            bool ok;
            char exception[128];
            if (mode == 0) {
                mc_item_use_error error;
                const mc_item_use_status status = mc_item_use_in_air_begin(
                    f->app.registry, stack, NULL, p[5] ? NULL : f->player, &call, &error);
                CHECK(mc_item_use_inspect(call, &state));
                ok = status == MC_ITEM_USE_OK;
                returned = state.result_stack;
                food_exception(exception, error.source_exception, error.exception_class, ok);
            } else {
                mc_singleplayer_use_error error;
                const mc_singleplayer_use_status status = mc_client_singleplayer_use_in_air_begin(
                    controller, p[5] ? NULL : f->player, NULL, stack, &controller_call, &error);
                CHECK(mc_client_singleplayer_use_inspect(controller_call, &controller_state));
                ok = status == MC_SINGLEPLAYER_USE_OK;
                CHECK(mc_inventory_player_borrow(inventory, 2, &returned, NULL) == MC_INVENTORY_OK);
                food_exception(exception, error.source_exception, error.exception_class, ok);
            }
            mc_item_stack_fields old,
                result = {.id = -1, .count = -1, .damage = -1, .animation = -1};
            CHECK(mc_item_stack_fields_get(stack, &old) == MC_ITEM_STACK_OK &&
                  (returned == NULL ||
                   mc_item_stack_fields_get(returned, &result) == MC_ITEM_STACK_OK) &&
                  mc_client_stats_writer_value(f->app.stats_writer, used, &after, NULL) ==
                      MC_CLIENT_STATS_OK);
            printf("%s\t%zu\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32
                   "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%s\t",
                   mode == 0 ? "FOOD" : "CONTROLLERFOOD", index, p[0], old.count,
                   f->player->local_player->fields.living.Y, f->player->base.by, result.id,
                   result.count, result.damage, result.animation,
                   returned == stack ? "true" : "false");
            if (mode == 1)
                printf("%s\t", controller_state.returned
                                   ? (controller_state.result ? "true" : "false")
                                   : "UNRETURNED");
            printf("%s\t%" PRId32 "\t%s\n", exception, mc_java_i32_sub(after, before),
                   f->world->fields.B ? "true" : "false");
            if (mode == 0)
                CHECK(mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK);
            else
                CHECK(mc_client_singleplayer_use_retire(&controller_call, NULL) ==
                          MC_SINGLEPLAYER_USE_OK &&
                      mc_inventory_player_set(inventory, 2, NULL, NULL) == MC_INVENTORY_OK);
            mc_item_stack_release(stack);
            ++index;
        }
        CHECK(!ferror(input) && fclose(input) == 0 && index == 14);
    }
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "food-reference", true) || !food_profiles(f, argv[3]) ||
        !teardown(f))
        return 1;
    free(f);
    printf("PASS: 28 food Stack/controller rows, %zu actual-owner checks\n", checks);
    return 0;
}
