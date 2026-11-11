/* Actual SourceSaving constructors and native terminal helpers. The imported
 * main is never run. These finite controls own all external cold graphs. */
int ps_unused_scalar_main(int, char **);
#define MC_WORLD_BLOCK_SCALAR_MAIN ps_unused_scalar_main
#include "test_block_scalar_execution.c"
#undef MC_WORLD_BLOCK_SCALAR_MAIN

#include "block/registry/runtime_internal.h"
#include "world/runtime/plants_execution_internal.h"
#include "world/storage/generated_internal.h"

typedef struct ps_fixture {
    bs_fixture scalar;
    mc_world_plant_execution plant;
    mc_world_query_execution query;
    mc_world_runtime_execution_result owned;
    mc_entity_tick_execution_world_token activation, descendant;
    mc_world_access_receipt *receipt;
    mc_world_chunk *generated;
    const mc_block *flower;
    mc_entity *item;
    mc_world_storage_status raw;
    size_t callbacks;
    const char *mode;
    bool source, leave_descendant, callback_valid, sky;
} ps_fixture;

static const mc_entity_tick_execution_site ps_site = {"native.plant.survival", "run", "()V", 0};

static int ps_open(ps_fixture *s) {
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
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    VERIFY(blocks != NULL &&
           mc_world_store_create_generated(f->stores[0], f->worlds[0], 0, 0, 0, &blocks,
                                           MC_CHUNK_BLOCKS, &s->generated,
                                           &source->error) == MC_WORLD_STORAGE_OK &&
           blocks == NULL);
    f->chunks[0] = s->generated;
    f->states[0] = mc_world_chunk_state(s->generated);
    VERIFY(f->states[0] != NULL && f->states[0]->data != NULL &&
           mc_chunk_cache_publish(f->providers[0].cache, 0, 0, f->states[0]) &&
           !mc_chunk_cache_contains(f->providers[0].cache, 1, 0));
    mc_registry_block_m_view array;
    VERIFY(mc_registry_block_m_get(f->registry[0], &array, NULL) == MC_REGISTRY_OK &&
           mc_registry_block_m_at(&array, 37, &s->flower, NULL) == MC_REGISTRY_OK &&
           s->flower != NULL && s->flower->kind == MC_BLOCK_FLOWER);
    /* Declared pre-entry Chunk payload inputs. The unequal transposed height
     * entries make fd.l's actual (localZ<<4)|localX indexing observable. */
    mc_chunk_data *data = f->states[0]->data;
    data->blocks[cell(7, 64, 8)] = 37;
    data->blocks[cell(7, 63, 8)] = 2;
    memset(data->skylight, 0, sizeof(data->skylight));
    memset(data->block_light, 0, sizeof(data->block_light));
    memset(data->heightmap, 127, sizeof(data->heightmap));
    data->heightmap[(8u << 4u) | 7u] = (uint8_t)(strcmp(s->mode, "sky-hidden") == 0 ? 65 : 64);
    if (strcmp(s->mode, "defining-class") == 0) {
        const size_t index = cell(7, 64, 8);
        data->skylight[index >> 1] = (uint8_t)(9u << ((index & 1u) * 4u));
        data->block_light[index >> 1] = (uint8_t)(12u << ((index & 1u) * 4u));
        VERIFY(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                               &f->classes[1], NULL) == MC_WORLD_STORAGE_OK);
    }
    VERIFY(!mc_world_runtime_chunk_light_seen(f->classes[0]) && f->providers[0].cache->count == 1 &&
           f->providers[0].cache->insertion_count == 1 &&
           mc_world_store_chunk_count(f->stores[0]) == 1);
    return 0;
}

static mc_world_storage_status ps_added(void *context, mc_world_runtime *world, mc_entity *item,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    ps_fixture *s = context;
    fixture *f = &s->scalar.cq.source.game;
    mc_entity_tick_execution *execution = &s->scalar.cq.execution;
    ++s->callbacks;
    mc_item_stack_fields stack = {0};
    mc_world_access_failure_lease *lease = mc_entity_tick_execution_world_lease(execution);
    s->callback_valid = s->callbacks == 1 && world == f->worlds[0] && item != NULL &&
                        mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) && item->base.bF &&
                        item->base.x == 24.25 && item->base.y == 64. && item->base.z == 8.25 &&
                        mc_item_stack_fields_get(item->stack, &stack) == MC_ITEM_STACK_OK &&
                        stack.id == 262 && stack.count == 5 && stack.damage == 7 &&
                        world->fields.b->count == 1 && world->fields.b->items[0] == item &&
                        s->plant.entered && s->plant.query.entered && s->plant.query.get_entered &&
                        !s->plant.query.get_returned &&
                        s->plant.query.receiver_site.pc == (s->sky ? 18 : 5) && lease != NULL &&
                        lease->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
                        mc_entity_tick_execution_current_world(execution, &s->activation) &&
                        !cq_token_same(s->activation, s->scalar.outer) &&
                        execution->world_slots[s->scalar.outer.slot].loader_open;
    if (!s->callback_valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Plant survival actual loaded Item callback", 0};
        return cause->status;
    }
    s->item = item;
    if (s->source) {
        const mc_world_storage_status status = mc_world_access_receipt_create(
            s->scalar.cq.receiver, NULL, &s->receipt, failure, cause);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        s->receipt->exception_class = "java/lang/IllegalStateException";
    }
    if (s->leave_descendant) {
        const mc_world_storage_status status =
            mc_entity_tick_execution_push_world(execution, world, ps_site, &s->descendant);
        if (status != MC_WORLD_STORAGE_OK) {
            *cause = (mc_world_storage_error){status, "Plant survival descendant capture", 0};
            return status;
        }
    }
    *cause = s->source ? (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                                  "Plant survival Item observer", 71}
                       : (mc_world_storage_error){0};
    return cause->status;
}

static int ps_failure(ps_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_entity_tick_execution *execution = &s->scalar.cq.execution;
    VERIFY(bs_begin(&s->scalar) == 0);
    const mc_world_access_source_callbacks callbacks = {.entity_added = ps_added};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK);
    mc_world_runtime_execution_result_init(&s->owned);
    mc_world_plant_execution_init(&s->plant, execution, f->classes[0], cq_code, s->scalar.outer,
                                  ps_site, &s->owned);
    s->raw = MC_WORLD_STORAGE_LIMIT;
    bool answer = true;
    /* Negative Y returns zero from fd.m without acquisition, so the same
     * Flower body conditionally reaches the real fd.l cold getter. */
    const bool continued = mc_world_runtime_flower_survives_in_execution(
        &s->plant, world, s->flower, 16, s->sky ? -1 : 64, 8, &answer, &s->raw);
    const mc_world_storage_status expected =
        s->source ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_BUSY;
    mc_world_query_execution *query = &s->plant.query;
    VERIFY(!continued && answer && s->raw == expected && s->plant.entered && !s->plant.completed &&
           s->plant.native_stopped == s->leave_descendant &&
           s->plant.detail.bytecode == (s->sky ? 18u : 5u) && s->plant.detail.status == expected &&
           query->entered && query->get_entered && query->get_returned &&
           query->get_status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && query->status == expected &&
           !query->completed && query->native_stopped == s->leave_descendant &&
           query->detail.status == expected && query->detail.bytecode == (s->sky ? 7u : 23u) &&
           query->detail.provider_bytecode == 6 && query->detail.chunk_bytecode == 0 &&
           query->detail.acquisition == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
           query->acquisition.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
           query->acquisition.cause.status == expected &&
           bs_error_same(query->detail.cause, query->acquisition.cause) &&
           bs_error_same(world->last_error, query->acquisition.cause) && !s->plant.block.entered &&
           !mc_world_runtime_chunk_light_seen(f->classes[0]) && s->callbacks == 1 &&
           s->callback_valid && s->item != NULL && world->fields.b->count == 1 &&
           world->fields.b->items[0] == s->item && f->providers[0].cache->count == 2 &&
           f->providers[0].cache->insertion_count == 2 &&
           mc_world_store_chunk_count(f->stores[0]) == 2 && world->active_calls == 0 &&
           mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY);
    if (s->leave_descendant) {
        VERIFY(s->plant.detail.cause.status == MC_WORLD_STORAGE_OK &&
               s->plant.detail.query.cause.status == MC_WORLD_STORAGE_OK &&
               s->owned.ownership_status == MC_WORLD_STORAGE_BUSY && s->owned.root_pending &&
               !s->owned.selection_ready);
        size_t closes = 0, close_index = SIZE_MAX;
        for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
            if (s->owned.closes[i].used) {
                ++closes;
                close_index = i;
            }
        VERIFY(closes == 1 && close_index < MC_ENTITY_TICK_EXECUTION_WORLDS);
        const mc_world_runtime_execution_close close = s->owned.closes[close_index];
        VERIFY(close.kind == MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_ENTITY &&
               close.phase == MC_WORLD_RUNTIME_EXECUTION_CLOSE_LOCALS_DRAINED &&
               close.source_status ==
                   (s->source ? MC_WORLD_STORAGE_IO_ERROR : MC_WORLD_STORAGE_OK) &&
               close.child_failed == s->source && close.refused_status == MC_WORLD_STORAGE_BUSY &&
               cq_token_same(close.world, s->activation) &&
               execution->current_lease == s->descendant.slot &&
               execution->world_slots[s->descendant.slot].parent == close.world.slot &&
               mc_world_runtime_execution_finish_pending(execution, &s->owned) ==
                   MC_WORLD_STORAGE_BUSY);
        const mc_entity_tick_execution_error first = s->owned.ownership_error;
        VERIFY(mc_entity_tick_execution_pop_world(execution, &s->descendant, false) ==
                   MC_WORLD_STORAGE_OK &&
               mc_world_runtime_execution_retry_close(execution, &s->owned, &close) ==
                   MC_WORLD_STORAGE_OK &&
               mc_world_runtime_execution_finish_pending(execution, &s->owned) ==
                   MC_WORLD_STORAGE_OK &&
               !cq_pending(&s->owned) && s->owned.selection_ready &&
               s->owned.selection.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
               cq_owner_error_same(first, s->owned.ownership_error) && query->native_stopped &&
               !query->completed && s->raw == expected && answer && s->callbacks == 1 &&
               !mc_world_runtime_chunk_light_seen(f->classes[0]));
    } else {
        VERIFY(s->owned.ownership_status == MC_WORLD_STORAGE_OK && !cq_pending(&s->owned) &&
               s->owned.selection_ready &&
               s->owned.selection.status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
               bs_error_same(s->plant.detail.cause, query->detail.cause) &&
               s->plant.detail.query.acquisition == MC_CHUNK_ACQUIRE_CALLBACK_FAILED);
    }
    if (s->source) {
        const char *exception = NULL;
        const mc_world_access_failure *failure =
            &query->acquisition.batch.publication.world_access_failure;
        VERIFY(failure->receipt == s->receipt &&
               mc_world_access_failure_source_exception(failure, &exception) &&
               strcmp(exception, "java/lang/IllegalStateException") == 0 &&
               mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
    }
    printf("PLANT_SURVIVAL_RESULT mode=%s raw=%s native=%s callbacks=1 site=wb.g%d "
           "query=%s%u output-preserved=1 Chunk.a=0 retained=1 drained=1 replay=0\n",
           s->mode, s->source ? "IO_ERROR" : "BUSY", s->leave_descendant ? "BUSY" : "OK",
           query->receiver_site.pc, s->sky ? "fd.l" : "fd.m", query->detail.bytecode);
    /* Clear typed borrows before retiring their actual execution owners. */
    s->plant = (mc_world_plant_execution){0};
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

static int ps_values(ps_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_entity_tick_execution *execution = &s->scalar.cq.execution;
    VERIFY(bs_begin(&s->scalar) == 0);
    mc_world_runtime_execution_result_init(&s->owned);
    s->raw = MC_WORLD_STORAGE_LIMIT;
    if (strcmp(s->mode, "negative-y") == 0 || strcmp(s->mode, "defining-class") == 0) {
        const bool negative = strcmp(s->mode, "negative-y") == 0;
        mc_world_runtime_classes *defining = negative ? f->classes[0] : f->classes[1];
        mc_world_query_execution_init(&s->query, execution, defining, cq_code, ps_site, &s->owned);
        int32_t answer = 12345;
        VERIFY(mc_world_runtime_plant_light_in_execution(&s->query, world, negative ? 16 : 7,
                                                         negative ? -1 : 64, 8, &answer, &s->raw) &&
               answer == (negative ? 0 : 12) && s->raw == MC_WORLD_STORAGE_OK && s->query.entered &&
               s->query.completed && !s->query.native_stopped &&
               s->query.get_entered == !negative && s->query.get_returned == !negative &&
               s->query.detail.status == MC_WORLD_STORAGE_OK &&
               !mc_world_runtime_chunk_light_seen(f->classes[0]) &&
               mc_world_runtime_chunk_light_seen(defining) == !negative && s->callbacks == 0 &&
               f->providers[0].cache->count == 1 &&
               !mc_chunk_cache_contains(f->providers[0].cache, 1, 0));
        printf("PLANT_SURVIVAL_RESULT mode=%s raw=OK native=OK callbacks=0 query=fd.m "
               "value=%d reached-get=%d defining-Chunk.a=%d World-Chunk.a=0 replay=0\n",
               s->mode, answer, negative ? 0 : 1, negative ? 0 : 1);
    } else {
        const bool visible = strcmp(s->mode, "sky-visible") == 0;
        mc_world_plant_execution_init(&s->plant, execution, f->classes[0], cq_code, s->scalar.outer,
                                      ps_site, &s->owned);
        bool answer = !visible;
        VERIFY(mc_world_runtime_flower_survives_in_execution(&s->plant, world, s->flower, 7, 64, 8,
                                                             &answer, &s->raw) &&
               answer == visible && s->raw == MC_WORLD_STORAGE_OK && s->plant.completed &&
               !s->plant.native_stopped && s->plant.query.entered && s->plant.query.completed &&
               s->plant.query.get_entered && s->plant.query.get_returned &&
               s->plant.query.receiver_site.pc == 18 && s->plant.block.entered == visible &&
               s->plant.block.completed == visible &&
               !mc_world_runtime_chunk_light_seen(f->classes[0]) && s->callbacks == 0 &&
               f->providers[0].cache->count == 1 &&
               f->states[0]->data->heightmap[(7u << 4u) | 8u] == 127 &&
               f->states[0]->data->blocks[cell(7, 63, 8)] == 2);
        printf("PLANT_SURVIVAL_RESULT mode=%s raw=OK native=OK callbacks=0 site=wb.g18 "
               "query=fd.l value=%d substrate=%d asymmetric-height=1 Chunk.a=0 replay=0\n",
               s->mode, visible ? 1 : 0, visible ? 1 : 0);
    }
    VERIFY(s->owned.ownership_status == MC_WORLD_STORAGE_OK && !s->owned.selection_ready &&
           !cq_pending(&s->owned) && world->active_calls == 0);
    s->plant = (mc_world_plant_execution){0};
    s->query = (mc_world_query_execution){0};
    s->owned = (mc_world_runtime_execution_result){0};
    VERIFY(bs_end(&s->scalar, false) == 0);
    return 0;
}

static int ps_finish(ps_fixture *s) {
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
           mc_world_source_regions_flush(source->source_regions, &source->region) ==
               MC_WORLD_SOURCE_OK &&
           cq_files(&s->scalar.cq, false) == 0);
    if (f->classes[1] != NULL) {
        VERIFY(mc_world_runtime_classes_free(f->classes[1], &source->error) == MC_WORLD_STORAGE_OK);
        f->classes[1] = NULL;
    }
    VERIFY(ss_finish(source) == 0);
    for (size_t i = 0; i < COUNT(s->scalar.cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->scalar.cq.inputs[i]);
    return 0;
}

int main(int argc, char **argv) {
    static const char *const modes[] = {
        "descendant", "source-descendant", "source",         "sky-visible",           "sky-hidden",
        "negative-y", "defining-class",    "sky-descendant", "sky-source-descendant", "sky-source"};
    VERIFY(argc == 3);
    size_t index = 0;
    while (index < COUNT(modes) && strcmp(argv[1], modes[index]) != 0)
        ++index;
    VERIFY(index < COUNT(modes));
    printf("PLANT_SURVIVAL_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    ps_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->mode = argv[1];
    s->sky = strcmp(s->mode, "sky-descendant") == 0 ||
             strcmp(s->mode, "sky-source-descendant") == 0 || strcmp(s->mode, "sky-source") == 0;
    const char *failure_mode = s->sky ? s->mode + 4 : s->mode;
    s->source =
        strcmp(failure_mode, "source-descendant") == 0 || strcmp(failure_mode, "source") == 0;
    s->leave_descendant =
        strcmp(failure_mode, "descendant") == 0 || strcmp(failure_mode, "source-descendant") == 0;
    VERIFY(ss_setup(&s->scalar.cq.source, argv[2]) == 0 && cq_files(&s->scalar.cq, true) == 0 &&
           ps_open(s) == 0 && ((index < 3 || s->sky) ? ps_failure(s) : ps_values(s)) == 0 &&
           ps_finish(s) == 0);
    free(s);
    puts("PLANT_SURVIVAL_OPEN finite-owned-leaf/full-wake/Arrow/original");
    printf("PASS: %u actual owned plant survival checks;mode=%s\n", checks, argv[1]);
    return 0;
}
