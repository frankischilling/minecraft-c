#include "client/application_access.h"
#include "client/resources/download_thread_internal.h"
#include "client/startup/application_owner.h"
#include "cz_test_files.h"
#include <time.h>
static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "cz owner %u line%d: %s\n", checks, __LINE__, #x);                     \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    if (argc != 2)
        return 77;
    int result = 1;
    mc_java_files *files = NULL;
    mc_java_file *root = NULL, *dir = NULL, *leaf = NULL, *blocker = NULL, *file_parent = NULL,
                 *resource_file = NULL;
    mc_client_download_classes *classes = NULL;
    mc_client_download_thread *worker = NULL, *extra = NULL, *rejected = NULL;
    client_application *app = calloc(1, sizeof(*app));
    mc_client_download_error *e = calloc(1, sizeof(*e));
    mc_client_application_owner_error ae = {0};
    mc_java_file_error fe = {0};
    CHECK(app != NULL && e != NULL);
    CHECK(test_directory(argv[1], &files, &root));
    CHECK(mc_client_application_lifetime_register(app, &ae) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_download_classes_create(files, NULL, &classes, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_allocate(classes, NULL, &worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_construct(worker, root, app, e) == MC_CLIENT_DOWNLOAD_OK);
    mc_client_download_view v;
    CHECK(mc_client_download_thread_inspect(worker, &v, e) == MC_CLIENT_DOWNLOAD_OK &&
          v.last_constructor_pc == 89 && v.b == app && v.a != root && v.daemon &&
          v.thread_constructed && !v.started && !v.c && v.app_borrow_counted);
    CHECK(mc_client_application_resource_thread_borrows_pending(app));
    CHECK(mc_client_application_lifetime_retire(app, &ae) == MC_CLIENT_APPLICATION_OWNER_BUSY);
    CHECK(mc_client_download_thread_retain(worker, e) == MC_CLIENT_DOWNLOAD_OK);
    extra = worker;
    CHECK(mc_client_download_thread_retire(&worker, e) == MC_CLIENT_DOWNLOAD_BUSY &&
          worker == extra);
    CHECK(mc_client_download_thread_inspect(worker, &v, e) == MC_CLIENT_DOWNLOAD_OK &&
          v.holds == 2 && !v.retiring && !v.c);
    CHECK(mc_client_download_thread_release(&extra, e) == MC_CLIENT_DOWNLOAD_OK && extra == NULL);
    CHECK(mc_client_application_resource_thread_borrows_pending(app));
    CHECK(mc_client_download_thread_cancel(worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_scan(worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_inspect(worker, &v, e) == MC_CLIENT_DOWNLOAD_OK && v.c &&
          v.local_visits == 0);
    CHECK(test_child(files, (mc_java_file *)(void *)v.a, "ignored", &dir));
    bool made = false;
    CHECK(mc_java_file_mkdirs(dir, &made, &fe) == MC_JAVA_FILE_OK && made);
    CHECK(test_child(files, dir, "data.bin", &leaf));
    const uint8_t bytes[] = {7};
    CHECK(test_write(files, leaf, bytes, 1));
    CHECK(mc_client_download_thread_scan(worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_inspect(worker, &v, e) == MC_CLIENT_DOWNLOAD_OK &&
          v.registrations == 0 && app->B == NULL && v.local_visits == 2);
    CHECK(mc_client_download_thread_release(&worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(!mc_client_application_resource_thread_borrows_pending(app));
    CHECK(test_child(files, root, "blocker", &blocker));
    CHECK(test_write(files, blocker, bytes, 1));
    CHECK(mc_client_download_thread_allocate(classes, NULL, &worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_construct(worker, blocker, app, e) ==
              MC_CLIENT_DOWNLOAD_UNIMPLEMENTED &&
          e->site.pc == 59);
    CHECK(mc_client_download_thread_inspect(worker, &v, e) == MC_CLIENT_DOWNLOAD_OK &&
          v.last_constructor_pc == 36 && v.a != NULL && v.app_borrow_counted);
    CHECK(mc_client_download_thread_release(&worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(!mc_client_application_resource_thread_borrows_pending(app));
    CHECK(test_child(files, root, "file-parent", &file_parent));
    made = false;
    CHECK(mc_java_file_mkdirs(file_parent, &made, &fe) == MC_JAVA_FILE_OK && made);
    CHECK(test_child(files, file_parent, "resources", &resource_file));
    CHECK(test_write(files, resource_file, bytes, 1));
    CHECK(mc_client_download_thread_allocate(classes, NULL, &worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_construct(worker, file_parent, app, e) ==
          MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_scan(worker, e) == MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION &&
          e->site.pc == 11 && e->caller_count == 1 && e->callers[0].pc == 7);
    CHECK(mc_client_download_thread_release(&worker, e) == MC_CLIENT_DOWNLOAD_OK);
    /* Real OS thread, with a deliberate reached native HTTP input admission
     * failure before any socket creation. This is not successful run parity. */
    mc_client_download_options o = mc_client_download_default_options();
    o.http.max_buffer_bytes = 1;
    CHECK(mc_client_download_thread_allocate(classes, &o, &worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_construct(worker, root, app, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_start(worker, e) == MC_CLIENT_DOWNLOAD_OK);
    struct timespec start;
    CHECK(timespec_get(&start, TIME_UTC) == TIME_UTC);
    for (;;) {
        mc_client_download_status s = mc_client_download_thread_inspect(worker, &v, e);
        if (s == MC_CLIENT_DOWNLOAD_OK && v.completed)
            break;
        CHECK(s == MC_CLIENT_DOWNLOAD_OK || s == MC_CLIENT_DOWNLOAD_BUSY);
        struct timespec now;
        CHECK(timespec_get(&now, TIME_UTC) == TIME_UTC && now.tv_sec - start.tv_sec < 10);
        thrd_yield();
    }
    CHECK(v.started && v.completed && !v.run_returned && v.failure->site.pc == 21);
    CHECK(mc_client_download_thread_release(&worker, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(!mc_client_application_resource_thread_borrows_pending(app));
    app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_download_thread_allocate(classes, NULL, &rejected, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_download_thread_construct(rejected, root, app, e) == MC_CLIENT_DOWNLOAD_BUSY);
    CHECK(mc_client_download_thread_inspect(rejected, &v, e) == MC_CLIENT_DOWNLOAD_OK &&
          v.last_constructor_pc == 6 && v.b == NULL && !v.app_borrow_counted);
    CHECK(mc_client_download_thread_release(&rejected, e) == MC_CLIENT_DOWNLOAD_OK);
    CHECK(mc_client_application_lifetime_retire(app, &ae) == MC_CLIENT_APPLICATION_OWNER_OK);
    result = 0;
cleanup:
    if (e != NULL) {
        (void)mc_client_download_thread_release(&rejected, e);
        (void)mc_client_download_thread_release(&extra, e);
        for (unsigned int i = 0; i < 10000u && worker != NULL; ++i) {
            if (mc_client_download_thread_release(&worker, e) == MC_CLIENT_DOWNLOAD_BUSY)
                thrd_yield();
        }
        (void)mc_client_download_classes_release(&classes, e);
    }
    (void)mc_java_file_release(&resource_file, &fe);
    (void)mc_java_file_release(&file_parent, &fe);
    (void)mc_java_file_release(&blocker, &fe);
    (void)mc_java_file_release(&leaf, &fe);
    (void)mc_java_file_release(&dir, &fe);
    (void)mc_java_file_release(&root, &fe);
    (void)mc_java_files_release(&files, &fe);
    if (worker == NULL && rejected == NULL)
        free(app);
    free(e);
    if (result == 0)
        printf("download owner: %u checks\n", checks);
    return result;
}
