/* Compile the canonical owner once in this test TU to assert its private
 * allocation layout. Link-map admission must exclude info.c's archive member.
 * This is not a copied Info kernel or mutable public layout shadow. */
#include "../../../src/world/info/info.c"
#include "mc/world/storage/java_file.h"
#include <stdio.h>
#include <threads.h>

typedef struct name_job {
    mc_java_files *files;
    mc_world_info_name *shared, *expected;
    atomic_bool *go;
    bool failed;
} name_job;

static int run_names(void *context) {
    name_job *j = context;
    while (!atomic_load_explicit(j->go, memory_order_acquire))
        thrd_yield();
    mc_world_info_name *empty = mc_world_info_name_empty();
    for (unsigned int i = 0; i < 10000u; ++i) {
        if (mc_world_info_name_retain(j->shared) != MC_WORLD_INFO_OK) {
            j->failed = true;
            break;
        }
        mc_nbt_string t = mc_world_info_name_text(j->shared);
        if (t.length != 3 || t.units[0] != 'a' || t.units[2] != 'c')
            j->failed = true;
        mc_world_info_name_release(j->shared);
        mc_java_file *f = NULL;
        mc_java_file_error e = {0};
        if (mc_java_file_allocate(j->files, &f, &e) != MC_JAVA_FILE_OK ||
            mc_java_file_construct_strings(f, empty, empty, &e) != MC_JAVA_FILE_OK ||
            mc_java_file_path(f) != j->expected)
            j->failed = true;
        if (mc_java_file_release(&f, &e) != MC_JAVA_FILE_OK)
            j->failed = true;
        if (j->failed)
            break;
    }
    return j->failed ? 1 : 0;
}

int main(void) {
    int result = 1;
    mc_java_files *files = NULL;
    mc_java_file *root = NULL;
    mc_world_info_name *name = NULL;
    mc_java_file_error e = {0};
    thrd_t threads[2];
    unsigned int started = 0;
    atomic_bool go;
    atomic_init(&go, false);
    const uint16_t text[] = {'a', 'b', 'c'};
    if (sizeof(mc_world_info_name) != 24u || _Alignof(mc_world_info_name) != 8u ||
        offsetof(mc_world_info_name, units) != 16u)
        goto cleanup;
    if (mc_world_info_name_copy((mc_nbt_string){text, 3}, sizeof(mc_world_info_name) + 6u, &name) !=
        MC_WORLD_INFO_OK)
        goto cleanup;
    if (mc_java_files_create(NULL, &files, &e) != MC_JAVA_FILE_OK ||
        mc_java_file_allocate(files, &root, &e) != MC_JAVA_FILE_OK ||
        mc_java_file_construct_strings(root, mc_world_info_name_empty(), mc_world_info_name_empty(),
                                       &e) != MC_JAVA_FILE_OK)
        goto cleanup;
    name_job jobs[2] = {{files, name, mc_java_file_path(root), &go, false},
                        {files, name, mc_java_file_path(root), &go, false}};
    for (unsigned int i = 0; i < 2u; ++i) {
        if (thrd_create(&threads[i], run_names, &jobs[i]) != thrd_success)
            goto join;
        ++started;
    }
    atomic_store_explicit(&go, true, memory_order_release);
join:
    atomic_store_explicit(&go, true, memory_order_release);
    for (unsigned int i = 0; i < started; ++i) {
        int status = 1;
        if (thrd_join(threads[i], &status) != thrd_success || status != 0)
            jobs[i].failed = true;
    }
    if (started != 2u || jobs[0].failed || jobs[1].failed || atomic_load(&name->holds) != 1u)
        goto cleanup;
    if (atomic_load(&mc_java_file_path(root)->holds) != 2u)
        goto cleanup; /* namespace + root, no leaked worker path */
    printf("InfoName shared lifetime: 2 workers, 20000 aliases, size%zu align%zu offset%zu\n",
           sizeof(mc_world_info_name), _Alignof(mc_world_info_name),
           offsetof(mc_world_info_name, units));
    result = 0;
cleanup:
    mc_world_info_name_release(name);
    mc_java_file_release(&root, &e);
    mc_java_files_release(&files, &e);
    return result;
}
