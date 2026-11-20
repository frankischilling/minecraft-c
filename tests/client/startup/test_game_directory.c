#include "game_directory.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wchar.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

static unsigned int checks;
static const char *root;

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool path_for(char *out, size_t capacity, const char *suffix) {
    const int count = snprintf(out, capacity, "%s/%s", root, suffix);
    return count >= 0 && (size_t)count < capacity;
}

static bool same_path(const char *left, const char *right) {
    if (left == NULL || right == NULL)
        return false;
    for (size_t i = 0;; ++i) {
        char a = left[i], b = right[i];
#ifdef _WIN32
        if (a == '/')
            a = '\\';
        if (b == '/')
            b = '\\';
#endif
        if (a != b)
            return false;
        if (a == '\0')
            return true;
    }
}

/* Independent native queries verify effects, rather than supplying services to
 * GameDirectory. A zero result means absent or an unsuccessful native query. */
static int path_kind(const char *path) {
#ifdef _WIN32
    wchar_t native[4096];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, native,
                            (int)(sizeof(native) / sizeof(native[0]))) == 0)
        return 0;
    const DWORD attributes = GetFileAttributesW(native);
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return 0;
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? 2 : 1;
#else
    struct stat information;
    if (stat(path, &information) != 0)
        return 0;
    return S_ISDIR(information.st_mode) ? 2 : S_ISREG(information.st_mode) ? 1 : 3;
#endif
}

static bool same_state(const mc_client_game_directory_state *left,
                       const mc_client_game_directory_state *right) {
    return left->home_origin == right->home_origin &&
           left->filesystem_charset == right->filesystem_charset &&
           left->selected_directory == right->selected_directory &&
           left->appdata_present == right->appdata_present &&
           left->initial_exists == right->initial_exists &&
           left->mkdirs_reached == right->mkdirs_reached &&
           left->directory_created == right->directory_created &&
           left->exists_calls == right->exists_calls && left->mkdir_calls == right->mkdir_calls &&
           left->canonicalize_calls == right->canonicalize_calls &&
           left->last_native_error == right->last_native_error &&
           left->profile_query_hresult == right->profile_query_hresult &&
           left->legacy_profile_query_hresult == right->legacy_profile_query_hresult;
}

static bool existing_and_independent_owners(void) {
    bool okay = false;
    char path[4096];
    mc_client_game_directory *first = NULL, *second = NULL, *file = NULL;
    mc_client_game_directory_state prefix = {0}, state = {0};
    mc_client_game_directory_error error = {0};
    CHECK(path_for(path, sizeof(path), "existing") && path_kind(path) == 2);
    CHECK(mc_client_game_directory_create(path, NULL, &first, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(first != NULL && error.status == MC_CLIENT_GAME_DIRECTORY_OK &&
          same_path(mc_client_game_directory_path(first), path));
    CHECK(prefix.selected_directory && prefix.initial_exists && !prefix.mkdirs_reached &&
          !prefix.directory_created && prefix.home_origin == MC_CLIENT_GAME_DIRECTORY_HOME_NONE);
    CHECK(mc_client_game_directory_read_state(first, &state, &error) ==
              MC_CLIENT_GAME_DIRECTORY_OK &&
          same_state(&prefix, &state));
    CHECK(mc_client_game_directory_create(path, NULL, &second, NULL, NULL) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(first != second &&
          mc_client_game_directory_path(first) != mc_client_game_directory_path(second));
    mc_client_game_directory_free(first);
    first = NULL;
    CHECK(same_path(mc_client_game_directory_path(second), path) && path_kind(path) == 2);
    CHECK(path_for(path, sizeof(path), "regular-file") && path_kind(path) == 1);
    CHECK(mc_client_game_directory_create(path, NULL, &file, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(prefix.initial_exists && !prefix.mkdirs_reached && !prefix.directory_created &&
          same_path(mc_client_game_directory_path(file), path) && path_kind(path) == 1);
    okay = true;
cleanup:
    mc_client_game_directory_free(first);
    mc_client_game_directory_free(second);
    mc_client_game_directory_free(file);
    return okay;
}

static bool creation_and_retirement(void) {
    bool okay = false;
    char path[4096], actual[4096], missing[4096];
    mc_client_game_directory *nested = NULL, *canonical = NULL, *unicode = NULL;
    mc_client_game_directory_state prefix = {0}, state = {0};
    CHECK(path_for(path, sizeof(path), "nested/a/b/leaf") && path_kind(path) == 0);
    CHECK(mc_client_game_directory_create(path, NULL, &nested, &prefix, NULL) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(prefix.selected_directory && !prefix.initial_exists && prefix.mkdirs_reached &&
          prefix.directory_created && path_kind(path) == 2);
    CHECK(mc_client_game_directory_read_state(nested, &state, NULL) ==
              MC_CLIENT_GAME_DIRECTORY_OK &&
          same_state(&prefix, &state));
    CHECK(same_path(mc_client_game_directory_path(nested), path));
    mc_client_game_directory_free(nested);
    nested = NULL;
    CHECK(path_kind(path) == 2);

    CHECK(path_for(path, sizeof(path), "missing/../canonical-child/leaf") &&
          path_for(actual, sizeof(actual), "canonical-child/leaf") &&
          path_for(missing, sizeof(missing), "missing"));
    CHECK(path_kind(path) == 0 && path_kind(actual) == 0 && path_kind(missing) == 0);
    CHECK(mc_client_game_directory_create(path, NULL, &canonical, &prefix, NULL) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(prefix.mkdirs_reached && prefix.directory_created && !prefix.initial_exists &&
          same_path(mc_client_game_directory_path(canonical), path));
    CHECK(path_kind(actual) == 2 && path_kind(missing) == 0);
#ifndef _WIN32
    /* POSIX stat traverses the missing prefix; Win32's path API can collapse
     * the dots first. Neither behavior changes the retained owner text. */
    CHECK(path_kind(path) == 0);
#endif
    mc_client_game_directory_free(canonical);
    canonical = NULL;
    CHECK(path_kind(actual) == 2 && path_kind(missing) == 0);

    CHECK(path_for(path, sizeof(path), "unicode-\xc3\xa9-\xf0\x9f\x9a\x80/child") &&
          path_kind(path) == 0);
    CHECK(mc_client_game_directory_create(path, NULL, &unicode, &prefix, NULL) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(prefix.directory_created && same_path(mc_client_game_directory_path(unicode), path) &&
          path_kind(path) == 2);
    mc_client_game_directory_free(unicode);
    unicode = NULL;
    CHECK(path_kind(path) == 2);
    okay = true;
cleanup:
    mc_client_game_directory_free(nested);
    mc_client_game_directory_free(canonical);
    mc_client_game_directory_free(unicode);
    return okay;
}

static bool failure_and_argument_controls(void) {
    bool okay = false;
    char good[4096], blocked[4096], file[4096];
    char *too_long = NULL;
    mc_client_game_directory *owner = NULL, *output = NULL;
    mc_client_game_directory_state prefix = {0}, held = {0};
    mc_client_game_directory_error error = {0};
    CHECK(path_for(good, sizeof(good), "existing") &&
          path_for(blocked, sizeof(blocked), "file-parent/child/leaf") &&
          path_for(file, sizeof(file), "file-parent"));
    CHECK(mc_client_game_directory_create(good, NULL, &owner, &held, NULL) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    output = owner;
    CHECK(mc_client_game_directory_create(blocked, NULL, &output, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED);
    CHECK(output == owner && prefix.selected_directory && !prefix.initial_exists &&
          prefix.mkdirs_reached && !prefix.directory_created && prefix.last_native_error != 0);
    CHECK(error.status == MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED && error.operation != NULL);
    CHECK(path_kind(file) == 1 && path_kind(blocked) == 0 &&
          same_path(mc_client_game_directory_path(owner), good));
    mc_client_game_directory_state current = {0};
    CHECK(mc_client_game_directory_read_state(owner, &current, NULL) ==
              MC_CLIENT_GAME_DIRECTORY_OK &&
          same_state(&held, &current));

    prefix = (mc_client_game_directory_state){.exists_calls = 987u, .selected_directory = true};
    CHECK(mc_client_game_directory_create(good, NULL, NULL, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT);
    CHECK(prefix.exists_calls == 987u && prefix.selected_directory &&
          error.status == MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT);
    const mc_client_game_directory_profile invalid = {
        (mc_java_reader_charset)(MC_JAVA_READER_US_ASCII + 1)};
    CHECK(mc_client_game_directory_create(good, &invalid, &output, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT);
    CHECK(output == owner && prefix.exists_calls == 987u);
    CHECK(mc_client_game_directory_read_state(NULL, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT);
    CHECK(prefix.exists_calls == 987u && mc_client_game_directory_path(NULL) == NULL);
    CHECK(mc_client_game_directory_read_state(owner, NULL, &error) ==
          MC_CLIENT_GAME_DIRECTORY_INVALID_ARGUMENT);

    CHECK(mc_client_game_directory_create("", NULL, &output, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_CREATION_FAILED);
    CHECK(output == owner && prefix.selected_directory && !prefix.initial_exists &&
          prefix.mkdirs_reached && !prefix.directory_created && !prefix.appdata_present &&
          prefix.home_origin == MC_CLIENT_GAME_DIRECTORY_HOME_NONE);

    const char malformed[] = "x\xc0\xaf";
    CHECK(mc_client_game_directory_create(malformed, NULL, &output, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING);
    CHECK(output == owner && prefix.selected_directory && !prefix.mkdirs_reached &&
          !prefix.initial_exists &&
          error.status == MC_CLIENT_GAME_DIRECTORY_UNSUPPORTED_FILENAME_ENCODING);
    too_long = malloc((size_t)MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES + 2u);
    CHECK(too_long != NULL);
    memset(too_long, 'x', (size_t)MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES + 1u);
    too_long[(size_t)MC_CLIENT_GAME_DIRECTORY_MAX_PATH_BYTES + 1u] = '\0';
    CHECK(mc_client_game_directory_create(too_long, NULL, &output, &prefix, &error) ==
          MC_CLIENT_GAME_DIRECTORY_LIMIT);
    CHECK(output == owner && prefix.selected_directory && !prefix.mkdirs_reached &&
          !prefix.initial_exists && error.status == MC_CLIENT_GAME_DIRECTORY_LIMIT);
    mc_client_game_directory_free(NULL);
    CHECK(same_path(mc_client_game_directory_path(owner), good) && path_kind(good) == 2);
    okay = true;
cleanup:
    free(too_long);
    mc_client_game_directory_free(owner);
    return okay;
}

#ifdef _WIN32
static bool confined_windows_default(void) {
    bool okay = false;
    char appdata[4096], expected[4096];
    wchar_t environment[4096], native_expected[4096];
    mc_client_game_directory *directory = NULL;
    mc_client_game_directory_state prefix = {0};
    CHECK(path_for(appdata, sizeof(appdata), "appdata") &&
          path_for(expected, sizeof(expected), "appdata/.minecraft") && path_kind(appdata) == 2);
    /* Refuse before default creation unless the actual child environment names
     * the fresh test parent exactly. Never fall through to a user directory. */
    const DWORD units = GetEnvironmentVariableW(
        L"APPDATA", environment, (DWORD)(sizeof(environment) / sizeof(environment[0])));
    CHECK(units > 0 && units < sizeof(environment) / sizeof(environment[0]));
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, appdata, -1, native_expected,
                              (int)(sizeof(native_expected) / sizeof(native_expected[0]))) > 0);
    for (size_t i = 0; native_expected[i] != L'\0'; ++i)
        if (native_expected[i] == L'/')
            native_expected[i] = L'\\';
    CHECK(wcscmp(environment, native_expected) == 0 && path_kind(expected) == 0);
    CHECK(mc_client_game_directory_create(NULL, NULL, &directory, &prefix, NULL) ==
          MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(!prefix.selected_directory && prefix.appdata_present && prefix.mkdirs_reached &&
          prefix.directory_created && prefix.home_origin != MC_CLIENT_GAME_DIRECTORY_HOME_NONE);
    CHECK(same_path(mc_client_game_directory_path(directory), expected) &&
          path_kind(expected) == 2);
    mc_client_game_directory_free(directory);
    directory = NULL;
    CHECK(path_kind(expected) == 2);
    okay = true;
cleanup:
    mc_client_game_directory_free(directory);
    return okay;
}
#endif

static int test_body(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;
    root = argv[1];
    if (path_kind(root) != 2 || !existing_and_independent_owners() || !creation_and_retirement() ||
        !failure_and_argument_controls())
        return EXIT_FAILURE;
#ifdef _WIN32
    if (!confined_windows_default())
        return EXIT_FAILURE;
#endif
    if (printf("PASS: %u GameDirectory real-file interface checks\n", checks) < 0 ||
        fflush(stdout) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide) {
    if (argc != 2)
        return EXIT_FAILURE;
    const int bytes =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[1], -1, NULL, 0, NULL, NULL);
    if (bytes <= 0 || bytes > 4096)
        return EXIT_FAILURE;
    char *path = malloc((size_t)bytes);
    if (path == NULL)
        return EXIT_FAILURE;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[1], -1, path, bytes, NULL, NULL) !=
        bytes) {
        free(path);
        return EXIT_FAILURE;
    }
    char *arguments[] = {NULL, path};
    const int status = test_body(argc, arguments);
    free(path);
    return status;
}
#else
int main(int argc, char **argv) {
    return test_body(argc, argv);
}
#endif
