/* Authored controls over real Files/FOS and the canonical output primitive. */
#include "mc/world/storage/compressed_output.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static mc_compressed_output *observed;
static mc_java_file_output *observed_file;
static const mc_nbt_tag *observed_tree;
static bool armed, write_fault, close_fault, callback_ok = true;
static size_t actual_writes, actual_closes;

static void reentry(void) {
    if (!armed)
        return;
    mc_compressed_output_error e;
    mc_java_output_error fe;
    mc_compressed_output *alias = observed;
    mc_java_file_output *fa = observed_file;
    mc_compressed_output_state state;
    if (observed != NULL &&
        (mc_compressed_output_release(&alias, &e) != MC_JAVA_OUTPUT_BUSY || alias != observed ||
         mc_compressed_output_write_compound(observed, observed_tree, NULL, &e) !=
             MC_JAVA_OUTPUT_BUSY ||
         mc_compressed_output_inspect(observed, &state)))
        callback_ok = false;
    if (observed_file != NULL &&
        (mc_java_file_output_retain(observed_file, &fe) != MC_JAVA_OUTPUT_BUSY ||
         mc_java_file_output_release(&fa, &fe) != MC_JAVA_OUTPUT_BUSY || fa != observed_file))
        callback_ok = false;
}

size_t mc_test_output_write(const void *p, size_t size, size_t count, FILE *file) {
    ++actual_writes;
    const size_t n = fwrite(p, size, armed && write_fault && count > 3 ? 3 : count, file);
    reentry();
    if (armed && write_fault)
        errno = EIO;
    return n;
}

int mc_test_output_ferror(FILE *file) {
    const int r = ferror(file);
    return armed && write_fault ? 1 : r;
}

int mc_test_output_close(FILE *file) {
    ++actual_closes;
    const int r = fclose(file);
    reentry();
    if (armed && close_fault) {
        errno = EIO;
        return EOF;
    }
    return r;
}

static bool ascii(const char *text, mc_world_info_name **out) {
    uint16_t units[4096];
    const size_t n = strlen(text);
    if (n > sizeof(units) / sizeof(*units))
        return false;
    for (size_t i = 0; i < n; ++i) {
        if ((unsigned char)text[i] > 127)
            return false;
        units[i] = (unsigned char)text[i];
    }
    return mc_world_info_name_copy((mc_nbt_string){units, n}, 16384, out) == MC_WORLD_INFO_OK;
}

#define NEED(x)                                                                                    \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "compressed output line%d: %s\n", __LINE__, #x);                       \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    int result = 1;
    mc_java_files *files = NULL;
    mc_java_file *parent = NULL, *file = NULL;
    mc_world_info_name *parent_name = NULL, *leaf = NULL;
    mc_java_file_output *fos = NULL, *alias = NULL;
    mc_compressed_output *o = NULL;
    mc_java_file_error fe;
    mc_java_output_error oe;
    mc_compressed_output_error e;
    mc_compressed_output_state cs;
    mc_java_file_output_state fs;
    uint8_t *random = malloc(65536);
    uint16_t *long_text = malloc(40000 * sizeof(*long_text));
    NEED(random != NULL && long_text != NULL);
    uint32_t rng = UINT32_C(0x6d2b79f5);
    for (size_t i = 0; i < 65536; ++i) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        random[i] = (uint8_t)rng;
    }
    for (size_t i = 0; i < 40000; ++i)
        long_text[i] = (uint16_t)('A' + i % 26);
    static const uint16_t names[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l'};
    static const uint16_t x = 'x';
    static const uint16_t text[] = {0, 0x41, 0x7ff, 0x800, 0xd800, 0xdc00, 0xd83d, 0xde03, 0xffff};
    uint8_t bytes[32];
    for (size_t i = 0; i < sizeof(bytes); ++i)
        bytes[i] = (uint8_t)i;
    mc_nbt_tag values[12] = {0};
    const mc_nbt_tag *items[12];
    for (size_t i = 0; i < 12; ++i) {
        values[i].name = (mc_nbt_string){&names[i], 1};
        items[i] = &values[i];
    }
    values[0].type = MC_NBT_BYTE;
    values[0].value.byte_value = -7;
    values[1].type = MC_NBT_SHORT;
    values[1].value.short_value = -12345;
    values[2].type = MC_NBT_INT;
    values[2].value.int_value = -123456789;
    values[3].type = MC_NBT_LONG;
    values[3].value.long_value = INT64_MIN + 7;
    values[4].type = MC_NBT_FLOAT;
    values[4].value.float_bits = UINT32_C(0xff800001);
    values[5].type = MC_NBT_DOUBLE;
    values[5].value.double_bits = UINT64_C(0xfff0000000000001);
    values[6].type = MC_NBT_BYTE_ARRAY;
    values[6].value.byte_array.data = bytes;
    values[6].value.byte_array.length = sizeof(bytes);
    values[7].type = MC_NBT_STRING;
    values[7].value.string = (mc_nbt_string){text, sizeof(text) / sizeof(*text)};
    mc_nbt_tag list_values[3] = {{.type = MC_NBT_INT}, {.type = MC_NBT_INT}, {.type = MC_NBT_INT}};
    list_values[0].value.int_value = -3;
    list_values[1].value.int_value = 0;
    list_values[2].value.int_value = INT32_MAX;
    const mc_nbt_tag *list_items[] = {&list_values[0], &list_values[1], &list_values[2]};
    values[8].type = MC_NBT_LIST;
    values[8].value.list.element_type = MC_NBT_INT;
    values[8].value.list.items = list_items;
    values[8].value.list.length = 3;
    values[9].type = MC_NBT_LIST;
    values[9].value.list.element_type = MC_NBT_END;
    mc_nbt_tag deep[16] = {0};
    const mc_nbt_tag *deep_items[16];
    for (size_t i = 0; i < 16; ++i) {
        deep[i].type = MC_NBT_COMPOUND;
        deep[i].name = (mc_nbt_string){&x, 1};
        if (i + 1 < 16) {
            deep_items[i] = &deep[i + 1];
            deep[i].value.compound.items = &deep_items[i];
            deep[i].value.compound.length = 1;
        }
    }
    values[10] = deep[0];
    values[10].name = (mc_nbt_string){&names[10], 1};
    values[11].type = MC_NBT_STRING;
    values[11].value.string = (mc_nbt_string){long_text, 40000};
    mc_nbt_tag root = {.type = MC_NBT_COMPOUND};
    root.value.compound.items = items;
    root.value.compound.length = 12;
    mc_nbt_tag array = {.type = MC_NBT_BYTE_ARRAY};
    array.name = (mc_nbt_string){&x, 1};
    array.value.byte_array.data = random;
    array.value.byte_array.length = 65536;
    const mc_nbt_tag *random_items[] = {&array};
    mc_nbt_tag large = {.type = MC_NBT_COMPOUND};
    large.value.compound.items = random_items;
    large.value.compound.length = 1;
    mc_java_output_options options = mc_java_output_default_options();
    options.max_owner_bytes = 1048576;
    options.max_written_bytes = 4194304;
    NEED(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    NEED(ascii(argv[1], &parent_name));
    NEED(mc_java_file_allocate(files, &parent, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_construct_string(parent, parent_name, &fe) == MC_JAVA_FILE_OK);

#define OPEN(name)                                                                                 \
    do {                                                                                           \
        NEED(ascii((name), &leaf));                                                                \
        NEED(mc_java_file_allocate(files, &file, &fe) == MC_JAVA_FILE_OK);                         \
        NEED(mc_java_file_construct_child(file, parent, leaf, &fe) == MC_JAVA_FILE_OK);            \
        NEED(mc_java_file_output_allocate(files, NULL, &fos, &oe) == MC_JAVA_OUTPUT_OK);           \
        NEED(mc_java_file_output_construct(fos, file, &oe) == MC_JAVA_OUTPUT_OK);                  \
        NEED(mc_compressed_output_allocate(&options, &o, &e) == MC_JAVA_OUTPUT_OK);                \
        NEED(mc_compressed_output_construct(o, fos, &e) == MC_JAVA_OUTPUT_OK);                     \
        observed = o;                                                                              \
        observed_file = fos;                                                                       \
        observed_tree = &root;                                                                     \
    } while (0)
#define RETIRE()                                                                                   \
    do {                                                                                           \
        armed = write_fault = close_fault = false;                                                 \
        observed = NULL;                                                                           \
        observed_file = NULL;                                                                      \
        NEED(mc_compressed_output_release(&o, &e) == MC_JAVA_OUTPUT_OK && o == NULL);              \
        NEED(mc_java_file_output_release(&fos, &oe) == MC_JAVA_OUTPUT_OK && fos == NULL);          \
        NEED(mc_java_file_release(&file, &fe) == MC_JAVA_FILE_OK && file == NULL);                 \
        mc_world_info_name_release(leaf);                                                          \
        leaf = NULL;                                                                               \
    } while (0)

    OPEN("types.gz");
    NEED(mc_compressed_output_inspect(o, &cs) && cs.header_written && cs.data_output_constructed &&
         cs.compressed_bytes == 10 && cs.raw_bytes == 0 && !cs.closed);
    armed = true;
    NEED(mc_compressed_output_write_compound(o, &root, NULL, &e) == MC_JAVA_OUTPUT_OK &&
         callback_ok);
    NEED(mc_compressed_output_source_close(o, &e) == MC_JAVA_OUTPUT_OK && callback_ok);
    armed = false;
    NEED(mc_compressed_output_inspect(o, &cs) && cs.closed && cs.finished && cs.file_closed &&
         cs.gzip_closed && cs.deflater_ended && cs.source_close_calls == 1 &&
         cs.utf_buffer_bytes == 80002 && cs.raw_bytes == (uint64_t)cs.written_count);
    NEED(mc_java_file_output_inspect(fos, &fs) && !fs.native_open && fs.source_close_calls == 1 &&
         fs.source_close_succeeded && fs.bytes_committed == cs.compressed_bytes);
    const size_t closed_before = actual_closes;
    NEED(mc_compressed_output_source_close(o, &e) == MC_JAVA_OUTPUT_OK &&
         actual_closes == closed_before);
    RETIRE();

    OPEN("large.gz");
    armed = true;
    observed_tree = &large;
    NEED(mc_compressed_output_write_compound(o, &large, NULL, &e) == MC_JAVA_OUTPUT_OK &&
         callback_ok);
    NEED(mc_compressed_output_source_close(o, &e) == MC_JAVA_OUTPUT_OK && callback_ok);
    RETIRE();

    /* A bounded tree rejection follows the real constructor/header. */
    OPEN("depth-limit.gz");
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_depth = 8;
    NEED(mc_compressed_output_write_compound(o, &root, &limits, &e) == MC_JAVA_OUTPUT_LIMIT &&
         !e.output.cause.source_exception && e.nbt.status == MC_NBT_LIMIT);
    NEED(mc_compressed_output_inspect(o, &cs) && cs.raw_bytes == 0 && cs.compressed_bytes == 10 &&
         !cs.closed && cs.source_close_calls == 0);
    NEED(mc_java_file_output_retain(fos, &oe) == MC_JAVA_OUTPUT_OK);
    alias = fos;
    NEED(mc_compressed_output_release(&o, &e) == MC_JAVA_OUTPUT_OK && o == NULL);
    NEED(mc_java_file_output_inspect(alias, &fs) && fs.native_open && !fs.source_close_reached);
    NEED(mc_java_file_output_release(&alias, &oe) == MC_JAVA_OUTPUT_OK && alias == NULL);
    RETIRE();

    OPEN("array-limit.gz");
    limits = mc_nbt_default_limits();
    limits.max_elements = 8;
    NEED(mc_compressed_output_write_compound(o, &large, &limits, &e) == MC_JAVA_OUTPUT_LIMIT &&
         !e.output.cause.source_exception);
    RETIRE();

    options.max_written_bytes = 10;
    OPEN("byte-limit.gz");
    NEED(mc_compressed_output_write_compound(o, &large, NULL, &e) == MC_JAVA_OUTPUT_LIMIT &&
         !e.output.cause.source_exception);
    NEED(mc_compressed_output_inspect(o, &cs) && cs.compressed_bytes == 10 && !cs.closed &&
         cs.source_close_calls == 0);
    RETIRE();
    options.max_written_bytes = 4194304;

    OPEN("cycle.gz");
    mc_nbt_tag cycle = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *cycle_items[] = {&cycle};
    cycle.value.compound.items = cycle_items;
    cycle.value.compound.length = 1;
    NEED(mc_compressed_output_write_compound(o, &cycle, NULL, &e) == MC_JAVA_OUTPUT_STATE &&
         e.nbt.status == MC_NBT_INVALID_TREE && !e.output.cause.source_exception);
    RETIRE();

    OPEN("write-fault.gz");
    armed = write_fault = true;
    observed_tree = &large;
    NEED(mc_compressed_output_write_compound(o, &large, NULL, &e) == MC_JAVA_OUTPUT_SOURCE_IO &&
         e.output.cause.source_exception && e.output.committed == 3 && callback_ok);
    armed = write_fault = false;
    NEED(mc_compressed_output_inspect(o, &cs) && !cs.closed && cs.raw_bytes > 0 && cs.crc != 0 &&
         cs.compressed_bytes == 13);
    /* Same borrowed array stays live through the actual enclosing close. */
    NEED(mc_compressed_output_source_close(o, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_compressed_output_inspect(o, &cs) && cs.closed && cs.file_closed &&
         cs.source_close_calls == 1);
    RETIRE();

    OPEN("finish-fault.gz");
    NEED(mc_compressed_output_write_compound(o, &root, NULL, &e) == MC_JAVA_OUTPUT_OK);
    armed = write_fault = true;
    const size_t finish_closes = actual_closes;
    NEED(mc_compressed_output_source_close(o, &e) == MC_JAVA_OUTPUT_SOURCE_IO &&
         e.output.cause.source_exception && callback_ok);
    armed = write_fault = false;
    NEED(mc_compressed_output_inspect(o, &cs) && cs.closed && cs.deflater_ended &&
         !cs.gzip_closed && !cs.file_closed);
    NEED(mc_java_file_output_inspect(fos, &fs) && fs.native_open && !fs.source_close_reached);
    NEED(mc_compressed_output_source_close(o, &e) == MC_JAVA_OUTPUT_OK &&
         actual_closes == finish_closes);
    RETIRE();

    OPEN("close-fault.gz");
    NEED(mc_compressed_output_write_compound(o, &root, NULL, &e) == MC_JAVA_OUTPUT_OK);
    armed = close_fault = true;
    const size_t source_closes = actual_closes;
    NEED(mc_compressed_output_source_close(o, &e) == MC_JAVA_OUTPUT_SOURCE_IO &&
         e.output.cause.source_exception && e.output.cause.consumed && callback_ok);
    armed = close_fault = false;
    NEED(mc_compressed_output_inspect(o, &cs) && cs.closed && cs.finished && cs.file_closed &&
         !cs.gzip_closed && cs.deflater_ended);
    NEED(mc_java_file_output_inspect(fos, &fs) && !fs.native_open && fs.source_closed &&
         !fs.source_close_succeeded && actual_closes == source_closes + 1);
    NEED(mc_compressed_output_source_close(o, &e) == MC_JAVA_OUTPUT_OK &&
         actual_closes == source_closes + 1);
    RETIRE();

    /* Final native cleanup consumes one close right, then retries without IO. */
    OPEN("native-close-fault.gz");
    NEED(mc_compressed_output_release(&o, &e) == MC_JAVA_OUTPUT_OK && o == NULL);
    observed = NULL;
    armed = close_fault = true;
    const size_t cleanup_closes = actual_closes;
    NEED(mc_java_file_output_release(&fos, &oe) == MC_JAVA_OUTPUT_NATIVE_FAILURE && fos != NULL &&
         !oe.cause.source_exception && oe.cause.consumed && callback_ok);
    armed = close_fault = false;
    NEED(mc_java_file_output_inspect(fos, &fs) && fs.phase == MC_JAVA_OUTPUT_RETIRING &&
         !fs.native_open && !fs.source_close_reached && actual_closes == cleanup_closes + 1);
    RETIRE();
    NEED(actual_closes == cleanup_closes + 1);
    result = 0;
done:
    armed = write_fault = close_fault = false;
    observed = NULL;
    observed_file = NULL;
    if (mc_compressed_output_release(&o, &e) != MC_JAVA_OUTPUT_OK)
        result = 1;
    if (mc_java_file_output_release(&alias, &oe) != MC_JAVA_OUTPUT_OK)
        result = 1;
    if (mc_java_file_output_release(&fos, &oe) != MC_JAVA_OUTPUT_OK)
        result = 1;
    if (mc_java_file_release(&file, &fe) != MC_JAVA_FILE_OK)
        result = 1;
    if (mc_java_file_release(&parent, &fe) != MC_JAVA_FILE_OK)
        result = 1;
    mc_world_info_name_release(leaf);
    mc_world_info_name_release(parent_name);
    if (mc_java_files_release(&files, &fe) != MC_JAVA_FILE_OK)
        result = 1;
    free(random);
    free(long_text);
    if (result == 0)
        printf("COMPRESSED_OUTPUT_OK checks=%u writes=%zu closes=%zu\n", checks, actual_writes,
               actual_closes);
    return result;
}
