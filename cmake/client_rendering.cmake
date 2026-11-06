# Concrete client construction and shared graphics owners.
target_sources(mc_client_source_resources PRIVATE
    src/client/gui/loading_call.c src/client/gui/loading_draw_body.c)
target_link_libraries(mc_client_source_resources PRIVATE Threads::Threads)

add_library(mc_client_entity_renderer STATIC
    src/client/world/entity_renderer/owner.c
    src/client/world/entity_renderer/held_item.c
    src/client/world/entity_renderer/map.c
    src/client/world/entity_renderer/held_owner.c)
target_include_directories(mc_client_entity_renderer PUBLIC include src src/platform/time)
target_link_libraries(mc_client_entity_renderer PUBLIC
    mc_client_source_resources mc_client_notice mc_c_atomics
    PRIVATE mc_platform_time mc_build_options)

add_library(mc_client_render_manager_cpu STATIC
    src/client/render/manager/owner.c
    src/client/render/manager/existing.c
    src/client/render/manager/map.c
    src/client/render/manager/models.c
    src/client/render/manager/renderers.c
    src/client/render/manager/constructor.c
    src/client/render/manager/frame.c)
target_include_directories(mc_client_render_manager_cpu PUBLIC include src src/platform/time)
target_link_libraries(mc_client_render_manager_cpu PUBLIC
    mc_client_notice mc_simulation mc_c_atomics mc_client_source_resources
    PRIVATE mc_build_options)

add_library(mc_client_tile_renderer_cpu STATIC
    src/client/render/tile_entity_renderer/owner.c
    src/client/render/tile_entity_renderer/constructor.c
    src/client/render/tile_entity_renderer/map.c
    src/client/render/tile_entity_renderer/world.c
    src/client/render/tile_entity_renderer/frame.c)
target_include_directories(mc_client_tile_renderer_cpu PUBLIC include src src/platform/time)
target_link_libraries(mc_client_tile_renderer_cpu PUBLIC
    mc_client_notice mc_simulation mc_c_atomics mc_client_source_resources
    PRIVATE mc_build_options)

add_library(mc_client_render_manager_hands STATIC src/client/render/manager/hands.c)
target_include_directories(mc_client_render_manager_hands PUBLIC include src src/platform/time)
target_link_libraries(mc_client_render_manager_hands PUBLIC
    mc_client_render_manager_cpu mc_client_entity_renderer PRIVATE mc_build_options)

if(TARGET mc_client_application_startup)
    target_sources(mc_client_application_startup PRIVATE
        src/client/startup/application_startup_entity_renderer.c
        src/client/startup/application_startup_manager.c
        src/client/startup/application_startup_statistics.c
        src/client/startup/application_logo.c)
    target_link_libraries(mc_client_application_startup PUBLIC
        mc_client_entity_renderer mc_client_render_manager_hands)
endif()
