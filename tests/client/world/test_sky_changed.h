/* Prepared canonical N/DK fields invoke actual n.e through its WorldAccess identity. */
#include "client/application_state.h"
#include "client/world/source_chunk_renderer_internal.h"

static bool sky_changed_controls(mc_client_render_engine_classes *graphics, mc_world_runtime *world,
                                 mc_entity_classes *entities) {
    bool result = false;
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    client_application *app = calloc(1, sizeof(*app));
    mc_world_access_failure captured = {0};
    CHECK(error != NULL && app != NULL);
    app->entities = entities;
    const size_t original_memberships = world->world_accesses.count;
    for (unsigned int mode = 0; mode < 9; ++mode) {
        CHECK(mc_client_render_global_allocate(graphics, NULL, &owner, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
        owner->fields.t = app;
        owner->fields.o =
            mc_render_global_allocate(owner, 12 * sizeof(*owner->fields.o),
                                      (mc_client_graphics_site){"n", "a", "()V", 239}, error);
        owner->fields.m =
            mc_render_global_allocate(owner, sizeof(*owner->fields.m),
                                      (mc_client_graphics_site){"n", "<init>", "()V", 16}, error);
        CHECK(owner->fields.o != NULL && owner->fields.m != NULL);
        owner->o_length = 12;
        mc_client_source_dk *all[12];
        for (size_t i = 0; i < 12; ++i) {
            all[i] = mc_source_dk_allocate(owner, error);
            CHECK(all[i] != NULL);
            owner->fields.o[i] = all[i];
        }
        if (mode == 0)
            for (size_t i = 0; i < 12; ++i) {
                all[i]->fields.A = i % 2 != 0;
                all[i]->fields.u = i % 3 == 0;
            }
        if (mode == 1) {
            const size_t order[] = {1, 1, 3, 2, 1, 3};
            owner->o_length = COUNT(order);
            for (size_t i = 0; i < COUNT(order); ++i)
                owner->fields.o[i] = all[order[i]];
            all[1]->fields.A = all[2]->fields.A = all[3]->fields.A = true;
        }
        if (mode == 2) {
            owner->o_length = 0;
            owner->fields.m = NULL;
        }
        if (mode == 3)
            owner->fields.o = NULL;
        if (mode == 4) {
            owner->o_length = 4;
            owner->fields.o[0] = all[0];
            owner->fields.o[1] = all[1];
            owner->fields.o[2] = NULL;
            owner->fields.o[3] = all[2];
            all[0]->fields.A = all[1]->fields.A = all[2]->fields.A = true;
        }
        if (mode == 5) {
            owner->o_length = 2;
            all[1]->fields.A = true;
            owner->fields.m = NULL;
        }
        if (mode == 6) {
            owner->o_length = 2;
            owner->fields.m = NULL;
        }
        if (mode == 7) {
            owner->o_length = 1;
            all[0]->fields.A = all[0]->fields.u = true;
            owner->fields.m = NULL;
        }
        if (mode == 8) {
            owner->o_length = 1;
            all[0]->fields.A = true;
            owner->fields.m->entries = mc_render_global_allocate(
                owner, 8 * sizeof(void *), (mc_client_graphics_site){"n", "e", "()V", 45}, error);
            CHECK(owner->fields.m->entries != NULL);
            owner->fields.m->capacity = owner->fields.m->count = 8;
            for (size_t i = 0; i < 8; ++i)
                owner->fields.m->entries[i] = all[1];
            owner->arena.options.max_constructor_allocations = owner->arena.allocations;
        }
        CHECK(mc_render_global_access_create(owner, &owner->access, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
        CHECK(mc_render_global_access_source_add(owner->access, world, error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
        mc_world_access *identity = world->world_accesses.entries[world->world_accesses.count - 1];
        mc_world_storage_error cause = {0};
        mc_world_access_failure failure = {0};
        const mc_world_storage_status status =
            mc_world_access_call_sky_changed(identity, world, &cause, &failure);
        const bool failed = mode == 3 || mode == 4 || mode == 5 || mode == 8;
        CHECK((status != MC_WORLD_STORAGE_OK) == failed);
        unsigned int mask = 0, code = 0;
        for (size_t i = 0; i < 12; ++i)
            if (all[i]->fields.u)
                mask |= 1u << i;
        mc_client_render_list *queue = owner->fields.m;
        if (queue != NULL)
            for (size_t i = 0; i < queue->count; ++i) {
                size_t id = 0;
                while (id < 12 && queue->entries[i] != all[id])
                    ++id;
                CHECK(id < 12);
                code = code * 17u + (unsigned int)id + 1u;
            }
        if (mode < 8)
            printf("SKY_CHANGED %u %u %d %u %u\n", mode, failed ? 1u : 0u,
                   queue == NULL ? -1 : (int)queue->count, mask, code);
        if (failed) {
            const unsigned int pc = mode == 3 ? 7u : mode == 4 ? 17u : 45u;
            CHECK(failure.receipt != NULL && failure.render_global != NULL &&
                  failure.render_global->source_pc == pc &&
                  strcmp(failure.render_global->source_method, "e") == 0 &&
                  strcmp(failure.render_global->source_descriptor, "()V") == 0);
            const char *exception = NULL;
            CHECK(mc_world_access_failure_source_exception(&failure, &exception) == (mode != 8));
            if (mode != 8)
                CHECK(strcmp(exception, "java/lang/NullPointerException") == 0 &&
                      !failure.render_global->native_policy);
            else
                CHECK(failure.render_global->status == MC_CLIENT_RENDER_GLOBAL_LIMIT &&
                      failure.render_global->native_policy && queue->count == 8 &&
                      !all[0]->fields.u);
            CHECK(mc_world_access_failure_retain(&failure, &captured) == MC_WORLD_STORAGE_OK);
            CHECK(mc_render_global_access_detach_all(owner->access, error) ==
                  MC_CLIENT_RENDER_GLOBAL_BUSY);
            /* A later empty source return leaves this particular failed call intact. */
            mc_client_source_dk *empty = NULL;
            mc_client_source_dk **saved = owner->fields.o;
            size_t length = owner->o_length;
            owner->fields.o = &empty;
            owner->o_length = 0;
            mc_world_access_failure later = {0};
            CHECK(mc_world_access_call_sky_changed(identity, world, &cause, &later) ==
                      MC_WORLD_STORAGE_OK &&
                  later.receipt == NULL && captured.render_global->source_pc == pc);
            owner->fields.o = saved;
            owner->o_length = length;
            mc_world_access_failure_release(&captured);
        } else
            CHECK(failure.receipt == NULL && cause.status == MC_WORLD_STORAGE_OK);
        CHECK(mc_client_render_global_retire(&owner, error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              owner == NULL);
        CHECK(world->world_accesses.count == original_memberships);
    }
    result = true;
cleanup:
    mc_world_access_failure_release(&captured);
    if (owner != NULL)
        (void)mc_client_render_global_release(&owner, error);
    free(error);
    free(app);
    return result;
}
