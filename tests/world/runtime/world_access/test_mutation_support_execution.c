/* Real SourceSaving/Kernel and generated boundary Chunk owners. Imported mains
 * are never run; callbacks supply declared native/Throwable boundary inputs. */
int ms_unused_scalar_main(int, char **);
#define MC_WORLD_BLOCK_SCALAR_MAIN ms_unused_scalar_main
#include "test_block_scalar_execution.c"
#undef MC_WORLD_BLOCK_SCALAR_MAIN

#include "world/runtime/mutation/mutation_internal.h"
#include "world/runtime/mutation/world_execution_internal.h"
#include "world/runtime/solid_support_execution_internal.h"
#include "world/storage/generated_internal.h"

typedef struct ms_fixture {
    bs_fixture scalar;
    mc_world_mutation_execution_call *call;
    mc_world_mutation_error error;
    mc_world_runtime_execution_result owned;
    mc_entity_tick_execution_world_token activation, descendant;
    mc_world_access_receipt *receipt;
    mc_entity *item; /* Borrowed from the actual loaded World occurrence. */
    mc_world_chunk *generated;
    mc_world_storage_status raw;
    size_t callbacks, cells;
    const char *mode;
    bool source, torch, callback_valid;
} ms_fixture;

static const mc_entity_tick_execution_site ms_site = {"native.mutation.support", "run", "()V", 0};

/* Use the shared real SourceSaving constructors, with two actual provider
 * slots for the generated root and the cold loaded support Chunk. */
static int ms_open(ms_fixture *s) {
    source_save_fixture *source = &s->scalar.cq.source;
    fixture *f = &source->game;
    mc_world_info_name *folder = ss_name("world");
    VERIFY(folder != NULL &&
           mc_world_store_source_allocate(source->handlers, &f->stores[0], &source->source) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_construct(f->stores[0], source->directory, folder, false,
                                           &source->source) == MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    limits.max_scheduled_entries = 0;
    VERIFY(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
           mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                           &limits, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    VERIFY(mc_world_runtime_construct_world(f->worlds[0], &arguments, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
               MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    mc_world_runtime_source_construction_state construction;
    VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &construction) &&
           construction.metadata_called && construction.metadata_returned &&
           construction.loader != NULL && f->providers[0].cache->capacity >= 2);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    VERIFY(blocks != NULL &&
           mc_world_store_create_generated(f->stores[0], f->worlds[0], 0, 0, 0, &blocks,
                                           MC_CHUNK_BLOCKS, &s->generated,
                                           &source->error) == MC_WORLD_STORAGE_OK &&
           blocks == NULL);
    f->chunks[0] = s->generated;
    f->states[0] = mc_world_chunk_state(s->generated);
    VERIFY(f->states[0] != NULL &&
           mc_chunk_cache_publish(f->providers[0].cache, 0, 0, f->states[0]) &&
           mc_chunk_cache_contains(f->providers[0].cache, 0, 0) &&
           !mc_chunk_cache_contains(f->providers[0].cache, 1, 0));
    mc_chunk_data *data = f->states[0]->data;
    VERIFY(data != NULL && s->generated->storage_kind == CHUNK_STORAGE_GENERATED);
    data->blocks[cell(14, 64, 8)] = (uint8_t)(s->torch ? 0 : 1);
    data->blocks[cell(15, 64, 8)] = (uint8_t)(s->torch ? 50 : 65);
    const size_t index = cell(15, 64, 8);
    data->metadata[index >> 1] = (uint8_t)((s->torch ? 2u : 4u) << ((index & 1u) * 4u));
    return 0;
}

static mc_world_storage_status ms_cell(void *context, mc_world_runtime *world, int32_t x, int32_t y,
                                       int32_t z, mc_world_storage_error *cause,
                                       mc_world_access_failure *failure) {
    (void)failure;
    ms_fixture *s = context;
    ++s->cells;
    if (world != s->scalar.cq.source.game.worlds[0] || x != 14 || y != 64 || z != 8 ||
        s->cells != 1) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Mutation support unexpected cell callback", 0};
        return cause->status;
    }
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ms_added(void *context, mc_world_runtime *world, mc_entity *item,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ms_fixture *s = context;
    fixture *f = &s->scalar.cq.source.game;
    mc_entity_tick_execution *execution = &s->scalar.cq.execution;
    ++s->callbacks;
    mc_item_stack_fields stack = {0};
    mc_world_access_failure_lease *lease = mc_entity_tick_execution_world_lease(execution);
    s->callback_valid = s->callbacks == 1 && world == f->worlds[0] && s->call != NULL &&
                        item != NULL && mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) &&
                        item->base.bF && item->base.x == 24.25 && item->base.y == 64. &&
                        item->base.z == 8.25 &&
                        mc_item_stack_fields_get(item->stack, &stack) == MC_ITEM_STACK_OK &&
                        stack.id == 262 && stack.count == 5 && stack.damage == 7 &&
                        world->fields.b->count == 1 && world->fields.b->items[0] == item &&
                        lease != NULL && lease->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                        mc_entity_tick_execution_current_world(execution, &s->activation) &&
                        !cq_token_same(s->activation, s->scalar.outer) &&
                        execution->world_slots[s->scalar.outer.slot].loader_open;
    if (!s->callback_valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Mutation support actual loaded Item callback", 0};
        return cause->status;
    }
    s->item = item;
    if (s->source) {
        const mc_world_storage_status receipt = mc_world_access_receipt_create(
            s->scalar.cq.receiver, NULL, &s->receipt, failure, cause);
        if (receipt != MC_WORLD_STORAGE_OK)
            return receipt;
        s->receipt->exception_class = "java/lang/IllegalStateException";
    }
    const mc_world_storage_status pushed =
        mc_entity_tick_execution_push_world(execution, world, ms_site, &s->descendant);
    if (pushed != MC_WORLD_STORAGE_OK) {
        *cause = (mc_world_storage_error){pushed, "Mutation support descendant capture", 0};
        return pushed;
    }
    *cause = s->source ? (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                                  "Mutation support Item observer", 71}
                       : (mc_world_storage_error){0};
    return cause->status;
}

static int ms_run(ms_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_entity_tick_execution *execution = &s->scalar.cq.execution;
    VERIFY(bs_begin(&s->scalar) == 0);
    const mc_world_access_source_callbacks callbacks = {.cell = ms_cell, .entity_added = ms_added};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK);
    const mc_world_mutation_execution_context context = {.execution = execution,
                                                         .code_classes = f->classes[0],
                                                         .code = cq_code,
                                                         .result = &s->owned,
                                                         .world = s->scalar.outer};
    s->raw = MC_WORLD_STORAGE_LIMIT;
    mc_world_mutation_error_init(&s->error);
    const bool continued = mc_world_runtime_metadata_set_in_execution(&context, world, 14, 64, 8, 1,
                                                                      &s->call, &s->raw, &s->error);
    const mc_world_storage_status expected =
        s->source ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_BUSY;
    mc_world_storage_status support_raw = MC_WORLD_STORAGE_LIMIT;
    const mc_world_solid_support_execution *support =
        mc_world_mutation_execution_failed_support(s->call, &support_raw);
    const mc_chunk_data *data = f->states[0]->data;
    if (s->raw != expected)
        fprintf(stderr, "MUTATION_SUPPORT_RAW expected=%d actual=%d support=%d native=%d\n",
                (int)expected, (int)s->raw, (int)support_raw, (int)s->owned.ownership_status);
    VERIFY(!continued && s->call != NULL && s->raw == expected && s->error.status == expected &&
           support != NULL && support_raw == expected && support->entered &&
           support->native_stopped && !support->completed &&
           support->receiver_site.pc == (s->torch ? 22 : 70) &&
           support->receiver_site.class_name != NULL &&
           strcmp(support->receiver_site.class_name, s->torch ? "vm" : "dp") == 0 &&
           support->receiver_site.method != NULL &&
           strcmp(support->receiver_site.method, s->torch ? "a" : "b") == 0 &&
           support->detail.error.bytecode == 7 &&
           support->detail.error.status == MC_WORLD_STORAGE_OK &&
           support->detail.error.operation == NULL &&
           support->detail.error.cause.status == MC_WORLD_STORAGE_OK &&
           support->detail.access.query.cause.status == MC_WORLD_STORAGE_OK &&
           support->detail.error.registry.status == MC_REGISTRY_OK &&
           support->lower.nearby.query.cause.status == expected &&
           support->lower.nearby.query.acquisition == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
           s->owned.ownership_status == MC_WORLD_STORAGE_BUSY && s->owned.root_pending &&
           !s->owned.selection_ready && s->callbacks == 1 && s->callback_valid &&
           world->fields.b->count == 1 && world->fields.b->items[0] == s->item &&
           mc_world_mutation_execution_failed_drop(s->call) == NULL &&
           data->blocks[cell(14, 64, 8)] == (s->torch ? 0 : 1) &&
           data->blocks[cell(15, 64, 8)] == (s->torch ? 50 : 65) &&
           (((unsigned int)data->metadata[cell(15, 64, 8) >> 1] >> ((cell(15, 64, 8) & 1u) * 4u)) &
            15u) == (s->torch ? 2u : 4u) &&
           (((unsigned int)data->metadata[cell(14, 64, 8) >> 1] >> ((cell(14, 64, 8) & 1u) * 4u)) &
            15u) == 1 &&
           f->providers[0].cache->count == 2 && f->providers[0].cache->insertion_count == 2 &&
           mc_world_store_chunk_count(f->stores[0]) == 2 && world->active_calls == 0);
    if (s->torch) {
        VERIFY(support->receiver_site.descriptor != NULL &&
               strcmp(support->receiver_site.descriptor, "(Lfd;III)Z") == 0 &&
               support->lower.x == 16 && support->lower.y == 64 && support->lower.z == 8 &&
               s->error.hook_class != NULL && strcmp(s->error.hook_class, "vm") == 0 &&
               s->error.hook_method != NULL && strcmp(s->error.hook_method, "b(Lfd;IIII)V") == 0 &&
               s->error.hook_bytecode == 6 && s->error.torch_neighbor_bytecode == 6 &&
               s->error.torch_can_place_bytecode == 22 && s->error.torch_survive_bytecode == 6 &&
               s->error.cause_hook_class != NULL && strcmp(s->error.cause_hook_class, "vm") == 0 &&
               s->error.cause_hook_method != NULL &&
               strcmp(s->error.cause_hook_method, "a(Lfd;III)Z") == 0 &&
               s->error.cause_hook_bytecode == 22 && s->error.cause.status == MC_WORLD_STORAGE_OK);
        /* register_entities_run stores the actual activation cause in both
         * World.last_error and per-attempt acquisition.cause. Collision acquire
         * retains that same cause here. Native-stopped support detail is empty;
         * neither nested Torch caller may overwrite this genuine lower cause. */
        const mc_world_storage_error lower = support->lower.nearby.query.cause;
        if (!bs_error_same(world->last_error, lower))
            fprintf(stderr, "MUTATION_SUPPORT_WORLD_ERROR expected=%d/%s/%lu actual=%d/%s/%lu\n",
                    (int)lower.status, lower.operation == NULL ? "null" : lower.operation,
                    (unsigned long)lower.system_error, (int)world->last_error.status,
                    world->last_error.operation == NULL ? "null" : world->last_error.operation,
                    (unsigned long)world->last_error.system_error);
        VERIFY(bs_error_same(world->last_error, lower));
    }
    size_t closes = 0, close_index = SIZE_MAX;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (s->owned.closes[i].used) {
            ++closes;
            close_index = i;
        }
    VERIFY(closes == 1 && close_index < MC_ENTITY_TICK_EXECUTION_WORLDS);
    const mc_world_runtime_execution_close close = s->owned.closes[close_index];
    VERIFY(
        close.kind == MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_ENTITY &&
        close.phase == MC_WORLD_RUNTIME_EXECUTION_CLOSE_LOCALS_DRAINED &&
        close.source_status == (s->source ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_OK) &&
        close.child_failed == s->source && close.refused_status == MC_WORLD_STORAGE_BUSY &&
        cq_token_same(close.world, s->activation) &&
        execution->current_lease == s->descendant.slot &&
        execution->world_slots[s->descendant.slot].parent == close.world.slot &&
        mc_world_mutation_execution_native_close(s->call) == MC_WORLD_STORAGE_BUSY &&
        mc_world_mutation_execution_retire(&s->call) == MC_WORLD_STORAGE_BUSY && s->call != NULL &&
        mc_world_mutation_execution_failed_support(s->call, &support_raw) == support &&
        support_raw == expected &&
        mc_world_runtime_execution_finish_pending(execution, &s->owned) == MC_WORLD_STORAGE_BUSY);
    const mc_entity_tick_execution_error first = s->owned.ownership_error;
    VERIFY(mc_entity_tick_execution_pop_world(execution, &s->descendant, false) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_execution_retry_close(execution, &s->owned, &close) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_execution_finish_pending(execution, &s->owned) == MC_WORLD_STORAGE_OK &&
           !cq_pending(&s->owned) && s->owned.selection_ready &&
           s->owned.selection.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
           cq_owner_error_same(first, s->owned.ownership_error) &&
           mc_world_mutation_execution_failed_support(s->call, &support_raw) == support &&
           support_raw == expected && s->raw == expected && s->error.status == expected &&
           support->native_stopped && !support->completed && s->callbacks == 1 &&
           support->detail.error.cause.status == MC_WORLD_STORAGE_OK &&
           data->blocks[cell(15, 64, 8)] == (s->torch ? 50 : 65) &&
           mc_world_mutation_execution_failed_drop(s->call) == NULL);
    if (s->source) {
        const mc_world_access_failure *protected = NULL;
        const char *exception = NULL;
        VERIFY(mc_world_access_failure_lease_inspect(&execution->leases[s->activation.slot],
                                                     &protected) &&
               protected != NULL && protected->receipt == s->receipt &&
               mc_world_access_failure_source_exception(protected, &exception) &&
               strcmp(exception, "java/lang/IllegalStateException") == 0);
    }
    if (s->torch)
        printf("MUTATION_SUPPORT_RESULT mode=%s raw=%s support_raw=%s native=BUSY callbacks=1 "
               "torch=50 metadata=2 support=vm.a22 outer=vm.b6 can-place=22 survive=6 "
               "world-error=lower-query retained=1 drained=1 replay=0\n",
               s->mode, s->source ? "IO_ERROR" : "BUSY", s->source ? "IO_ERROR" : "BUSY");
    else
        printf("MUTATION_SUPPORT_RESULT mode=%s raw=%s support_raw=%s native=BUSY callbacks=1 "
               "ladder=65 metadata=4 retained=1 drained=1 replay=0\n",
               s->mode, s->source ? "IO_ERROR" : "BUSY", s->source ? "IO_ERROR" : "BUSY");
    VERIFY(mc_world_mutation_execution_native_close(s->call) == MC_WORLD_STORAGE_OK &&
           mc_world_mutation_execution_retire(&s->call) == MC_WORLD_STORAGE_OK && s->call == NULL);
    support = NULL;
    s->error = (mc_world_mutation_error){0};
    s->owned = (mc_world_runtime_execution_result){0};
    VERIFY(bs_end(&s->scalar, true) == 0);
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(world, s->scalar.cq.receiver, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed && mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_access_free(s->scalar.cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->scalar.cq.receiver = NULL;
    s->item = NULL;
    return 0;
}

static int ms_finish(ms_fixture *s) {
    source_save_fixture *source = &s->scalar.cq.source;
    fixture *f = &source->game;
    VERIFY(mc_world_runtime_retire_entity_containers(f->worlds[0], &source->error) ==
           MC_WORLD_STORAGE_OK);
    f->terminal[0] = true;
    VERIFY(retire_descriptors(f, 0) &&
           mc_world_runtime_retire_world(f->worlds[0], &source->error) == MC_WORLD_STORAGE_OK);
    while (mc_world_store_chunk_count(f->stores[0]) != 0) {
        mc_chunk_state *state = NULL;
        VERIFY(mc_world_store_chunk_at(f->stores[0], 0, &state, &source->error) ==
                   MC_WORLD_STORAGE_OK &&
               state != NULL &&
               mc_world_store_retire_chunk(f->stores[0], state, &source->error) ==
                   MC_WORLD_STORAGE_OK);
    }
    f->states[0] = NULL;
    f->chunks[0] = s->generated = NULL;
    VERIFY(mc_world_runtime_free(f->worlds[0], &source->error) == MC_WORLD_STORAGE_OK);
    f->worlds[0] = NULL;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    VERIFY(mc_world_store_source_release(&f->stores[0], &source->source) == MC_WORLD_SOURCE_OK &&
           f->stores[0] == NULL &&
           mc_world_source_regions_flush(source->source_regions, &source->region) ==
               MC_WORLD_SOURCE_OK &&
           cq_files(&s->scalar.cq, false) == 0 && ss_finish(source) == 0);
    for (size_t i = 0; i < COUNT(s->scalar.cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->scalar.cq.inputs[i]);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 &&
           (strcmp(argv[1], "descendant") == 0 || strcmp(argv[1], "source-descendant") == 0 ||
            strcmp(argv[1], "torch-descendant") == 0 ||
            strcmp(argv[1], "torch-source-descendant") == 0));
    printf("MUTATION_SUPPORT_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    ms_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->mode = argv[1];
    s->torch =
        strcmp(argv[1], "torch-descendant") == 0 || strcmp(argv[1], "torch-source-descendant") == 0;
    s->source = strcmp(argv[1], "source-descendant") == 0 ||
                strcmp(argv[1], "torch-source-descendant") == 0;
    VERIFY(ss_setup(&s->scalar.cq.source, argv[2]) == 0 && cq_files(&s->scalar.cq, true) == 0 &&
           ms_open(s) == 0 && ms_run(s) == 0 && ms_finish(s) == 0);
    free(s);
    puts("MUTATION_SUPPORT_OPEN finite-native-boundary/full-wake/Arrow");
    printf("PASS: %u actual owned mutation support checks;mode=%s\n", checks, argv[1]);
    return 0;
}
