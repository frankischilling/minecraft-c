#include "mc/world/storage/chunk.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PROBE_BYTES = 2 * 1024 * 1024 };

static const char *transport_status(mc_stored_chunk_status status) {
    /* This reader's existing transport retains its recorded status spelling;
     * the current diagnostic API uses the canonical TileEntity name. */
    return status == MC_STORED_CHUNK_UNSUPPORTED_EDIT ? "unsupported entity or tile edit"
                                                      : mc_stored_chunk_status_string(status);
}

static int digit(int value) {
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    return -1;
}

static bool hex_bytes(const char *text, uint8_t *out, size_t length) {
    if (strlen(text) != length * 2u)
        return false;
    for (size_t i = 0; i < length; ++i) {
        int high = digit((unsigned char)text[i * 2u]);
        int low = digit((unsigned char)text[i * 2u + 1u]);
        if (high < 0 || low < 0)
            return false;
        out[i] = (uint8_t)(high * 16 + low);
    }
    return true;
}

static bool time_bits(const char *text, int64_t *out) {
    uint8_t bytes[8];
    if (!hex_bytes(text, bytes, sizeof(bytes)))
        return false;
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i)
        value = value * UINT64_C(256) + bytes[i];
    *out = value <= INT64_MAX ? (int64_t)value : -1 - (int64_t)(UINT64_MAX - value);
    return true;
}

static void print_hex(const uint8_t *bytes, size_t length) {
    for (size_t i = 0; i < length; ++i)
        printf("%02x", (unsigned int)bytes[i]);
}

static void array(const char *name, const uint8_t *bytes, size_t length) {
    printf("\"%s\":{\"length\":%zu,\"values\":\"", name, length);
    print_hex(bytes, length);
    fputs("\"}", stdout);
}

static const mc_nbt_tag *field(const mc_nbt_tag *parent, const char *name) {
    uint16_t units[16];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(parent, units, length);
}

static bool opaque_records(const mc_stored_chunk *owner) {
    const mc_nbt_tag *level = field(mc_stored_chunk_root(owner), "Level");
    const mc_nbt_tag *entities = field(level, "Entities");
    const mc_nbt_tag *tile_entities = field(level, "TileEntities");
    return (entities != NULL && entities->value.list.length != 0) ||
           (tile_entities != NULL && tile_entities->value.list.length != 0);
}

static void state(const mc_chunk_state *value) {
    const mc_chunk_data *data = value->data;
    printf("{\"coordinates\":[%" PRId32 ",%" PRId32 "],\"blank\":%s,\"flags\":{\"c\":%s,\"n\":%s,"
           "\"o\":%s,\"p\":%s,\"q\":%s},\"last_save_time\":\"%016" PRIx64 "\",\"data\":{",
           value->x, value->z, value->blank ? "true" : "false", value->active ? "true" : "false",
           value->terrain_populated ? "true" : "false", value->dirty ? "true" : "false",
           value->never_save ? "true" : "false", value->has_entities ? "true" : "false",
           (uint64_t)value->last_save_time);
    array("blocks", data->blocks, MC_CHUNK_BLOCKS);
    fputc(',', stdout);
    array("metadata", data->metadata, sizeof(data->metadata));
    fputc(',', stdout);
    array("skylight", data->skylight, sizeof(data->skylight));
    fputc(',', stdout);
    array("block_light", data->block_light, sizeof(data->block_light));
    fputc(',', stdout);
    array("heightmap", data->heightmap, sizeof(data->heightmap));
    printf(",\"minimum_height\":%" PRId32 "}}", data->minimum_height);
}

int main(int argc, char **argv) {
    bool build = argc == 6 && strcmp(argv[1], "build") == 0;
    if (!build && !(argc == 3 && strcmp(argv[1], "decode") == 0)) {
        fputs("usage: chunk_probe decode MAP|- | build TIME_HEX MAP|- keep|fresh "
              "none|x|block|population|q\n",
              stderr);
        return 2;
    }
    const char *map_text = argv[build ? 3 : 2];
    uint8_t map[256];
    bool normalize = strcmp(map_text, "-") != 0;
    if (normalize && !hex_bytes(map_text, map, sizeof(map)))
        return 2;
    int64_t world_time = 0;
    if (build && (!time_bits(argv[2], &world_time) ||
                  (strcmp(argv[4], "keep") != 0 && strcmp(argv[4], "fresh") != 0) ||
                  (strcmp(argv[5], "none") != 0 && strcmp(argv[5], "x") != 0 &&
                   strcmp(argv[5], "block") != 0 && strcmp(argv[5], "population") != 0 &&
                   strcmp(argv[5], "q") != 0)))
        return 2;
    uint8_t *input = malloc(PROBE_BYTES);
    if (input == NULL)
        return 2;
    size_t length = 0;
    int high;
    while ((high = getchar()) != EOF && high != '\n') {
        int low = digit(getchar());
        high = digit(high);
        if (high < 0 || low < 0 || length == PROBE_BYTES) {
            free(input);
            return 2;
        }
        input[length++] = (uint8_t)(high * 16 + low);
    }
    if (ferror(stdin) || (high == '\n' && getchar() != EOF)) {
        free(input);
        return 2;
    }
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_input_bytes = limits.max_output_bytes = PROBE_BYTES;
    limits.max_allocation_bytes = 64u * 1024u * 1024u;
    mc_stored_chunk *owner = NULL;
    mc_stored_chunk_error error = {0};
    size_t consumed = 0;
    mc_stored_chunk_status status =
        mc_stored_chunk_decode(input, length, &limits, &owner, &consumed, &error);
    free(input);
    if (status != MC_STORED_CHUNK_OK) {
        printf("{\"status\":\"%s\",\"field\":", transport_status(status));
        if (error.field == NULL)
            fputs("null", stdout);
        else
            printf("\"%s\"", error.field);
        printf(",\"nbt_status\":\"%s\"}\n", mc_nbt_status_string(error.nbt.status));
        return ferror(stdout) ? 2 : 0;
    }
    if (consumed != length) {
        mc_stored_chunk_free(owner);
        puts("{\"status\":\"trailing input\"}");
        return ferror(stdout) ? 2 : 0;
    }
    if (normalize) {
        status = mc_stored_chunk_normalize(owner, map, sizeof(map));
        if (status != MC_STORED_CHUNK_OK) {
            mc_stored_chunk_free(owner);
            printf("{\"status\":\"%s\"}\n", transport_status(status));
            return ferror(stdout) ? 2 : 0;
        }
    }
    mc_stored_chunk *fresh = NULL;
    if (build && strcmp(argv[4], "fresh") == 0) {
        if (opaque_records(owner)) {
            mc_stored_chunk_free(owner);
            puts("{\"status\":\"unsupported entity or tile edit\"}");
            return ferror(stdout) ? 2 : 0;
        }
        status = mc_stored_chunk_create(mc_stored_chunk_state(owner), &limits, &fresh, &error);
        if (status != MC_STORED_CHUNK_OK) {
            mc_stored_chunk_free(owner);
            printf("{\"status\":\"%s\"}\n", transport_status(status));
            return ferror(stdout) ? 2 : 0;
        }
    }
    mc_stored_chunk *selected = fresh == NULL ? owner : fresh;
    mc_chunk_state *view = mc_stored_chunk_state(selected);
    if (build) {
        if (strcmp(argv[5], "x") == 0)
            view->x = view->x == INT32_MAX ? INT32_MIN : view->x + 1;
        else if (strcmp(argv[5], "block") == 0)
            view->data->blocks[0] ^= UINT8_C(1);
        else if (strcmp(argv[5], "population") == 0)
            view->terrain_populated = !view->terrain_populated;
        else if (strcmp(argv[5], "q") == 0)
            view->has_entities = !view->has_entities;
    }
    mc_nbt_document *document = NULL;
    uint8_t *encoded = NULL;
    size_t written = 0;
    if (build) {
        mc_chunk_state before = *view;
        mc_chunk_data before_data = *view->data;
        uint8_t *before_blocks = malloc(MC_CHUNK_BLOCKS);
        if (before_blocks == NULL) {
            mc_stored_chunk_free(fresh);
            mc_stored_chunk_free(owner);
            return 2;
        }
        memcpy(before_blocks, view->data->blocks, MC_CHUNK_BLOCKS);
        status = mc_stored_chunk_build(selected, world_time, &limits, &document, &error);
        bool preserved =
            view->data == before.data && view->x == before.x && view->z == before.z &&
            view->blank == before.blank && view->active == before.active &&
            view->dirty == before.dirty && view->terrain_populated == before.terrain_populated &&
            view->never_save == before.never_save && view->has_entities == before.has_entities &&
            view->last_save_time == before.last_save_time &&
            view->data->blocks == before_data.blocks &&
            view->data->blocks_capacity == before_data.blocks_capacity &&
            view->data->minimum_height == before_data.minimum_height &&
            memcmp(view->data->metadata, before_data.metadata, sizeof(before_data.metadata)) == 0 &&
            memcmp(view->data->skylight, before_data.skylight, sizeof(before_data.skylight)) == 0 &&
            memcmp(view->data->block_light, before_data.block_light,
                   sizeof(before_data.block_light)) == 0 &&
            memcmp(view->data->heightmap, before_data.heightmap, sizeof(before_data.heightmap)) ==
                0 &&
            memcmp(view->data->blocks, before_blocks, MC_CHUNK_BLOCKS) == 0;
        free(before_blocks);
        if (!preserved) {
            mc_nbt_document_free(document);
            mc_stored_chunk_free(fresh);
            mc_stored_chunk_free(owner);
            return 2;
        }
        if (status == MC_STORED_CHUNK_OK) {
            mc_nbt_error nbt;
            size_t measured = 0;
            mc_nbt_status result =
                mc_nbt_measure(mc_nbt_document_root(document), &limits, &measured, &nbt);
            if (result == MC_NBT_OK) {
                encoded = malloc(measured);
                if (encoded != NULL)
                    result = mc_nbt_write(mc_nbt_document_root(document), &limits, encoded,
                                          measured, &written, &nbt);
            }
            if (result != MC_NBT_OK || encoded == NULL) {
                free(encoded);
                mc_nbt_document_free(document);
                mc_stored_chunk_free(fresh);
                mc_stored_chunk_free(owner);
                return 2;
            }
        }
    }
    printf("{\"status\":\"%s\",\"consumed\":%zu,\"chunk\":", transport_status(status), consumed);
    state(view);
    fputs(",\"nbt_hex\":", stdout);
    if (encoded == NULL)
        fputs("null", stdout);
    else {
        fputc('"', stdout);
        print_hex(encoded, written);
        fputc('"', stdout);
    }
    fputs("}\n", stdout);
    free(encoded);
    mc_nbt_document_free(document);
    mc_stored_chunk_free(fresh);
    mc_stored_chunk_free(owner);
    return ferror(stdout) ? 2 : 0;
}
