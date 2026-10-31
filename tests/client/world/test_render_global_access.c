#define main mc_render_access_entity_publication_fixture_main
#include "../../world/runtime/test_entity_publication.c"
#undef main

#include "client/render_engine/image_buffer.h"
#include "client/render_engine/internal.h"
#include "client/world/render_global_access.h"
#include "client/world/render_global_internal.h"
#include "mc/entity/appearance.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/presentation.h"
#include "world/runtime/world_access_internal.h"

#include "test_sky_changed.h"

/* Real GAME Worlds and a genuine cold native n owner exercise interface
 * lifetime, without pretending the n graphics constructor has completed. The
 * sound method's actual NULL App failure needs no window or audio operation. */
int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_client_render_engine_classes *render_classes = NULL;
    mc_client_render_global *owner = NULL, *external = NULL;
    mc_client_render_global_error error = {0};
    mc_world_mutation_error *first = NULL, *second = NULL;
    mc_world_access_failure captured = {0};
    mc_world_access_failure captured_processor = {0};
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL;
    mc_entity *player = NULL;
    char root[8192];
    const uint16_t title[] = {'A', 'c', 'c', 'e', 's', 's'};
    if (argc != 3 || strcmp(argv[1], "world-access") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    first = calloc(1, sizeof(*first));
    second = calloc(1, sizeof(*second));
    CHECK(f != NULL && first != NULL && second != NULL);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    CHECK(open_store(f, 0, root) && create_world(f, 0));
    CHECK(mc_client_render_engine_classes_create(NULL, &render_classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_global_allocate(render_classes, NULL, &owner, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_render_global_access_create(owner, &owner->access, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    mc_world_runtime *world = f->worlds[0];
    /* The completed GAME constructor already bound dimension.a to this exact
     * World. Its independent persistent occurrence survives bridge detach. */
    const size_t baseline_bindings = world->persistent_bindings;
    CHECK(mc_world_provider_world(world->fields.t) == world && baseline_bindings != 0 &&
          baseline_bindings <= SIZE_MAX - 2u && world->fields.u->count == 0);
    CHECK(mc_render_global_access_source_add(owner->access, world, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    mc_world_access *identity = world->world_accesses.entries[0];
    const uint64_t owned_first = world->world_accesses.identities[0];
    CHECK(identity != NULL && world->fields.u->count == 1 &&
          world->persistent_bindings == baseline_bindings + 1u);
    CHECK(sky_changed_controls(render_classes, world, f->entity_classes[0]));
    CHECK(mc_world_runtime_add_world_access(world, identity, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(world->persistent_bindings == baseline_bindings + 1u && world->fields.u->count == 2);
    const uint64_t foreign = world->world_accesses.identities[1];
    CHECK(mc_render_global_access_source_add(owner->access, world, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    const uint64_t owned_last = world->world_accesses.identities[2];
    CHECK(owned_first < foreign && foreign < owned_last &&
          world->persistent_bindings == baseline_bindings + 2u);
    CHECK(mc_render_global_access_source_remove(owner->access, world, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(world->fields.u->count == 2 && world->world_accesses.identities[0] == foreign &&
          world->world_accesses.identities[1] == owned_last &&
          world->persistent_bindings == baseline_bindings + 1u);
    CHECK(mc_world_runtime_sound(world, 1, 2, 3, NULL, 1, 1, first) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    const mc_world_access_failure original = first->world_access_failure;
    CHECK(first->world_access_bytecode == 41 && original.receiver == identity &&
          original.receipt != NULL && original.render_global != NULL &&
          original.render_global->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          original.render_global->source_pc == 22 &&
          strcmp(original.render_global->source_descriptor, "(Ljava/lang/String;DDDFF)V") == 0);
    const char *exception_class = NULL;
    CHECK(mc_world_access_failure_source_exception(&original, &exception_class) &&
          strcmp(exception_class, "java/lang/NullPointerException") == 0);
    CHECK(mc_world_access_failure_retain(&original, &captured) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_sound(world, 4, 5, 6, NULL, 1, 1, second) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(second->world_access_failure.receipt != original.receipt &&
          second->world_access_failure.render_global != original.render_global &&
          original.render_global->source_pc == 22 &&
          captured.render_global == original.render_global);
    mc_render_global_access_state access_state;
    CHECK(mc_render_global_access_inspect(owner->access, &access_state) &&
          access_state.registrations == 1 && access_state.active_callbacks == 0 &&
          access_state.retained_failures == 2 &&
          access_state.failure == second->world_access_failure.render_global);
    CHECK(mc_render_global_access_detach_all(owner->access, &error) ==
          MC_CLIENT_RENDER_GLOBAL_BUSY);
    CHECK(world->fields.u->count == 2 && world->persistent_bindings == baseline_bindings + 1u);
    mc_world_access_failure_release(&captured);
    external = owner;
    CHECK(mc_client_render_global_retain(external, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_render_global_access_detach_all(owner->access, &error) ==
          MC_CLIENT_RENDER_GLOBAL_BUSY);
    CHECK(world->fields.u->count == 2 && world->persistent_bindings == baseline_bindings + 1u);
    CHECK(mc_client_render_global_release(&external, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    /* The genuine Player override stores cloak first, then n creates a real
     * rr before its NULL captured JI fails at26. Keep that lower prefix alive. */
    CHECK(mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
          MC_PLAYER_OK);
    const uint16_t player_name[] = {'R', 'e', 'c', 'e', 'i', 'p', 't'};
    CHECK(mc_player_string_create(player_name, COUNT(player_name), &username) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(sessions, username, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK);
    const size_t before_player_bindings = world->persistent_bindings;
    CHECK(mc_local_player_construct(player, world, NULL, session, 0, NULL, NULL) == MC_PLAYER_OK);
    CHECK(world->persistent_bindings == before_player_bindings + 1u);
    CHECK(mc_entity_cloak_url(player) == NULL && mc_entity_skin_url(player) != NULL);
    mc_world_entity_error publication = {0};
    bool insert_result = false;
    const mc_world_storage_status insertion =
        mc_world_runtime_insert_entity(world, player, &insert_result, &publication);
    CHECK(insertion == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(!insert_result && publication.world_access_bytecode == 29 &&
          publication.world_access_failure.render_global != NULL &&
          publication.world_access_failure.render_global->source_pc == 26 &&
          publication.world_access_failure.render_global->status ==
              MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          mc_entity_cloak_url(player) != NULL);
    /* fd.b(sn) true106/return107 is not reached after c(sn)103 throws.
     * The failed call preserves its false output sentinel while all three
     * earlier publication occurrences retain this SAME concrete Player. */
    mc_entity *one_player[] = {player};
    mc_entity_snapshot inserted_player;
    CHECK(world_is(f, 0, one_player, COUNT(one_player)) && world->fields.d->count == 1 &&
          world->fields.d->items[0] == player && world->player_membership.list == world->fields.d &&
          world->player_membership.count == 1 && world->player_membership.entries[0] == player);
    CHECK(mc_entity_get_snapshot(player, &inserted_player) == MC_ENTITY_OK &&
          inserted_player.base.bH >= 0 && inserted_player.base.bH < 8 &&
          membership(player, world, inserted_player.base.bH) &&
          slice_is(f, 0, (size_t)inserted_player.base.bH, one_player, COUNT(one_player)));
    mc_render_global_callback_state callback;
    CHECK(mc_render_global_access_callback_inspect(owner->access, &publication.world_access_failure,
                                                   &callback));
    CHECK(callback.processors[0] != NULL && callback.processors[1] == NULL &&
          callback.images[0] == NULL && callback.images[1] == NULL && !callback.source_returned);
    mc_client_image_buffer_state processor;
    CHECK(mc_client_image_buffer_inspect(callback.processors[0], &processor) &&
          processor.native_holds == 1 && processor.a == NULL && processor.b == 0 &&
          processor.c == 0 && processor.last_source_pc == 4);
    CHECK(mc_world_access_failure_retain(&publication.world_access_failure, &captured_processor) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_render_global_access_detach_all(owner->access, &error) ==
          MC_CLIENT_RENDER_GLOBAL_BUSY);
    CHECK(mc_render_global_access_callback_inspect(owner->access, &captured_processor, &callback) &&
          callback.processors[0] != NULL && world->persistent_bindings == baseline_bindings + 2u);
    mc_world_access_failure_release(&captured_processor);
    CHECK(mc_render_global_access_detach_all(owner->access, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(world->fields.u->count == 1 && world->world_accesses.identities[0] == foreign &&
          world->world_accesses.entries[0] == identity &&
          world->persistent_bindings == baseline_bindings + 1u);
    CHECK(mc_render_global_access_retire(&owner->access, &error) == MC_CLIENT_RENDER_GLOBAL_BUSY &&
          owner->access != NULL && original.render_global->source_pc == 22);
    CHECK(mc_render_global_access_inspect(owner->access, &access_state) &&
          access_state.registrations == 0 && access_state.retained_failures == 3);
    CHECK(mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_OK);
    f->terminal[0] = true;
    CHECK(world->fields.u->count == 0 && identity->memberships == 0 &&
          world->persistent_bindings == baseline_bindings + 1u);
    CHECK(mc_render_global_access_retire(&owner->access, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner->access == NULL);
    result = 0;
cleanup:
    mc_world_access_failure_release(&captured);
    mc_world_access_failure_release(&captured_processor);
    if (external != NULL)
        (void)mc_client_render_global_release(&external, &error);
    if (owner != NULL && owner->access != NULL) {
        (void)mc_render_global_access_detach_all(owner->access, &error);
        if (f != NULL && f->worlds[0] != NULL && !f->terminal[0] &&
            mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK)
            f->terminal[0] = true;
        if (mc_render_global_access_retire(&owner->access, &error) != MC_CLIENT_RENDER_GLOBAL_OK)
            return 1;
    }
    if (mc_client_render_global_release(&owner, &error) != MC_CLIENT_RENDER_GLOBAL_OK ||
        (render_classes != NULL &&
         mc_client_render_engine_classes_release(render_classes) != MC_CLIENT_RENDER_ENGINE_OK))
        return 1;
    mc_entity_release(player);
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK)
        return 1;
    if (!cleanup_fixture(f))
        return 1;
    free(first);
    free(second);
    if (result == 0)
        printf("PASS: %u actual RenderGlobal interface ownership checks\n", checks);
    return result;
}
