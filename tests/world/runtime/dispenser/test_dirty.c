/* Authored native lifetime control with genuine loaded World/Tile owners.
 * Actual self-removing WorldAccess producers own their immutable invocation
 * receipts and heap class names. No original Throwable/GUI parity is claimed. */
int mc_dispenser_dirty_unused_publication_main(int, char **);
#define main mc_dispenser_dirty_unused_publication_main
#include "../test_entity_publication.c"
#undef main
#include "inventory/dispenser_internal.h"
#include "world/runtime/world_access_internal.h"

typedef struct azd_producer {
    mc_world_access *self;
    mc_world_access_receipt *receipts[3];
    char *names[3];
    mc_world_runtime *world;
    mc_tile_entity *expected;
    mc_chunk_state *chunk;
    size_t calls;
    bool observations_ok;
} azd_producer;

static mc_world_storage_status azd_dirty(void *context, mc_world_runtime *world, int32_t x,
                                         int32_t y, int32_t z, mc_tile_entity *tile,
                                         mc_world_storage_error *cause,
                                         mc_world_access_failure *failure) {
    azd_producer *o = context;
    const size_t i = o->calls++;
    o->observations_ok &= world == o->world && tile == o->expected && x == 0 && y == 64 && z == 0 &&
                          o->chunk->dirty && i < COUNT(o->receipts);
    if (i >= COUNT(o->receipts)) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "AZ authored callback bound", 92};
        return cause->status;
    }
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, o->self, &removed, cause);
    o->observations_ok &= status == MC_WORLD_STORAGE_OK && removed &&
                          mc_world_access_free(o->self, NULL) == MC_WORLD_STORAGE_BUSY;
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_INVALID_DATA : status;
    const char value[] = "test/OwnedDispenserFailure";
    o->names[i] = malloc(sizeof(value));
    if (o->names[i] == NULL) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                          "AZ authored class-name allocation", 0};
        return cause->status;
    }
    memcpy(o->names[i], value, sizeof(value));
    status = mc_world_access_receipt_create(o->self, NULL, &o->receipts[i], failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    /* Producer-owned immutable class name. The adapter must retain this
     * actual receipt and must not infer storage lifetime from the string. */
    o->receipts[i]->exception_class = o->names[i];
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "AZ authored dirty producer failure", 91};
    return cause->status;
}

int main(int argc, char **argv) {
    bool ok = false;
    char root[8192];
    fixture *f = NULL;
    mc_tile_entity *tiles[2] = {0};
    mc_inventory_dispenser *adapters[2] = {0};
    mc_inventory_dispenser_state states[2] = {0};
    mc_inventory_error source[3] = {0}, copies[3] = {0}, index = {0};
    mc_item_stack *stack = NULL, *sentinel = NULL, *returned = NULL, *held = NULL;
    azd_producer producer = {.observations_ok = true};
    const uint16_t title[] = {'A', 'Z', ' ', 'd', 'i', 'r', 't', 'y'};
    if (argc != 3 || strcmp(argv[1], "world-dispenser-dirty") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = (stream){.bytes = language};
    r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options registry = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&registry, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
              MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          open_store(f, 0, root) && create_world(f, 0));

    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 1, 95, 7) == MC_ITEM_STACK_OK &&
          mc_item_stack_allocate(&sentinel) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(sentinel, 4, 3, 2) == MC_ITEM_STACK_OK);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_tile_entity_allocate(f->worlds[0], MC_TILE_ENTITY_DISPENSER, &tiles[i]) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_construct(tiles[i]) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_bind_world(tiles[i], f->worlds[0]) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_set_coordinates(tiles[i], 0, 64, 0) == MC_TILE_ENTITY_OK &&
              mc_inventory_dispenser_from_tile(MC_REGISTRY_CLIENT, tiles[i], NULL, &adapters[i],
                                               NULL) == MC_INVENTORY_OK &&
              mc_inventory_dispenser_read_state(adapters[i], &states[i]) &&
              states[i].diagnostic_owner_id != 0);
    }
    CHECK(states[0].diagnostic_owner_id != states[1].diagnostic_owner_id);
    producer.world = f->worlds[0];
    producer.chunk = f->states[0];
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = azd_dirty};
    CHECK(mc_world_access_source_create(&callbacks, &producer, &producer.self, NULL) ==
          MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 3; ++i) {
        const size_t owner = i == 2 ? 1u : 0u;
        producer.expected = tiles[owner];
        CHECK(mc_world_runtime_add_world_access(f->worlds[0], producer.self, NULL) ==
              MC_WORLD_STORAGE_OK);
        mc_inventory_status status;
        if (i == 1) {
            returned = sentinel;
            status = mc_inventory_dispenser_split(adapters[owner], 0, 0, &returned, &source[i]);
            CHECK(returned == sentinel);
            returned = NULL;
        } else
            status = mc_inventory_dispenser_set(adapters[owner], 0, stack, &source[i]);
        CHECK(status == MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY && source[i].source_exception &&
              source[i].exception_class_from_dispenser &&
              source[i].exception_class == producer.names[i] &&
              source[i].dispenser_diagnostic_owner_id == states[owner].diagnostic_owner_id &&
              source[i].dispenser != NULL && source[i].frames[0].pc == (i == 1 ? 73 : 31) &&
              strcmp(source[i].frames[0].class_name, "az") == 0 &&
              source[i].dispenser->bytecode == 55 &&
              source[i].dispenser->tile_entity_bytecode == 24 &&
              source[i].dispenser->cause.system_error == 91 && producer.observations_ok &&
              producer.calls == i + 1 && producer.chunk->dirty);
        copies[i] = source[i];
        mc_item_stack_fields value;
        CHECK(mc_inventory_dispenser_get(adapters[owner], 0, &held, NULL) == MC_INVENTORY_OK &&
              held == stack && mc_item_stack_fields_get(held, &value) == MC_ITEM_STACK_OK &&
              value.count == 64);
        mc_item_stack_release(held);
        held = NULL;
        for (size_t j = 0; j <= i; ++j)
            CHECK(producer.receipts[j]->holds == 2 &&
                  mc_world_access_receipt_retire(&producer.receipts[j], NULL) ==
                      MC_WORLD_STORAGE_BUSY);
    }
    CHECK(copies[0].exception_class != copies[2].exception_class &&
          strcmp(copies[0].exception_class, copies[2].exception_class) == 0);
    CHECK(mc_inventory_dispenser_set(adapters[0], 9, NULL, &index) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          index.source_exception && !index.exception_class_from_dispenser &&
          index.dispenser_diagnostic_owner_id == states[0].diagnostic_owner_id);
    CHECK(mc_inventory_dispenser_end_calls(adapters[0], NULL) == MC_INVENTORY_OK);
    for (size_t i = 0; i < 3; ++i) {
        mc_inventory_dispenser_error_expire(&copies[i], states[0].diagnostic_owner_id);
        mc_inventory_dispenser_error_expire(&source[i], states[0].diagnostic_owner_id);
    }
    mc_inventory_dispenser_error_expire(&index, states[0].diagnostic_owner_id);
    for (size_t i = 0; i < 2; ++i)
        CHECK(copies[i].dispenser == NULL && copies[i].exception_class == NULL &&
              copies[i].source_exception && copies[i].frames[0].pc == (i == 1 ? 73 : 31) &&
              copies[i].status == source[i].status);
    CHECK(copies[2].exception_class == producer.names[2] && copies[2].dispenser != NULL &&
          copies[2].dispenser_diagnostic_owner_id == states[1].diagnostic_owner_id &&
          copies[2].exception_class_from_dispenser && producer.receipts[2]->holds == 2 &&
          index.dispenser == NULL && index.source_exception && index.exception_class != NULL &&
          strcmp(index.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0 &&
          index.frames[0].pc == 6);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(producer.receipts[i]->holds == 1 &&
              mc_world_access_receipt_retire(&producer.receipts[i], NULL) == MC_WORLD_STORAGE_OK);
        free(producer.names[i]);
        producer.names[i] = NULL;
        CHECK(copies[i].exception_class == NULL && copies[i].source_exception);
    }
    CHECK(mc_inventory_dispenser_end_calls(adapters[1], NULL) == MC_INVENTORY_OK);
    mc_inventory_dispenser_error_expire(&copies[2], states[1].diagnostic_owner_id);
    mc_inventory_dispenser_error_expire(&source[2], states[1].diagnostic_owner_id);
    CHECK(copies[2].exception_class == NULL && copies[2].source_exception &&
          copies[2].frames[0].pc == 31 && producer.receipts[2]->holds == 1 &&
          mc_world_access_receipt_retire(&producer.receipts[2], NULL) == MC_WORLD_STORAGE_OK);
    free(producer.names[2]);
    producer.names[2] = NULL;
    CHECK(mc_world_access_free(producer.self, NULL) == MC_WORLD_STORAGE_OK);
    producer.self = NULL;
    ok = true;
cleanup:
    if (returned != sentinel)
        mc_item_stack_release(returned);
    returned = NULL; /* The sentinel keeps its separate actual occurrence. */
    mc_item_stack_release(held);
    mc_item_stack_release(stack);
    mc_item_stack_release(sentinel);
    for (size_t i = 0; i < 2; ++i) {
        if (mc_inventory_dispenser_retire(&adapters[i], NULL) != MC_INVENTORY_OK)
            return 1;
        for (size_t j = 0; j < COUNT(source); ++j) {
            mc_inventory_dispenser_error_expire(&source[j], states[i].diagnostic_owner_id);
            mc_inventory_dispenser_error_expire(&copies[j], states[i].diagnostic_owner_id);
        }
        mc_inventory_dispenser_error_expire(&index, states[i].diagnostic_owner_id);
        mc_tile_entity_release(tiles[i]);
    }
    for (size_t i = 0; i < COUNT(producer.receipts); ++i) {
        if (mc_world_access_receipt_retire(&producer.receipts[i], NULL) != MC_WORLD_STORAGE_OK)
            return 1;
        free(producer.names[i]);
    }
    if (producer.self != NULL) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(producer.world, producer.self, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            mc_world_access_free(producer.self, NULL) != MC_WORLD_STORAGE_OK)
            return 1;
    }
    if (!cleanup_fixture(f) || !ok)
        return 1;
    printf("Dispenser dirty producer ownership passed: %u checks\n", checks);
    return 0;
}
