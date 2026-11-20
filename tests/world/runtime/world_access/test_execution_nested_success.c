/* Reuse the actual Registry resources/constructors and native teardown helpers.
 * The renamed program is never run; no legacy Store/region writer is called. */
int ns_unused_tile_main(int, char **);
#define main ns_unused_tile_main
#include "../tile_entities/test_tile_entities.c"
#undef main
#undef CHECK

#include "mc/world/runtime/saving/saving.h"
#include "mc/world/runtime/source_construction.h"
#include "mc/world/storage/source_handler.h"
#include "mc/world/storage/source_region_output.h"
#include "platform/time/millis.h"
#include "world/runtime/source_terminal_internal.h"

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

/* NATIVE positive control only: real source load, then genuine cache hit.
 * No imported saving/tile main or source saving body is executed. */
#include "entity/tick/execution_internal.h"

typedef struct ns_fixture {
    source_save_fixture source;
    mc_entity_tick_execution execution;
    mc_world_runtime_acquisition_error acquisition;
} ns_fixture;

static int ns_gets(ns_fixture *s) {
    fixture *f = &s->source.game;
    mc_world_runtime *world = f->worlds[0];
    mc_entity_tick_execution *e = &s->execution;
    mc_entity_tick_execution_world_token previous = {0};
    mc_world_source_chunk_loader_state loaded = {0};
    mc_world_runtime_source_construction_state constructor;
    VERIFY(mc_world_runtime_source_construction_inspect(world, &constructor) &&
           constructor.loader != NULL);
    for (int pass = 0; pass < 2; ++pass) {
        const size_t bindings = world->persistent_bindings;
        mc_entity_tick_execution_world_token token;
        mc_entity_tick_execution_loader_attempt attempt;
        mc_entity_tick_execution_loader_selection selection;
        const mc_entity_tick_execution_site site = {"native.source.success", "get", "()V", pass};
        VERIFY(mc_entity_tick_execution_push_world(e, world, site, &token) == MC_WORLD_STORAGE_OK &&
               mc_entity_tick_execution_loader_begin(e, &token, &attempt) == MC_WORLD_STORAGE_OK &&
               world->persistent_bindings == bindings + 1u);
        if (pass != 0)
            VERIFY(token.slot == previous.slot && token.generation == previous.generation + 1u);
        mc_chunk_state *actual = NULL;
        const mc_chunk_acquire_status acquired =
            mc_world_runtime_get_detailed(world, 0, 0, &actual, &s->acquisition);
        const mc_world_storage_status finished = mc_entity_tick_execution_loader_finish(
            e, &attempt, acquired,
            acquired == MC_CHUNK_ACQUIRE_OK ? NULL : s->acquisition.source_chunk, &selection);
        VERIFY(finished == MC_WORLD_STORAGE_OK);
        if (acquired != MC_CHUNK_ACQUIRE_OK)
            fprintf(
                stderr,
                "real source success-control get failed pass%d acquire%d storage%d operation=%s\n",
                pass, acquired, s->acquisition.cause.status,
                s->acquisition.cause.operation == NULL ? "none" : s->acquisition.cause.operation);
        VERIFY(acquired == MC_CHUNK_ACQUIRE_OK && actual != NULL && actual->active);
        if (pass == 0) {
            f->states[0] = actual;
            f->chunks[0] = mc_world_store_chunk_object(f->stores[0], actual);
            VERIFY(f->chunks[0] != NULL && mc_world_store_owns_chunk(f->stores[0], actual) &&
                   mc_world_source_chunk_loader_inspect(constructor.loader, &loaded) &&
                   loaded.phase == MC_WORLD_SOURCE_READY && loaded.reader != NULL &&
                   loaded.document != NULL);
        } else {
            mc_world_source_chunk_loader_state after;
            VERIFY(actual == f->states[0] &&
                   mc_world_source_chunk_loader_inspect(constructor.loader, &after) &&
                   after.phase == loaded.phase && after.reader == loaded.reader &&
                   after.document == loaded.document && after.record == loaded.record &&
                   after.last_pc == loaded.last_pc);
        }
        VERIFY(f->providers[0].cache->count == 1 && f->providers[0].cache->insertion_count == 1 &&
               f->providers[0].cache->insertions[0] == actual &&
               e->phase == MC_ENTITY_TICK_EXECUTION_OPEN && e->loader_count == 0 &&
               e->receipt_count == 0 && e->first.status == MC_WORLD_STORAGE_OK &&
               e->capture.status == MC_WORLD_STORAGE_OK);
        mc_entity_tick_execution_loader_view view;
        VERIFY(mc_entity_tick_execution_loader_read(e, &selection, &view) &&
               view.status == acquired && view.failure == NULL);
        const size_t held_bindings = world->persistent_bindings;
        VERIFY(mc_entity_tick_execution_pop_world(e, &token, false) == MC_WORLD_STORAGE_OK &&
               world->persistent_bindings == held_bindings - 1u && !e->world_open &&
               e->world_count == 0 && e->lease_count == 0 &&
               !mc_entity_tick_execution_loader_read(e, &selection, &view));
        previous = token;
        printf("NESTED_SUCCESS_CASE id=%d\n", pass);
    }
    s->acquisition.source_chunk = NULL; /* Actual outer typed borrow, not producer state. */
    VERIFY(mc_entity_tick_execution_retire(e) == MC_WORLD_STORAGE_OK);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 2);
    ns_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(&s->source, argv[1]) == 0 && ss_open(&s->source) == 0);
    mc_entity_tick_execution_init(&s->execution, NULL);
    VERIFY(ns_gets(s) == 0);
    VERIFY(ss_stop(&s->source) == 0 && ss_finish(&s->source) == 0);
    free(s);
    printf("PASS: %u actual source-load/cache owner checks;2 groups\n", checks);
    return 0;
}
