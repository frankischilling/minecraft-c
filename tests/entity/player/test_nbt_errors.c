/* Native passive diagnostic-storage contracts, not Player gameplay answers. */
#include "mc/entity/nbt/errors.h"
#include "mc/entity/nbt/factory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            result = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool equal_site(mc_mob_site a, mc_mob_site b) {
    return a.class_name == b.class_name && a.method == b.method && a.descriptor == b.descriptor &&
           a.pc == b.pc;
}

static bool equal_cart(mc_minecart_site a, mc_minecart_site b) {
    return a.selected == b.selected && a.entered == b.entered &&
           a.declaring_class == b.declaring_class && a.method == b.method &&
           a.bytecode == b.bytecode;
}

static bool equal_entity(mc_entity_error a, mc_entity_error b) {
    return a.status == b.status && a.operation == b.operation && a.bytecode == b.bytecode &&
           a.profile == b.profile && a.item == b.item && a.world == b.world &&
           equal_cart(a.minecart_world_constructor, b.minecart_world_constructor) &&
           equal_cart(a.minecart_constructor, b.minecart_constructor) &&
           equal_cart(a.minecart_initializer, b.minecart_initializer) &&
           equal_cart(a.minecart_initializer_cause, b.minecart_initializer_cause);
}

static bool equal_registry(mc_registry_error a, mc_registry_error b) {
    return a.status == b.status && a.message == b.message && a.initializer_pc == b.initializer_pc &&
           a.block_id == b.block_id && a.constructor_class == b.constructor_class &&
           a.constructor_pc == b.constructor_pc && a.profile_status == b.profile_status &&
           a.active_initializer_class == b.active_initializer_class &&
           a.active_initializer_pc == b.active_initializer_pc &&
           a.item_argument == b.item_argument && a.format_error.status == b.format_error.status &&
           a.format_error.offset == b.format_error.offset &&
           a.format_error.length == b.format_error.length &&
           a.format_error.value == b.format_error.value &&
           a.format_error.flags == b.format_error.flags &&
           a.format_error.conversion == b.format_error.conversion &&
           a.format_error.date_time == b.format_error.date_time;
}

static bool equal_cause(mc_mob_class_cause a, mc_mob_class_cause b) {
    return a.status == b.status && a.operation == b.operation && equal_site(a.site, b.site) &&
           equal_site(a.child, b.child) && a.native_policy == b.native_policy &&
           a.source_exception == b.source_exception && a.exception_class == b.exception_class &&
           equal_entity(a.entity, b.entity) && equal_registry(a.registry, b.registry) &&
           a.stack == b.stack;
}

static bool equal_call(const mc_mob_error *a, const mc_mob_error *b) {
    return a->status == b->status && a->operation == b->operation && equal_site(a->site, b->site) &&
           equal_site(a->child, b->child) && a->native_policy == b->native_policy &&
           a->source_exception == b->source_exception && a->exception_class == b->exception_class &&
           equal_entity(a->entity, b->entity) && equal_registry(a->registry, b->registry) &&
           a->stack == b->stack && equal_cause(a->first_class_failure, b->first_class_failure);
}

/* Authored passive values fill every lower member; they are not a claim that
 * any prepared diagnostic shape was produced by a genuine Mob. */
static int passive_mob_controls(void) {
    int result = 0;
    mc_entity_nbt_error *error = malloc(sizeof(*error)), *copy = malloc(sizeof(*copy));
    mc_mob_error *input = calloc(1, sizeof(*input)), *expected = malloc(sizeof(*expected)),
                 *expanded = malloc(sizeof(*expanded));
    CHECK(error && copy && input && expected && expanded);
    mc_entity_nbt_error_init(error);
    CHECK(sizeof(*error) == 544 && sizeof(error->mob_call) == 464 && sizeof(error->mob) <= 224);
    error->factory_bytecode = 45;
    input->status = MC_MOB_REGISTRY_FAILED;
    input->operation = "authored lower operation";
    input->site = (mc_mob_site){"fr", "<clinit>", "()V", 4};
    input->child = (mc_mob_site){"gm", "<clinit>", "()V", 17};
    input->source_exception = true;
    input->exception_class = "java/lang/NullPointerException";
    input->entity = (mc_entity_error){.status = MC_ENTITY_PROFILE_FAILED,
                                      .operation = "authored Entity",
                                      .bytecode = 19,
                                      .profile = MC_JAVA_RANDOM_PROFILE_LIMIT,
                                      .item = MC_ITEM_STACK_LIMIT,
                                      .world = MC_WORLD_STORAGE_IO_ERROR};
    input->entity.minecart_constructor = (mc_minecart_site){true, true, "sn", "a", 1};
    input->entity.minecart_world_constructor = (mc_minecart_site){true, false, "sn", "b", 2};
    input->entity.minecart_initializer = (mc_minecart_site){false, true, "sn", "c", 3};
    input->entity.minecart_initializer_cause = (mc_minecart_site){true, true, "sn", "d", 4};
    input->registry = (mc_registry_error){.status = MC_REGISTRY_LIMIT,
                                          .message = "authored Registry",
                                          .initializer_pc = 29,
                                          .block_id = 3,
                                          .constructor_class = "bn",
                                          .constructor_pc = 31,
                                          .profile_status = MC_JAVA_RANDOM_PROFILE_LIMIT,
                                          .active_initializer_class = "gm",
                                          .active_initializer_pc = 37,
                                          .item_argument = 41,
                                          .format_error = {.status = MC_JAVA_FORMAT_LIMIT,
                                                           .offset = 43,
                                                           .length = 47,
                                                           .value = -53,
                                                           .flags = 59,
                                                           .conversion = 61,
                                                           .date_time = true}};
    input->stack = MC_ITEM_STACK_LIMIT;
    *expected = *input;
    CHECK(mc_entity_nbt_select_mob_call_detail(error, input) &&
          error->late_hook == MC_ENTITY_NBT_LATE_MOB_CALL && error->factory_bytecode == 45 &&
          error->mob_call.payload == MC_ENTITY_MOB_CALL_CURRENT &&
          mc_entity_nbt_mob_call_error(error, expanded) && equal_call(expected, expanded));
    *copy = *error;
    mc_entity_nbt_select_cart_detail(copy);
    CHECK(copy->late_hook == MC_ENTITY_NBT_LATE_CART && copy->factory_bytecode == 45 &&
          equal_entity(copy->construction, expected->entity) &&
          copy->stack.status == expected->stack && copy->stack.field == NULL &&
          copy->stack.nbt.status == MC_NBT_OK && copy->nbt.status == MC_NBT_OK &&
          copy->registry.status == MC_ENTITY_REGISTRY_OK);
    input->first_class_failure = (mc_mob_class_cause){input->status,
                                                      input->operation,
                                                      input->site,
                                                      input->child,
                                                      input->native_policy,
                                                      input->source_exception,
                                                      input->exception_class,
                                                      input->entity,
                                                      input->registry,
                                                      input->stack};
    input->exception_class = "java/lang/ExceptionInInitializerError";
    *expected = *input;
    CHECK(mc_entity_nbt_select_mob_call_detail(error, input) &&
          error->mob_call.payload == MC_ENTITY_MOB_CALL_FIRST_SHARED &&
          mc_entity_nbt_select_mob_call_detail(error, input) &&
          mc_entity_nbt_mob_call_error(error, expanded) && equal_call(expected, expanded));
    input->status = MC_MOB_CLASS_ERRONEOUS;
    input->operation = "authored later active use";
    input->exception_class = "java/lang/NoClassDefFoundError";
    input->entity = (mc_entity_error){0};
    input->registry = (mc_registry_error){0};
    input->stack = MC_ITEM_STACK_OK;
    *expected = *input;
    CHECK(mc_entity_nbt_select_mob_call_detail(error, input) &&
          error->mob_call.payload == MC_ENTITY_MOB_CALL_FIRST_PREVIOUS);
    *copy = *error;
    memset(input, 0, sizeof(*input));
    CHECK(mc_entity_nbt_mob_call_error(copy, expanded) && equal_call(expected, expanded));
    mc_entity_nbt_error_init(copy);
    copy->nbt.offset = 9;
    unsigned char before[sizeof(*copy)];
    memcpy(before, copy, sizeof(*copy));
    CHECK(!mc_entity_nbt_select_mob_call_detail(copy, expected) &&
          memcmp(before, copy, sizeof(*copy)) == 0);
    *copy = *error;
    input->first_class_failure = expected->first_class_failure;
    input->entity.status = MC_ENTITY_LIMIT;
    memcpy(before, copy, sizeof(*copy));
    CHECK(!mc_entity_nbt_select_mob_call_detail(copy, input) &&
          memcmp(before, copy, sizeof(*copy)) == 0);
    mc_entity_nbt_select_mob_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_MOB && error->mob.concrete.pc == -1 &&
          error->mob.child.pc == -1 && error->mob.call_kind == MC_ENTITY_MOB_NBT_HOOK &&
          error->construction.status == MC_ENTITY_OK &&
          error->registry.status == MC_ENTITY_REGISTRY_OK && error->factory_bytecode == 45);
    error->mob.living_bytecode = 55;
    mc_entity_nbt_select_mob_detail(error);
    CHECK(error->mob.living_bytecode == 55);
    error->mob.call_kind = MC_ENTITY_MOB_NBT_SLIME_SIZE;
    error->mob.call_status = MC_MOB_UNSUPPORTED_FP;
    error->mob.child = (mc_mob_site){"uw", "e", "(I)V", 23};
    error->mob.watcher.pc = -1;
    error->mob.native_policy = true;
    error->operation = "Mob source strict floating controls";
    CHECK(mc_entity_nbt_mob_call_error(error, expanded) &&
          expanded->status == MC_MOB_UNSUPPORTED_FP && expanded->site.pc == 23 &&
          expanded->child.pc == -1 && expanded->native_policy && !expanded->source_exception &&
          expanded->exception_class == NULL &&
          equal_entity(expanded->entity, (mc_entity_error){0}) &&
          equal_registry(expanded->registry, (mc_registry_error){0}) &&
          equal_cause(expanded->first_class_failure, (mc_mob_class_cause){0}));
    mc_entity_nbt_select_player_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_PLAYER &&
          error->player_inventory.index == SIZE_MAX &&
          !mc_entity_nbt_mob_call_error(error, expanded));
cleanup:
    free(error);
    free(copy);
    free(input);
    free(expected);
    free(expanded);
    return result;
}

/* Actual factory/allocation calls, separate from the passive authored matrix.
 * NULLWorld Skeleton has no native Registry and fails before SN.e. A real
 * reached Sheep class-allocation ceiling supplies first and repeated causes. */
static int actual_mob_factory_controls(void) {
    int result = 0;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL;
    mc_entity_nbt_error *error = malloc(sizeof(*error)), *saved = malloc(sizeof(*saved));
    mc_mob_error *direct = malloc(sizeof(*direct)), *first = malloc(sizeof(*first)),
                 *later = malloc(sizeof(*later));
    CHECK(error && saved && direct && first && later);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    static const uint16_t id[] = {'i', 'd'}, skeleton[] = {'S', 'k', 'e', 'l', 'e', 't', 'o', 'n'},
                          sheep[] = {'S', 'h', 'e', 'e', 'p'};
    mc_nbt_tag name = {.type = MC_NBT_STRING, .name = {id, 2}, .value.string = {skeleton, 8}};
    const mc_nbt_tag *items[] = {&name};
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {items, 1}};
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_factory_nbt(classes, NULL, &root, &entity, error) ==
              MC_ENTITY_NBT_ENTITY_FAILED &&
          entity == NULL && error->factory_bytecode == 45 &&
          error->late_hook == MC_ENTITY_NBT_LATE_MOB_CALL &&
          error->mob_call.payload == MC_ENTITY_MOB_CALL_CURRENT &&
          mc_entity_nbt_mob_call_error(error, direct) && direct->status == MC_MOB_ARGUMENT &&
          direct->native_policy && !direct->source_exception && direct->exception_class == NULL &&
          strcmp(direct->site.class_name, "fr") == 0 && direct->site.pc == 4 &&
          strcmp(direct->operation, "Skeleton initializer requires its actual Item namespace") ==
              0);
    *saved = *error;
    *first = *direct;
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    classes = NULL;
    CHECK(mc_entity_nbt_mob_call_error(saved, later) && equal_call(first, later));

    /* Measure genuine live owner allocations in a separate successful class
     * namespace. The next fresh class owner is authored with room for exactly
     * its completed EntityList and Mob class namespace, then reaches Sheep's
     * first real float[][] allocation with no remaining budget. */
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK &&
          mc_entity_classes_initialize_registry(classes, NULL) == MC_ENTITY_REGISTRY_OK);
    mc_entity_class_snapshot baseline;
    const mc_entity_class_literal *type = NULL;
    mc_mob_class_state mob;
    CHECK(mc_entity_classes_get_snapshot(classes, &baseline) == MC_ENTITY_OK &&
          mc_entity_class_literal_get(classes, MC_ENTITY_CLASS_KIND_SHEEP, &type) == MC_ENTITY_OK &&
          mc_mob_allocate(classes, NULL, type, &entity, direct) == MC_MOB_OK && entity != NULL &&
          mc_mob_classes_inspect(classes, &mob) && mob.sheep_phase == MC_ENTITY_CLASS_READY &&
          mob.allocation_bytes >= mob.sheep_array_bytes && mob.skeleton_stack_bytes == 0);
    const size_t namespace_bytes = mob.allocation_bytes - mob.sheep_array_bytes;
    mc_entity_release(entity);
    entity = NULL;
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    classes = NULL;
    CHECK(namespace_bytes > 0 && baseline.allocation_bytes <= SIZE_MAX - namespace_bytes);
    const mc_entity_limits limits = {baseline.allocation_bytes + namespace_bytes, SIZE_MAX};
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, &limits, &classes) == MC_ENTITY_OK);
    name.value.string = (mc_nbt_string){sheep, 5};
    CHECK(mc_entity_factory_nbt(classes, NULL, &root, &entity, error) == MC_ENTITY_NBT_LIMIT &&
          entity == NULL && error->factory_bytecode == 45 &&
          error->mob_call.payload == MC_ENTITY_MOB_CALL_FIRST_SHARED &&
          mc_entity_nbt_mob_call_error(error, first) && first->status == MC_MOB_LIMIT &&
          first->native_policy && !first->source_exception && first->site.pc == 2 &&
          first->first_class_failure.status == MC_MOB_LIMIT &&
          first->first_class_failure.entity.status == MC_ENTITY_LIMIT &&
          mc_mob_classes_inspect(classes, &mob) && mob.sheep_phase == MC_ENTITY_CLASS_FAILED &&
          equal_cause(first->first_class_failure,
                      (mc_mob_class_cause){first->status, first->operation, first->site,
                                           first->child, first->native_policy,
                                           first->source_exception, first->exception_class,
                                           first->entity, first->registry, first->stack}));
    CHECK(mc_entity_factory_nbt(classes, NULL, &root, &entity, error) ==
              MC_ENTITY_NBT_ENTITY_FAILED &&
          entity == NULL && error->factory_bytecode == 45 &&
          error->mob_call.payload == MC_ENTITY_MOB_CALL_FIRST_PREVIOUS &&
          mc_entity_nbt_mob_call_error(error, later) && later->status == MC_MOB_CLASS_ERRONEOUS &&
          later->native_policy && !later->source_exception && later->exception_class == NULL &&
          equal_cause(later->first_class_failure, first->first_class_failure) &&
          equal_entity(later->entity, (mc_entity_error){0}) &&
          equal_registry(later->registry, (mc_registry_error){0}));
    *saved = *error;
    *direct = *later;
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    classes = NULL;
    CHECK(mc_entity_nbt_mob_call_error(saved, later) && equal_call(direct, later));
cleanup:
    mc_entity_release(entity);
    if (classes != NULL && mc_entity_classes_free(classes) != MC_ENTITY_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    free(error);
    free(saved);
    free(direct);
    free(first);
    free(later);
    return result;
}

int main(void) {
    int result = 0;
    mc_entity_nbt_error *error = malloc(sizeof(*error));
    CHECK(error != NULL);
    mc_entity_nbt_error_init(error);
    CHECK(sizeof(*error) == 544 && error->late_hook == MC_ENTITY_NBT_LATE_NONE &&
          error->index == SIZE_MAX && !error->minecart_read.selected &&
          !error->minecart_write.selected && !error->inventory.slot_selected &&
          error->inventory.status == MC_MINECART_INVENTORY_OK);
    error->status = MC_ENTITY_NBT_WRONG_TAG;
    error->operation = "authored common diagnostic";
    error->index = 7;
    error->base_bytecode = 311;
    error->local_player_bytecode = 2;
    error->player_bytecode = 17;
    error->stack_bytecode = 14;
    error->nbt.offset = 9;
    error->registry.initializer_bytecode = 13;
    error->stack.status = MC_ITEM_STACK_INVALID_FIELD;
    mc_entity_nbt_select_player_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_PLAYER && error->living_bytecode == 0 &&
          error->player_inventory.status == MC_INVENTORY_NBT_OK &&
          error->player_inventory.index == SIZE_MAX && error->player_inventory.slot == -1 &&
          error->player_inventory.field == NULL && error->player_inventory.operation == NULL &&
          error->player_inventory.registry.initializer_pc == -1 &&
          error->player_inventory.registry.constructor_pc == -1 &&
          error->player_inventory.registry.block_id == -1 &&
          error->player_inventory.registry.item_argument == -1 &&
          error->player_inventory.registry.active_initializer_pc == -1);
    CHECK(error->status == MC_ENTITY_NBT_WRONG_TAG && error->operation != NULL &&
          error->index == 7 && error->base_bytecode == 311 && error->local_player_bytecode == 2 &&
          error->player_bytecode == 17 && error->stack_bytecode == 14 && error->nbt.offset == 9 &&
          error->registry.initializer_bytecode == 13 &&
          error->stack.status == MC_ITEM_STACK_INVALID_FIELD);
    error->player_inventory.registry.status = MC_REGISTRY_LIMIT;
    error->player_inventory.registry.initializer_pc = 123;
    error->player_inventory.index = 3;
    error->living_bytecode = 55;
    mc_entity_nbt_select_player_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_PLAYER && error->living_bytecode == 55 &&
          error->player_inventory.index == 3 &&
          error->player_inventory.registry.status == MC_REGISTRY_LIMIT &&
          error->player_inventory.registry.initializer_pc == 123);
    mc_entity_nbt_select_cart_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_CART && !error->minecart_read.selected &&
          !error->minecart_read.entered && error->minecart_read.declaring_class == NULL &&
          !error->minecart_write.selected && error->minecart_write.method == NULL &&
          error->inventory.status == MC_MINECART_INVENTORY_OK && !error->inventory.slot_selected &&
          error->inventory.operation == NULL &&
          error->inventory.stack_cause == MC_MINECART_INVENTORY_STACK_NONE);
    error->minecart_read.selected = true;
    error->minecart_read.bytecode = 137;
    error->inventory.slot_selected = true;
    error->inventory.slot = 5;
    mc_entity_nbt_select_cart_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_CART && error->minecart_read.selected &&
          error->minecart_read.bytecode == 137 && error->inventory.slot_selected &&
          error->inventory.slot == 5 && error->status == MC_ENTITY_NBT_WRONG_TAG &&
          error->index == 7 && error->base_bytecode == 311 && error->player_bytecode == 17 &&
          error->nbt.offset == 9 && error->registry.initializer_bytecode == 13);
    mc_entity_nbt_select_player_detail(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_PLAYER && error->living_bytecode == 0 &&
          error->player_inventory.index == SIZE_MAX &&
          error->player_inventory.registry.status == MC_REGISTRY_OK &&
          error->player_inventory.registry.initializer_pc == -1);
    mc_entity_nbt_error_init(error);
    CHECK(error->late_hook == MC_ENTITY_NBT_LATE_NONE && error->status == MC_ENTITY_NBT_OK &&
          error->index == SIZE_MAX && error->base_bytecode == 0 && error->player_bytecode == 0 &&
          !error->minecart_read.selected && !error->minecart_write.selected &&
          !error->inventory.slot_selected && error->inventory.status == MC_MINECART_INVENTORY_OK);
    mc_entity_nbt_select_cart_detail(NULL);
    mc_entity_nbt_select_player_detail(NULL);
    CHECK(passive_mob_controls() == 0);
    CHECK(actual_mob_factory_controls() == 0);
cleanup:
    free(error);
    if (result == 0 && printf("Player NBT error contracts: %u checks\n", checks) < 0)
        return 1;
    return result;
}
