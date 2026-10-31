#include "internal.h"
#include <stdlib.h>
#include <string.h>

typedef union metadata_block metadata_block;

union metadata_block {
    struct {
        metadata_block *next;
    } link;

    long double alignment;
    void *pointer_alignment;
};

typedef struct metadata_reader {
    metadata_block *blocks;
    size_t used, bound;
    mc_client_texture_stream *stream;
    mc_texture_pack_error failure;
    bool limited;
} metadata_reader;

static void *allocate(void *context, size_t bytes) {
    metadata_reader *reader = context;
    if (bytes > SIZE_MAX - sizeof(metadata_block) ||
        bytes + sizeof(metadata_block) > reader->bound - reader->used) {
        reader->limited = true;
        return NULL;
    }
    metadata_block *block = calloc(1, sizeof(*block) + bytes);
    if (block == NULL)
        return NULL;
    block->link.next = reader->blocks;
    reader->blocks = block;
    reader->used += sizeof(*block) + bytes;
    return block + 1;
}

static void retire_reader(metadata_reader *reader) {
    while (reader->blocks != NULL) {
        metadata_block *next = reader->blocks->link.next;
        free(reader->blocks);
        reader->blocks = next;
    }
}

static mc_java_properties_status read_bytes(void *context, uint8_t *bytes, size_t capacity,
                                            size_t *count) {
    metadata_reader *reader = context;
    const mc_texture_pack_status status =
        mc_client_texture_stream_read(reader->stream, bytes, capacity, count, &reader->failure);
    return status == MC_TEXTURE_PACK_OK                  ? MC_JAVA_PROPERTIES_OK
           : status == MC_TEXTURE_PACK_LIMIT             ? MC_JAVA_PROPERTIES_LIMIT
           : status == MC_TEXTURE_PACK_ALLOCATION_FAILED ? MC_JAVA_PROPERTIES_ALLOCATION_FAILED
                                                         : MC_JAVA_PROPERTIES_IO_ERROR;
}

static bool ready(void *context) {
    metadata_reader *reader = context;
    if (reader->stream->zip != NULL)
        return texture_zip_available(reader->stream->zip) != 0;
    const mc_registry_resources *resources = reader->stream->pack->resources;
    return resources->ready != NULL &&
           resources->ready(resources->context, &reader->stream->resource);
}

static mc_java_reader_status close_reader(void *context) {
    metadata_reader *reader = context;
    return mc_client_texture_stream_close(reader->stream, &reader->failure) == MC_TEXTURE_PACK_OK
               ? MC_JAVA_READER_OK
               : MC_JAVA_READER_IO_ERROR;
}

static mc_texture_pack_status reader_failure(metadata_reader *reader, mc_java_reader_status status,
                                             mc_texture_pack_error *error) {
    if (reader->failure.status != MC_TEXTURE_PACK_OK) {
        *error = reader->failure;
        return error->status;
    }
    error->reader_status = status;
    return texture_pack_fail(
        error,
        reader->limited || status == MC_JAVA_READER_LIMIT ? MC_TEXTURE_PACK_LIMIT
        : status == MC_JAVA_READER_ALLOCATION_FAILED      ? MC_TEXTURE_PACK_ALLOCATION_FAILED
        : status == MC_JAVA_READER_UNSUPPORTED_CHARSET    ? MC_TEXTURE_PACK_UNSUPPORTED_NATIVE
                                                          : MC_TEXTURE_PACK_SOURCE_IO,
        "vp actual metadata reader", NULL, NULL, NULL, 0, 196);
}

static void metadata_site(mc_texture_pack_error *error, unsigned int pc) {
    if (error->source_class == NULL) {
        error->source_class = "vp";
        error->source_method = "a";
        error->source_descriptor = "(Lnet/minecraft/client/Minecraft;)V";
        error->source_pc = pc;
    }
    error->ik_pc = 196;
}

static mc_texture_pack_status open_archive(mc_client_texture_pack *pack, texture_zip **out,
                                           mc_texture_pack_error *error) {
    FILE *file = texture_pack_file_open(pack, error);
    if (file == NULL)
        return error->status;
    const texture_zip_limits limits = {
        pack->options.max_archive_bytes, pack->options.max_zip_entries,
        pack->options.max_encoded_image_bytes, pack->options.max_decoded_image_bytes,
        pack->options.max_stream_allocation_bytes};
    texture_zip_error cause = {0};
    if (texture_zip_open(file, &limits, out, &cause) != TEXTURE_ZIP_OK)
        return texture_pack_zip_error(error, &cause);
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status texture_zip_construct(mc_client_texture_pack *pack, const char *path,
                                             mc_texture_pack_text name,
                                             mc_texture_pack_error *error) {
    pack->icon_id = -1; /* vp6, after literal-return i constructor1. */
    pack->name = texture_string_copy(name, pack->options.max_name_units, error);
    if (pack->name == NULL) {
        error->source_pc = 11;
        goto failed;
    }
    const size_t bytes = strlen(path);
    if (bytes > pack->options.max_path_bytes) {
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "vp retained File native path bound", "vp",
                          "<init>", "(Ljava/io/File;)V", 19, 164);
        return error->status;
    }
    pack->file = malloc(bytes + 1);
    if (pack->file == NULL) {
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED, "vp retained native File owner",
                          "vp", "<init>", "(Ljava/io/File;)V", 19, 164);
        return error->status;
    }
    memcpy(pack->file, path, bytes + 1);
    return MC_TEXTURE_PACK_OK;
failed:
    error->source_class = "vp";
    error->source_method = "<init>";
    error->source_descriptor = "(Ljava/io/File;)V";
    return error->status;
}

mc_texture_pack_status texture_pack_load(mc_client_texture_pack *pack,
                                         mc_texture_pack_error *error) {
    if (pack->kind == MC_TEXTURE_PACK_DEFAULT)
        return MC_TEXTURE_PACK_OK; /* Actual inherited i.a()V return0. */
    texture_zip *opened = NULL;
    const mc_texture_pack_status status = open_archive(pack, &opened, error);
    if (status == MC_TEXTURE_PACK_SOURCE_IO) {
        /* Actual vp.a() catches Exception18. A failed new ZipFile leaves the
         * previous e field intact; it is not cleared before attempting open. */
        *error = (mc_texture_pack_error){0};
        return MC_TEXTURE_PACK_OK;
    }
    if (status != MC_TEXTURE_PACK_OK) {
        error->source_class = "vp";
        error->source_method = "a";
        error->source_descriptor = "()V";
        error->source_pc = 9;
        return status;
    }
    texture_zip *old = pack->zip;
    pack->zip = opened; /* Actual vp.e12. */
    texture_zip_error retirement = {0};
    if (texture_zip_release(old, &retirement) != TEXTURE_ZIP_OK) {
        texture_pack_zip_error(error, &retirement);
        error->cause_status = error->status;
        error->status = MC_TEXTURE_PACK_PROVIDER_FAILED;
        error->native_close_error = retirement.system_error;
        error->source_class = "vp";
        error->source_method = "a";
        error->source_descriptor = "()V";
        error->source_pc = 12;
        return error->status;
    } /* Dropped native occurrence, not source b() close. */
    return MC_TEXTURE_PACK_OK;
}

static mc_texture_pack_status metadata_stream(mc_client_texture_pack *pack, texture_zip *zip,
                                              const char *name, mc_client_texture_stream **out,
                                              mc_texture_pack_error *error) {
    texture_zip_stream *entry = NULL;
    texture_zip_error cause = {0};
    if (texture_zip_lookup(zip, (const uint8_t *)name, strlen(name), &entry, &cause) !=
        TEXTURE_ZIP_OK)
        return texture_pack_zip_error(error, &cause);
    if (entry == NULL)
        return texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE,
                                 "vp metadata ZipFile.getInputStream null entry", "vp", "a",
                                 "(Lnet/minecraft/client/Minecraft;)V", 23, 196);
    const mc_texture_pack_status status = texture_stream_from_zip(pack, entry, out, error);
    if (status != MC_TEXTURE_PACK_OK) {
        error->source_class = NULL;
        metadata_site(error, strcmp(name, "pack.txt") == 0 ? 23u : 91u);
    }
    return status;
}

static bool source_exception(mc_texture_pack_status status) {
    return status == MC_TEXTURE_PACK_SOURCE_IO || status == MC_TEXTURE_PACK_NULL_REFERENCE ||
           status == MC_TEXTURE_PACK_SOURCE_ILLEGAL_ARGUMENT;
}

static mc_texture_pack_status line_store(mc_client_texture_string **field,
                                         const mc_java_reader_line *line,
                                         const mc_texture_pack_options *options,
                                         mc_texture_pack_error *error) {
    mc_client_texture_string *stored = NULL;
    if (line != NULL) {
        const size_t units = line->length > 34 ? 34 : line->length;
        stored = texture_string_copy((mc_texture_pack_text){line->units, units, false},
                                     options->max_name_units, error);
        if (stored == NULL)
            return error->status;
    }
    texture_string_free(*field);
    *field = stored;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status texture_zip_metadata(mc_client_texture_pack *pack,
                                            mc_texture_pack_error *error) {
    texture_zip *zip = NULL;
    mc_client_texture_stream *current = NULL, *text_stream = NULL, *icon_stream = NULL;
    metadata_reader reader = {.bound = pack->options.max_stream_allocation_bytes};
    if (open_archive(pack, &zip, error) != MC_TEXTURE_PACK_OK) {
        if (error->status == MC_TEXTURE_PACK_SOURCE_IO)
            goto print;
        error->source_pc = 12;
        goto done;
    }
    mc_texture_pack_status status = metadata_stream(pack, zip, "pack.txt", &text_stream, error);
    if (status == MC_TEXTURE_PACK_OK) {
        current = text_stream; /* Actual InputStream local store26. */
        reader.stream = current;
        const mc_java_properties_allocator allocator = {&reader, allocate};
        const mc_java_reader_input input = {{&reader, read_bytes}, ready, close_reader};
        mc_java_reader *text_reader = NULL;
        mc_java_reader_status read =
            mc_java_reader_create(&allocator, &input, pack->options.metadata_charset, &text_reader);
        const mc_java_reader_line *line = NULL;
        const unsigned int first_read_pc = read == MC_JAVA_READER_OK                    ? 48u
                                           : read == MC_JAVA_READER_UNSUPPORTED_CHARSET ? 36u
                                                                                        : 39u;
        if (read == MC_JAVA_READER_OK)
            read = mc_java_reader_read_line(text_reader, &line);
        if (read != MC_JAVA_READER_OK) {
            status = reader_failure(&reader, read, error);
            metadata_site(error, first_read_pc);
        } else {
            status = line_store(&pack->description, line, &pack->options, error); /* b54 */
            if (status != MC_TEXTURE_PACK_OK)
                metadata_site(error, 54);
            if (status == MC_TEXTURE_PACK_OK) {
                read = mc_java_reader_read_line(text_reader, &line);
                if (read != MC_JAVA_READER_OK) {
                    status = reader_failure(&reader, read, error);
                    metadata_site(error, 61);
                } else {
                    status = line_store(&pack->description2, line, &pack->options, error); /* c67 */
                    if (status != MC_TEXTURE_PACK_OK)
                        metadata_site(error, 67);
                }
            }
            if (status == MC_TEXTURE_PACK_OK) {
                read = mc_java_reader_close(text_reader); /* Actual BufferedReader.close72. */
                if (read != MC_JAVA_READER_OK) {
                    status = reader_failure(&reader, read, error);
                    metadata_site(error, 72);
                } else {
                    status = mc_client_texture_stream_close(current, error); /* InputStream76. */
                    if (status != MC_TEXTURE_PACK_OK)
                        metadata_site(error, 76);
                }
            }
        }
    }
    if (status != MC_TEXTURE_PACK_OK && !source_exception(status))
        goto done;
    *error = (mc_texture_pack_error){0}; /* Actual inner Exception catch82. */
    status = metadata_stream(pack, zip, "pack.png", &icon_stream, error);
    if (status == MC_TEXTURE_PACK_OK) {
        current = icon_stream; /* Actual local store94; failed lookup keeps old local. */
        mc_client_argb_image *image = NULL;
        status = texture_pack_image_read(current, &image, error);
        if (status != MC_TEXTURE_PACK_OK)
            metadata_site(error, 97);
        if (status == MC_TEXTURE_PACK_OK) {
            mc_client_argb_image_release(pack->icon);
            pack->icon = image; /* Actual g100 precedes source InputStream.close104. */
            status = mc_client_texture_stream_close(current, error);
            if (status != MC_TEXTURE_PACK_OK)
                metadata_site(error, 104);
        }
    }
    if (status != MC_TEXTURE_PACK_OK && !source_exception(status))
        goto done;
    *error = (mc_texture_pack_error){0}; /* Actual inner Exception catch110. */
    texture_zip_error close_error = {0};
    if (texture_zip_close(zip, &close_error) != TEXTURE_ZIP_OK) { /* Source ZipFile.close112. */
        texture_pack_zip_error(error, &close_error);
        goto print;
    }
    if (current != NULL) {
        mc_texture_pack_error ignored = {0};
        status = mc_client_texture_stream_close(current, &ignored); /* Source117. */
        if (status != MC_TEXTURE_PACK_OK && !source_exception(status)) {
            *error = ignored;
            goto done;
        }
    } /* Null current's source NPE is caught by the actual catch123. */
    texture_zip_close(zip, &close_error); /* Repeated source125, literal already-closed leaf. */
    goto done;
print:
    error->cause_status = MC_TEXTURE_PACK_SOURCE_IO;
    error->source_diagnostic_reached = true;
    texture_pack_fail(error, MC_TEXTURE_PACK_DIAGNOSTIC_DEPENDENCY,
                      "vp caught Exception.printStackTrace dependency", "vp", "a",
                      "(Lnet/minecraft/client/Minecraft;)V", 141, 196);
done:
    retire_reader(&reader);
    /* Native local retirement is separate from every reached source close.
     * Do not advance a source finally beyond an unimplemented print/provider. */
    mc_client_texture_stream *streams[] = {text_stream, icon_stream};
    for (size_t i = 0; i < sizeof(streams) / sizeof(streams[0]); ++i) {
        mc_texture_pack_error cause = {0};
        if (mc_client_texture_stream_release(streams[i], &cause) != MC_TEXTURE_PACK_OK) {
            if (error->status == MC_TEXTURE_PACK_OK)
                *error = cause;
            else
                error->native_close_status = cause.native_close_status;
        }
    }
    if (zip != NULL) {
        texture_zip_error cause = {0};
        if (texture_zip_close(zip, &cause) != TEXTURE_ZIP_OK) {
            if (error->status == MC_TEXTURE_PACK_OK)
                texture_pack_zip_error(error, &cause);
            else
                error->native_close_error = cause.system_error;
        }
        texture_zip_release(zip, NULL);
    }
    if (error->status != MC_TEXTURE_PACK_OK && error->source_class == NULL) {
        error->source_class = "vp";
        error->source_method = "a";
        error->source_descriptor = "(Lnet/minecraft/client/Minecraft;)V";
    }
    return error->status;
}
