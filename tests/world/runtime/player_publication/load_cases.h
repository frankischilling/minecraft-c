/* Authored controls through the real fd.a(gs) interface and production owners.
 * Receipt payloads below are opaque identity tokens, not RenderGlobal bodies. */
#include "../../../../src/world/runtime/world_access_internal.h"
#include "mc/entity/nbt/nbt.h"
#include "mc/world/runtime/loading/player.h"

typedef struct load_receipt_observer {
    mc_world_access_failure failure;
    size_t calls;
} load_receipt_observer;

static mc_world_storage_status load_receipt_added(void *context, mc_world_runtime *world,
                                                  mc_entity *entity, mc_world_storage_error *error,
                                                  mc_world_access_failure *failure) {
    load_receipt_observer *observer = context;
    (void)world;
    (void)entity;
    ++observer->calls;
    *failure = observer->failure;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Authored same-invocation receipt control", 917};
    return error->status;
}

static bool detailed_load_cases(fixture *f, mc_entity *player) {
    mc_world_player_load_error *error = malloc(sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_world_runtime_load_player(NULL, player, error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->bytecode == 1 && error->catch_bytecode == 88 && error->source_exception &&
          error->caught_exception && error->print_stack_trace_pending && !error->source_returned &&
          !error->native_policy &&
          strcmp(error->exception_class, "java/lang/NullPointerException") == 0);
    mc_world_info *const original_info = f->world->fields.x;
    f->world->fields.x = NULL;
    const mc_world_storage_status null_info = mc_world_runtime_load_player(f->world, player, error);
    f->world->fields.x = original_info;
    CHECK(null_info == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error->bytecode == 4 &&
          error->catch_bytecode == 88 && error->source_exception && !error->nbt_returned &&
          !error->info_cleared && !error->publication_returned && f->world->active_calls == 0);

    /* Save real current Player/Inventory state, then clear one actual slot.
     * Loading must reconstruct it before clearing Info.Player and publishing. */
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 1, 3, 7) == MC_ITEM_STACK_OK);
    mc_inventory_player *inventory = player->local_player->fields.player.c;
    CHECK(mc_inventory_player_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK);
    mc_nbt_tree *tree = NULL;
    CHECK(mc_entity_build_nbt(player, NULL, &tree, &error->nbt) == MC_ENTITY_NBT_OK);
    mc_world_info_player *saved = NULL, *current = NULL;
    CHECK(mc_world_info_player_copy(mc_nbt_tree_root(tree), NULL, &saved, NULL) ==
          MC_WORLD_INFO_OK);
    mc_nbt_tree_free(tree);
    CHECK(mc_world_info_player_set(original_info, saved) == MC_WORLD_INFO_OK &&
          mc_inventory_player_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK);
    const size_t player_count = f->world->fields.d->count;
    CHECK(mc_world_runtime_load_player(f->world, player, error) == MC_WORLD_STORAGE_OK &&
          error->saved_data_present && error->nbt_returned && error->info_cleared &&
          error->publication_returned && error->source_returned && error->bytecode == 91 &&
          !error->source_exception && !error->print_stack_trace_pending && !error->native_policy &&
          error->nbt.status == MC_ENTITY_NBT_OK &&
          error->publication.status == MC_WORLD_STORAGE_OK &&
          error->publication.world_access_failure.receipt == NULL &&
          f->world->fields.d->count == player_count + 1);
    const mc_item_stack *restored = NULL;
    mc_item_stack_fields fields;
    CHECK(mc_inventory_player_borrow(inventory, 0, &restored, NULL) == MC_INVENTORY_OK &&
          restored != NULL && restored != stack &&
          mc_item_stack_fields_get(restored, &fields) == MC_ITEM_STACK_OK && fields.id == 1 &&
          fields.count == 3 && fields.damage == 7 &&
          mc_world_info_player_get(original_info, &current) == MC_WORLD_INFO_OK && current == NULL);
    mc_item_stack_release(stack);
    mc_world_info_player_release(saved);

    /* Failure preserves the exact source Info tag, and carries the real child
     * instead of reporting a successfully swallowed Java Exception. */
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    saved = NULL;
    CHECK(mc_world_info_player_copy(&empty, NULL, &saved, NULL) == MC_WORLD_INFO_OK &&
          mc_world_info_player_set(original_info, saved) == MC_WORLD_INFO_OK);
    const size_t before_failure = f->world->fields.d->count;
    CHECK(mc_world_runtime_load_player(f->world, player, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->bytecode == 14 && error->catch_bytecode == 88 && error->source_exception &&
          error->print_stack_trace_pending && !error->native_policy && !error->nbt_returned &&
          !error->info_cleared && !error->publication_returned &&
          error->nbt.status == MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS &&
          error->nbt.base_bytecode == 25 && error->nbt.index == 0 &&
          strcmp(error->exception_class, "java/lang/IndexOutOfBoundsException") == 0 &&
          mc_world_info_player_get(original_info, &current) == MC_WORLD_INFO_OK &&
          current == saved && f->world->fields.d->count == before_failure);
    CHECK(
        mc_world_runtime_load_player(f->world, NULL, error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
        error->bytecode == 14 && error->catch_bytecode == 88 && error->source_exception &&
        error->nbt.status == MC_ENTITY_NBT_OK && !error->nbt_returned &&
        mc_world_info_player_get(original_info, &current) == MC_WORLD_INFO_OK && current == saved);
    CHECK(mc_world_info_player_set(original_info, NULL) == MC_WORLD_INFO_OK);
    mc_world_info_player_release(saved);

    CHECK(mc_world_runtime_load_player(f->world, NULL, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !error->saved_data_present && error->bytecode == 79 && error->catch_bytecode == 88 &&
          error->publication.bytecode == 1 && error->source_exception &&
          !error->publication_returned && !error->info_cleared);

    static const unsigned char opaque_cause = 0;
    const mc_client_render_global_error *const identity =
        (const mc_client_render_global_error *)(const void *)&opaque_cause;
    const mc_world_access_source_callbacks callbacks = {.entity_added = load_receipt_added};
    load_receipt_observer first = {0}, foreign = {0};
    mc_world_access *first_access = NULL, *foreign_access = NULL;
    mc_world_access_receipt *receipt = NULL;
    CHECK(mc_world_access_source_create(&callbacks, &first, &first_access, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_access_receipt_create(first_access, identity, &receipt, &first.failure, NULL) ==
          MC_WORLD_STORAGE_OK);
    receipt->exception_class = "java/lang/NullPointerException";
    CHECK(mc_world_runtime_add_world_access(f->world, first_access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_load_player(f->world, player, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          first.calls == 1 && error->bytecode == 79 && error->catch_bytecode == 88 &&
          error->source_exception && error->print_stack_trace_pending && !error->native_policy &&
          error->publication.bytecode == 103 && error->publication.world_access_bytecode == 29 &&
          error->publication.cause.system_error == 917 &&
          error->publication.world_access_failure.receiver == first_access &&
          error->publication.world_access_failure.render_global == identity &&
          error->publication.world_access_failure.receipt == receipt);
    mc_world_access_failure capture = {0};
    CHECK(mc_world_access_failure_retain(&error->publication.world_access_failure, &capture) ==
          MC_WORLD_STORAGE_OK);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, first_access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_receipt_retire(&receipt, NULL) == MC_WORLD_STORAGE_BUSY &&
          capture.render_global == identity && capture.receiver == first_access);
    /* A different actual callback returns a foreign/stale producer receipt.
     * Its scalar failure remains visible but it cannot classify as the first
     * producer's Java Exception or acquire that producer's occurrence. */
    foreign.failure = first.failure;
    CHECK(mc_world_access_source_create(&callbacks, &foreign, &foreign_access, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, foreign_access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_load_player(f->world, player, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          foreign.calls == 1 && error->bytecode == 79 && error->catch_bytecode == 0 &&
          !error->source_exception && !error->print_stack_trace_pending && error->native_policy &&
          error->publication.cause.system_error == 917 &&
          error->publication.world_access_failure.receiver == foreign_access &&
          error->publication.world_access_failure.render_global == identity &&
          error->publication.world_access_failure.receipt == receipt);
    mc_world_access_failure rejected = {0};
    CHECK(mc_world_access_failure_retain(&error->publication.world_access_failure, &rejected) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          rejected.receipt == NULL);
    CHECK(mc_world_runtime_remove_world_access(f->world, foreign_access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(foreign_access, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_access_failure_release(&capture);
    CHECK(mc_world_access_receipt_retire(&receipt, NULL) == MC_WORLD_STORAGE_OK && receipt == NULL);
    mc_world_player_load_error_init(error); /* Expire consumed borrowed receipt. */
    CHECK(mc_world_access_free(first_access, NULL) == MC_WORLD_STORAGE_OK &&
          f->world->active_calls == 0 && f->world->local_leases == 0);
    free(error);
    return true;
}
