#include "texture_fixture.inc"

enum { DB_ROWS = 160, DB_NAME = 128 };

typedef struct db_input {
    char name[DB_NAME];
    int32_t values[6];
} db_input;

static bool read_profiles(const char *path, db_input inputs[DB_ROWS]) {
    bool okay = false;
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;
    char line[512];
    size_t count = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        const size_t length = strlen(line);
        if (count == DB_ROWS || length == 0 || line[length - 1] != '\n' ||
            strchr(line, '\r') != NULL)
            goto cleanup;
        line[length - 1] = '\0';
        char *cursor = strchr(line, '\t');
        if (cursor == NULL || cursor == line || (size_t)(cursor - line) >= DB_NAME)
            goto cleanup;
        *cursor++ = '\0';
        for (const char *name = line; *name != '\0'; ++name)
            if (!((*name >= 'a' && *name <= 'z') || (*name >= '0' && *name <= '9') || *name == '-'))
                goto cleanup;
        memcpy(inputs[count].name, line, strlen(line) + 1);
        for (size_t i = 0; i < 6; ++i) {
            char *next = strchr(cursor, '\t');
            if ((i != 5 && next == NULL) || (i == 5 && next != NULL))
                goto cleanup;
            if (next != NULL)
                *next = '\0';
            char *end = NULL;
            errno = 0;
            const intmax_t value = strtoimax(cursor, &end, 10);
            char canonical[32];
            if (errno != 0 || end == cursor || *end != '\0' || value < INT32_MIN ||
                value > INT32_MAX ||
                snprintf(canonical, sizeof(canonical), "%" PRIdMAX, value) <= 0 ||
                strcmp(cursor, canonical) != 0)
                goto cleanup;
            inputs[count].values[i] = (int32_t)value;
            cursor = next == NULL ? NULL : next + 1;
        }
        ++count;
    }
    okay = !ferror(file) && count == DB_ROWS;
cleanup:
    if (fclose(file) != 0)
        okay = false;
    return okay;
}

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

int main(int argc, char **argv) {
    int result = 1;
    fixture f = {0};
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    db_input inputs[DB_ROWS] = {0};
    mc_registry_error error = poison_error();
    const mc_block *captured = NULL;
    mc_block *slots[MC_REGISTRY_BLOCK_SLOTS];
    mc_block **array = NULL;
    mc_block *dust = NULL;
    bool stable = false, resource_ok = false, owners_restored = false;
    size_t emitted = 0;
#ifdef _WIN32
    CHECK(_setmode(_fileno(stdout), _O_BINARY) != -1);
#endif
    CHECK(argc == 2 && read_profiles(argv[1], inputs));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = profile,
                                         .materials = materials,
                                         .max_allocation_bytes = SIZE_MAX};
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK);
    CHECK(fflush(stdout) == 0);
    CHECK(mc_registry_classes_initialize_blocks(f.owner, &error) == MC_REGISTRY_OK);
    CHECK(!f.bad && f.opens == 3 && f.closes == 1 && f.busy_checks == 9 &&
          f.owner->blocks_phase == MC_REGISTRY_READY && f.owner->blocks.m != NULL);
    CHECK(mc_registry_block_static_get(f.owner, "aw", &captured, &error) == MC_REGISTRY_OK &&
          captured != NULL && captured->kind == MC_BLOCK_REDSTONE && captured->fields.bn == 55);
    dust = (mc_block *)(void *)captured;
    array = f.owner->blocks.m;
    memcpy(slots, array, sizeof(slots));
    CHECK(slots[55] == dust && f.owner->blocks.named[54] == dust && slots[75] != NULL &&
          slots[76] != NULL && slots[75]->kind == MC_BLOCK_REDSTONE_TORCH &&
          slots[76]->kind == MC_BLOCK_REDSTONE_TORCH);
    const int32_t initial_dust = dust->fields.bm, initial75 = slots[75]->fields.bm,
                  initial76 = slots[76]->fields.bm;
    const uint64_t random_before = uniquifier(profile);
    const size_t bytes_before = f.owner->allocation_bytes;
    for (size_t index = 0; index < DB_ROWS; ++index) {
        db_input *input = &inputs[index];
        const int32_t *p = input->values;
        CHECK((p[0] == 75 || p[0] == 76) && (p[3] == 0 || p[3] == 1) &&
              f.owner->blocks.m == array && f.owner->blocks.named[54] == dust);
        mc_block *block = f.owner->blocks.m[p[0]];
        CHECK(block != NULL && block->classes == f.owner && dust->classes == f.owner &&
              block->fields.bn == p[0] && dust->fields.bn == 55);
        mc_block_fields saved_self, saved_dust;
        memcpy(&saved_self, &block->fields, sizeof(saved_self));
        memcpy(&saved_dust, &dust->fields, sizeof(saved_dust));
        if (p[3] == 1) {
            block->fields.bm = p[4];
            dust->fields.bm = p[5];
        }
        mc_block_fields entered_self, entered_dust;
        memcpy(&entered_self, &block->fields, sizeof(entered_self));
        memcpy(&entered_dust, &dust->fields, sizeof(entered_dust));
        const int32_t sentinel = INT32_C(-123456789);
        int32_t output = sentinel;
        /* The SAME actual public call is compiled into both paired executables.
         * No dependency on the candidate's private detail symbol exists. */
        const mc_registry_status status =
            mc_block_render_texture_metadata(block, p[1], p[2], &output);
        const int32_t after_self = block->fields.bm, after_dust = dust->fields.bm;
        const bool fields_unchanged =
            memcmp(&entered_self, &block->fields, sizeof(entered_self)) == 0 &&
            memcmp(&entered_dust, &dust->fields, sizeof(entered_dust)) == 0;
        const bool array_same =
            f.owner->blocks.m == array && memcmp(slots, array, sizeof(slots)) == 0;
        const bool named_same = f.owner->blocks.named[54] == dust;
        const bool same_registry = block->classes == f.owner && dust->classes == f.owner;
        const bool self_is_db = block->kind == MC_BLOCK_REDSTONE_TORCH;
        const bool dust_is_sm = dust->kind == MC_BLOCK_REDSTONE;
        /* Restore only the two declared input stores, before any assertion can
         * leave this interval. Other mutations remain observable and red. */
        block->fields.bm = saved_self.bm;
        dust->fields.bm = saved_dust.bm;
        const bool restored = memcmp(&saved_self, &block->fields, sizeof(saved_self)) == 0 &&
                              memcmp(&saved_dust, &dust->fields, sizeof(saved_dust)) == 0;
        CHECK(printf("DB_NATIVE\t{\"index\":%zu,\"name\":\"%s\",\"values\":[%" PRId32 ",%" PRId32
                     ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "],\"status\":%d,"
                     "\"out_before\":%" PRId32 ",\"out\":%" PRId32 ",\"saved_self\":%" PRId32
                     ",\"saved_dust\":%" PRId32 ",\"entered_self\":%" PRId32
                     ",\"entered_dust\":%" PRId32 ",\"after_self\":%" PRId32
                     ",\"after_dust\":%" PRId32 ",\"restored_self\":%" PRId32
                     ",\"restored_dust\":%" PRId32 ",\"self_id\":%" PRId32 ",\"dust_id\":%" PRId32
                     ",\"self_is_db\":%s,\"dust_is_sm\":%s,\"same_registry\":%s,\"array_same\":%s,"
                     "\"named_same\":%s,\"fields_unchanged\":%s,\"restored\":%s}\n",
                     index, input->name, p[0], p[1], p[2], p[3], p[4], p[5], (int)status, sentinel,
                     output, saved_self.bm, saved_dust.bm, entered_self.bm, entered_dust.bm,
                     after_self, after_dust, block->fields.bm, dust->fields.bm, block->fields.bn,
                     dust->fields.bn, boolean(self_is_db), boolean(dust_is_sm),
                     boolean(same_registry), boolean(array_same), boolean(named_same),
                     boolean(fields_unchanged), boolean(restored)) > 0);
        ++emitted;
        CHECK(fields_unchanged && array_same && named_same && same_registry && self_is_db &&
              dust_is_sm && restored);
    }
    owners_restored = dust->fields.bm == initial_dust && slots[75]->fields.bm == initial75 &&
                      slots[76]->fields.bm == initial76 && f.owner->blocks.m == array &&
                      f.owner->blocks.named[54] == dust && memcmp(slots, array, sizeof(slots)) == 0;
    stable = uniquifier(profile) == random_before && f.owner->allocation_bytes == bytes_before;
    resource_ok = !f.bad && f.opens == 3 && f.closes == 1 && f.busy_checks == 9 &&
                  f.owner->entered == 0 && f.owner->initializer == NULL;
    CHECK(owners_restored && stable && resource_ok && emitted == DB_ROWS);
    CHECK(retire(&f));
    mc_materials_destroy(materials);
    materials = NULL;
    mc_java_random_profile_free(profile);
    profile = NULL;
    CHECK(printf("DB_NATIVE_FINISH\t{\"rows\":%zu,\"checks\":%u,\"stable\":%s,\"resources_ok\":%s,"
                 "\"owners_restored\":%s,\"retired\":true}\n",
                 emitted, checks, boolean(stable), boolean(resource_ok),
                 boolean(owners_restored)) > 0);
    CHECK(fflush(stdout) == 0 && !ferror(stdout));
    result = 0;
cleanup:
    if (!retirement_failed && f.owner != NULL && !retire(&f))
        result = 1;
    if (!retirement_failed) {
        mc_materials_destroy(materials);
        mc_java_random_profile_free(profile);
    }
    return result;
}
