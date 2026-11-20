#include "static_index_cases.h"

static bool block_static_linear_oracle(const char *name, size_t *out) {
    for (size_t i = 0; i < COUNT(block_static_names); ++i) {
        if (strcmp(block_static_names[i], name) == 0) {
            *out = i;
            return true;
        }
    }
    return false;
}

static int test_block_static_index_values(void) {
    int result = 1;
    CHECK(COUNT(block_static_names) == 96);
    for (size_t i = 0; i < COUNT(block_static_names); ++i) {
        size_t index = SIZE_MAX;
        CHECK(strcmp(mc_registry_block_static_name(i), block_static_names[i]) == 0);
        CHECK(mc_registry_block_static_index(block_static_names[i], &index) && index == i);
    }
    for (size_t i = 0; i < COUNT(block_static_invalid); ++i) {
        size_t index = SIZE_MAX;
        CHECK(!mc_registry_block_static_index(block_static_invalid[i], &index) &&
              index == SIZE_MAX);
    }
    /* Exact-sized objects expose an accidental read beyond the first NUL to
     * ASan. All inputs here and below are valid terminated C strings. */
    const char empty[1] = {0};
    const char single[2] = {'u', 0};
    const char double_name[3] = {'b', 'l', 0};
    const char embedded[] = {'a', 'A', 0, 'x', 'x', 0};
    size_t index = SIZE_MAX;
    CHECK(!mc_registry_block_static_index(empty, &index) && index == SIZE_MAX);
    CHECK(mc_registry_block_static_index(single, &index) && index == 0);
    CHECK(mc_registry_block_static_index(double_name, &index) && index == 95);
    CHECK(mc_registry_block_static_index(embedded, &index) && index == 58);
    index = SIZE_MAX;
    CHECK(!mc_registry_block_static_index(NULL, &index) && index == SIZE_MAX);
    CHECK(!mc_registry_block_static_index("u", NULL));
    char long_name[4096];
    memset(long_name, 'a', sizeof(long_name));
    long_name[sizeof(long_name) - 1] = 0;
    CHECK(!mc_registry_block_static_index(long_name, &index) && index == SIZE_MAX);

    /* All one- and two-byte strings, including signed-char/non-ASCII bytes,
     * are compared with the independent literal-list oracle. Embedded NULs
     * naturally select the same shorter C string in both implementations. */
    for (unsigned int first = 0; first < 256; ++first) {
        char name[2] = {(char)(unsigned char)first, 0};
        size_t expected = SIZE_MAX, actual = SIZE_MAX;
        const bool found = block_static_linear_oracle(name, &expected);
        CHECK(mc_registry_block_static_index(name, &actual) == found && actual == expected);
        for (unsigned int second = 0; second < 256; ++second) {
            char pair[3] = {(char)(unsigned char)first, (char)(unsigned char)second, 0};
            expected = actual = SIZE_MAX;
            const bool pair_found = block_static_linear_oracle(pair, &expected);
            CHECK(mc_registry_block_static_index(pair, &actual) == pair_found &&
                  actual == expected);
        }
    }
    result = 0;
cleanup:
    return result;
}
