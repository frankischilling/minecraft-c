#include "application_startup_internal.h"

mc_client_startup_status application_startup_pack(mc_client_application_startup *startup) {
    startup->failure.caller =
        (mc_client_application_owner_site){"net/minecraft/client/Minecraft", "a", "()V", 207};
    if (mc_client_texture_pack_list_source_allocate(
            startup->services.files, startup->services.texture_resources, NULL,
            &startup->pack_candidate, &startup->failure.pack) != MC_TEXTURE_PACK_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_PACK_FAILED,
                                        "actual startup new ik native allocation",
                                        "net/minecraft/client/Minecraft", "a", "()V", 207);
    client_application *receiver = startup->app;
    mc_java_file *directory = receiver->game_directory; /* actual getfield Z213 */
    startup->failure.caller.pc = 216;
    if (mc_client_texture_pack_list_source_construct(startup->pack_candidate, receiver, directory,
                                                     &startup->failure.pack) != MC_TEXTURE_PACK_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_PACK_FAILED,
                                        "actual ik(Minecraft,File) constructor",
                                        "net/minecraft/client/Minecraft", "a", "()V", 216);
    mc_texture_pack_list_state state;
    if (!mc_client_texture_pack_list_inspect(startup->pack_candidate, &state) ||
        state.phase != MC_TEXTURE_PACK_READY)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "actual ik complete constructor publication admission",
                                        NULL, NULL, NULL, 0);
    receiver->D = startup->pack_candidate; /* Actual SAME completed ik, putfield219. */
    startup->D_stored = true;
    startup->last_startup_store = 219;
    return application_startup_render_engine(startup);
}
