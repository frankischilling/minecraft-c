#include "cz_test_files.h"
#include "mc/client/sound/sound_file_registration.h"
#include <stdatomic.h>
#include <threads.h>
static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "cz pool %u line%d: %s\n", checks, __LINE__, #x);                      \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct job {
    mc_sound_pool *pool;
    mc_java_file *file;
    atomic_bool done, failed;
} job;

static int register_files(void *context) {
    job *j = context;
    const uint16_t name[] = {'g', 'r', 'o', 'u', 'p', '1', '.', 'o', 'g', 'g'};
    mc_sound_text text = {name, 10, false};
    for (unsigned int i = 0; i < 64u; ++i) {
        mc_sound_pool_file_call *call = NULL;
        const mc_sound_pool_entry *entry = NULL;
        mc_sound_pool_file_error e = {0};
        if (mc_sound_pool_add_java_file(j->pool, text, j->file, &call, &entry, &e) != MC_SOUND_OK ||
            entry == NULL || !e.source_returned) {
            atomic_store(&j->failed, true);
            (void)mc_sound_pool_file_call_release(&call, &e);
            break;
        }
        if (mc_sound_pool_file_call_release(&call, &e) != MC_SOUND_OK || call != NULL) {
            atomic_store(&j->failed, true);
            break;
        }
    }
    atomic_store(&j->done, true);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 77;
    int result = 1;
    mc_java_files *files = NULL;
    mc_java_file *root = NULL, *file = NULL;
    mc_java_file_error fe = {0};
    mc_java_random_profile *random = NULL;
    mc_sound_pool *pool = NULL;
    mc_sound_pool_file_call *initial_call = NULL;
    mc_sound_error se = {0};
    bool started = false;
    thrd_t thread = {0};
    job j = {0};
    atomic_init(&j.done, false);
    atomic_init(&j.failed, false);
    CHECK(test_directory(argv[1], &files, &root));
    CHECK(test_child(files, root, "real sound.ogg", &file));
    const uint8_t bytes[] = {1, 2, 3};
    CHECK(test_write(files, file, bytes, 3));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &random) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_sound_pool_create(random, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &pool, &se) ==
          MC_SOUND_OK);
    mc_sound_pool_state before, after;
    CHECK(mc_sound_pool_read_state(pool, &before, &se) == MC_SOUND_OK);
    const uint16_t initial_name[] = {'g', 'r', 'o', 'u', 'p', '0', '.', 'o', 'g', 'g'};
    const mc_sound_pool_entry *initial_entry = NULL;
    mc_sound_pool_file_error initial_error = {0};
    CHECK(mc_sound_pool_add_java_file(pool, (mc_sound_text){initial_name, 10, false}, file,
                                      &initial_call, &initial_entry,
                                      &initial_error) == MC_SOUND_OK &&
          initial_entry != NULL);
    CHECK(mc_sound_pool_file_call_release(&initial_call, &initial_error) == MC_SOUND_OK &&
          initial_call == NULL);
    CHECK(mc_sound_pool_read_state(pool, &after, &se) == MC_SOUND_OK &&
          after.random.state == before.random.state &&
          after.random.have_next_next_gaussian == before.random.have_next_next_gaussian &&
          after.random.next_next_gaussian == before.random.next_next_gaussian);
    j.pool = pool;
    j.file = file;
    CHECK(thrd_create(&thread, register_files, &j) == thrd_success);
    started = true;
    const uint16_t group[] = {'g', 'r', 'o', 'u', 'p'};
    for (unsigned int i = 0; i < 1024u && !atomic_load(&j.done); ++i) {
        const mc_sound_pool_entry *selected = NULL;
        CHECK(mc_sound_pool_select(pool, (mc_sound_text){group, 5, false}, &selected, NULL, &se) ==
              MC_SOUND_OK);
        if (selected != NULL) {
            mc_sound_pool_entry_view view;
            CHECK(mc_sound_pool_entry_read(selected, &view, &se) == MC_SOUND_OK &&
                  view.file.native_utf8_path != NULL);
        }
        thrd_yield();
    }
    int worker = 1;
    CHECK(thrd_join(thread, &worker) == thrd_success && worker == 0);
    started = false;
    CHECK(!atomic_load(&j.failed));
    mc_sound_pool_state state;
    CHECK(mc_sound_pool_read_state(pool, &state, &se) == MC_SOUND_OK && state.count == 65 &&
          state.full_entry_count == 65 && state.group_count == 1 && !state.registration_active);
    const mc_sound_pool_entry *first = NULL, *last = NULL;
    CHECK(mc_sound_pool_entry_at(pool, 0, &first, &se) == MC_SOUND_OK &&
          mc_sound_pool_entry_at(pool, 64, &last, &se) == MC_SOUND_OK && first != last);
    result = 0;
cleanup:
    if (started) {
        int ignored;
        (void)thrd_join(thread, &ignored);
    }
    mc_sound_pool_file_error cleanup_error = {0};
    (void)mc_sound_pool_file_call_release(&initial_call, &cleanup_error);
    (void)mc_sound_pool_free(pool, &se);
    mc_java_random_profile_free(random);
    (void)mc_java_file_release(&file, &fe);
    (void)mc_java_file_release(&root, &fe);
    (void)mc_java_files_release(&files, &fe);
    if (result == 0)
        printf("download pool: %u checks\n", checks);
    return result;
}
