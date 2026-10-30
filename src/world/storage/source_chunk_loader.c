#include "mc/world/storage/source_chunk_loader.h"
#include "../../block/registry/runtime_internal.h"
#include "../runtime/source_terminal_internal.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/runtime.h"
#include "source_game_stage.h"
#include <stdlib.h>
#include <string.h>

struct mc_world_source_chunk_loader {
    mc_world_source_chunk_loader_state state;
    mc_world_source_chunk_error failure, cleanup, work;
    mc_world_source_region_record *record;
    mc_gzip_input *compression;
    mc_nbt_stream *reader;
    mc_nbt_document *document, *parsed_document;
    mc_world_info_name *dimension_name;
};

static mc_world_source_status result(mc_world_source_error *e, mc_world_source_status status,
                                     const char *operation, const char *cl, int pc) {
    if (e != NULL) {
        *e = (mc_world_source_error){.status = status,
                                     .cause_status = status,
                                     .operation = operation,
                                     .native_policy = status != MC_WORLD_SOURCE_OK};
        if (cl != NULL) {
            e->frames[0] = (mc_world_source_site){cl, "<init>", "(Ljava/io/File;)V", pc};
            if (strcmp(cl, "ld") == 0 && pc != 6) {
                e->frames[0].method = "a";
                e->frames[0].descriptor = "(Lfd;II)Llm;";
            } else if (strcmp(cl, "mx") == 0) {
                e->frames[0].method = "a";
                e->frames[0].descriptor = "(Lxa;)Lbf;";
            } else if (strcmp(cl, "vi") == 0) {
                e->frames[0].method = "<clinit>";
                e->frames[0].descriptor = "()V";
            }
            e->frame_count = 1;
        }
    }
    return status;
}

static mc_world_source_status file_error(mc_world_source_error *e, mc_java_file_status status,
                                         mc_java_file_error child, const char *cl, int pc) {
    result(e,
           status == MC_JAVA_FILE_BUSY                ? MC_WORLD_SOURCE_BUSY
           : status == MC_JAVA_FILE_LIMIT             ? MC_WORLD_SOURCE_LIMIT
           : status == MC_JAVA_FILE_ALLOCATION_FAILED ? MC_WORLD_SOURCE_ALLOCATION
                                                      : MC_WORLD_SOURCE_FILE,
           child.operation, cl, pc);
    e->file = child;
    return e->status;
}

static mc_world_storage_status
storage_result(mc_world_storage_error *e, mc_world_storage_status status, const char *operation) {
    if (e != NULL)
        *e = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status source_status(mc_world_source_status status) {
    switch (status) {
        case MC_WORLD_SOURCE_OK:
            return MC_WORLD_STORAGE_OK;
        case MC_WORLD_SOURCE_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        case MC_WORLD_SOURCE_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        case MC_WORLD_SOURCE_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_WORLD_SOURCE_ALLOCATION:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_WORLD_SOURCE_FILE:
            return MC_WORLD_STORAGE_IO_ERROR;
        case MC_WORLD_SOURCE_SOURCE_EXCEPTION:
            return MC_WORLD_STORAGE_INVALID_DATA;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

mc_world_source_status mc_world_store_source_loader(mc_world_store *store,
                                                    mc_world_provider *provider,
                                                    mc_world_source_chunk_loader **out,
                                                    mc_world_source_error *e) {
    mc_world_source_state source;
    mc_world_source_class_state classes;
    if (out == NULL || !mc_world_store_is_source(store))
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "mx.a actual source Store/output", NULL, -1);
    if (!mc_world_store_source_inspect(store, &source))
        return result(e, MC_WORLD_SOURCE_BUSY, "mx.a active Store", NULL, -1);
    if (source.phase != MC_WORLD_SOURCE_READY ||
        !mc_world_store_source_classes_inspect(source.classes, &classes))
        return result(e, MC_WORLD_SOURCE_BUSY, "mx.a Store/class phase", NULL, -1);
    mc_world_provider_state p = {0};
    if (provider != NULL && mc_world_provider_state_get(provider, &p) != MC_WORLD_PROVIDER_OK)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "mx.a actual dimension", NULL, -1);
    mc_world_source_regions *regions = mc_world_store_source_regions(store);
    if (regions == NULL)
        return result(e, MC_WORLD_SOURCE_STATE, "mx.a shared rj namespace binding", NULL, -1);
    mc_world_source_status status = mc_world_source_chunk_loader_allocate(store, regions, out, e);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    status = mc_world_source_chunk_loader_construct(
        *out, source.source_b, provider != NULL && p.kind == MC_WORLD_PROVIDER_NETHER, e);
    if (status != MC_WORLD_SOURCE_OK && e != NULL) {
        size_t n = e->frame_count < 6 ? e->frame_count : 5;
        memmove(e->frames + 1, e->frames, n * sizeof(*e->frames));
        e->frames[0] = (mc_world_source_site){
            "mx", "a", "(Lxa;)Lbf;",
            provider != NULL && p.kind == MC_WORLD_PROVIDER_NETHER ? 33 : 42};
        e->frame_count = n + 1;
    }
    return status;
}

mc_world_source_status mc_world_source_chunk_loader_allocate(mc_world_store *store,
                                                             mc_world_source_regions *regions,
                                                             mc_world_source_chunk_loader **out,
                                                             mc_world_source_error *e) {
    if (store == NULL || regions == NULL || out == NULL ||
        mc_world_store_source_regions(store) != regions)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "ld native Store/rj/output", NULL, -1);
    mc_world_source_chunk_loader *l = calloc(1, sizeof(*l));
    if (l == NULL)
        return result(e, MC_WORLD_SOURCE_ALLOCATION, "ld native owner", NULL, -1);
    mc_world_source_status status = mc_world_store_source_retain(store, e);
    if (status != MC_WORLD_SOURCE_OK) {
        free(l);
        return status;
    }
    l->state.store = store;
    l->state.holds = 1;
    l->state.last_pc = -1;
    mc_world_source_region_error re;
    status = mc_world_source_regions_retain(regions, &re);
    if (status != MC_WORLD_SOURCE_OK) {
        (void)mc_world_store_source_release(&l->state.store, NULL);
        free(l);
        if (e != NULL)
            *e = re.source;
        return status;
    }
    l->state.regions = regions;
    *out = l;
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, -1);
}

mc_world_source_status mc_world_source_chunk_loader_construct(mc_world_source_chunk_loader *l,
                                                              mc_java_file *directory, bool nether,
                                                              mc_world_source_error *e) {
    if (l == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "ld constructor receiver", NULL, -1);
    if (l->state.active || l->state.phase != MC_WORLD_SOURCE_RAW)
        return result(e, l->state.active ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_STATE,
                      "ld constructor phase", NULL, -1);
    l->state.active = true;
    l->state.phase = MC_WORLD_SOURCE_CONSTRUCTING;
    mc_world_source_error *work = &l->work.source;
    result(work, MC_WORLD_SOURCE_OK, NULL, NULL, -1);
    mc_java_file_error fe;
    mc_java_file_status fs;
    if (nether) {
        fs = mc_java_file_allocate(mc_world_store_source_files(l->state.store),
                                   &l->state.pending_file, &fe);
        if (fs != MC_JAVA_FILE_OK) {
            file_error(work, fs, fe, "mx", 12);
            goto failed;
        }
        const uint16_t units[] = {'D', 'I', 'M', '-', '1'};
        mc_world_info_status ns =
            mc_world_info_name_copy((mc_nbt_string){units, 5}, 1024, &l->dimension_name);
        if (ns != MC_WORLD_INFO_OK) {
            result(work,
                   ns == MC_WORLD_INFO_LIMIT ? MC_WORLD_SOURCE_LIMIT : MC_WORLD_SOURCE_ALLOCATION,
                   "mx DIM-1 literal", "mx", 17);
            goto failed;
        }
        fs = mc_java_file_construct_child(l->state.pending_file, directory, l->dimension_name, &fe);
        if (fs != MC_JAVA_FILE_OK) {
            file_error(work, fs, fe, "mx", 19);
            goto failed;
        }
        bool ignored;
        fs = mc_java_file_mkdirs(l->state.pending_file, &ignored, &fe);
        if (fs != MC_JAVA_FILE_OK) {
            file_error(work, fs, fe, "mx", 24);
            goto failed;
        }
        l->state.source_a = l->state.pending_file;
        l->state.pending_file = NULL;
    } else if (directory != NULL) {
        fs = mc_java_file_retain(directory, &fe);
        if (fs != MC_JAVA_FILE_OK) {
            file_error(work, fs, fe, "ld", 6);
            goto failed;
        }
        l->state.source_a = directory;
    }
    /* Actual ld6 permits a null File; rj's later File(parent,child) owns its
     * semantics. The real mx factory normally provides its nonnull b File. */
    l->state.last_pc = 6;
    l->state.phase = MC_WORLD_SOURCE_READY;
    l->state.active = false;
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, -1);
failed:
    l->state.phase = MC_WORLD_SOURCE_FAILED;
    l->state.active = false;
    l->failure = l->work;
    if (e != NULL)
        *e = *work;
    return work->status;
}

mc_world_source_status mc_world_source_chunk_loader_retain(mc_world_source_chunk_loader *l,
                                                           mc_world_source_error *e) {
    if (l == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "ld retain", NULL, -1);
    if (l->state.active || l->state.retiring)
        return result(e, MC_WORLD_SOURCE_BUSY, "ld active/retiring hold", NULL, -1);
    if (l->state.holds == SIZE_MAX)
        return result(e, MC_WORLD_SOURCE_LIMIT, "ld hold count", NULL, -1);
    ++l->state.holds;
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, -1);
}

static mc_world_source_status clear_input(mc_world_source_chunk_loader *l,
                                          mc_world_source_chunk_error *e) {
    mc_nbt_status ns = mc_nbt_stream_release(&l->reader, &e->input);
    if (ns != MC_NBT_OK) {
        result(&e->source, MC_WORLD_SOURCE_BUSY, "ld native DataInput retirement", NULL, -1);
        e->source.input = e->input;
        return e->source.status;
    }
    mc_gzip_status gs = mc_gzip_input_release(&l->compression, &e->compression);
    if (gs != MC_GZIP_OK) {
        result(&e->source, gs == MC_GZIP_BUSY ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_STATE,
               "ld native compression retirement", NULL, -1);
        e->source.gzip = e->compression;
        return e->source.status;
    }
    mc_nbt_document_free(l->document);
    l->document = NULL;
    mc_nbt_document_free(l->parsed_document);
    l->parsed_document = NULL;
    mc_world_source_region_record_free(l->record);
    l->record = NULL;
    return MC_WORLD_SOURCE_OK;
}

mc_world_source_status mc_world_source_chunk_loader_release(mc_world_source_chunk_loader **slot,
                                                            mc_world_source_error *e) {
    if (slot == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "ld release slot", NULL, -1);
    mc_world_source_chunk_loader *l = *slot;
    if (l == NULL)
        return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, -1);
    if (l->state.active)
        return result(e, MC_WORLD_SOURCE_BUSY, "ld active retirement", NULL, -1);
    if (l->state.holds > 1) {
        --l->state.holds;
        *slot = NULL;
        return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, -1);
    }
    l->state.active = true;
    l->state.retiring = true;
    l->state.phase = MC_WORLD_SOURCE_RETIRING;
    mc_world_source_status status = clear_input(l, &l->cleanup);
    mc_java_file_error fe;
    if (status == MC_WORLD_SOURCE_OK) {
        mc_java_file_status fs = mc_java_file_release(&l->state.pending_file, &fe);
        if (fs == MC_JAVA_FILE_OK)
            fs = mc_java_file_release(&l->state.source_a, &fe);
        if (fs != MC_JAVA_FILE_OK)
            status = file_error(&l->cleanup.source, fs, fe, NULL, -1);
    }
    if (status == MC_WORLD_SOURCE_OK) {
        mc_world_source_region_error re;
        status = mc_world_source_regions_release(&l->state.regions, &re);
        if (status != MC_WORLD_SOURCE_OK)
            l->cleanup.source = re.source;
    }
    if (status == MC_WORLD_SOURCE_OK)
        status = mc_world_store_source_release(&l->state.store, &l->cleanup.source);
    l->state.active = false;
    if (status != MC_WORLD_SOURCE_OK) {
        if (e != NULL)
            *e = l->cleanup.source;
        return status;
    }
    mc_world_info_name_release(l->dimension_name);
    free(l);
    *slot = NULL;
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, -1);
}

mc_world_source_status mc_world_source_chunk_loader_retire(mc_world_source_chunk_loader **slot,
                                                           mc_world_source_error *e) {
    if (slot == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "ld strict slot", NULL, -1);
    if (*slot != NULL && ((*slot)->state.holds != 1 || (*slot)->state.active))
        return result(e, MC_WORLD_SOURCE_BUSY, "ld live occurrences", NULL, -1);
    return mc_world_source_chunk_loader_release(slot, e);
}

bool mc_world_source_chunk_loader_inspect(const mc_world_source_chunk_loader *l,
                                          mc_world_source_chunk_loader_state *out) {
    if (l == NULL || out == NULL || l->state.active)
        return false;
    *out = l->state;
    out->record = l->record;
    out->reader = l->reader;
    out->document = l->document == NULL ? l->parsed_document : l->document;
    out->failure = &l->failure;
    out->cleanup = &l->cleanup;
    return true;
}

static mc_world_storage_status terminal_loader_file(const mc_java_file *file,
                                                    mc_world_storage_error *error) {
    if (file == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_java_file_state state;
    if (mc_java_file_read_state(file, &state, NULL) != MC_JAVA_FILE_OK)
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "ld terminal actual File owner");
    if (state.active || state.phase == MC_JAVA_FILE_CONSTRUCTING ||
        state.phase == MC_JAVA_FILE_RETIRING)
        return storage_result(error, MC_WORLD_STORAGE_BUSY, "ld terminal active File");
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status terminal_loader_inflater(const mc_raw_inflater *inflater,
                                                        mc_world_storage_error *error) {
    if (inflater == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_raw_inflater_state state;
    if (mc_raw_inflater_state_read(inflater, &state, NULL) != MC_RAW_INFLATER_OK)
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "ld terminal actual Inflater owner");
    if (state.active || state.phase == MC_RAW_INFLATER_RETIRING)
        return storage_result(error, MC_WORLD_STORAGE_BUSY, "ld terminal active Inflater");
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_world_source_chunk_loader_terminal_inspect(const mc_world_source_chunk_loader *l,
                                              mc_world_terminal_loader_view *out,
                                              mc_world_storage_error *error) {
    if (l == NULL || out == NULL)
        return storage_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "ld terminal owner/output");
    if (l->state.active || l->state.retiring)
        return storage_result(error, MC_WORLD_STORAGE_BUSY, "ld terminal active/retiring owner");
    if (l->state.phase != MC_WORLD_SOURCE_READY || l->state.holds == 0 || l->state.store == NULL ||
        l->state.regions == NULL)
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "ld terminal READY canonical owner scope");
    mc_world_source_state store;
    if (!mc_world_store_source_inspect(l->state.store, &store) || store.active ||
        store.phase == MC_WORLD_SOURCE_CONSTRUCTING || store.phase == MC_WORLD_SOURCE_RETIRING)
        return storage_result(error, MC_WORLD_STORAGE_BUSY, "ld terminal active Store");
    if (mc_world_store_source_regions(l->state.store) != l->state.regions)
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "ld terminal actual Store/regions association");
    mc_world_source_regions_state regions;
    if (!mc_world_source_regions_inspect(l->state.regions, &regions) || regions.active ||
        regions.retiring)
        return storage_result(error, MC_WORLD_STORAGE_BUSY, "ld terminal active regions owner");
    mc_world_storage_status status = terminal_loader_file(l->state.source_a, error);
    if (status == MC_WORLD_STORAGE_OK)
        status = terminal_loader_file(l->state.pending_file, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (l->reader != NULL) {
        mc_nbt_stream_state reader;
        if (mc_nbt_stream_state_read(l->reader, &reader, NULL) != MC_NBT_OK)
            return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "ld terminal actual DataInput owner");
        if (reader.active || reader.phase == MC_NBT_STREAM_CONSTRUCTING ||
            reader.phase == MC_NBT_STREAM_PARSING || reader.phase == MC_NBT_STREAM_RETIRING)
            return storage_result(error, MC_WORLD_STORAGE_BUSY, "ld terminal active DataInput");
        if (reader.source_in != NULL && reader.source_in != l->compression)
            return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "ld terminal borrowed input identity");
    }
    if (l->compression != NULL) {
        mc_gzip_state gzip;
        if (mc_gzip_input_state_read(l->compression, &gzip, NULL) != MC_GZIP_OK)
            return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "ld terminal actual compression owner");
        if (gzip.active || gzip.retiring || gzip.phase == MC_GZIP_CONSTRUCTING ||
            gzip.phase == MC_GZIP_RETIRING)
            return storage_result(error, MC_WORLD_STORAGE_BUSY, "ld terminal active compression");
        /* Allocate supplies l->compression's sole owning occurrence. The
         * DataInput byte-port merely borrows it; it adds NO second GZIP hold.
         * clear_input must destroy this memory borrower before record bytes.
         * A separate occurrence cannot be admitted by guessing an allowance. */
        if (gzip.holds != 1)
            return storage_result(error, MC_WORLD_STORAGE_BUSY,
                                  "ld terminal independent compression occurrence");
        if (l->record == NULL || gzip.source_in != NULL || gzip.pending_input != NULL ||
            ((gzip.source_memory != NULL || gzip.source_memory_length != 0) &&
             (gzip.source_memory != l->record->bytes ||
              gzip.source_memory_length != l->record->length)))
            return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "ld terminal actual borrowed compressed record");
        status = terminal_loader_inflater(gzip.source_inf, error);
        if (status == MC_WORLD_STORAGE_OK)
            status = terminal_loader_inflater(gzip.pending_inflater, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    /* Parsed documents and failed constructor children may remain owned.
     * Their reference counts are not invented World roots or exemptions.
     * Store admission separately inventories its shared classes/regions. */
    *out = (mc_world_terminal_loader_view){&l->state.store, &l->state.regions, l->state.holds};
    return storage_result(error, MC_WORLD_STORAGE_OK, NULL);
}

static mc_world_storage_status compression_failure(mc_world_source_chunk_loader *l,
                                                   mc_gzip_status gs) {
    mc_world_source_status status = gs == MC_GZIP_LIMIT        ? MC_WORLD_SOURCE_LIMIT
                                    : gs == MC_GZIP_ALLOCATION ? MC_WORLD_SOURCE_ALLOCATION
                                    : gs == MC_GZIP_BUSY       ? MC_WORLD_SOURCE_BUSY
                                    : l->work.compression.source_exception
                                        ? MC_WORLD_SOURCE_SOURCE_EXCEPTION
                                        : MC_WORLD_SOURCE_STATE;
    result(&l->work.source, status, l->work.compression.operation, "ld", 18);
    l->work.source.gzip = l->work.compression;
    l->work.source.source_exception = l->work.compression.source_exception;
    l->work.source.native_policy = !l->work.source.source_exception;
    return source_status(status);
}

mc_world_storage_status mc_world_source_chunk_loader_begin(mc_world_source_chunk_loader *l,
                                                           mc_world_runtime *world,
                                                           int32_t dimension, int32_t x, int32_t z,
                                                           size_t maximum_reader_bytes,
                                                           mc_world_store_game_stage **out,
                                                           mc_world_storage_error *e) {
    if (l == NULL || world == NULL || out == NULL)
        return storage_result(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "ld read arguments");
    if (l->state.active || l->state.retiring)
        return storage_result(e, MC_WORLD_STORAGE_BUSY, "ld active read");
    if (l->state.phase != MC_WORLD_SOURCE_READY) {
        if (l->failure.storage.status != MC_WORLD_STORAGE_OK) {
            if (e != NULL)
                *e = l->failure.storage;
            return l->failure.storage.status;
        }
        return storage_result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "ld nonready read");
    }
    l->state.active = true;
    mc_world_source_status ss = clear_input(l, &l->work);
    mc_world_storage_status status = source_status(ss);
    if (status != MC_WORLD_STORAGE_OK)
        goto failed;
    l->work = (mc_world_source_chunk_error){0};
    l->state.last_pc = 6;
    ss = mc_world_source_regions_read(l->state.regions, l->state.source_a, x, z, &l->record,
                                      &l->work.region);
    if (ss != MC_WORLD_SOURCE_OK) {
        l->work.source = l->work.region.source;
        status = source_status(ss);
        goto failed;
    }
    if (l->record == NULL) {
        l->state.active = false;
        return storage_result(e, MC_WORLD_STORAGE_MISSING, "actual qj null record");
    }
    /* qj's NEW DataInputStream precedes NEW GZIP/InflaterInputStream. */
    mc_nbt_limits limits = mc_nbt_default_limits();
    size_t reader_budget =
        maximum_reader_bytes == 0 ? limits.max_allocation_bytes : maximum_reader_bytes;
    mc_nbt_status ns = mc_nbt_stream_allocate(reader_budget, &l->reader, &l->work.input);
    if (ns != MC_NBT_OK) {
        status = ns == MC_NBT_LIMIT ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_ALLOCATION_FAILED;
        goto input_failed;
    }
    mc_gzip_status gs = mc_gzip_input_allocate(NULL, &l->compression, &l->work.compression);
    if (gs != MC_GZIP_OK) {
        status = compression_failure(l, gs);
        goto failed;
    }
    gs = mc_gzip_input_construct_memory(l->compression, l->record->bytes, l->record->length,
                                        l->record->compression == 1, &l->work.compression);
    if (gs != MC_GZIP_OK) {
        if (l->work.compression.source_exception) {
            /* GZIP header IOException occurs inside qj's catch. Later parse
             * read errors occur outside qj and never become generation. */
            l->state.active = false;
            return storage_result(e, MC_WORLD_STORAGE_MISSING,
                                  "qj caught compression constructor IOException");
        }
        status = compression_failure(l, gs);
        goto failed;
    }
    mc_nbt_byte_input port = mc_gzip_input_byte_port(l->compression);
    ns = mc_nbt_stream_construct(l->reader, &port, &limits, &l->work.input);
    if (ns == MC_NBT_OK) {
        l->state.last_pc = 18;
        ns = mc_nbt_stream_parse(l->reader, true, &l->document, &l->work.input);
    }
    if (ns != MC_NBT_OK) {
        (void)mc_gzip_input_last_error(l->compression, &l->work.compression);
        status = ns == MC_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                 : ns == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                  : MC_WORLD_STORAGE_INVALID_DATA;
        goto input_failed;
    }
    ns = mc_nbt_document_retain(l->document);
    if (ns != MC_NBT_OK) {
        status = MC_WORLD_STORAGE_LIMIT;
        goto input_failed;
    }
    l->parsed_document = l->document;
    const uint16_t level_units[] = {'L', 'e', 'v', 'e', 'l'};
    const uint16_t blocks_units[] = {'B', 'l', 'o', 'c', 'k', 's'};
    const mc_nbt_tag *level =
        mc_nbt_compound_get(mc_nbt_document_root(l->document), level_units, 5);
    if (level == NULL ||
        (level->type == MC_NBT_COMPOUND && mc_nbt_compound_get(level, blocks_units, 6) == NULL)) {
        int pc = level == NULL ? 74 : 130;
        l->state.last_pc = pc;
        result(&l->work.source, MC_WORLD_SOURCE_UNIMPLEMENTED,
               "actual ld missing-data PrintStream.println", "ld", pc);
        l->work.source.frames[0].method = "a";
        l->work.source.frames[0].descriptor = "(Lfd;II)Llm;";
        l->work.source.diagnostic_frontier = true;
        status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        goto failed;
    }
    l->state.last_pc = 143;
    status = mc_world_store_source_adopt_game(l->state.store, world, dimension, &l->document, out,
                                              &l->work.storage);
    if (status != MC_WORLD_STORAGE_OK) {
        /* Missing-Level/Blocks diagnostics are actual reached PrintStream
         * dependencies, never an unreported absent chunk/generation result. */
        result(&l->work.source, MC_WORLD_SOURCE_STATE, l->work.storage.operation, "ld", 143);
        goto failed;
    }
    /* ld does not close or drain its DataInputStream. The retained real RAM
     * stream prefix is retired natively before the next call/loader cleanup. */
    l->state.active = false;
    return storage_result(e, MC_WORLD_STORAGE_OK, NULL);
input_failed:
    result(&l->work.source,
           status == MC_WORLD_STORAGE_LIMIT               ? MC_WORLD_SOURCE_LIMIT
           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_WORLD_SOURCE_ALLOCATION
                                                          : MC_WORLD_SOURCE_SOURCE_EXCEPTION,
           "actual ld as.a(DataInput)", "ld", 18);
    l->work.source.input = l->work.input;
    l->work.source.source_exception = status == MC_WORLD_STORAGE_INVALID_DATA;
    l->work.source.native_policy = !l->work.source.source_exception;
failed:
    if (l->work.storage.status == MC_WORLD_STORAGE_OK)
        storage_result(&l->work.storage, status, l->work.source.operation);
    l->failure = l->work;
    l->state.phase = MC_WORLD_SOURCE_FAILED;
    l->state.active = false;
    if (e != NULL)
        *e = l->failure.storage;
    return status;
}

const mc_world_source_chunk_error *
mc_world_source_chunk_loader_last_error(const mc_world_source_chunk_loader *l) {
    return l == NULL ? NULL : &l->failure;
}

mc_world_storage_status mc_world_source_chunk_loader_prepare_normalizer(
    mc_world_source_chunk_loader *l, mc_world_runtime *world, mc_world_storage_error *e) {
    if (l == NULL || world == NULL || l->state.phase != MC_WORLD_SOURCE_READY || l->state.active)
        return storage_result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "ld Chunk.i owner phase");
    if (mc_world_store_source_normalization_map(l->state.store) != NULL)
        return storage_result(e, MC_WORLD_STORAGE_OK, NULL);
    /* Actual vi.<clinit> owns one immutable map per shared handler namespace;
     * its first active use is lm.i at ld272, after both import loops. */
    mc_world_source_normalizer shared;
    if (!mc_world_store_source_normalizer(l->state.store, &shared))
        return storage_result(e, MC_WORLD_STORAGE_BUSY, "actual shared vi class owner");
    if (*shared.failed) {
        l->failure.source = *shared.failure;
        l->failure.registry = *shared.registry;
        l->state.phase = MC_WORLD_SOURCE_FAILED;
        storage_result(&l->failure.storage, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       l->failure.source.operation);
        return storage_result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, l->failure.source.operation);
    }
    mc_registry_classes *registry = mc_world_runtime_registry(world);
    for (int32_t i = 0; i < 256; ++i) {
        uint8_t value = (uint8_t)i;
        if (value != 0) {
            mc_registry_block_m_view array;
            mc_registry_error cause = {0};
            const mc_block *block;
            if (registry == NULL) {
                cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
                cause.message = "actual vi shared Registry binding";
            }
            if (registry == NULL ||
                mc_registry_block_m_get(registry, &array, &cause) != MC_REGISTRY_OK ||
                mc_registry_block_m_at(&array, i, &block, &cause) != MC_REGISTRY_OK) {
                result(shared.failure, MC_WORLD_SOURCE_UNIMPLEMENTED,
                       "actual vi Block.m initializer/element", "vi", 24);
                *shared.registry = cause;
                *shared.failed = true;
                l->failure.source = *shared.failure;
                l->failure.registry = cause;
                l->state.phase = MC_WORLD_SOURCE_FAILED;
                storage_result(&l->failure.storage, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               l->failure.source.operation);
                return storage_result(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      l->failure.source.operation);
            }
            if (block == NULL)
                value = 0;
        }
        shared.map[i] = value;
    }
    *shared.ready = true;
    return storage_result(e, MC_WORLD_STORAGE_OK, NULL);
}
