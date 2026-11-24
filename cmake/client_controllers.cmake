# Include after the canonical mc_client_sdk_runtime has been defined.
include_guard(GLOBAL)
if(NOT MC_BUILD_CLIENT)
    return()
endif()
if(NOT TARGET mc_client_sdk_runtime OR NOT TARGET SDL3::SDL3-static)
    message(FATAL_ERROR "Controllers require the actual client SDK/SDL targets")
endif()
include(FetchContent)
FetchContent_GetProperties(mc_sdl3 BINARY_DIR _mc_controller_sdl_binary
    POPULATED _mc_controller_sdl_populated)
if(NOT _mc_controller_sdl_populated OR NOT _mc_controller_sdl_binary)
    message(FATAL_ERROR "Controllers require the pinned SDL build configuration")
endif()
# Private compile-time capability evidence from this exact SDL configuration.
# The source never substitutes a project macro for a selected native backend.
set(_mc_controller_config_path
    "${_mc_controller_sdl_binary}/include-config-$<LOWER_CASE:$<CONFIG>>/build_config")
target_sources(mc_client_sdk_runtime PRIVATE
    src/client/display/sdk_controllers.c
    src/client/display/sdk_controllers_native.c)
target_include_directories(mc_client_sdk_runtime PRIVATE "${_mc_controller_config_path}")
