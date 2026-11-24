#include "mc/world/storage/source_handler.h"
#include "platform/time/millis.h"
#include "world/runtime/source_terminal_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static mc_world_store *observed_store;
static mc_world_store **observed_slot;
static mc_world_store_source_classes *observed_classes;
static const mc_world_info_record *observed_record;
static mc_world_source_error *callback_error;
static bool armed, close_fault, fault_fired, reentry_ok = true;
static bool write_fault, header_fault, write_fault_fired, pending_write_error;
static size_t fault_requested, fault_committed;
static size_t writes, closes, reentries;

#define REQUIRE(value)                                                                             \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "metadata output line%d check%u: %s\n", __LINE__, checks, #value);     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static void reentry(void) {
    if (!armed)
        return;
    ++reentries;
    mc_world_store *store = observed_store;
    mc_world_store_source_classes *classes = observed_classes;
    mc_world_source_output_state view;
    mc_world_storage_error admission;
    const mc_world_terminal_store_anchor anchor = {observed_slot};
    if (mc_world_store_terminal_source_admit(store, &anchor, 1, &admission) !=
            MC_WORLD_STORAGE_BUSY ||
        mc_world_store_source_retain(store, callback_error) != MC_WORLD_SOURCE_BUSY ||
        mc_world_store_source_release(&store, callback_error) != MC_WORLD_SOURCE_BUSY ||
        store != observed_store ||
        mc_world_store_source_classes_release(&classes, callback_error) != MC_WORLD_SOURCE_BUSY ||
        classes != observed_classes ||
        mc_world_store_source_output_clear(store, callback_error) != MC_WORLD_SOURCE_BUSY ||
        mc_world_store_source_write_info_record(store, observed_record, callback_error) !=
            MC_WORLD_SOURCE_BUSY ||
        mc_world_store_source_output_inspect(store, &view))
        reentry_ok = false;
}

/* The canonical native primitive TU is linked under these controls. Real IO
 * runs first. The declared close fault preserves its consumed native handle. */
size_t mc_test_output_write(const void *data, size_t size, size_t count, FILE *file) {
    const bool failing =
        armed && write_fault && !write_fault_fired && (header_fault || writes != 0);
    const size_t result = fwrite(data, size, failing && count > 3 ? 3 : count, file);
    if (armed)
        ++writes;
    reentry();
    if (failing) {
        write_fault_fired = pending_write_error = true;
        fault_requested = count;
        fault_committed = result;
        errno = EIO;
    }
    return result;
}

int mc_test_output_ferror(FILE *file) {
    const int result = ferror(file);
    if (pending_write_error) {
        pending_write_error = false;
        return 1;
    }
    return result;
}

int mc_test_output_close(FILE *file) {
    const int result = fclose(file);
    if (armed)
        ++closes;
    reentry();
    if (armed && close_fault && !fault_fired) {
        fault_fired = true;
        errno = EIO;
        return EOF;
    }
    return result;
}

static mc_world_info_name *text(const char *ascii) {
    const size_t length = strlen(ascii);
    if (length > 4096)
        return NULL;
    uint16_t units[4096];
    for (size_t i = 0; i < length; ++i) {
        if ((unsigned char)ascii[i] >= 128)
            return NULL;
        units[i] = (unsigned char)ascii[i];
    }
    mc_world_info_name *name = NULL;
    return mc_world_info_name_copy((mc_nbt_string){units, length}, 1048576, &name) ==
                   MC_WORLD_INFO_OK
               ? name
               : NULL;
}

static void named(mc_nbt_tag *tag, uint16_t units[16], const char *key, mc_nbt_type type) {
    const size_t length = strlen(key);
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)key[i];
    tag->name = (mc_nbt_string){units, length};
    tag->type = type;
}

static int make_info(mc_world_info **out, bool second, bool utf_limit) {
    static const char *const keys[] = {"RandomSeed", "SpawnX",     "SpawnY",     "SpawnZ",
                                       "Time",       "LastPlayed", "SizeOnDisk", "LevelName",
                                       "version",    "rainTime",   "raining",    "thunderTime",
                                       "thundering", "Player",     "Unknown"};
    static const mc_nbt_type types[] = {MC_NBT_LONG, MC_NBT_INT,      MC_NBT_INT,   MC_NBT_INT,
                                        MC_NBT_LONG, MC_NBT_LONG,     MC_NBT_LONG,  MC_NBT_STRING,
                                        MC_NBT_INT,  MC_NBT_INT,      MC_NBT_BYTE,  MC_NBT_INT,
                                        MC_NBT_BYTE, MC_NBT_COMPOUND, MC_NBT_STRING};
    static const uint16_t first_name[] = {70, 105, 114, 115, 116, 0, 937, 55357, 56832};
    static const uint16_t second_name[] = {83, 101, 99, 111, 110, 100, 0, 937, 55357, 56832};
    static const uint16_t ignored[] = {'i', 'g', 'n', 'o', 'r', 'e', 'd'};
    static const uint16_t dimension[] = {'D', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n'},
                          keep[] = {'K', 'e', 'e', 'p'};
    mc_nbt_tag tags[15] = {0};
    const mc_nbt_tag *items[15];
    uint16_t names[15][16];
    for (size_t i = 0; i < 15; ++i) {
        named(&tags[i], names[i], keys[i], types[i]);
        items[i] = &tags[i];
    }
    tags[0].value.long_value = -INT64_C(9223372036854775807);
    tags[1].value.int_value = -17;
    tags[2].value.int_value = 65;
    tags[3].value.int_value = 1003;
    tags[4].value.long_value = second ? -INT64_C(1234567890123) : INT64_C(23999);
    tags[5].value.long_value = -5;
    tags[6].value.long_value = INT64_C(9876543210);
    tags[7].value.string = (mc_nbt_string){second ? second_name : first_name, second ? 10u : 9u};
    tags[8].value.int_value = -12;
    tags[9].value.int_value = -345;
    tags[10].value.byte_value = 1;
    tags[11].value.int_value = 678;
    tags[12].value.byte_value = 0;
    mc_nbt_tag player_tags[] = {{.type = MC_NBT_INT, .name = {dimension, 9}, .value.int_value = -1},
                                {.type = MC_NBT_INT, .name = {keep, 4}, .value.int_value = 42}};
    const mc_nbt_tag *player_items[] = {&player_tags[0], &player_tags[1]};
    tags[13].value.compound.items = player_items;
    tags[13].value.compound.length = 2;
    tags[14].value.string = (mc_nbt_string){ignored, 7};
    uint16_t *large_name = NULL;
    if (utf_limit) {
        large_name = malloc(32768u * sizeof(*large_name));
        REQUIRE(large_name != NULL);
        for (size_t i = 0; i < 32768; ++i)
            large_name[i] = 'X';
        tags[7].value.string = (mc_nbt_string){large_name, 32768};
    }
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {items, 15}};
    mc_world_info_data *data = NULL;
    mc_world_info_error error;
    REQUIRE(mc_world_info_data_copy(&root, NULL, &data, &error) == MC_WORLD_INFO_OK);
    free(large_name);
    REQUIRE(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, out) == MC_WORLD_INFO_OK);
    REQUIRE(mc_world_info_construct_nbt(*out, data, &error) == MC_WORLD_INFO_OK);
    mc_world_info_data_release(data);
    /* This is the genuine mx version setter before the reached fm transport.
     * Player omission below uses the real prepared-record API, not a fake List. */
    REQUIRE(mc_world_info_version_set(*out, 19132) == MC_WORLD_INFO_OK);
    mc_world_info_fields fields;
    REQUIRE(mc_world_info_fields_get(*out, &fields) == MC_WORLD_INFO_OK && fields.player != NULL &&
            fields.value.last_played == -5 && fields.value.version == 19132);
    return 0;
}

static int deep_player(mc_world_info_player **out) {
    /* Player alone has63 tag levels and fits its existing copy limit64.
     * The real output root/Data wrappers make the reached writer depth65. */
    mc_nbt_tag tags[63] = {0};
    const mc_nbt_tag *links[62];
    static const uint16_t key[] = {'x'};
    for (size_t i = 0; i < 63; ++i) {
        tags[i].name = (mc_nbt_string){key, 1};
        if (i != 62) {
            links[i] = &tags[i + 1];
            tags[i].type = MC_NBT_COMPOUND;
            tags[i].value.compound.items = &links[i];
            tags[i].value.compound.length = 1;
        } else {
            tags[i].type = MC_NBT_BYTE;
            tags[i].value.byte_value = 7;
        }
    }
    mc_world_info_error error;
    REQUIRE(mc_world_info_player_copy(tags, NULL, out, &error) == MC_WORLD_INFO_OK);
    return 0;
}

static int array_player(mc_world_info_player **out) {
    /* Authored selected Player payload, not a LocalPlayer/List serializer. */
    uint8_t *bytes = malloc(65536);
    REQUIRE(bytes != NULL);
    uint32_t rng = UINT32_C(0x6d2b79f5);
    for (size_t i = 0; i < 65536; ++i) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        bytes[i] = (uint8_t)rng;
    }
    static const uint16_t name[] = {'B', 'y', 't', 'e', 's'};
    const mc_nbt_tag array = {
        .type = MC_NBT_BYTE_ARRAY, .name = {name, 5}, .value.byte_array = {bytes, 65536}};
    const mc_nbt_tag *items[] = {&array};
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {items, 1}};
    mc_world_info_error error;
    const mc_world_info_status status = mc_world_info_player_copy(&root, NULL, out, &error);
    free(bytes);
    REQUIRE(status == MC_WORLD_INFO_OK);
    return 0;
}

static int check_rotation(const char *mode, const mc_world_source_output_state *r) {
    REQUIRE(r->source_returned && r->last_pc == 150 && r->compressed_pc == 35 &&
            r->close_pc == 22 && r->root_constructed && r->data_attached && r->file_constructed &&
            r->compressed_constructed && r->nbt_written && r->closed);
    REQUIRE(r->old_exists_called && r->old_rename_called && r->current_exists_called &&
            r->new_rename_called && r->new_exists_called);
    if (strcmp(mode, "repeat") == 0) {
        REQUIRE(!r->old_exists && !r->old_delete_called && r->old_renamed && !r->current_exists &&
                !r->current_delete_called && r->new_renamed && !r->new_exists &&
                !r->new_delete_called);
    } else if (strcmp(mode, "second") == 0 || strcmp(mode, "existing-first") == 0) {
        REQUIRE(r->old_exists && r->old_delete_called && r->old_deleted && r->old_renamed &&
                !r->current_exists && !r->current_delete_called && r->new_renamed &&
                !r->new_exists && !r->new_delete_called);
    } else if (strcmp(mode, "old-obstruction") == 0) {
        REQUIRE(r->old_exists && r->old_delete_called && !r->old_deleted && !r->old_renamed &&
                r->current_exists && r->current_delete_called && r->current_deleted &&
                r->new_renamed && !r->new_exists && !r->new_delete_called);
    } else if (strcmp(mode, "both-obstructions") == 0) {
        REQUIRE(r->old_exists && r->old_delete_called && !r->old_deleted && !r->old_renamed &&
                r->current_exists && r->current_delete_called && !r->current_deleted &&
                !r->new_renamed && r->new_exists && r->new_delete_called && r->new_deleted);
    } else {
        REQUIRE(!r->old_exists && !r->old_delete_called && !r->old_renamed && !r->current_exists &&
                !r->current_delete_called && r->new_renamed && !r->new_exists &&
                !r->new_delete_called);
    }
    return 0;
}

static int terminal_checks(mc_world_store **slot) {
    mc_world_store *store = *slot;
    mc_world_source_output_state before, after;
    REQUIRE(mc_world_store_source_output_inspect(store, &before));
    mc_java_file_output_state old_fos = {0}, new_fos = {0};
    if (before.file_output != NULL)
        REQUIRE(mc_java_file_output_inspect(before.file_output, &old_fos));
    const size_t old_writes = writes, old_closes = closes;
    mc_world_storage_error error;
    const mc_world_terminal_store_anchor one = {slot};
    REQUIRE(mc_world_store_terminal_source_admit(store, &one, 1, &error) == MC_WORLD_STORAGE_OK);
    mc_world_store *extra = store;
    REQUIRE(mc_world_store_source_retain(extra, callback_error) == MC_WORLD_SOURCE_OK);
    REQUIRE(mc_world_store_terminal_source_admit(store, &one, 1, &error) == MC_WORLD_STORAGE_BUSY);
    const mc_world_terminal_store_anchor two[] = {{slot}, {&extra}};
    REQUIRE(mc_world_store_terminal_source_admit(store, two, 2, &error) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_world_store_source_release(&extra, callback_error) == MC_WORLD_SOURCE_OK &&
            extra == NULL);
    const mc_world_terminal_store_anchor repeated[] = {{slot}, {slot}};
    REQUIRE(mc_world_store_terminal_source_admit(store, repeated, 2, &error) ==
            MC_WORLD_STORAGE_INVALID_ARGUMENT);
    /* Declared invalid native graph inputs: neither alias below owns a hold.
     * Preflight must reject them before inspecting or changing descendants. */
    mc_world_store *unheld = store, *foreign = NULL;
    const mc_world_terminal_store_anchor deficit[] = {{slot}, {&unheld}}, bad = {&foreign};
    REQUIRE(mc_world_store_terminal_source_admit(store, deficit, 2, &error) ==
            MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    REQUIRE(mc_world_store_terminal_source_admit(store, &bad, 1, &error) ==
            MC_WORLD_STORAGE_INVALID_ARGUMENT);
    REQUIRE(mc_world_store_terminal_source_admit(store, &one, 1, &error) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_world_store_source_output_inspect(store, &after));
    REQUIRE(before.new_file == after.new_file && before.old_file == after.old_file &&
            before.current_file == after.current_file && before.file_output == after.file_output &&
            before.compressed_output == after.compressed_output &&
            before.last_pc == after.last_pc && before.close_pc == after.close_pc &&
            before.closed == after.closed && before.source_returned == after.source_returned &&
            writes == old_writes && closes == old_closes);
    if (after.file_output != NULL) {
        REQUIRE(mc_java_file_output_inspect(after.file_output, &new_fos));
        REQUIRE(old_fos.phase == new_fos.phase && old_fos.holds == new_fos.holds &&
                old_fos.bytes_committed == new_fos.bytes_committed &&
                old_fos.source_close_calls == new_fos.source_close_calls &&
                old_fos.native_close_calls == new_fos.native_close_calls &&
                old_fos.native_open == new_fos.native_open);
    }
    return 0;
}

int main(int argc, char **argv) {
    REQUIRE(argc == 3);
    const char *mode = argv[2];
    const bool cleanup_failure = strcmp(mode, "cleanup-failure") == 0;
    const bool codec_limit = strcmp(mode, "codec-limit") == 0 || cleanup_failure;
    const bool gzip_limit = strcmp(mode, "gzip-limit") == 0;
    const bool utf_limit = strcmp(mode, "utf-limit") == 0;
    const bool finish_limit = strcmp(mode, "finish-limit") == 0;
    const int literal_limit =
        strncmp(mode, "literal-", 8) == 0 && mode[8] >= '4' && mode[8] <= '7' && mode[9] == 0
            ? mode[8] - '0'
            : 0;
    const bool open_failure = strcmp(mode, "open-failure") == 0;
    const bool close_failure = strcmp(mode, "close-failure") == 0;
    const bool header_failure = strcmp(mode, "header-failure") == 0;
    const bool finally_close_failure = strcmp(mode, "write-finally-close") == 0;
    const bool write_failure = strcmp(mode, "write-finally") == 0 || finally_close_failure;
    const bool array_selected = write_failure || strcmp(mode, "player-array") == 0;
    const bool new_fault_control = header_failure || write_failure;
    const bool retained = strcmp(mode, "retained") == 0 || (codec_limit && !cleanup_failure);
    const bool second = strcmp(mode, "second") == 0 || strcmp(mode, "old-obstruction") == 0;
    const bool both = strcmp(mode, "both-obstructions") == 0;
    const bool repeat = strcmp(mode, "repeat") == 0;
    REQUIRE(codec_limit || gzip_limit || utf_limit || finish_limit || literal_limit != 0 ||
            open_failure || close_failure || new_fault_control || array_selected || retained ||
            second || both || repeat || strcmp(mode, "existing-first") == 0 ||
            strcmp(mode, "null-first") == 0 || strcmp(mode, "first") == 0);
    mc_world_source_error *error = calloc(1, sizeof(*error));
    callback_error = calloc(1, sizeof(*callback_error));
    REQUIRE(error != NULL && callback_error != NULL);
    mc_java_files *files = NULL;
    mc_java_file_error fe;
    REQUIRE(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    mc_java_file *root = NULL;
    mc_world_info_name *root_name = text(argv[1]), *folder = text("world");
    REQUIRE(root_name != NULL && folder != NULL);
    REQUIRE(mc_java_file_allocate(files, &root, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_construct_string(root, root_name, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(root_name);
    mc_java_logging *logging = NULL;
    mc_java_logging_error le;
    REQUIRE(mc_java_logging_create(NULL, &logging, &le) == MC_JAVA_LOGGING_OK);
    mc_world_store_source_classes *classes = NULL;
    mc_world_source_options options = mc_world_store_source_default_options();
    if (literal_limit != 0)
        options.max_allocations = (size_t)literal_limit;
    if (gzip_limit)
        options.max_owner_bytes = 200000;
    if (utf_limit)
        options.max_owner_bytes = 327680;
    if (finish_limit)
        options.output.max_written_bytes = 10;
    REQUIRE(mc_world_store_source_classes_create(files, logging, &options, &classes, error) ==
            MC_WORLD_SOURCE_OK);
    mc_world_store *store = NULL;
    REQUIRE(mc_world_store_source_allocate(classes, &store, error) == MC_WORLD_SOURCE_OK);
    REQUIRE(mc_world_store_source_construct(store, root, folder, false, error) ==
            MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    mc_world_info *info = NULL;
    REQUIRE(make_info(&info, second, utf_limit) == 0);
    const int64_t time = second ? -INT64_C(1234567890123) : INT64_C(23999);
    mc_world_info_player *player = NULL;
    if (codec_limit)
        REQUIRE(deep_player(&player) == 0);
    if (array_selected)
        REQUIRE(array_player(&player) == 0);
    mc_world_info_record *record = NULL;
    mc_world_info_error ie;
    int64_t first, last;
    REQUIRE(mc_platform_current_time_millis(&first) == MC_PLATFORM_MILLIS_OK);
    REQUIRE(mc_world_info_record_create_with_player(info, player, &record, &ie) ==
            MC_WORLD_INFO_OK);
    REQUIRE(mc_platform_current_time_millis(&last) == MC_PLATFORM_MILLIS_OK && first <= last);
    static const uint16_t played[] = {'L', 'a', 's', 't', 'P', 'l', 'a', 'y', 'e', 'd'};
    const mc_nbt_tag *clock = mc_nbt_compound_get(mc_world_info_record_root(record), played, 10);
    REQUIRE(clock != NULL && clock->type == MC_NBT_LONG && clock->value.long_value >= first &&
            clock->value.long_value <= last);
    const int64_t last_played = clock->value.long_value;
    observed_store = store;
    observed_slot = &store;
    observed_classes = classes;
    observed_record = record;
    close_fault = close_failure || finally_close_failure;
    write_fault = new_fault_control;
    header_fault = header_failure;
    armed = true;
    mc_world_source_status status = mc_world_store_source_write_info_record(store, record, error);
    armed = false;
    REQUIRE(reentry_ok);
    mc_world_source_output_state r;
    REQUIRE(mc_world_store_source_output_inspect(store, &r));
    if (repeat) {
        REQUIRE(status == MC_WORLD_SOURCE_OK && check_rotation("first", &r) == 0 && closes == 1);
        REQUIRE(terminal_checks(&store) == 0);
        armed = true;
        status = mc_world_store_source_write_info_record(store, record, error);
        armed = false;
        REQUIRE(reentry_ok && mc_world_store_source_output_inspect(store, &r));
    }
    mc_java_file_output_state fos = {0};
    mc_compressed_output_state compressed = {0};
    if (r.file_output != NULL)
        REQUIRE(mc_java_file_output_inspect(r.file_output, &fos));
    if (literal_limit != 0) {
        REQUIRE(status == MC_WORLD_SOURCE_LIMIT && error->native_policy &&
                !error->source_exception && !r.source_returned && !r.old_exists_called &&
                r.file_output == NULL && r.compressed_output == NULL && writes == 0 &&
                closes == 0 && reentries == 0);
        REQUIRE(r.root_constructed && r.data_attached == (literal_limit > 4));
        const int expected_pc[] = {17, 31, 46, 61};
        REQUIRE(r.last_pc == expected_pc[literal_limit - 4]);
        mc_java_file *prefix[] = {r.new_file, r.old_file, r.current_file};
        for (int i = 0; i < 3; ++i) {
            if (i < literal_limit - 4) {
                mc_java_file_state state;
                REQUIRE(prefix[i] != NULL &&
                        mc_java_file_read_state(prefix[i], &state, &fe) == MC_JAVA_FILE_OK);
                REQUIRE(state.phase ==
                        (i == literal_limit - 5 ? MC_JAVA_FILE_RAW : MC_JAVA_FILE_READY));
            } else
                REQUIRE(prefix[i] == NULL);
        }
    } else if (open_failure) {
        REQUIRE(status == MC_WORLD_SOURCE_UNIMPLEMENTED && error->diagnostic_frontier &&
                error->caught_exception && error->source_exception && error->frames[0].pc == 147 &&
                error->cause_site.pc == 76 && !r.file_constructed && r.compressed_output == NULL &&
                !r.old_exists_called && !r.old_rename_called && !r.source_returned && writes == 0 &&
                closes == 0 && reentries == 0);
    } else {
        REQUIRE(mc_compressed_output_inspect(r.compressed_output, &compressed));
        REQUIRE(r.file_constructed && compressed.file == r.file_output);
        if (header_failure) {
            REQUIRE(status == MC_WORLD_SOURCE_UNIMPLEMENTED && error->diagnostic_frontier &&
                    error->source_exception && error->caught_exception && !error->native_policy &&
                    error->frame_count == 1 && error->frames[0].pc == 147 &&
                    error->cause_site.pc == 79 && error->prior_site.pc == 9 &&
                    r.compressed_pc == 9 && r.close_pc == -1 && !r.compressed_constructed &&
                    !r.nbt_written && !r.closed && !r.source_returned && !r.old_exists_called &&
                    !compressed.header_written && !compressed.data_output_constructed &&
                    compressed.phase == MC_JAVA_OUTPUT_FAILED && compressed.raw_bytes == 0 &&
                    compressed.compressed_bytes == 3 && compressed.source_close_calls == 0 &&
                    !compressed.finished && !compressed.deflater_ended && fos.native_open &&
                    !fos.source_close_reached && fos.bytes_committed == 3 && writes == 1 &&
                    closes == 0 && write_fault_fired && !fault_fired && fault_requested == 10 &&
                    fault_committed == 3 && !pending_write_error);
            REQUIRE(error->output.status == MC_JAVA_OUTPUT_SOURCE_IO &&
                    error->compressed.output.cause.source_exception &&
                    error->compressed.output.committed == 3 &&
                    strcmp(error->output.cause.site.method, "write") == 0 &&
                    error->output.cause.site.pc == 16 &&
                    error->compressed_close.output.status == MC_JAVA_OUTPUT_OK &&
                    error->output.suppressed.status == MC_JAVA_OUTPUT_OK);
        } else if (gzip_limit) {
            REQUIRE(status == MC_WORLD_SOURCE_LIMIT && error->native_policy &&
                    !error->source_exception && !compressed.header_written &&
                    !r.compressed_constructed && !r.nbt_written && !r.closed &&
                    !r.source_returned && !r.old_exists_called && fos.native_open &&
                    !fos.source_close_reached && fos.bytes_committed == 0 && writes == 0 &&
                    closes == 0);
        } else {
            REQUIRE(writes != 0 && reentries != 0 && r.compressed_constructed &&
                    compressed.header_written);
            if (write_failure) {
                REQUIRE(status == MC_WORLD_SOURCE_UNIMPLEMENTED && error->diagnostic_frontier &&
                        error->source_exception && error->caught_exception &&
                        !error->native_policy && error->frame_count == 1 &&
                        error->frames[0].pc == 147 && error->cause_site.pc == 79 &&
                        r.close_pc == 30 && !r.nbt_written && !r.source_returned &&
                        !r.old_exists_called && compressed.closed && compressed.finished &&
                        compressed.file_closed && compressed.deflater_ended &&
                        compressed.source_close_calls == 1 && fos.source_close_calls == 1 &&
                        fos.source_closed && !fos.native_open && fos.bytes_committed > 13 &&
                        compressed.compressed_bytes == fos.bytes_committed &&
                        compressed.raw_bytes >= 65536 && compressed.written_count >= 0 &&
                        (uint64_t)compressed.written_count < compressed.raw_bytes && writes > 2 &&
                        closes == 1 && write_fault_fired && fault_requested == 512 &&
                        fault_committed == 3 && !pending_write_error &&
                        error->output.status == MC_JAVA_OUTPUT_SOURCE_IO &&
                        error->output.suppressed.status == MC_JAVA_OUTPUT_OK &&
                        error->compressed.output.suppressed.status == MC_JAVA_OUTPUT_OK &&
                        error->compressed_close.output.suppressed.status == MC_JAVA_OUTPUT_OK);
                if (finally_close_failure) {
                    REQUIRE(fault_fired && !r.closed && !compressed.gzip_closed &&
                            !fos.source_close_succeeded && error->prior_site.pc == 30 &&
                            r.compressed_pc == 30 && error->output.cause.consumed &&
                            error->output.cause.site.pc == 65 &&
                            strcmp(error->output.cause.site.class_name,
                                   "java/io/FileOutputStream") == 0 &&
                            strcmp(error->output.cause.site.method, "close") == 0 &&
                            error->compressed_close.output.status == MC_JAVA_OUTPUT_SOURCE_IO &&
                            error->compressed_close.output.cause.consumed &&
                            error->compressed_close.output.cause.site.pc == 65);
                } else {
                    REQUIRE(!fault_fired && r.closed && compressed.gzip_closed &&
                            fos.source_close_succeeded && error->prior_site.pc == 18 &&
                            r.compressed_pc == 18 && !error->output.cause.consumed &&
                            error->output.committed == 3 && error->output.cause.site.pc == 16 &&
                            strcmp(error->output.cause.site.class_name,
                                   "java/io/FileOutputStream") == 0 &&
                            strcmp(error->output.cause.site.method, "write") == 0 &&
                            error->compressed_close.output.status == MC_JAVA_OUTPUT_OK);
                }
            } else if (codec_limit) {
                REQUIRE(status == MC_WORLD_SOURCE_LIMIT && error->native_policy &&
                        !error->source_exception && !error->caught_exception &&
                        error->compressed.nbt.status == MC_NBT_LIMIT && r.close_pc == -1 &&
                        !r.nbt_written && !r.closed && !r.source_returned && !r.old_exists_called &&
                        fos.native_open && !fos.source_close_reached && fos.bytes_committed == 10 &&
                        compressed.raw_bytes == 0 && compressed.written_count == 0 &&
                        compressed.source_close_calls == 0 && !compressed.finished && closes == 0);
            } else if (utf_limit) {
                REQUIRE(status == MC_WORLD_SOURCE_LIMIT && error->native_policy &&
                        !error->source_exception && error->compressed.output.cause.site.pc == 123 &&
                        strcmp(error->compressed.output.cause.site.method, "writeUTF") == 0 &&
                        !r.nbt_written && !r.closed && !r.source_returned && r.close_pc == -1 &&
                        !r.old_exists_called && fos.native_open && !fos.source_close_reached &&
                        compressed.raw_bytes > 0 && compressed.written_count > 0 &&
                        compressed.source_close_calls == 0 && closes == 0);
            } else if (finish_limit) {
                REQUIRE(status == MC_WORLD_SOURCE_LIMIT && error->native_policy &&
                        !error->source_exception && r.nbt_written && !r.closed &&
                        !r.source_returned && r.close_pc == 22 && !r.old_exists_called &&
                        fos.native_open && !fos.source_close_reached && fos.bytes_committed == 10 &&
                        compressed.closed && compressed.finished && !compressed.file_closed &&
                        !compressed.gzip_closed && !compressed.deflater_ended &&
                        compressed.source_close_calls == 1 && closes == 0);
            } else if (close_failure) {
                REQUIRE(status == MC_WORLD_SOURCE_UNIMPLEMENTED && error->diagnostic_frontier &&
                        error->source_exception && error->frames[0].pc == 147 &&
                        error->prior_site.pc == 22 && r.close_pc == 22 && r.nbt_written &&
                        !r.closed && !r.old_exists_called && !r.source_returned && fault_fired &&
                        closes == 1 && compressed.closed && compressed.finished &&
                        compressed.deflater_ended && !compressed.gzip_closed &&
                        compressed.file_closed && compressed.source_close_calls == 1 &&
                        fos.source_close_calls == 1 && fos.source_closed && !fos.native_open);
            } else {
                REQUIRE(status == MC_WORLD_SOURCE_OK && check_rotation(mode, &r) == 0 &&
                        closes == (repeat ? 2u : 1u));
                REQUIRE(compressed.closed && compressed.finished && compressed.gzip_closed &&
                        compressed.file_closed && compressed.deflater_ended &&
                        compressed.source_close_calls == 1 && fos.source_close_calls == 1 &&
                        fos.source_closed && !fos.native_open && fos.source_close_succeeded);
            }
        }
    }
    REQUIRE(terminal_checks(&store) == 0);
    const int source_prior_pc = error->prior_site.pc;
    char calls[9], values[9];
    (void)snprintf(calls, sizeof(calls), "%d%d%d%d%d%d%d%d", r.old_exists_called,
                   r.old_delete_called, r.old_rename_called, r.current_exists_called,
                   r.current_delete_called, r.new_rename_called, r.new_exists_called,
                   r.new_delete_called);
    (void)snprintf(values, sizeof(values), "%d%d%d%d%d%d%d%d", r.old_exists, r.old_deleted,
                   r.old_renamed, r.current_exists, r.current_deleted, r.new_renamed, r.new_exists,
                   r.new_deleted);
    mc_java_file_output *external_output = retained ? r.file_output : NULL;
    mc_java_file *external_file = retained ? r.new_file : NULL;
    mc_java_output_error oe;
    if (retained) {
        REQUIRE(mc_java_file_output_retain(external_output, &oe) == MC_JAVA_OUTPUT_OK);
        REQUIRE(mc_java_file_retain(external_file, &fe) == MC_JAVA_FILE_OK);
    }
    if (cleanup_failure) {
        close_fault = true;
        armed = true;
        const mc_world_source_status cleared = mc_world_store_source_output_clear(store, error);
        armed = false;
        REQUIRE(cleared != MC_WORLD_SOURCE_OK && error->native_policy && !error->source_exception &&
                !error->diagnostic_frontier && fault_fired && closes == 1);
        REQUIRE(mc_world_store_source_output_inspect(store, &r) && r.compressed_output == NULL &&
                r.file_output != NULL);
        REQUIRE(mc_java_file_output_inspect(r.file_output, &fos) && !fos.native_open &&
                fos.source_close_calls == 0 && fos.native_close_calls == 1);
        REQUIRE(terminal_checks(&store) == 0);
    }
    const size_t before_retry = closes;
    armed = cleanup_failure || new_fault_control;
    const mc_world_source_status cleared = mc_world_store_source_output_clear(store, error);
    armed = false;
    REQUIRE(cleared == MC_WORLD_SOURCE_OK && (!cleanup_failure || closes == before_retry));
    REQUIRE(reentry_ok &&
            (!new_fault_control || closes == before_retry + (header_failure ? 1u : 0u)));
    REQUIRE(!mc_world_store_source_output_inspect(store, &r));
    mc_world_info_record_release(record);
    mc_world_info_player_release(player);
    mc_world_info_release(info);
    REQUIRE(mc_world_store_source_retire(&store, error) == MC_WORLD_SOURCE_OK && store == NULL);
    REQUIRE(mc_world_store_source_classes_retire(&classes, error) == MC_WORLD_SOURCE_OK &&
            classes == NULL);
    if (retained) {
        REQUIRE(mc_java_file_output_inspect(external_output, &fos) &&
                fos.source_close_calls == (codec_limit ? 0u : 1u) &&
                fos.native_open == codec_limit);
        REQUIRE(mc_java_file_output_release(&external_output, &oe) == MC_JAVA_OUTPUT_OK &&
                external_output == NULL);
        mc_java_file_state file;
        REQUIRE(mc_java_file_read_state(external_file, &file, &fe) == MC_JAVA_FILE_OK &&
                file.phase == MC_JAVA_FILE_READY);
        REQUIRE(mc_java_file_release(&external_file, &fe) == MC_JAVA_FILE_OK &&
                external_file == NULL);
    }
    REQUIRE(mc_java_logging_retire(&logging, &le) == MC_JAVA_LOGGING_OK && logging == NULL);
    REQUIRE(mc_java_file_release(&root, &fe) == MC_JAVA_FILE_OK && root == NULL);
    REQUIRE(mc_java_files_release(&files, &fe) == MC_JAVA_FILE_OK && files == NULL);
    printf("METADATA mode=%s time=%lld last_played=%lld first=%lld last=%lld checks=%u writes=%zu "
           "closes=%zu reentries=%zu status=%d calls=%s values=%s raw_bytes=%llu "
           "compressed_bytes=%llu crc=%u written=%d write_fault=%d close_fault=%d prior_pc=%d "
           "close_pc=%d\n",
           mode, (long long)time, (long long)last_played, (long long)first, (long long)last, checks,
           writes, closes, reentries, (int)status, calls, values,
           (unsigned long long)compressed.raw_bytes,
           (unsigned long long)compressed.compressed_bytes, (unsigned)compressed.crc,
           compressed.written_count, write_fault_fired, fault_fired, source_prior_pc, r.close_pc);
    free(callback_error);
    free(error);
    return 0;
}
