#include "mc/world/storage/chunk.h"
#include "mc/world/storage/source_regions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source regions line%d check%u: %s\n", __LINE__, checks, #x);          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static mc_world_info_name *name(const char *s) {
    size_t n = strlen(s);
    uint16_t *units = malloc((n == 0 ? 1 : n) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < n; ++i) {
        if ((unsigned char)s[i] > 127) {
            free(units);
            return NULL;
        }
        units[i] = (uint8_t)s[i];
    }
    mc_world_info_name *out = NULL;
    mc_world_info_status status =
        mc_world_info_name_copy((mc_nbt_string){units, n}, 1048576u, &out);
    free(units);
    return status == MC_WORLD_INFO_OK ? out : NULL;
}

static mc_java_file *file(mc_java_files *files, const char *s) {
    mc_world_info_name *n = name(s);
    mc_java_file *out = NULL;
    mc_java_file_error error;
    if (n != NULL && mc_java_file_allocate(files, &out, &error) == MC_JAVA_FILE_OK &&
        mc_java_file_construct_string(out, n, &error) == MC_JAVA_FILE_OK) {
        mc_world_info_name_release(n);
        return out;
    }
    mc_world_info_name_release(n);
    (void)mc_java_file_release(&out, NULL);
    return NULL;
}

static int parse_record(mc_world_source_region_record *record, int32_t expected, uint8_t block,
                        bool bad_trailer) {
    mc_gzip_input *gzip = NULL;
    mc_gzip_error ge;
    CHECK(mc_gzip_input_allocate(NULL, &gzip, &ge) == MC_GZIP_OK);
    CHECK(mc_gzip_input_construct_memory(gzip, record->bytes, record->length,
                                         record->compression == 1, &ge) == MC_GZIP_OK);
    mc_nbt_stream *reader = NULL;
    mc_nbt_stream_error ne;
    mc_nbt_limits limits = mc_nbt_default_limits();
    CHECK(mc_nbt_stream_allocate(limits.max_allocation_bytes, &reader, &ne) == MC_NBT_OK);
    mc_nbt_byte_input port = mc_gzip_input_byte_port(gzip);
    CHECK(mc_nbt_stream_construct(reader, &port, &limits, &ne) == MC_NBT_OK);
    mc_nbt_document *document = NULL;
    CHECK(mc_nbt_stream_parse(reader, true, &document, &ne) == MC_NBT_OK && document != NULL);
    mc_nbt_stream_state rs;
    CHECK(mc_nbt_stream_state_read(reader, &rs, &ne) == MC_NBT_OK && rs.close_calls == 0);
    mc_gzip_state gs;
    CHECK(mc_gzip_input_state_read(gzip, &gs, &ge) == MC_GZIP_OK && !gs.eos && !gs.source_closed);
    mc_stored_chunk *stored = NULL;
    mc_stored_chunk_error ce;
    CHECK(mc_stored_chunk_adopt_game_document(&document, &limits, &stored, &ce) ==
              MC_STORED_CHUNK_OK &&
          document == NULL);
    mc_chunk_state *state = mc_stored_chunk_state(stored);
    CHECK(state != NULL && state->x == expected && state->z == 0 &&
          state->data->blocks[0] == block && !state->has_entities);
    if (bad_trailer) {
        int byte = 999;
        CHECK(mc_gzip_input_read_byte(gzip, &byte, &ge) == MC_GZIP_SOURCE_FORMAT && byte == 999 &&
              strcmp(ge.operation, "GZIP trailer CRC") == 0);
    }
    CHECK(mc_nbt_stream_release(&reader, &ne) == MC_NBT_OK);
    CHECK(mc_gzip_input_release(&gzip, &ge) == MC_GZIP_OK);
    mc_stored_chunk_free(stored);
    return 0;
}

static int replace_payload(const char *region, const char *root) {
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/replacement-record.bin", root);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    uint8_t bytes[4096];
    size_t count = fread(bytes, 1, sizeof(bytes), input);
    CHECK(count > 5 && count < sizeof(bytes) && !ferror(input));
    CHECK(fclose(input) == 0);
    FILE *output = fopen(region, "r+b");
    CHECK(output != NULL);
    const uint8_t absent[4] = {0};
    CHECK(fwrite(absent, 1, 4, output) == 4 && fseek(output, 8192, SEEK_SET) == 0);
    CHECK(fwrite(bytes, 1, count, output) == count && fclose(output) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    mc_java_files *files = NULL;
    mc_java_file_error fe;
    CHECK(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    mc_java_file *root = file(files, argv[1]), *equal = file(files, argv[1]);
    CHECK(root != NULL && equal != NULL && root != equal);
    mc_world_source_regions *regions = NULL;
    mc_world_source_region_error error;
    CHECK(mc_world_source_regions_create(files, NULL, &regions, &error) == MC_WORLD_SOURCE_OK);
    mc_world_source_regions_state rs;
    CHECK(mc_world_source_regions_inspect(regions, &rs) && !rs.activated && rs.entries == 0 &&
          rs.files == files);
    mc_world_source_region_record *record = NULL;
    CHECK(mc_world_source_regions_read(regions, root, 0, 0, &record, &error) ==
              MC_WORLD_SOURCE_OK &&
          record != NULL && record->compression == 2);
    CHECK(parse_record(record, 0, 254, false) == 0);
    mc_world_source_region_record_free(record);
    record = NULL;
    CHECK(mc_world_source_regions_inspect(regions, &rs) && rs.activated && rs.entries == 1);
    CHECK(mc_world_source_regions_read(regions, equal, 1, 0, &record, &error) ==
              MC_WORLD_SOURCE_OK &&
          record != NULL && record->compression == 1);
    CHECK(parse_record(record, 1, 254, true) == 0);
    mc_world_source_region_record_free(record);
    record = NULL;
    CHECK(mc_world_source_regions_read(regions, root, 2, 0, &record, &error) ==
              MC_WORLD_SOURCE_OK &&
          record == NULL && error.source_null);
    CHECK(mc_world_source_regions_read(regions, root, 3, 0, &record, &error) ==
              MC_WORLD_SOURCE_OK &&
          record == NULL && error.source_null);
    CHECK(mc_world_source_regions_read(regions, root, 31, 31, &record, &error) ==
              MC_WORLD_SOURCE_OK &&
          record == NULL);
    char old[4096], moved[4096];
    int a = snprintf(old, sizeof(old), "%s/region/r.0.0.mcr", argv[1]);
    int b = snprintf(moved, sizeof(moved), "%s/region/moved.mcr", argv[1]);
    CHECK(a > 0 && (size_t)a < sizeof(old) && b > 0 && (size_t)b < sizeof(moved));
    /* Cached qj retains its header offsets, but its real RAF reads the changed
     * body. A whole-file cached image would wrongly return the old block254. */
    CHECK(replace_payload(old, argv[1]) == 0);
#ifdef _WIN32
    /* Same selected JDK17 RandomAccessFile sharing: read/write, deny delete. */
    CHECK(rename(old, moved) != 0);
#else
    CHECK(rename(old, moved) == 0);
#endif
    CHECK(mc_world_source_regions_read(regions, equal, 0, 0, &record, &error) ==
              MC_WORLD_SOURCE_OK &&
          record != NULL);
    CHECK(parse_record(record, 0, 253, false) == 0);
    mc_world_source_region_record_free(record);
    record = NULL;
    CHECK(mc_world_source_regions_inspect(regions, &rs) && rs.entries == 1);
    CHECK(mc_world_source_regions_flush(regions, &error) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_source_regions_inspect(regions, &rs) && rs.entries == 0);
    CHECK(mc_world_source_regions_read(regions, equal, 0, 0, &record, &error) ==
              MC_WORLD_SOURCE_OK &&
          record == NULL);
    CHECK(mc_world_source_regions_read(regions, equal, 32, 0, &record, &error) ==
              MC_WORLD_SOURCE_OK &&
          record == NULL);
    char fresh_path[4096];
    int made = snprintf(fresh_path, sizeof(fresh_path), "%s/region/r.1.0.mcr", argv[1]);
    CHECK(made > 0 && (size_t)made < sizeof(fresh_path));
    mc_java_file *fresh = file(files, fresh_path);
    CHECK(fresh != NULL);
    int64_t size = -1;
    CHECK(mc_java_file_length(fresh, &size, &fe) == MC_JAVA_FILE_OK && size == 8192);
    CHECK(mc_world_source_regions_retain(regions, &error) == MC_WORLD_SOURCE_OK);
    mc_world_source_regions *held = regions;
    CHECK(mc_world_source_regions_retire(&regions, &error) == MC_WORLD_SOURCE_BUSY &&
          regions == held);
    CHECK(mc_world_source_regions_release(&held, &error) == MC_WORLD_SOURCE_OK && held == NULL);
    CHECK(mc_world_source_regions_retire(&regions, &error) == MC_WORLD_SOURCE_OK &&
          regions == NULL);
    CHECK(mc_java_file_release(&fresh, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_release(&equal, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_release(&root, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&files, &fe) == MC_JAVA_FILE_OK && files == NULL);
    printf("source region controls: %u checks\n", checks);
    return 0;
}
