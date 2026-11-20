/* Exact current source-save fixture helpers; only the unused program entry is renamed. */
/* Reuse the actual Registry resources/constructors and native teardown helpers.
 * The renamed program is never run; no legacy Store/region writer is called. */
#define main mc_source_save_unused_tile_program
#include "../tile_entities/test_tile_entities.c"
#undef main
#undef CHECK

#include "mc/world/runtime/saving/saving.h"
#include "mc/world/runtime/source_construction.h"
#include "mc/world/storage/source_handler.h"
#include "mc/world/storage/source_region_output.h"
#include "platform/time/millis.h"

#ifdef _MSC_VER
#include <share.h>
#endif

#define VERIFY(x)                                                                                  \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source saving line%d check%u: %s\n", __LINE__, checks, #x);           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct source_save_fixture {
    fixture game;
    mc_java_files *files;
    mc_java_logging *logging;
    mc_java_file *directory;
    mc_world_store_source_classes *handlers;
    mc_world_source_regions *source_regions;
    mc_world_source_error source;
    mc_world_source_region_error region;
    mc_world_storage_error error;
    mc_world_runtime_lifecycle_error lifecycle;
    mc_world_game_save_error save;
    const char *root;
    int64_t first, last;
} source_save_fixture;

static FILE *ss_fopen(const char *path, const char *mode) {
#ifdef _MSC_VER
    return _fsopen(path, mode, _SH_DENYNO);
#else
    return fopen(path, mode);
#endif
}

static mc_world_info_name *ss_name(const char *value) {
    const size_t n = strlen(value);
    uint16_t *units = malloc((n + 1) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < n; ++i)
        units[i] = (unsigned char)value[i];
    mc_world_info_name *out = NULL;
    const mc_world_info_status result =
        mc_world_info_name_copy((mc_nbt_string){units, n}, SIZE_MAX, &out);
    free(units);
    return result == MC_WORLD_INFO_OK ? out : NULL;
}

static int ss_setup(source_save_fixture *s, const char *root) {
    fixture *f = &s->game;
    s->root = f->root = root;
    VERIFY(mc_math_trig_init(&trig) && mc_materials_create(&f->materials));
    VERIFY(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
           MC_JAVA_RANDOM_PROFILE_OK);
    f->name = ss_name("SourceSave");
    VERIFY(f->name != NULL);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options registry = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    VERIFY(mc_registry_classes_allocate(&registry, &f->registry[0], NULL) == MC_REGISTRY_OK &&
           mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                           NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
               MC_REGISTRY_OK);
    VERIFY(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
               MC_ENTITY_OK &&
           mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
               MC_ENTITY_OK &&
           mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
           r->opens == 3 && r->closes == 1 && !r->bad);
    const mc_block *chest = NULL;
    VERIFY(mc_registry_block_static_get(f->registry[0], "av", &chest, NULL) == MC_REGISTRY_OK &&
           chest != NULL);
    f->chest_id[0] = mc_block_fields_get(chest)->bn;
    VERIFY(mc_java_files_create(NULL, &s->files, &s->source.file) == MC_JAVA_FILE_OK &&
           mc_java_logging_create(NULL, &s->logging, &s->source.logging) == MC_JAVA_LOGGING_OK &&
           mc_world_source_regions_create(s->files, NULL, &s->source_regions, &s->region) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_classes_create(s->files, s->logging, NULL, &s->handlers,
                                                &s->source) == MC_WORLD_SOURCE_OK &&
           mc_world_store_source_classes_bind_regions(s->handlers, s->source_regions, &s->source) ==
               MC_WORLD_SOURCE_OK);
    mc_world_info_name *path = ss_name(root);
    VERIFY(path != NULL &&
           mc_java_file_allocate(s->files, &s->directory, &s->source.file) == MC_JAVA_FILE_OK &&
           mc_java_file_construct_string(s->directory, path, &s->source.file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(path);
    return 0;
}

static int ss_open(source_save_fixture *s) {
    fixture *f = &s->game;
    mc_world_info_name *folder = ss_name("world");
    VERIFY(folder != NULL &&
           mc_world_store_source_allocate(s->handlers, &f->stores[0], &s->source) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_construct(f->stores[0], s->directory, folder, false, &s->source) ==
               MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    VERIFY(mc_world_store_is_source(f->stores[0]) && mc_world_store_root(f->stores[0]) == NULL);
    VERIFY(create_world(f, 0));
    mc_world_runtime_source_construction_state constructor;
    VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &constructor) &&
           constructor.metadata_called && constructor.metadata_returned &&
           constructor.loader != NULL);
    mc_chunk_state *state = NULL;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get(f->worlds[0], 0, 0, &state);
    if (acquired != MC_CHUNK_ACQUIRE_OK) {
        const mc_world_storage_error *cause = mc_world_runtime_last_error(f->worlds[0]);
        fprintf(stderr, "actual source acquire status%d storage%d operation=%s\n", (int)acquired,
                (int)cause->status, cause->operation == NULL ? "none" : cause->operation);
    }
    VERIFY(acquired == MC_CHUNK_ACQUIRE_OK && state != NULL && state->active);
    f->states[0] = state;
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], state);
    VERIFY(f->chunks[0] != NULL && mc_world_store_owns_chunk(f->stores[0], state) &&
           f->providers[0].cache->insertion_count == 1 &&
           f->providers[0].cache->insertions[0] == state);
    return 0;
}

static int ss_copy(source_save_fixture *s, const char *label, const char *leaf,
                   const char *suffix) {
    char input[8192], output[8192];
    const int a = snprintf(input, sizeof(input), "%s/world/%s", s->root, leaf);
    const int b = snprintf(output, sizeof(output), "%s/snapshots/%s-%s", s->root, label, suffix);
    VERIFY(a > 0 && (size_t)a < sizeof(input) && b > 0 && (size_t)b < sizeof(output));
    FILE *in = ss_fopen(input, "rb"), *out = ss_fopen(output, "wb");
    VERIFY(in != NULL && out != NULL);
    size_t total = 0, n;
    uint8_t bytes[4096];
    while ((n = fread(bytes, 1, sizeof(bytes), in)) != 0) {
        VERIFY(total <= 1048576 && n <= 1048576 - total && fwrite(bytes, 1, n, out) == n);
        total += n;
    }
    VERIFY(!ferror(in) && fclose(in) == 0 && fclose(out) == 0);
    return 0;
}

static int ss_event(source_save_fixture *s, const char *phase) {
    fixture *f = &s->game;
    mc_world_runtime_source_construction_state constructor;
    mc_world_source_chunk_loader_state ld;
    mc_world_source_region_output_state out = {0};
    mc_world_info_fields info;
    VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &constructor) &&
           mc_world_source_chunk_loader_inspect(constructor.loader, &ld) &&
           mc_world_info_fields_get(f->worlds[0]->fields.x, &info) == MC_WORLD_INFO_OK);
    const bool has_output = ld.save.output != NULL;
    VERIFY(!has_output || mc_world_source_region_output_inspect(ld.save.output, &out));
    VERIFY(ss_copy(s, phase, "region/r.0.0.mcr", "region.mcr") == 0 &&
           ss_copy(s, phase, "level.dat", "level.dat") == 0);
    printf(
        "{\"event\":\"state\",\"phase\":\"%s\",\"size\":%lld,\"time\":%lld,\"q\":%d,\"dirty\":%d,"
        "\"last_save\":%lld,\"first\":%lld,\"last\":%lld,\"root\":%d,\"level\":%d,\"attached\":%d,"
        "\"prefix\":%d,"
        "\"entities\":%d,\"tiles\":%d,\"written\":%d,\"closed\":%d,\"returned\":%d,\"failed\":%d,"
        "\"save_pc\":%d,"
        "\"serializer_pc\":%d,"
        "\"growth\":%d,\"has_output\":%d,\"output_closed\":%d,\"commit\":%d,\"commit_returned\":%d,"
        "\"source_closes\":%zu,"
        "\"raw_bytes\":%llu,\"compressed\":%zu,\"error_status\":%d,\"error_ld_pc\":%u,\"error_"
        "serializer_pc\":%u,"
        "\"native_policy\":%d,\"source_exception\":%d,\"diagnostic\":%d,"
        "\"cause_pc\":%d,\"source_frames\":%zu,\"entity_wrapper\":%u,\"entity_name\":%u,"
        "\"entity_base\":%u,\"entity_item\":%u}\n",
        phase, (long long)info.value.size_on_disk, (long long)info.value.time,
        f->states[0]->has_entities, f->states[0]->dirty, (long long)f->states[0]->last_save_time,
        (long long)s->first, (long long)s->last, ld.save.root_constructed,
        ld.save.level_constructed, ld.save.level_attached, ld.save.prefix_returned,
        ld.save.entities_attached, ld.save.tiles_attached, ld.save.nbt_written, ld.save.closed,
        ld.save.source_returned, ld.save.failed, ld.save.last_pc, ld.save.serializer_pc,
        ld.save.growth, has_output, out.closed, out.commit_entered, out.commit_returned,
        out.source_close_calls, (unsigned long long)out.raw_bytes, out.compressed_length,
        (int)s->save.status, s->save.region_bytecode, s->save.serializer_bytecode,
        s->save.source_chunk.source.native_policy, s->save.source_chunk.source.source_exception,
        s->save.source_chunk.source.diagnostic_frontier, s->save.source_chunk.source.cause_site.pc,
        s->save.source_chunk.source.frame_count, s->save.entity.wrapper_bytecode,
        s->save.entity.name_bytecode, s->save.entity.base_bytecode, s->save.entity.item_bytecode);
    VERIFY(fflush(stdout) == 0);
    return 0;
}

static int ss_stop(source_save_fixture *s) {
    fixture *f = &s->game;
    VERIFY(mc_world_runtime_retire_entity_containers(f->worlds[0], &s->error) ==
           MC_WORLD_STORAGE_OK);
    f->terminal[0] = true;
    VERIFY(retire_descriptors(f, 0));
    VERIFY(mc_world_runtime_retire_world(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    VERIFY(mc_world_store_retire_chunk(f->stores[0], f->states[0], &s->error) ==
           MC_WORLD_STORAGE_OK);
    f->states[0] = NULL;
    f->chunks[0] = NULL;
    VERIFY(mc_world_runtime_free(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    f->worlds[0] = NULL;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    VERIFY(mc_world_store_source_release(&f->stores[0], &s->source) == MC_WORLD_SOURCE_OK &&
           f->stores[0] == NULL);
    VERIFY(mc_world_source_regions_flush(s->source_regions, &s->region) == MC_WORLD_SOURCE_OK);
    f->bound[0] = f->terminal[0] = f->descriptors_retired[0] = false;
    return 0;
}

static int ss_finish(source_save_fixture *s) {
    fixture *f = &s->game;
    VERIFY(mc_world_runtime_classes_free(f->classes[0], &s->error) == MC_WORLD_STORAGE_OK &&
           mc_registry_classes_free(f->registry[0], NULL) == MC_REGISTRY_OK &&
           mc_entity_classes_free(f->entity_classes[0]) == MC_ENTITY_OK);
    VERIFY(mc_world_store_source_classes_retire(&s->handlers, &s->source) == MC_WORLD_SOURCE_OK &&
           mc_world_source_regions_retire(&s->source_regions, &s->region) == MC_WORLD_SOURCE_OK &&
           mc_java_logging_retire(&s->logging, &s->source.logging) == MC_JAVA_LOGGING_OK &&
           mc_java_file_release(&s->directory, &s->source.file) == MC_JAVA_FILE_OK &&
           mc_java_files_release(&s->files, &s->source.file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(f->name);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    return 0;
}

static int ss_success(source_save_fixture *s, bool objects) {
    fixture *f = &s->game;
    uint8_t expected_blocks[MC_CHUNK_BLOCKS];
    uint32_t random = UINT32_C(0x87654321);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        f->chunks[0]->data->blocks[i] = (uint8_t)(random & 1u);
    }
    if (objects)
        f->chunks[0]->data->blocks[cell(8, 64, 8)] = (uint8_t)f->chest_id[0];
    memcpy(expected_blocks, f->chunks[0]->data->blocks, sizeof(expected_blocks));
    f->states[0]->dirty = true;
    VERIFY(mc_world_info_time_set(f->worlds[0]->fields.x, 20000) == MC_WORLD_INFO_OK);
    int64_t before, after;
    VERIFY(mc_world_info_size_get(f->worlds[0]->fields.x, &before) == MC_WORLD_INFO_OK);
    VERIFY(mc_platform_current_time_millis(&s->first) == MC_PLATFORM_MILLIS_OK);
    const mc_chunk_lifecycle_status first_save =
        mc_world_runtime_save_detailed(f->worlds[0], true, &s->lifecycle);
    s->save = s->lifecycle.save;
    VERIFY(mc_platform_current_time_millis(&s->last) == MC_PLATFORM_MILLIS_OK);
    if (first_save != MC_CHUNK_LIFECYCLE_OK) {
        (void)ss_event(s, "first-save-failed");
        fprintf(stderr, "actual source first World save status%d ld%u serializer%u operation=%s\n",
                (int)first_save, s->save.region_bytecode, s->save.serializer_bytecode,
                s->lifecycle.cause.operation == NULL ? "none" : s->lifecycle.cause.operation);
    }
    VERIFY(first_save == MC_CHUNK_LIFECYCLE_OK);
    VERIFY(mc_world_info_size_get(f->worlds[0]->fields.x, &after) == MC_WORLD_INFO_OK &&
           after > before && !f->states[0]->dirty && f->states[0]->last_save_time == 20000 &&
           ss_event(s, "saved-first") == 0);
    const int64_t retained = after;
    f->states[0]->dirty = true;
    VERIFY(mc_platform_current_time_millis(&s->first) == MC_PLATFORM_MILLIS_OK);
    const mc_chunk_lifecycle_status second_save =
        mc_world_runtime_save_detailed(f->worlds[0], true, &s->lifecycle);
    s->save = s->lifecycle.save;
    VERIFY(mc_platform_current_time_millis(&s->last) == MC_PLATFORM_MILLIS_OK);
    if (second_save != MC_CHUNK_LIFECYCLE_OK) {
        (void)ss_event(s, "second-save-failed");
        fprintf(stderr, "actual source second World save status%d ld%u serializer%u operation=%s\n",
                (int)second_save, s->save.region_bytecode, s->save.serializer_bytecode,
                s->lifecycle.cause.operation == NULL ? "none" : s->lifecycle.cause.operation);
    }
    VERIFY(second_save == MC_CHUNK_LIFECYCLE_OK);
    VERIFY(mc_world_info_size_get(f->worlds[0]->fields.x, &after) == MC_WORLD_INFO_OK &&
           after == retained && ss_event(s, "saved-second") == 0);
    VERIFY(ss_stop(s) == 0 && ss_open(s) == 0);
    VERIFY(mc_world_info_size_get(f->worlds[0]->fields.x, &after) == MC_WORLD_INFO_OK &&
           after == retained && ss_event(s, "reloaded") == 0);
    VERIFY(memcmp(f->chunks[0]->data->blocks, expected_blocks, sizeof(expected_blocks)) == 0);
    size_t tiles = 0, entities_count = 0;
    VERIFY(mc_world_chunk_tile_entity_count(f->chunks[0]->l, &tiles) == MC_WORLD_STORAGE_OK &&
           tiles == (objects ? 1u : 0u));
    for (size_t slice = 0; slice < CHUNK_ENTITY_SLICES; ++slice)
        entities_count += f->chunks[0]->m->items[slice]->count;
    VERIFY(entities_count == (objects ? 1u : 0u));
    if (objects) {
        mc_tile_entity *chest = NULL;
        mc_item_stack *stack = NULL;
        mc_item_stack_fields fields;
        VERIFY(mc_live_tile_entities_get(f->worlds[0], 8, 64, 8, &chest, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               chest != NULL && mc_tile_entity_chest_get(chest, 0, &stack) == MC_TILE_ENTITY_OK &&
               stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
               fields.id == 262 && fields.count == 7 && fields.damage == 3);
        mc_item_stack_release(stack);
        mc_tile_entity_release(chest);
    }
    return ss_stop(s);
}

static int ss_failure(source_save_fixture *s, const char *mode) {
    fixture *f = &s->game;
    const bool session = strcmp(mode, "session-mismatch") == 0;
    const bool null_stack = strcmp(mode, "null-stack") == 0;
    const bool null_m = strcmp(mode, "null-m") == 0, null_l = strcmp(mode, "null-l") == 0;
    uint8_t token[8];
    char path[8192];
    FILE *file = NULL;
    mc_item_stack *held = NULL;
    mc_entity *item = NULL;
    mc_world_chunk_list_array *saved_m = f->chunks[0]->m;
    mc_world_chunk_tile_entity_map *saved_l = f->chunks[0]->l;
    if (session) {
        const int n = snprintf(path, sizeof(path), "%s/world/session.lock", s->root);
        VERIFY(n > 0 && (size_t)n < sizeof(path));
        file = ss_fopen(path, "rb");
        VERIFY(file != NULL && fread(token, 1, 8, file) == 8 && fclose(file) == 0);
        const uint8_t mismatch[8] = {0};
        file = ss_fopen(path, "wb");
        VERIFY(file != NULL && fwrite(mismatch, 1, 8, file) == 8 && fclose(file) == 0);
    } else if (null_stack) {
        for (size_t slice = 0; slice < CHUNK_ENTITY_SLICES; ++slice)
            if (f->chunks[0]->m->items[slice]->count != 0)
                item = f->chunks[0]->m->items[slice]->items[0];
        VERIFY(item != NULL && item->stack != NULL);
        held = item->stack;
        item->stack = NULL;
    } else if (null_m) {
        VERIFY(saved_m != NULL);
        f->chunks[0]->m = NULL;
    } else if (null_l) {
        VERIFY(saved_l != NULL);
        f->chunks[0]->l = NULL;
    }
    uint32_t growth = UINT32_MAX;
    const mc_world_storage_status status = mc_world_runtime_write_game_chunk(
        f->worlds[0], f->states[0], 0x13572468, &growth, &s->save);
    if (null_stack)
        item->stack = held;
    /* Restore the same actual source projections before any check or native
     * cleanup. Embedded ownership/capacity/membership ledgers were not edited. */
    if (null_m)
        f->chunks[0]->m = saved_m;
    if (null_l)
        f->chunks[0]->l = saved_l;
    if (session) {
        file = ss_fopen(path, "wb");
        VERIFY(file != NULL && fwrite(token, 1, 8, file) == 8 && fclose(file) == 0);
    }
    VERIFY(status != MC_WORLD_STORAGE_OK && growth == UINT32_MAX && ss_event(s, "failed") == 0);
    if (session)
        VERIFY(s->save.region_bytecode == 1);
    else if (null_m || null_l) {
        const int pc = null_m ? 125 : 224;
        const mc_world_source_error *source = &s->save.source_chunk.source;
        VERIFY(!f->states[0]->has_entities && s->save.region_bytecode == 51 &&
               s->save.serializer_bytecode == (unsigned)pc &&
               s->save.entity.status == MC_ENTITY_NBT_OK && s->save.entity.wrapper_bytecode == 0 &&
               s->save.entity.name_bytecode == 0 && s->save.entity.base_bytecode == 0 &&
               s->save.entity.item_bytecode == 0 &&
               source->status == MC_WORLD_SOURCE_UNIMPLEMENTED &&
               source->cause_status == MC_WORLD_SOURCE_SOURCE_EXCEPTION &&
               source->source_exception && !source->native_policy && source->caught_exception &&
               source->diagnostic_frontier && source->exception_class != NULL &&
               strcmp(source->exception_class, "java/lang/NullPointerException") == 0 &&
               source->frame_count == 2 && source->frames[0].class_name != NULL &&
               strcmp(source->frames[0].class_name, "ld") == 0 && source->frames[0].pc == 102 &&
               source->frames[1].class_name != NULL &&
               strcmp(source->frames[1].class_name, "to") == 0 && source->frames[1].pc == pc &&
               source->cause_site.class_name != NULL &&
               strcmp(source->cause_site.class_name, "to") == 0 && source->cause_site.pc == pc);
        mc_world_runtime_source_construction_state constructor;
        mc_world_source_chunk_loader_state ld;
        mc_world_source_region_output_state output;
        VERIFY(mc_world_runtime_source_construction_inspect(f->worlds[0], &constructor) &&
               mc_world_source_chunk_loader_inspect(constructor.loader, &ld) && ld.save.failed &&
               ld.save.prefix_returned && ld.save.entities_attached == null_l &&
               !ld.save.tiles_attached && !ld.save.nbt_written && !ld.save.closed &&
               !ld.save.source_returned && ld.save.output != NULL &&
               mc_world_source_region_output_inspect(ld.save.output, &output) && !output.closed &&
               !output.finished && !output.commit_entered && output.source_close_calls == 0 &&
               output.raw_bytes == 0);
    } else if (null_stack)
        VERIFY(
            f->states[0]->has_entities && s->save.entity.status == MC_ENTITY_NBT_NULL_REFERENCE &&
            s->save.serializer_bytecode == 183 && s->save.source_chunk.source.diagnostic_frontier &&
            s->save.source_chunk.source.frames[0].pc == 102);
    else
        VERIFY(!f->states[0]->has_entities && s->save.source_chunk.source.native_policy);
    return ss_stop(s);
}

int mn_unused_source_save_program(int argc, char **argv) {
    VERIFY(argc == 3);
    source_save_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(s, argv[1]) == 0 && ss_open(s) == 0);
    VERIFY(ss_event(s, "loaded") == 0);
    const bool empty = strcmp(argv[2], "empty") == 0, objects = strcmp(argv[2], "item-chest") == 0;
    VERIFY(empty || objects || strcmp(argv[2], "session-mismatch") == 0 ||
           strcmp(argv[2], "opaque") == 0 || strcmp(argv[2], "null-stack") == 0 ||
           strcmp(argv[2], "null-m") == 0 || strcmp(argv[2], "null-l") == 0);
    VERIFY((empty || objects ? ss_success(s, objects) : ss_failure(s, argv[2])) == 0 &&
           ss_finish(s) == 0);
    printf("{\"event\":\"complete\",\"mode\":\"%s\",\"checks\":%u,\"cleanup\":1,\"reload\":%d}\n",
           argv[2], checks, empty || objects);
    free(s);
    return 0;
}
