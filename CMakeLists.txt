cmake_minimum_required(VERSION 3.24)
project(minecraft_c VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MC_BUILD_CLIENT "Build the native graphical client" ON)
option(MC_BUILD_CLIENT_SOUND "Build client sound decoding modules" ${MC_BUILD_CLIENT})
option(MC_BUILD_CLIENT_AUDIO "Build the native OpenAL client audio backend" ${MC_BUILD_CLIENT})
option(MC_ENABLE_CLIENT_DISPLAY_TESTS "Run client tests requiring a desktop OpenGL context" OFF)
option(MC_ENABLE_RENDER_ENGINE_REFERENCE_TESTS "Compare default-font constructor with supplied client JAR" OFF)
option(MC_ENABLE_RENDER_TEXTURE_REFERENCE_TESTS "Compare image upload with supplied Windows client" OFF)
option(MC_ENABLE_RENDER_RESOURCES_REFERENCE_TESTS "Run unchanged original ge/nw CPU reference observations" OFF)
option(MC_ENABLE_CLIENT_LOOKUP_DATA_TESTS "Run genuine Pack/lookup/color authored file profiles" OFF)
option(MC_ENABLE_RESOURCE_LOOKUP_OBSERVERS "Build genuine full-owner resource observation probes" OFF)
option(MC_ENABLE_COLOR_LOOKUP_REFERENCE_TESTS "Observe unchanged original CPU color calls" OFF)
option(MC_ENABLE_TEXTURE_LOOKUP_REFERENCE_TESTS "Observe unchanged original String texture calls on real Display" OFF)
set(MC_LOOKUP_OBSERVATION_INPUT_DIR "" CACHE PATH "Privately prepared texture/ and color/ real input roots")
set(MC_LOOKUP_NATIVE_PRODUCT_DIR "" CACHE PATH "Actual color.json and texture.json cohort-artifact maps")
foreach(role IN ITEMS GAME SDK NESTS RUNTIME)
    set(MC_LOOKUP_AUTHORITY_${role} "" CACHE FILEPATH "Exact existing original ${role} metadata")
endforeach()
set(MC_LOOKUP_NATIVE_ARCHIVE "" CACHE FILEPATH "Authenticated versioned LWJGL native archive")
set(MC_CLIENT_LOOKUP_FIXTURE_DIR "" CACHE PATH "Prepared texture/ and color/ lookup fixture roots")
option(MC_ENABLE_SOUND_REFERENCE_TESTS "Compare Vorbis page policy with the supplied client" OFF)
option(MC_ENABLE_WAV_REFERENCE_TESTS "Compare WAV source policy with the supplied client" OFF)
option(MC_ENABLE_RENDERING_REFERENCE_TESTS "Compare CPU block meshes with the supplied client" OFF)
option(MC_ENABLE_CAMERA_REFERENCE_TESTS "Compare first-person matrices with supplied Windows client" OFF)
option(MC_ENABLE_SINGLEPLAYER_CONTROLLER_REFERENCE_TESTS "Compare singleplayer controller with supplied Windows client" OFF)
option(MC_ENABLE_INPUT_REFERENCE_TESTS "Compare MovementInput with the supplied client" OFF)
option(MC_ENABLE_TURN_REFERENCE_TESTS "Compare Entity turning with the supplied client" OFF)
option(MC_ENABLE_CRAFTING_REFERENCE_TESTS "Compare crafting with supplied client/server JARs" OFF)
option(MC_ENABLE_PLAYER_REFERENCE_TESTS "Compare local Player construction with supplied client JAR" OFF)
option(MC_ENABLE_PLAYER_PUBLICATION_REFERENCE_TESTS "Compare World Player publication with supplied client JAR" OFF)
option(MC_ENABLE_PLAYER_INPUT_REFERENCE_TESTS "Compare local Player input forwarding with supplied client JAR" OFF)
option(MC_ENABLE_MAINTENANCE_REFERENCE_TESTS "Compare World provider maintenance with supplied client JAR" OFF)
option(MC_ENABLE_WORLD_ENTITY_TICK_REFERENCE_TESTS "Compare client World Entity updates with supplied client JAR" OFF)
option(MC_ENABLE_WEATHER_REFERENCE_TESTS "Compare client weather updates with supplied client JAR" OFF)
option(MC_ENABLE_MATERIAL_MOVEMENT_REFERENCE_TESTS "Compare material movement with supplied client JAR" OFF)
option(MC_ENABLE_ENTITY_ENVIRONMENT_REFERENCE_TESTS "Compare Entity environment helpers with supplied original JARs" OFF)
option(MC_ENABLE_PLAYER_MOVEMENT_REFERENCE_TESTS "Compare LocalPlayer movement with supplied client JAR" OFF)
option(MC_ENABLE_CLIENT_STATS_REFERENCE_TESTS "Compare client statistics files with supplied client JAR" OFF)
option(MC_ENABLE_PLAYER_TRAVEL_REFERENCE_TESTS "Compare Player travel with supplied Windows client" OFF)
option(MC_ENABLE_PLAYER_BASE_TICK_REFERENCE_TESTS "Compare Player environment ticks with supplied client JAR" OFF)
option(MC_ENABLE_INVENTORY_TICK_REFERENCE_TESTS "Compare inventory updates with supplied client JAR" OFF)
option(MC_ENABLE_PLAYER_ACTION_REFERENCE_TESTS "Compare Player action bodies with supplied client JAR" OFF)
option(MC_ENABLE_ITEM_PICKUP_REFERENCE_TESTS "Compare Inventory insertion and Item pickup with supplied client JAR" OFF)
option(MC_ENABLE_ITEM_TICK_REFERENCE_TESTS "Compare Item updates with supplied client JAR" OFF)
option(MC_ENABLE_PLAYER_ACTIVE_TICK_REFERENCE_TESTS "Compare inherited Player ticks with supplied client JAR" OFF)
option(MC_ENABLE_PLAYER_SPAWN_REFERENCE_TESTS "Compare Player spawn/reset with supplied Windows client" OFF)
option(MC_ENABLE_POSITION_REFERENCE_TESTS "Compare Entity position arithmetic with supplied Windows client" OFF)
option(MC_ENABLE_PARTICLE_REFERENCE_TESTS "Compare pickup particles with supplied client JAR and observed JDK17 constructor inputs" OFF)
option(MC_ENABLE_PLAYER_STATISTICS_REFERENCE_TESTS "Compare Player statistic dispatch with supplied Windows client" OFF)
option(MC_ENABLE_WATER_ANIMATION_REFERENCE_TESTS "Compare water texture animation with supplied client JAR" OFF)
option(MC_ENABLE_LAVA_ANIMATION_REFERENCE_TESTS "Compare Lava texture animation with supplied client JAR" OFF)
option(MC_ENABLE_PLAYER_NBT_REFERENCE_TESTS "Compare Player NBT with the supplied Windows client" OFF)
option(MC_ENABLE_BIPED_REFERENCE_TESTS "Compare normal-zero Biped CPU construction with supplied client JAR" OFF)
option(MC_ENABLE_NOTICE_REFERENCE_TESTS "Compare achievement notice with supplied Windows client" OFF)
option(MC_ENABLE_PROPAGATION_REFERENCE_TESTS "Compare real World lighting with supplied client JAR" OFF)
option(MC_ENABLE_STARTUP_GRID_REFERENCE_TESTS "Compare expensive 289-point World preparation profiles" OFF)
option(MC_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(MC_ENABLE_JAVA_REFERENCE_TESTS "Compare numeric and collection helpers with a local JDK" OFF)
option(MC_ENABLE_MATH_REFERENCE_TESTS "Compare float lookup and floor helpers with originals" OFF)
option(MC_ENABLE_NBT_REFERENCE_TESTS "Compare NBT with locally supplied original JARs" OFF)
option(MC_ENABLE_COMPRESSION_REFERENCE_TESTS "Compare compression with original helpers" OFF)
option(MC_ENABLE_NOISE_REFERENCE_TESTS "Compare terrain noise with original samplers" OFF)
option(MC_ENABLE_SIMPLEX_REFERENCE_TESTS "Compare 2D simplex noise with original samplers" OFF)
option(MC_ENABLE_BIOME_REFERENCE_TESTS "Compare climate and biomes with original methods" OFF)
option(MC_ENABLE_DENSITY_REFERENCE_TESTS "Compare Overworld density and base terrain with originals" OFF)
option(MC_ENABLE_SURFACE_REFERENCE_TESTS "Compare Overworld surface replacement with originals" OFF)
option(MC_ENABLE_CAVES_REFERENCE_TESTS "Compare Overworld cave scheduling and carving with originals" OFF)
option(MC_ENABLE_NIBBLES_REFERENCE_TESTS "Compare packed nibble access with original classes" OFF)
option(MC_ENABLE_CHUNK_REFERENCE_TESTS "Compare local chunk initialization with original methods" OFF)
option(MC_ENABLE_LIGHT_REFERENCE_TESTS "Compare light regions and ordered queues with originals" OFF)
option(MC_ENABLE_FINISH_REFERENCE_TESTS "Compare full Overworld chunk finishing with original traces" OFF)
option(MC_ENABLE_GENERATION_REFERENCE_TESTS "Compare complete Overworld generation with original traces" OFF)
option(MC_ENABLE_REGION_CAPTURE_TESTS "Compare region reader with recorded observations" OFF)
option(MC_ENABLE_REGION_REFERENCE_TESTS "Compare region inspection with untouched original readers" OFF)
option(MC_ENABLE_CACHE_REFERENCE_TESTS "Compare synchronous acquisition with retained original providers" OFF)
option(MC_ENABLE_STORAGE_REFERENCE_TESTS "Compare world persistence with original storage methods" OFF)
option(MC_ENABLE_LIFECYCLE_REFERENCE_TESTS "Compare copied-world save/unload lifecycle with original controls" OFF)
option(MC_ENABLE_PLANT_REFERENCE_TESTS "Compare plant lifecycle projections with supplied originals" OFF)
option(MC_ENABLE_POPULATION_REFERENCE_TESTS "Compare natural GAME population with supplied originals" OFF)
set(MC_REFERENCE_CLIENT_JAR "" CACHE FILEPATH "Local verified original client JAR")
set(MC_REFERENCE_SERVER_JAR "" CACHE FILEPATH "Local verified original server JAR")
set(MC_REFERENCE_CLIENT_LIBRARY_DIR "" CACHE PATH "Local pinned original client library archives")
set(MC_REFERENCE_CLIENT_NATIVE_DIR "" CACHE PATH "Local pinned original Windows x64 native libraries")
set(MC_REFERENCE_CLIENT_RESOURCE_DIR "" CACHE PATH "Local imported original client JAR resource root")
set(MC_REFERENCE_TESTED_COMMIT "" CACHE STRING "Exact source commit for local reference reports")
set(MC_REGION_CAPTURE_MANIFEST "" CACHE FILEPATH "Local recorded region observation manifest")
if(MC_ENABLE_RENDERING_REFERENCE_TESTS AND NOT MC_BUILD_CLIENT)
    message(FATAL_ERROR "MC_ENABLE_RENDERING_REFERENCE_TESTS requires MC_BUILD_CLIENT")
endif()

if((MC_ENABLE_RENDER_ENGINE_REFERENCE_TESTS OR MC_ENABLE_RENDER_TEXTURE_REFERENCE_TESTS)
   AND NOT MC_BUILD_CLIENT)
    message(FATAL_ERROR "RenderEngine reference tests require MC_BUILD_CLIENT")
endif()
if(MC_ENABLE_RENDER_TEXTURE_REFERENCE_TESTS AND
   (NOT WIN32 OR NOT MC_ENABLE_CLIENT_DISPLAY_TESTS))
    message(FATAL_ERROR "Render texture reference tests require Windows and MC_ENABLE_CLIENT_DISPLAY_TESTS")
endif()

# Strict diagnostics apply to project-owned code; dependencies share sanitizers only.
add_library(mc_build_options INTERFACE)
add_library(mc_sanitizers INTERFACE)
target_link_libraries(mc_build_options INTERFACE mc_sanitizers)
if(MSVC)
    target_compile_options(mc_build_options INTERFACE /W4 /WX /fp:strict)
else()
    target_compile_options(mc_build_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
        -fno-fast-math -ffp-contract=off)
endif()
if(MC_ENABLE_SANITIZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR MSVC)
        message(FATAL_ERROR "MC_ENABLE_SANITIZERS requires GCC or Clang with ASan/UBSan")
    endif()
    target_compile_options(mc_sanitizers INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mc_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

# Official C source archive, verified against the release site's SHA-256.
# For offline builds set FETCHCONTENT_SOURCE_DIR_MC_ZLIB to an unpacked copy.
include(FetchContent)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mc_zlib
    URL https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
        https://zlib.net/fossils/zlib-1.3.2.tar.gz
    URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(mc_zlib)
target_link_libraries(zlibstatic PRIVATE mc_sanitizers)

if(MC_BUILD_CLIENT)
    include(cmake/client_dependencies.cmake)
    include(cmake/font_dependencies.cmake)
    add_library(mc_image STATIC src/resources/image/image.c)
    target_include_directories(mc_image PUBLIC include)
    target_link_libraries(mc_image PRIVATE mc_build_options png_static)
    add_library(mc_client_gui STATIC src/client/gui/scaled_resolution.c src/client/gui/font_renderer.c)
    target_include_directories(mc_client_gui PUBLIC include)
    target_link_libraries(mc_client_gui PUBLIC mc_core PRIVATE mc_build_options)
    add_library(mc_client_timer STATIC src/client/timer/timer.c)
    target_include_directories(mc_client_timer PUBLIC include)
    target_link_libraries(mc_client_timer PUBLIC mc_core PRIVATE mc_build_options)
    add_library(mc_client_timer_construction STATIC src/client/timer/construction.c)
    target_link_libraries(mc_client_timer_construction PUBLIC mc_client_timer
        PRIVATE mc_platform_time mc_build_options)
    add_library(mc_client_event_transport STATIC src/client/display/events.c)
    target_include_directories(mc_client_event_transport PUBLIC src/client/display)
    target_link_libraries(mc_client_event_transport PUBLIC SDL3::SDL3-static PRIVATE mc_build_options)
    add_library(mc_client_sdk_runtime STATIC
        src/client/display/sdk_mode.c src/client/display/sdk_platform.c
        src/client/display/sdk_input.c src/client/display/sdk_input_events.c
        src/client/display/sdk_input_poll.c src/client/display/sdk_update.c
        src/client/display/sdk_window_icons.c)
    target_include_directories(mc_client_sdk_runtime PUBLIC src/client/display)
    target_link_libraries(mc_client_sdk_runtime PUBLIC
        mc_client_sdk_queue mc_client_sdk_icons SDL3::SDL3-static
        PRIVATE mc_client_event_transport mc_simulation mc_game_settings mc_build_options)
    if(WIN32)
        target_link_libraries(mc_client_sdk_runtime PRIVATE user32 gdi32)
    else()
        find_package(X11 REQUIRED COMPONENTS Xrandr)
        target_link_libraries(mc_client_sdk_runtime PRIVATE X11::X11 X11::Xrandr)
    endif()
    include(cmake/client_controllers.cmake)
    add_library(mc_client_display STATIC src/client/display/display.c)
    target_include_directories(mc_client_display PUBLIC src/client/display)
    target_link_libraries(mc_client_display PUBLIC mc_image SDL3::SDL3-static
        PRIVATE mc_client_event_transport mc_client_sdk_runtime mc_build_options)
    if(UNIX)
        target_link_libraries(mc_client_display PRIVATE m)
    endif()
    add_library(mc_client_default_font STATIC src/client/fonts/default_font.c)
    target_include_directories(mc_client_default_font PUBLIC include)
    target_link_libraries(mc_client_default_font PUBLIC freetype PRIVATE mc_build_options)
    if(WIN32)
        target_link_libraries(mc_client_default_font PRIVATE gdi32)
    else()
        find_package(Fontconfig REQUIRED)
        target_link_libraries(mc_client_default_font PRIVATE Fontconfig::Fontconfig)
    endif()
    add_library(mc_client_render_engine STATIC
        src/client/render_engine/owner.c src/client/render_engine/arena.c
        src/client/render_engine/context.c src/client/render_engine/ge.c
        src/client/render_engine/nw_owner.c)
    target_link_libraries(mc_client_render_engine PUBLIC
        mc_client_default_font mc_game_settings PRIVATE mc_build_options)
    target_sources(mc_client_render_engine PRIVATE
        src/client/render_engine/color_array.c
        src/client/render_engine/colorizer.c)
    add_library(mc_client_render_textures STATIC
        src/client/render_engine/textures.c src/client/render_engine/context_display.c
        src/client/render_engine/ge_ranges.c src/client/render_engine/nw.c
        src/client/render_engine/lighting.c src/client/render_engine/lighting_enable.c)
    target_include_directories(mc_client_render_textures PUBLIC src/client/render_engine)
    target_link_libraries(mc_client_render_textures PUBLIC
        mc_client_render_engine mc_client_display mc_entity_classes mc_math_classes
        PRIVATE mc_build_options)
    # Class-resource and Pack inputs share the scoped ImageIO core. The optional
    # Pack owners/adapter are not part of this component.
    add_library(mc_client_image_io STATIC
        src/client/images/class_stream.c src/client/images/image_io.c)
    target_link_libraries(mc_client_image_io PUBLIC mc_client_render_engine
        PRIVATE mc_simulation ZLIB::ZLIBSTATIC mc_build_options)

    # images.c is the exact shared-Pack adapter, never the old second decoder.
    add_library(mc_client_texture_packs STATIC
        src/client/texture_packs/default_pack.c
        src/client/texture_packs/filesystem.c
        src/client/texture_packs/images.c
        src/client/texture_packs/owner.c
        src/client/texture_packs/rescan.c
        src/client/texture_packs/resources.c
        src/client/texture_packs/text.c
        src/client/texture_packs/zip.c
        src/client/texture_packs/zip_pack.c)
    target_include_directories(mc_client_texture_packs PUBLIC include
        PRIVATE src src/platform/time)
    target_link_libraries(mc_client_texture_packs PUBLIC
        mc_client_render_engine mc_game_settings mc_client_game_directory
        PRIVATE mc_client_image_io mc_client_notice mc_simulation mc_resources
        mc_c_atomics ZLIB::ZLIBSTATIC mc_build_options)

    # Resource calls stay above the CPU owner; compile each body exactly once.
    target_sources(mc_client_render_textures PRIVATE
        src/client/render_engine/texture_lookup.c
        src/client/render_engine/color_lookup.c
        src/client/render_engine/resource_lookup.c
        src/client/render_engine/string_key.c)
    target_link_libraries(mc_client_render_textures PRIVATE mc_client_texture_packs)

    add_library(mc_client_source_resources STATIC
        src/client/gui/font_mapping.c
        src/client/gui/bitmap_font.c src/client/gui/bitmap_font_measure.c
        src/client/gui/bitmap_font_draw.c
        src/client/gui/loading_screen.c src/client/gui/loading_projection.c
        src/client/gui/loading_geometry.c src/client/gui/loading_draw.c
        src/client/startup/application_quad.c)
    target_include_directories(mc_client_source_resources PUBLIC src src/platform/time)
    target_link_libraries(mc_client_source_resources PUBLIC
        mc_client_render_textures mc_client_image_io mc_client_gui mc_c_atomics
        PRIVATE mc_simulation mc_resources mc_platform_time mc_build_options)
    if(UNIX)
        target_link_libraries(mc_client_source_resources PRIVATE m)
    endif()
    add_library(mc_client_world_renderer STATIC src/client/world/renderer.c)
    target_include_directories(mc_client_world_renderer PUBLIC src/client/world)
    target_link_libraries(mc_client_world_renderer PUBLIC mc_client_display PRIVATE mc_build_options)
    if(UNIX)
        target_link_libraries(mc_client_world_renderer PRIVATE m)
    endif()
    add_library(mc_client_world_mesher STATIC src/client/world/mesher.c
        src/client/world/mesher_faces.c src/client/world/mesher_lighting.c)
    target_include_directories(mc_client_world_mesher PUBLIC src/client/world)
    target_link_libraries(mc_client_world_mesher PUBLIC mc_simulation PRIVATE mc_build_options)
    add_library(mc_client_world_view STATIC src/client/world/view.c)
    target_include_directories(mc_client_world_view PUBLIC src/client/world)
    target_link_libraries(mc_client_world_view PUBLIC mc_client_world_renderer mc_client_world_mesher
        mc_client_assets PRIVATE mc_client_first_person mc_build_options)
    add_library(mc_client_assets STATIC src/client/resources/assets.c)
    target_include_directories(mc_client_assets PUBLIC src/client/resources)
    target_link_libraries(mc_client_assets PUBLIC mc_client_gui mc_resources mc_image PRIVATE mc_build_options)
    add_library(mc_client_game_settings_labels STATIC src/client/settings/labels.c)
    target_include_directories(mc_client_game_settings_labels PUBLIC src/client/settings)
    target_link_libraries(mc_client_game_settings_labels PUBLIC mc_game_settings mc_client_assets PRIVATE mc_build_options)
    add_library(mc_client_gui_main_menu STATIC src/client/gui/gui_main_menu.c)
    target_include_directories(mc_client_gui_main_menu PUBLIC src/client)
    target_link_libraries(mc_client_gui_main_menu PUBLIC mc_client_assets mc_client_display PRIVATE mc_build_options)
    add_executable(minecraft-client WIN32 src/client/main.c src/client/application.c)
    target_include_directories(minecraft-client PRIVATE src/client src/platform/time)
    target_link_libraries(minecraft-client PRIVATE mc_client_gui_main_menu mc_client_timer
        mc_client_game_directory mc_game_settings mc_platform_time mc_client_world_session
        mc_client_world_view mc_client_notice mc_client_source_resources mc_build_options)
endif()

add_library(mc_platform_time STATIC src/platform/time/nanotime.c src/platform/time/millis.c)
target_sources(mc_platform_time PRIVATE src/platform/time/calendar.c)
target_link_libraries(mc_platform_time PRIVATE mc_build_options)

add_library(mc_platform_text STATIC src/platform/text/charset.c src/platform/text/locale.c
    src/platform/text/stdout.c)
target_include_directories(mc_platform_text PRIVATE include)
target_link_libraries(mc_platform_text PRIVATE Threads::Threads mc_build_options)

find_package(Threads REQUIRED)
add_library(mc_c_atomics INTERFACE)
if(MSVC)
    target_compile_options(mc_c_atomics INTERFACE -experimental:c11atomics)
endif()

# The shared core has no window, graphics, audio, JDK, or asset dependencies.
add_library(mc_core STATIC
    src/core/java/int_set.c
    src/core/java/java.c
    src/core/java/float_parse.c
    src/core/java/float_format.c
    src/core/java/integer_parse.c
    src/core/java/format.c
    src/core/java/format_case.c
    src/core/java/properties.c
    src/core/java/reader.c
    src/core/java/writer.c
    src/core/java/logging.c
    src/core/java/random_profile.c
    src/core/math/math.c
    src/core/math/trig_double.c
    src/core/math/strict.c
    src/core/math/fdlibm/log.c
    src/core/text/mutf8.c)
target_include_directories(mc_core PUBLIC include)
target_link_libraries(mc_core PRIVATE mc_platform_time mc_platform_text Threads::Threads mc_c_atomics mc_build_options)

target_sources(mc_core PRIVATE
    src/core/java/print_stream.c
    src/core/java/system_output.c
    src/core/java/print_stream_native.c)

add_library(mc_client_sdk_queue STATIC src/client/display/sdk_queue.c)
target_include_directories(mc_client_sdk_queue PUBLIC src/client/display)
target_link_libraries(mc_client_sdk_queue PUBLIC mc_core PRIVATE mc_build_options)
add_library(mc_client_sdk_icons STATIC src/client/display/sdk_icons.c)
target_include_directories(mc_client_sdk_icons PUBLIC src/client/display)
target_link_libraries(mc_client_sdk_icons PUBLIC mc_core PRIVATE mc_build_options)

add_library(mc_game_settings STATIC
    src/client/settings/game_settings.c
    src/client/settings/game_option.c
    src/client/sound/sound_manager.c)
target_include_directories(mc_game_settings PUBLIC include)
target_link_libraries(mc_game_settings PUBLIC mc_core mc_client_sound_pool PRIVATE mc_build_options)
target_sources(mc_game_settings PRIVATE src/client/settings/construction_classes.c)
target_link_libraries(mc_game_settings PUBLIC mc_java_files PRIVATE mc_platform_text Threads::Threads)
add_library(mc_movement_input STATIC src/client/input/movement_input.c)
target_include_directories(mc_movement_input PUBLIC include)
target_link_libraries(mc_movement_input PUBLIC mc_game_settings PRIVATE mc_build_options)
add_library(mc_client_sound_files STATIC src/client/sound/sound_file.c)
target_include_directories(mc_client_sound_files PUBLIC include)
target_link_libraries(mc_client_sound_files PUBLIC mc_core PRIVATE mc_build_options)
add_library(mc_client_sound_pool STATIC src/client/sound/sound_pool.c)
target_include_directories(mc_client_sound_pool PUBLIC include)
target_link_libraries(mc_client_sound_pool PUBLIC mc_client_sound_files PRIVATE mc_build_options)
add_library(mc_sound_wav STATIC src/client/sound/wav_codec.c)
target_include_directories(mc_sound_wav PUBLIC include)
target_link_libraries(mc_sound_wav PUBLIC mc_client_sound_files PRIVATE mc_build_options)
if(MC_BUILD_CLIENT_SOUND)
    include(cmake/sound_dependencies.cmake)
    add_library(mc_sound_decoder STATIC
        src/client/sound/vorbis_decoder.c
        src/client/sound/vorbis_codec.c
        src/client/sound/pcm.c)
    target_include_directories(mc_sound_decoder PUBLIC include)
    # This owner supplies callbacks; omit Vorbisfile's unused header copies.
    target_compile_definitions(mc_sound_decoder PRIVATE OV_EXCLUDE_STATIC_CALLBACKS)
    target_link_libraries(mc_sound_decoder PUBLIC mc_client_sound_files PRIVATE vorbisfile mc_build_options)
endif()
if(MC_ENABLE_SOUND_REFERENCE_TESTS AND NOT MC_BUILD_CLIENT_SOUND)
    message(FATAL_ERROR "MC_ENABLE_SOUND_REFERENCE_TESTS requires MC_BUILD_CLIENT_SOUND")
endif()
if(MC_BUILD_CLIENT_AUDIO)
    if(NOT MC_BUILD_CLIENT_SOUND)
        message(FATAL_ERROR "MC_BUILD_CLIENT_AUDIO requires MC_BUILD_CLIENT_SOUND")
    endif()
    include(cmake/openal_dependencies.cmake)
    add_library(mc_sound_backend STATIC src/client/sound/backend/owner.c
        src/client/sound/backend/codec.c src/client/sound/backend/device.c
        src/client/sound/backend/source.c src/client/sound/backend/worker.c)
    target_include_directories(mc_sound_backend PUBLIC include)
    target_link_libraries(mc_sound_backend PUBLIC mc_game_settings PRIVATE
        mc_sound_decoder mc_sound_wav OpenAL::OpenAL mc_c_atomics Threads::Threads mc_build_options)
endif()
add_library(mc_client_game_directory STATIC src/client/startup/game_directory.c)
target_include_directories(mc_client_game_directory PUBLIC src/client/startup)
target_link_libraries(mc_client_game_directory PUBLIC mc_core
    PRIVATE mc_client_directory_native mc_build_options)
if(UNIX)
    target_link_libraries(mc_core PRIVATE m)
endif()

add_library(mc_storage STATIC
    src/storage/nbt/nbt.c
    src/storage/nbt/tree.c
    src/storage/nbt/overlay.c
    src/storage/level/level.c
    src/storage/compression/compression.c
    src/storage/region/region.c
    src/storage/region/writer.c)
target_link_libraries(mc_storage PUBLIC mc_core PRIVATE mc_build_options ZLIB::ZLIBSTATIC)
target_sources(mc_storage PRIVATE
    src/storage/compression/kernel.c src/storage/compression/inflater.c)

add_library(mc_item STATIC src/item/stack/stack.c)
target_link_libraries(mc_item PUBLIC mc_storage PRIVATE mc_build_options)

add_library(mc_block STATIC src/block/material/material.c)
target_include_directories(mc_block PUBLIC include)
target_link_libraries(mc_block PRIVATE mc_build_options)

# Passive Entity class/literal/namespace ownership stays below the Registry.
# Concrete Entity construction and World/NBT methods remain in simulation.
add_library(mc_entity_classes STATIC
    src/entity/nbt/errors.c
    src/entity/minecart/inventory_errors.c
    src/entity/classes/classes.c
    src/entity/classes/memory.c
    src/entity/minecart/classes.c
    src/entity/bounds/bounds.c
    src/entity/vector/vector.c
    src/entity/registry/registry.c)
target_link_libraries(mc_entity_classes PUBLIC mc_core PRIVATE mc_build_options)

add_library(mc_registry STATIC
    src/block/registry/rendering.c
    src/block/registry/registry.c
    src/block/registry/managed.c
    src/block/registry/step_sound.c
    src/block/registry/redstone_set.c
    src/block/registry/runtime.c
    src/block/registry/directions.c
    src/block/registry/horizontal_directions.c
    src/block/registry/falling.c
    src/block/registry/class_initializers.c
    src/block/registry/constructors.c
    src/block/registry/registration.c
    src/block/registry/opacity.c
    src/block/registry/texture.c
    src/block/registry/bootstrap.c
    src/item/registry/registry.c
    src/item/registry/runtime.c
    src/item/registry/class_initializers.c
    src/item/registry/constructors.c
    src/item/registry/registration.c
    src/item/registry/stacks.c
    src/item/recipes/registration.c
    src/item/recipes/families.c
    src/item/recipes/explicit.c
    src/item/recipes/managers.c
    src/item/recipes/smelting.c
    src/resources/translation/translation.c
    src/resources/translation/format.c
    src/statistics/keys/keys.c
    src/statistics/achievements/achievements.c
    src/statistics/registry/base.c
    src/statistics/registry/arrays.c
    src/statistics/registry/names.c
    src/statistics/registry/format.c
    src/statistics/registry/initializer.c
    src/statistics/registry/registration.c
    src/world/biome/registry.c
    src/world/biome/sampling.c
    src/world/biome/fixed_sampling.c)
target_link_libraries(mc_registry PUBLIC mc_block mc_core mc_item mc_world mc_entity_classes PRIVATE mc_platform_text mc_build_options)

target_sources(mc_registry PRIVATE src/client/achievements/description_registry.c)
add_library(mc_client_notice STATIC
    src/client/render/model.c
    src/client/render/model_graph.c
    src/client/render/biped.c
    src/client/gui/achievement_notice.c
    src/client/achievements/description.c
    src/client/texture_effects/water.c
    src/client/texture_effects/lava.c
    src/client/application_access.c)
target_include_directories(mc_client_notice PUBLIC include PRIVATE src/client src/platform/time)
target_link_libraries(mc_client_notice PUBLIC mc_registry mc_game_settings
    PRIVATE mc_platform_time mc_build_options)

add_library(mc_resources STATIC src/resources/files/files.c)
target_link_libraries(mc_resources PUBLIC mc_registry PRIVATE mc_build_options)

add_library(mc_inventory STATIC
    src/inventory/crafting/errors.c
    src/inventory/crafting/inventory.c
    src/inventory/crafting/result.c
    src/inventory/player/constructor.c
    src/inventory/container/container.c
    src/item/recipes/matching.c)
target_link_libraries(mc_inventory PUBLIC mc_registry mc_item PRIVATE mc_build_options)

add_library(mc_world_info STATIC src/world/info/info.c)
target_include_directories(mc_world_info PRIVATE src/platform/time)
target_link_libraries(mc_world_info PUBLIC mc_storage PRIVATE mc_platform_time mc_build_options)

# Canonical File/stream/SaveFormat prerequisites; no SDL/device dependency.
add_library(mc_platform_files STATIC src/platform/files/directory.c src/platform/files/byte_output.c)
target_include_directories(mc_platform_files PUBLIC src/platform/files)
target_link_libraries(mc_platform_files PRIVATE mc_build_options)
target_link_libraries(mc_core PRIVATE mc_platform_files)
add_library(mc_client_directory_native STATIC src/client/startup/directory_native.c)
target_include_directories(mc_client_directory_native PUBLIC src/client/startup)
target_link_libraries(mc_client_directory_native PUBLIC mc_core
    PRIVATE mc_platform_files mc_build_options)
if(WIN32)
    target_link_libraries(mc_client_directory_native PRIVATE shell32 ole32)
    if(MSVC)
        target_link_libraries(mc_client_directory_native PRIVATE delayimp)
        target_link_options(mc_client_directory_native INTERFACE "/DELAYLOAD:shell32.dll")
    endif()
endif()
add_library(mc_java_files STATIC
    src/world/storage/java_file.c src/world/storage/java_file_io.c
    src/world/storage/java_file_list.c src/world/storage/java_file_text.c)
target_link_libraries(mc_java_files PUBLIC mc_world_info mc_c_atomics
    PRIVATE mc_core mc_platform_files Threads::Threads mc_build_options)
add_library(mc_java_outputs STATIC
    src/world/storage/java_output.c src/world/storage/java_data_output.c)
target_link_libraries(mc_java_outputs PUBLIC mc_java_files
    PRIVATE mc_core mc_platform_files Threads::Threads mc_build_options)
include(cmake/java_file_writer.cmake)
include(cmake/java_file_urls.cmake)
include(cmake/client_settings_source_save.cmake)
add_library(mc_client_directory_inputs STATIC src/client/startup/directory_inputs.c)
target_include_directories(mc_client_directory_inputs PUBLIC src/client/startup)
target_link_libraries(mc_client_directory_inputs PUBLIC mc_world_info
    PRIVATE mc_client_directory_native mc_java_files mc_build_options)
add_library(mc_math_classes STATIC src/core/math/classes.c)
target_link_libraries(mc_math_classes PUBLIC mc_core PRIVATE mc_build_options)
add_library(mc_gzip_input STATIC src/world/storage/gzip_input.c)
target_link_libraries(mc_gzip_input PUBLIC mc_java_files mc_storage
    PRIVATE ZLIB::ZLIBSTATIC mc_build_options)
add_library(mc_client_saves STATIC
    src/client/saves/owner.c src/client/saves/metadata.c
    src/client/saves/selection.c src/client/saves/list.c)
target_link_libraries(mc_client_saves PUBLIC
    mc_java_files mc_world_info mc_math_classes mc_gzip_input mc_persistence PRIVATE mc_build_options)

add_executable(mc_region_inspect src/tools/storage/region_inspect.c)
target_link_libraries(mc_region_inspect PRIVATE mc_storage mc_build_options)
if(MINGW)
    target_link_options(mc_region_inspect PRIVATE -municode)
endif()

add_library(mc_world STATIC
    src/world/runtime/loading/errors.c
    src/world/runtime/tile_entities/errors.c
    src/world/schedule/entry.c
    src/world/schedule/membership.c
    src/world/schedule/ordered.c
    src/world/generation/overworld.c
    src/world/cache/cache.c
    src/world/cache/acquire.c
    src/world/cache/lifecycle.c
    src/world/chunk/finish.c
    src/world/lighting/world.c
    src/world/lighting/queue.c
    src/world/chunk/data.c
    src/world/chunk/object.c
    src/world/chunk/nibbles.c
    src/world/noise/noise.c
    src/world/noise/simplex.c
    src/world/biome/biome.c
    src/world/terrain/terrain.c
    src/world/terrain/surface.c
    src/world/terrain/caves.c)
target_link_libraries(mc_world PUBLIC mc_core mc_entity_classes PRIVATE mc_build_options)

add_library(mc_persistence STATIC
    src/world/storage/status.c
    src/world/storage/files.c
    src/world/storage/regions.c
    src/world/storage/store.c
    src/world/storage/chunk.c)
target_link_libraries(mc_persistence PUBLIC mc_world mc_storage mc_world_info mc_java_outputs
    mc_gzip_input PRIVATE mc_platform_time mc_build_options)

add_library(mc_world_saved_data STATIC
    src/world/saved_data/storage.c
    src/world/saved_data/value.c)
target_include_directories(mc_world_saved_data PRIVATE src/world/storage)
target_link_libraries(mc_world_saved_data PUBLIC mc_persistence PRIVATE mc_build_options)

add_library(mc_simulation STATIC
    src/client/statistics/application.c
    src/client/statistics/source_classes.c
    src/client/statistics/source_diagnostics.c
    src/client/statistics/source_files.c
    src/entity/player/statistics.c
    src/entity/action/action.c
    src/entity/action/common.c
    src/entity/action/input.c
    src/entity/action/push.c
    src/entity/action/atan.c
    src/entity/action/angle.c
    src/entity/item/pickup.c
    src/entity/item/tick.c
    src/entity/item/pushout.c
    src/inventory/runtime/insertion.c
    src/inventory/runtime/nbt.c
    src/entity/player/tick.c
    src/entity/player/local_tick.c
    src/entity/player/living_tick.c
    src/entity/player/player_tick.c
    src/entity/player/nbt.c
    src/entity/player/spawn.c
    src/entity/position.c
    src/entity/player/pushout.c
    src/entity/player/eligibility.c
    src/entity/player/atan2.c
    src/entity/particles/pickup.c
    src/entity/player/pickup.c
    src/client/particles/effect_renderer.c
    src/client/particles/application.c
    src/client/statistics/checksum.c
    src/client/statistics/diagnostic.c
    src/client/statistics/files.c
    src/client/statistics/json.c
    src/client/statistics/maps.c
    src/client/statistics/owner.c
    src/client/statistics/workers.c
    src/client/statistics/writer.c
    src/server/runtime/construction.c
    src/entity/living/travel.c
    src/entity/tick/common.c
    src/entity/tick/environment.c
    src/entity/tick/suffocation.c
    src/entity/tick/watcher.c
    src/inventory/runtime/tick.c
    src/inventory/runtime/map.c
    src/entity/movement/movement.c
    src/entity/movement/effects.c
    src/entity/movement/turn.c
    src/entity/movement/material.c
    src/entity/movement/environment.c
    src/entity/nbt/factory.c
    src/entity/nbt/nbt.c
    src/entity/nbt/storage.c
    src/world/runtime/tile_entities/tile_entities.c
    src/world/runtime/tile_entities/piston_math.c
    src/world/runtime/tile_entities/piston.c
    src/world/runtime/tile_entities/piston_tick.c
    src/world/runtime/loading/loading.c
    src/world/runtime/saving/saving.c
    src/world/runtime/saving/player_metadata.c
    src/world/runtime/runtime.c
    src/world/runtime/activation.c
    src/world/runtime/registry.c
    src/world/runtime/construction.c
    src/world/runtime/dedicated/entity_index.c
    src/world/runtime/environment.c
    src/world/runtime/movement.c
    src/world/runtime/contact.c
    src/world/runtime/step.c
    src/world/runtime/material_movement.c
    src/world/runtime/blocks.c
    src/world/runtime/height.c
    src/world/runtime/queries.c
    src/world/runtime/plants.c
    src/world/runtime/population/plants.c
    src/world/runtime/population/trees.c
    src/world/runtime/population/ore_clay.c
    src/world/runtime/population/lake_spring.c
    src/world/runtime/population/dungeon.c
    src/world/runtime/population/overworld.c
    src/world/runtime/lighting.c
    src/world/runtime/propagation.c
    src/world/runtime/chunks.c
    src/world/runtime/presence.c
    src/world/runtime/solid_support.c
    src/world/runtime/power/world.c
    src/world/runtime/power/blocks.c
    src/world/runtime/presentation.c
    src/world/runtime/world_access.c
    src/world/runtime/entity_publication.c
    src/world/chunk/entity_remove.c
    src/world/runtime/entity_tick.c
    src/world/runtime/entity_tick_chunks.c
    src/world/runtime/entity_tick_dispatch.c
    src/world/runtime/entity_tick_lists.c
    src/world/runtime/entity_tick_loops.c
    src/world/runtime/entity_tick_nearest.c
    src/world/runtime/entity_tick_observers.c
    src/world/runtime/entity_tick_spawner.c
    src/world/runtime/entity_tick_tiles.c
    src/world/runtime/weather.c
    src/world/runtime/entity_update.c
    src/world/runtime/collision/entities.c
    src/world/runtime/collision/class_entities.c
    src/world/runtime/collision/boxes.c
    src/world/runtime/collision/blocks.c
    src/world/chunk/entities.c
    src/world/runtime/scheduling.c
    src/world/runtime/mutation/support.c
    src/world/runtime/mutation/chunk.c
    src/world/runtime/mutation/height.c
    src/world/runtime/mutation/notifications.c
    src/world/runtime/mutation/hooks.c
    src/world/runtime/mutation/redstone/queries.c
    src/world/runtime/mutation/redstone/update.c
    src/world/runtime/mutation/redstone/lifecycle.c
    src/world/runtime/mutation/ticks.c
    src/world/runtime/mutation/fluids.c
    src/world/runtime/mutation/falling.c
    src/world/runtime/mutation/snow.c
    src/world/runtime/mutation/plants/survival.c
    src/world/runtime/mutation/plants/upright.c
    src/world/runtime/mutation/plants/mushroom.c
    src/world/runtime/mutation/plants/crop.c
    src/world/runtime/mutation/plants/sapling.c
    src/world/runtime/mutation/plants/farmland.c
    src/world/runtime/mutation/pressure_plate.c
    src/world/runtime/mutation/pressure_lifecycle.c
    src/world/runtime/mutation/rails/connections.c
    src/world/runtime/mutation/rails/detector.c
    src/world/runtime/mutation/rails/dispatch.c
    src/world/runtime/mutation/rails/error_sites.c
    src/world/runtime/mutation/rails/lifecycle.c
    src/world/runtime/mutation/rails/power_search.c
    src/world/runtime/mutation/rails/wrapper_transport.c
    src/world/runtime/mutation/drops.c
    src/world/runtime/mutation/world.c
    src/world/runtime/chunk_provider.c
    src/world/runtime/saved_data.c
    src/world/runtime/generation/generation.c
    src/world/runtime/generation/surface.c
    src/world/runtime/generation/caves.c
    src/world/dimension/world_provider.c
    src/world/dimension/temperature.c
    src/entity/entity.c
    src/entity/player/construction.c
    src/entity/player/input.c
    src/entity/player/session.c
    src/entity/player/strings.c
    src/entity/minecart/entity.c
    src/entity/minecart/inventory.c
    src/entity/minecart/nbt.c
    src/entity/minecart/storage.c)
target_include_directories(mc_simulation PRIVATE src/platform/time)
target_link_libraries(mc_simulation PUBLIC mc_persistence mc_java_files mc_world_saved_data mc_item mc_registry mc_inventory mc_movement_input
    PRIVATE mc_client_notice mc_client_game_directory mc_platform_time mc_platform_text Threads::Threads mc_build_options)

add_library(mc_client_first_person STATIC src/client/world/first_person.c)
target_include_directories(mc_client_first_person PRIVATE src/platform/time)
target_link_libraries(mc_client_first_person PUBLIC mc_simulation mc_game_settings PRIVATE mc_build_options)
if(UNIX)
    target_link_libraries(mc_client_first_person PRIVATE m)
endif()

add_library(mc_client_singleplayer_controller STATIC src/client/controller/singleplayer.c)
target_include_directories(mc_client_singleplayer_controller PRIVATE src/platform/time)
target_link_libraries(mc_client_singleplayer_controller PUBLIC mc_simulation PRIVATE mc_build_options)

add_library(mc_client_world_session STATIC src/client/world/session.c)
target_include_directories(mc_client_world_session PUBLIC src/client/world PRIVATE src/platform/time)
target_link_libraries(mc_client_world_session PUBLIC mc_simulation PRIVATE mc_platform_time mc_build_options)

# The real private App layout now contains actual C17 atomic fields.
target_link_libraries(mc_client_notice PUBLIC mc_c_atomics)
foreach(target IN ITEMS mc_simulation mc_client_first_person mc_client_singleplayer_controller)
    target_link_libraries(${target} PRIVATE mc_c_atomics)
endforeach()

include(cmake/client_texture_effects.cmake)
include(cmake/client_cx.cmake)
include(cmake/client_gui_ingame.cmake)
include(cmake/client_font_views.cmake)
include(cmake/client_source_screens.cmake)

add_library(mc_client_application_owner STATIC
    src/client/startup/application_classes.c
    src/client/startup/application_lifetime.c
    src/client/startup/timer_hack.c)
target_include_directories(mc_client_application_owner PUBLIC src PRIVATE src/client src/platform/time)
target_link_libraries(mc_client_application_owner PUBLIC mc_simulation mc_c_atomics
    PRIVATE Threads::Threads mc_build_options)
target_sources(mc_client_application_owner PRIVATE
    src/client/startup/application_directory_classes.c
    src/client/startup/application_startup_error.c)
target_link_libraries(mc_client_application_owner PUBLIC mc_java_files)
include(cmake/client_application_services.cmake)

# The real class namespace/publication API remains in its canonical owner target.
target_sources(mc_client_application_owner PRIVATE
    src/client/startup/application_publication.c)

if(MC_BUILD_CLIENT)
    add_library(mc_client_application_constructor STATIC
        src/client/startup/application_construction.c
        src/client/startup/application_construction_retirement.c)
    target_include_directories(mc_client_application_constructor PUBLIC
        src src/client src/platform/time)
    target_link_libraries(mc_client_application_constructor PUBLIC
        mc_client_application_owner mc_client_source_resources mc_client_notice
        mc_client_timer_construction mc_game_settings mc_c_atomics
        PRIVATE mc_platform_time mc_build_options)
endif()

if(MC_BUILD_CLIENT AND MC_BUILD_CLIENT_AUDIO AND TARGET mc_client_application_constructor)
    add_library(mc_client_application_startup STATIC
        src/client/startup/application_startup.c
        src/client/startup/application_startup_directory.c
        src/client/startup/application_startup_retirement.c)
    target_include_directories(mc_client_application_startup PUBLIC src src/client)
    target_link_libraries(mc_client_application_startup PUBLIC
        mc_client_application_constructor mc_client_display mc_client_saves
        mc_client_directory_inputs mc_c_atomics PRIVATE mc_build_options)
endif()

if(MC_BUILD_CLIENT)
    include(cmake/client_rendering.cmake)
    include(cmake/client_input.cmake)
    include(cmake/client_gl_diagnostics.cmake)
    include(cmake/client_startup_graphics.cmake)
endif()

include(cmake/client_startup_effects.cmake)
include(cmake/client_render_global.cmake)
include(cmake/client_startup_rendering.cmake)
include(cmake/client_resource_download.cmake)
include(cmake/client_startup_gui_ingame.cmake)
include(cmake/client_startup_menu.cmake)
target_link_libraries(mc_simulation PUBLIC mc_java_file_writers)
if(TARGET mc_client_application_startup)
    target_sources(mc_client_application_startup PRIVATE
        src/client/startup/application_startup_resources.c)
    target_link_libraries(mc_client_application_startup PUBLIC mc_client_resource_download)
endif()

option(MC_ENABLE_APPLICATION_STARTUP_TESTS "Run genuine source run/startup CPU prefix controls" OFF)
option(MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS
    "Run reviewed actual native HOME/APPDATA + imported-icon SDL startup profile" OFF)
option(MC_ENABLE_SAVE_STREAM_REFERENCE_TESTS "Compare actual ij/as CPU stream bodies" OFF)
set(MC_CLIENT_ASSET_IMPORT_DIR "" CACHE PATH "Actual imported root with private SDK icon RGBA assets")
set(MC_APPLICATION_STARTUP_IMPORTED_RESOURCES "" CACHE PATH
    "Same actual imported resource root for source startup window profiles")

# Native owner controls use explicit class resources or authored filesystem fixtures.
option(MC_ENABLE_APPLICATION_CONSTRUCTOR_TESTS
    "Run actual null-Applet constructor and real later-owner retirement controls" OFF)

include(CTest)
option(MC_ENABLE_SETTINGS_CAPTURE_ALLOCATION_CONTROL
    "Compile actual Info body with one failing native metadata allocation" OFF)
if(MC_ENABLE_SETTINGS_CAPTURE_ALLOCATION_CONTROL)
    if(NOT TARGET mc_client_application_startup OR NOT BUILD_TESTING)
        message(FATAL_ERROR "Settings capture control requires the real startup/client/test target")
    endif()
    add_library(mc_test_settings_capture_info OBJECT src/world/info/info.c)
    target_compile_definitions(mc_test_settings_capture_info PRIVATE MC_SETTINGS_CAPTURE_INFO_CONTROL=1)
    target_include_directories(mc_test_settings_capture_info PRIVATE src/platform/time)
    target_link_libraries(mc_test_settings_capture_info PRIVATE
        mc_storage mc_platform_time mc_build_options)
    if(MSVC)
        target_compile_options(mc_test_settings_capture_info PRIVATE
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/tests/client/settings/capture_alloc.h")
    else()
        target_compile_options(mc_test_settings_capture_info PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/settings/capture_alloc.h")
    endif()
    add_executable(mc_test_application_startup_capture
        tests/client/startup/test_application_startup.c
        tests/client/settings/capture_alloc.c
        $<TARGET_OBJECTS:mc_test_settings_capture_info>)
    target_compile_definitions(mc_test_application_startup_capture PRIVATE
        MC_SETTINGS_CAPTURE_ALLOCATION_CONTROL=1)
    target_link_libraries(mc_test_application_startup_capture PRIVATE
        mc_client_application_startup mc_sound_backend mc_client_assets mc_resources
        mc_c_atomics mc_build_options)
    set_property(TARGET mc_test_application_startup_capture PROPERTY LINKER_LANGUAGE CXX)
    if(MSVC)
        target_link_options(mc_test_application_startup_capture PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_startup_capture.map")
    else()
        target_link_options(mc_test_application_startup_capture PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_startup_capture.map")
    endif()
    if(WIN32)
        add_custom_command(TARGET mc_test_application_startup_capture POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_application_startup_capture>")
    endif()
endif()


if(MC_ENABLE_RESOURCE_LOOKUP_OBSERVERS AND (NOT MC_BUILD_CLIENT OR NOT MC_BUILD_CLIENT_AUDIO OR NOT BUILD_TESTING))
    message(FATAL_ERROR "Lookup observers require actual client/audio/testing owners")
endif()
if(MC_ENABLE_COLOR_LOOKUP_REFERENCE_TESTS OR MC_ENABLE_TEXTURE_LOOKUP_REFERENCE_TESTS)
    if(NOT MC_ENABLE_RESOURCE_LOOKUP_OBSERVERS)
        message(FATAL_ERROR "Original lookup observations require their actual native probes")
    endif()
endif()
if(MC_ENABLE_TEXTURE_LOOKUP_REFERENCE_TESTS AND NOT MC_ENABLE_CLIENT_DISPLAY_TESTS)
    message(FATAL_ERROR "Original texture lookup requires an actual display test profile")
endif()
if(MC_ENABLE_CLIENT_LOOKUP_DATA_TESTS)
    if(NOT MC_BUILD_CLIENT OR NOT BUILD_TESTING)
        message(FATAL_ERROR "Lookup data profiles require client/testing targets")
    endif()
    foreach(kind IN ITEMS texture color)
        foreach(part IN ITEMS resources default zip)
            if(NOT IS_DIRECTORY "${MC_CLIENT_LOOKUP_FIXTURE_DIR}/${kind}/${part}")
                message(FATAL_ERROR "Prepare actual lookup fixture directories before configuration")
            endif()
        endforeach()
    endforeach()
endif()
if(MC_ENABLE_APPLICATION_STARTUP_TESTS AND (NOT MC_BUILD_CLIENT OR NOT MC_BUILD_CLIENT_AUDIO OR NOT BUILD_TESTING))
    message(FATAL_ERROR "Source startup controls require actual client/audio/testing owners")
endif()
if(MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS)
    if("${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}" STREQUAL "")
        message(FATAL_ERROR "Actual startup window modes require the explicit SAME imported resource root")
    endif()
    file(REAL_PATH "${MC_CLIENT_ASSET_IMPORT_DIR}" _mc_sdk_import_root)
    file(REAL_PATH "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}" _mc_startup_import_root)
    if(NOT "${_mc_sdk_import_root}" STREQUAL "${_mc_startup_import_root}")
        message(FATAL_ERROR "Source startup and SDK icon admission require the SAME actual imported root")
    endif()
    unset(_mc_sdk_import_root)
    unset(_mc_startup_import_root)
    if(NOT MC_ENABLE_APPLICATION_STARTUP_TESTS OR NOT MC_ENABLE_CLIENT_DISPLAY_TESTS)
        message(FATAL_ERROR "Default-directory startup requires explicit source startup and real Display profiles")
    endif()
    if(NOT EXISTS "${MC_CLIENT_ASSET_IMPORT_DIR}/sdk/lwjgl-icon-16.rgba" OR
       NOT EXISTS "${MC_CLIENT_ASSET_IMPORT_DIR}/sdk/lwjgl-icon-32.rgba")
        message(FATAL_ERROR "Default-directory startup requires actual privately imported SDK icons")
    endif()
endif()
if(MC_ENABLE_APPLICATION_CONSTRUCTOR_TESTS AND (NOT MC_BUILD_CLIENT OR NOT MC_BUILD_CLIENT_AUDIO OR NOT BUILD_TESTING))
    message(FATAL_ERROR "Application constructor controls require client/audio/testing")
endif()
if(MC_ENABLE_RENDER_RESOURCES_REFERENCE_TESTS AND (NOT MC_BUILD_CLIENT OR NOT BUILD_TESTING))
    message(FATAL_ERROR "Resource CPU original profile requires client and testing targets")
endif()
if(BUILD_TESTING)
    add_executable(mc_test_display_sdk_queue tests/client/display/test_sdk_queue.c)
    target_link_libraries(mc_test_display_sdk_queue PRIVATE mc_client_sdk_queue mc_build_options)
    add_executable(mc_test_display_sdk_icons tests/client/display/test_sdk_icons.c)
    target_link_libraries(mc_test_display_sdk_icons PRIVATE mc_client_sdk_icons mc_resources mc_build_options)
    foreach(kind IN ITEMS queue icons)
        add_test(NAME client.display_sdk_${kind} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_display_sdk.py"
            --executable "$<TARGET_FILE:mc_test_display_sdk_${kind}>" --kind ${kind}
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/display-sdk-${kind}")
        set_tests_properties(client.display_sdk_${kind} PROPERTIES
            LABELS "public;unit;client;ownership" TIMEOUT 240)
    endforeach()
    foreach(mode IN ITEMS normal optimized)
        set(_mc_sdk_python_flags -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_sdk_python_flags -O)
        endif()
        add_test(NAME tools.display_sdk_protocol_${mode} COMMAND Python3::Interpreter
            ${_mc_sdk_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/display_sdk/test_protocol.py")
        set_tests_properties(tools.display_sdk_protocol_${mode} PROPERTIES
            LABELS "public;unit;tools;client;ownership" TIMEOUT 60)
        add_test(NAME tools.import_lwjgl_icons_${mode} COMMAND Python3::Interpreter
            ${_mc_sdk_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/resources/test_lwjgl_icons.py")
        set_tests_properties(tools.import_lwjgl_icons_${mode} PROPERTIES
            LABELS "public;unit;tools;resources" TIMEOUT 60)
    endforeach()
    unset(_mc_sdk_python_flags)
    add_executable(mc_test_save_format tests/client/saves/test_save_format.c)
    target_link_libraries(mc_test_save_format PRIVATE mc_client_saves mc_build_options)
    if(MINGW)
        target_link_options(mc_test_save_format PRIVATE -municode)
    endif()
    add_library(mc_test_save_close_io OBJECT src/world/storage/java_file_io.c)
    target_compile_definitions(mc_test_save_close_io PRIVATE
        MC_JAVA_FILE_NATIVE_CLOSE=mc_test_save_close)
    target_link_libraries(mc_test_save_close_io PRIVATE
        mc_world_info mc_core mc_platform_files mc_c_atomics mc_build_options)
    add_executable(mc_test_save_format_close tests/client/saves/test_save_format.c
        $<TARGET_OBJECTS:mc_test_save_close_io>)
    target_compile_definitions(mc_test_save_format_close PRIVATE MC_SAVE_FILE_CLOSE_CONTROL=1)
    target_link_libraries(mc_test_save_format_close PRIVATE mc_client_saves mc_build_options)
    if(MINGW)
        target_link_options(mc_test_save_format_close PRIVATE -municode)
    endif()
    if(MSVC)
        target_link_options(mc_test_save_format_close PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_save_format_close.map")
    else()
        target_link_options(mc_test_save_format_close PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_save_format_close.map")
    endif()
    add_executable(mc_test_source_stream tests/world/storage/test_source_stream.c)
    target_link_libraries(mc_test_source_stream PRIVATE
        mc_client_saves mc_client_directory_inputs mc_build_options)
    add_executable(mc_source_stream_probe tests/world/storage/trace_source_stream.c)
    target_link_libraries(mc_source_stream_probe PRIVATE mc_gzip_input mc_build_options)
    add_test(NAME client.save_format.models COMMAND mc_test_save_format)
    set_tests_properties(client.save_format.models PROPERTIES LABELS "public;unit;client;ownership" TIMEOUT 180)
    foreach(kind IN ITEMS fixture close)
        set(_mc_save_target mc_test_save_format)
        if(kind STREQUAL "close")
            set(_mc_save_target mc_test_save_format_close)
        endif()
        add_test(NAME client.save_format.${kind} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_save_format.py"
            --executable "$<TARGET_FILE:${_mc_save_target}>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/save-format-${kind}")
        set_tests_properties(client.save_format.${kind} PROPERTIES
            LABELS "public;unit;client;filesystem;ownership" TIMEOUT 240)
    endforeach()
    unset(_mc_save_target)
    add_test(NAME world.source_stream.authored COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_source_stream.py"
        --executable "$<TARGET_FILE:mc_test_source_stream>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-stream-authored")
    set_tests_properties(world.source_stream.authored PROPERTIES
        LABELS "public;unit;world;filesystem;ownership" TIMEOUT 240)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_stream_python -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_stream_python -O)
        endif()
        set(_mc_stream_case tools.source_stream_driver)
        if(mode STREQUAL "optimized")
            string(APPEND _mc_stream_case "_optimized")
        endif()
        add_test(NAME ${_mc_stream_case} COMMAND Python3::Interpreter ${_mc_stream_python}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/saves/test_stream_driver.py")
        set_tests_properties(${_mc_stream_case} PROPERTIES LABELS "public;unit;tools;storage" TIMEOUT 60)
    endforeach()
    unset(_mc_stream_case)
    unset(_mc_stream_python)
    add_test(NAME tools.source_stream_probe_inputs COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_source_stream_probe.py"
        --executable "$<TARGET_FILE:mc_source_stream_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-stream-probe-inputs")
    set_tests_properties(tools.source_stream_probe_inputs PROPERTIES LABELS "public;unit;tools;storage" TIMEOUT 180)
    if(MC_ENABLE_SAVE_STREAM_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT Java_VERSION_STRING MATCHES "^17[.]")
            message(FATAL_ERROR "The selected source-stream observation requires actual JDK17")
        endif()
        add_test(NAME reference.source_stream COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/saves/compare_stream.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --native "$<TARGET_FILE:mc_source_stream_probe>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-source-stream")
        set_tests_properties(reference.source_stream PROPERTIES LABELS "local;reference;original;storage" TIMEOUT 900)
    endif()
    add_executable(mc_test_game_settings_source tests/client/settings/test_source_construction.c)
    target_link_libraries(mc_test_game_settings_source PRIVATE mc_game_settings mc_build_options)
    add_test(NAME client.game_settings_source_construction COMMAND mc_test_game_settings_source cpu)
    set_tests_properties(client.game_settings_source_construction PROPERTIES
        LABELS "public;unit;client;settings;constructor;ownership" TIMEOUT 60)

    if(MC_ENABLE_APPLICATION_STARTUP_TESTS)
        if(NOT "${MC_REFERENCE_CLIENT_RESOURCE_DIR}" STREQUAL "")
            set(_mc_startup_resource_root "${MC_REFERENCE_CLIENT_RESOURCE_DIR}")
        else()
            set(_mc_startup_resource_root "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/startup/fixtures")
        endif()
        add_executable(mc_test_application_startup tests/client/startup/test_application_startup.c)
        target_link_libraries(mc_test_application_startup PRIVATE
            mc_client_application_startup mc_sound_backend mc_client_assets mc_resources
            mc_c_atomics mc_build_options)
        set_property(TARGET mc_test_application_startup PROPERTY LINKER_LANGUAGE CXX)
        if(WIN32)
            add_custom_command(TARGET mc_test_application_startup POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:mc_test_application_startup>")
        endif()
        add_test(NAME client.application_startup_prefix COMMAND mc_test_application_startup
            "${_mc_startup_resource_root}" cpu)
        set_tests_properties(client.application_startup_prefix PROPERTIES
            LABELS "public;unit;client;startup;ownership" TIMEOUT 180)
        if(MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS)
            add_test(NAME client.application_startup_default_directory COMMAND mc_test_application_startup
                "${_mc_startup_resource_root}" window-default-directory "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}")
            set_tests_properties(client.application_startup_default_directory PROPERTIES
                LABELS "integration;client;graphics;startup;filesystem;ownership" TIMEOUT 180)
        endif()
    endif()

    # Private additive qualification of the canonical raw/construct Sound owners.
    add_executable(mc_test_sound_construction tests/client/sound/test_construction.c)
    target_include_directories(mc_test_sound_construction PRIVATE src/core/java)
    target_link_libraries(mc_test_sound_construction PRIVATE
        mc_game_settings mc_c_atomics mc_build_options)
    add_test(NAME client.sound_construction COMMAND mc_test_sound_construction)
    set_tests_properties(client.sound_construction PROPERTIES
        LABELS "public;unit;client;constructor;ownership" TIMEOUT 60)

    # Compile the same two canonical owner bodies with allocation-only controls.
    # The executable definitions satisfy all manager/pool symbols; normal lower
    # archives remain ordinary selective dependencies, never whole-archive copies.
    add_library(mc_sound_constructor_failure_manager OBJECT src/client/sound/sound_manager.c)
    target_include_directories(mc_sound_constructor_failure_manager PRIVATE include)
    target_compile_definitions(mc_sound_constructor_failure_manager PRIVATE
        MC_SOUND_MANAGER_CALLOC=mc_test_manager_calloc)
    target_link_libraries(mc_sound_constructor_failure_manager PRIVATE mc_core mc_c_atomics mc_build_options)
    add_library(mc_sound_constructor_failure_pool OBJECT src/client/sound/sound_pool.c)
    target_include_directories(mc_sound_constructor_failure_pool PRIVATE include)
    target_compile_definitions(mc_sound_constructor_failure_pool PRIVATE
        MC_SOUND_POOL_CALLOC=mc_test_pool_calloc)
    target_link_libraries(mc_sound_constructor_failure_pool PRIVATE mc_client_sound_files mc_c_atomics mc_build_options)
    add_executable(mc_test_sound_construction_allocation
        tests/client/sound/test_construction_allocation.c
        $<TARGET_OBJECTS:mc_sound_constructor_failure_manager>
        $<TARGET_OBJECTS:mc_sound_constructor_failure_pool>)
    target_include_directories(mc_test_sound_construction_allocation PRIVATE src/core/java)
    target_link_libraries(mc_test_sound_construction_allocation PRIVATE
        mc_game_settings mc_c_atomics mc_build_options)
    add_test(NAME client.sound_construction_allocation COMMAND mc_test_sound_construction_allocation)
    set_tests_properties(client.sound_construction_allocation PROPERTIES
        LABELS "public;unit;client;constructor;ownership;allocation" TIMEOUT 60)

    if(MC_BUILD_CLIENT_AUDIO)
        add_executable(mc_test_sound_construction_backend tests/client/sound/test_construction_backend.c)
        target_include_directories(mc_test_sound_construction_backend PRIVATE src/core/java)
        target_link_libraries(mc_test_sound_construction_backend PRIVATE
            mc_sound_backend mc_c_atomics mc_build_options)
        set_property(TARGET mc_test_sound_construction_backend PROPERTY LINKER_LANGUAGE CXX)
        if(WIN32)
            add_custom_command(TARGET mc_test_sound_construction_backend POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:mc_test_sound_construction_backend>")
        endif()
        add_test(NAME client.sound_construction_backend COMMAND mc_test_sound_construction_backend)
        set_tests_properties(client.sound_construction_backend PROPERTIES
            LABELS "public;integration;client;constructor;audio;ownership" TIMEOUT 60)
    endif()


    if(MC_ENABLE_APPLICATION_CONSTRUCTOR_TESTS)
        if(NOT "${MC_REFERENCE_CLIENT_RESOURCE_DIR}" STREQUAL "")
            set(_mc_application_constructor_resource_root "${MC_REFERENCE_CLIENT_RESOURCE_DIR}")
        else()
            set(_mc_application_constructor_resource_root
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/startup/fixtures")
        endif()
        add_executable(mc_test_application_construction
            tests/client/startup/test_application_construction.c)
        target_include_directories(mc_test_application_construction PRIVATE src/core/java)
        target_link_libraries(mc_test_application_construction PRIVATE
            mc_client_application_constructor mc_sound_backend mc_client_assets
            mc_resources mc_c_atomics mc_build_options)
        set_property(TARGET mc_test_application_construction PROPERTY LINKER_LANGUAGE CXX)
        add_test(NAME client.application_construction
            COMMAND mc_test_application_construction "${_mc_application_constructor_resource_root}")
        set_tests_properties(client.application_construction PROPERTIES
            LABELS "public;unit;client;constructor;ownership" TIMEOUT 180)
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.application_construction_later_owners
                COMMAND mc_test_application_construction "${_mc_application_constructor_resource_root}" later-owners)
            set_tests_properties(client.application_construction_later_owners PROPERTIES
                LABELS "public;integration;client;graphics;ownership" TIMEOUT 180)
        endif()

        # Public constructor keeps only the corrected real-owner control.
        foreach(target IN ITEMS mc_test_application_construction)
            if(MSVC)
                target_link_options(${target} PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/${target}.map")
            else()
                target_link_options(${target} PRIVATE "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${target}.map")
            endif()
            if(WIN32)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:${target}>")
            endif()
        endforeach()
    endif()

    add_executable(mc_test_application_owner tests/client/startup/test_application_owner.c)
    target_include_directories(mc_test_application_owner PRIVATE src src/client src/platform/time)
    target_link_libraries(mc_test_application_owner PRIVATE
        mc_client_application_owner mc_c_atomics mc_build_options)
    add_test(NAME client.application_owner COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/application_owner/run_native.py"
        --executable "$<TARGET_FILE:mc_test_application_owner>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/application-owner-contracts")
    set_tests_properties(client.application_owner PROPERTIES
        LABELS "public;unit;client;ownership;thread" TIMEOUT 120)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_owner_python_flags -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_owner_python_flags -O)
        endif()
        add_test(NAME tools.application_owner_${mode} COMMAND Python3::Interpreter
            ${_mc_owner_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_application_owner_driver.py"
            --executable "$<TARGET_FILE:mc_test_application_owner>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/application-owner-${mode}")
        set_tests_properties(tools.application_owner_${mode} PROPERTIES
            LABELS "public;tools;client;ownership;thread" TIMEOUT 240)
    endforeach()
    unset(_mc_owner_python_flags)

    add_executable(mc_test_singleplayer_controller tests/client/controller/test_singleplayer.c)
    target_include_directories(mc_test_singleplayer_controller PRIVATE src/platform/time)
    target_link_libraries(mc_test_singleplayer_controller PRIVATE
        mc_client_singleplayer_controller mc_resources mc_c_atomics mc_build_options)
    add_executable(mc_test_singleplayer_controller_allocation tests/client/controller/test_singleplayer.c)
    target_compile_definitions(mc_test_singleplayer_controller_allocation PRIVATE
        MC_SINGLEPLAYER_CONTROLLER_TEST_ALLOCATOR=1)
    target_include_directories(mc_test_singleplayer_controller_allocation PRIVATE src/platform/time)
    target_link_libraries(mc_test_singleplayer_controller_allocation PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    foreach(mode singleplayer_controller singleplayer_controller_allocation)
        add_test(NAME client.${mode} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/controller/run_native.py"
            --executable "$<TARGET_FILE:mc_test_${mode}>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/${mode}-contracts")
        set_tests_properties(client.${mode} PROPERTIES
            LABELS "public;integration;client;entity;world;ownership" TIMEOUT 240)
    endforeach()
    add_test(NAME tools.singleplayer_controller COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/controller/test_reference.py")
    add_test(NAME tools.singleplayer_controller_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/controller/test_reference.py")
    set_tests_properties(tools.singleplayer_controller tools.singleplayer_controller_optimized PROPERTIES
        LABELS "public;unit;tools;client;entity;ownership" TIMEOUT 60)
    if(MC_ENABLE_SINGLEPLAYER_CONTROLLER_REFERENCE_TESTS)
        if(NOT WIN32)
            message(FATAL_ERROR "Original singleplayer controller profile requires qualified Windows x64")
        endif()
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR OR NOT MC_REFERENCE_CLIENT_NATIVE_DIR)
            message(FATAL_ERROR "Original singleplayer controller profile requires client JAR, libraries and native directory")
        endif()
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.singleplayer_controller COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/controller/compare.py"
            --executable "$<TARGET_FILE:mc_test_singleplayer_controller>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --natives "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-singleplayer-controller")
        set_tests_properties(reference.singleplayer_controller PROPERTIES
            LABELS "local;reference;original;client;entity;world;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_world_entity_tick tests/world/runtime/test_entity_tick.c)
    target_include_directories(mc_test_world_entity_tick PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_entity_tick PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_executable(mc_test_world_entity_tick_abi tests/world/runtime/test_entity_tick_abi.c)
    target_link_libraries(mc_test_world_entity_tick_abi PRIVATE mc_simulation mc_build_options)
    add_test(NAME world.entity_tick COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/entity_tick/run.py"
        --native "$<TARGET_FILE:mc_test_world_entity_tick>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-entity-tick")
    add_test(NAME world.entity_tick_abi COMMAND mc_test_world_entity_tick_abi)
    add_test(NAME tools.world_entity_tick_controller COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/entity_tick/test_controller.py")
    add_test(NAME tools.world_entity_tick_controller_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/entity_tick/test_controller.py")
    set_tests_properties(world.entity_tick PROPERTIES
        LABELS "public;integration;world;entity;ownership;math" TIMEOUT 240)
    set_tests_properties(world.entity_tick_abi tools.world_entity_tick_controller tools.world_entity_tick_controller_optimized
        PROPERTIES LABELS "public;unit;world;entity;ownership" TIMEOUT 60)
    if(MC_ENABLE_WORLD_ENTITY_TICK_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "World Entity reference requires the client JAR and libraries")
        endif()
        add_test(NAME reference.world_entity_tick COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/entity_tick/run.py"
            --native "$<TARGET_FILE:mc_test_world_entity_tick>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --library-dir "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-world-entity-tick")
        set_tests_properties(reference.world_entity_tick PROPERTIES
            LABELS "local;reference;world;entity;ownership;math" TIMEOUT 480)
    endif()

    add_executable(mc_test_world_weather tests/world/runtime/test_weather.c)
    target_link_libraries(mc_test_world_weather PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.weather COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/weather/run.py"
        --native "$<TARGET_FILE:mc_test_world_weather>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-weather")
    add_test(NAME tools.world_weather_controller COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/weather/test_controller.py")
    add_test(NAME tools.world_weather_controller_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/weather/test_controller.py")
    set_tests_properties(world.weather PROPERTIES
        LABELS "public;integration;world;weather;ownership;math" TIMEOUT 240)
    set_tests_properties(tools.world_weather_controller tools.world_weather_controller_optimized
        PROPERTIES LABELS "public;unit;tools;world;weather" TIMEOUT 60)
    if(MC_ENABLE_WEATHER_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "World weather reference requires the client JAR and libraries")
        endif()
        add_test(NAME reference.world_weather COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/weather/run.py"
            --native "$<TARGET_FILE:mc_test_world_weather>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --library-dir "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-world-weather")
        set_tests_properties(reference.world_weather PROPERTIES
            LABELS "local;reference;world;weather;ownership;math" TIMEOUT 480)
    endif()

    add_executable(mc_test_first_person tests/client/world/test_first_person.c)
    add_executable(mc_test_first_person_callbacks tests/client/world/test_first_person_callbacks.c)
    foreach(target mc_test_first_person mc_test_first_person_callbacks)
        target_include_directories(${target} PRIVATE src/platform/time)
        target_link_libraries(${target} PRIVATE
            mc_client_first_person mc_resources mc_c_atomics mc_build_options)
    endforeach()
    add_test(NAME client.first_person COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_first_person.py"
        --executable "$<TARGET_FILE:mc_test_first_person>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/first-person-contracts")
    add_test(NAME client.first_person_callbacks COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_first_person_callbacks.py"
        --executable "$<TARGET_FILE:mc_test_first_person_callbacks>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/first-person-callback-contracts")
    add_test(NAME client.first_person_callbacks_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_first_person_callbacks.py"
        --executable "$<TARGET_FILE:mc_test_first_person_callbacks>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/first-person-callback-optimized-contracts")
    add_test(NAME tools.first_person_driver COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_first_person_driver.py"
        --executable "$<TARGET_FILE:mc_test_first_person>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/first-person-driver-contracts")
    add_test(NAME tools.first_person_driver_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_first_person_driver.py"
        --executable "$<TARGET_FILE:mc_test_first_person>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/first-person-driver-optimized-contracts")
    set_tests_properties(client.first_person client.first_person_callbacks client.first_person_callbacks_optimized
        tools.first_person_driver tools.first_person_driver_optimized PROPERTIES
        LABELS "public;integration;client;entity;world;ownership;math" TIMEOUT 240)
    if(MC_BUILD_CLIENT AND MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_executable(mc_test_first_person_gpu tests/client/world/test_first_person.c)
        target_compile_definitions(mc_test_first_person_gpu PRIVATE MC_CAMERA_GPU=1)
        target_include_directories(mc_test_first_person_gpu PRIVATE src/platform/time)
        target_link_libraries(mc_test_first_person_gpu PRIVATE
            mc_client_first_person mc_client_world_renderer mc_resources mc_c_atomics mc_build_options)
        add_test(NAME client.first_person_gpu COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_first_person.py"
            --executable "$<TARGET_FILE:mc_test_first_person_gpu>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/first-person-gpu-contracts")
        set_tests_properties(client.first_person_gpu PROPERTIES
            LABELS "public;integration;client;graphics;entity;world" TIMEOUT 240)
    endif()
    if(MC_ENABLE_CAMERA_REFERENCE_TESTS AND WIN32)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR OR NOT MC_REFERENCE_CLIENT_NATIVE_DIR)
            message(FATAL_ERROR "Original camera profile requires Windows client JAR, libraries and native directory")
        endif()
        add_test(NAME reference.first_person_camera COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/camera/compare.py"
            --native "$<TARGET_FILE:mc_test_first_person>"
            --callbacks "$<TARGET_FILE:mc_test_first_person_callbacks>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --native-dir "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-first-person-camera")
        set_tests_properties(reference.first_person_camera PROPERTIES
            LABELS "local;reference;client;graphics;entity;world;math" TIMEOUT 480)
    endif()

    add_executable(mc_biped_probe tests/client/render/test_biped.c)
    target_include_directories(mc_biped_probe PRIVATE src src/core/java)
    target_link_libraries(mc_biped_probe PRIVATE mc_client_notice mc_c_atomics mc_build_options)
    add_executable(mc_biped_fp tests/client/render/biped_fp.c)
    target_include_directories(mc_biped_fp PRIVATE src/core/java)
    target_link_libraries(mc_biped_fp PRIVATE mc_client_notice mc_c_atomics mc_build_options)
    add_test(NAME client.biped_construction COMMAND mc_biped_probe contracts)
    add_test(NAME client.biped_item_fp COMMAND mc_biped_fp item)
    add_test(NAME client.biped_fp COMMAND mc_biped_fp biped)
    add_test(NAME tools.biped_native COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/biped/run_native.py"
        --native-probe "$<TARGET_FILE:mc_biped_probe>" --fp-probe "$<TARGET_FILE:mc_biped_fp>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/biped-native-contracts")
    add_test(NAME tools.biped_driver COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_biped_driver.py"
        --native-probe "$<TARGET_FILE:mc_biped_probe>" --fp-probe "$<TARGET_FILE:mc_biped_fp>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/biped-driver-contracts")
    add_test(NAME tools.biped_driver_optimized COMMAND Python3::Interpreter -B -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_biped_driver.py"
        --native-probe "$<TARGET_FILE:mc_biped_probe>" --fp-probe "$<TARGET_FILE:mc_biped_fp>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/biped-driver-optimized-contracts")
    set_tests_properties(client.biped_construction client.biped_item_fp client.biped_fp
        tools.biped_native tools.biped_driver tools.biped_driver_optimized PROPERTIES
        LABELS "public;integration;client;ownership;entity;math" TIMEOUT 180)
    if(MC_ENABLE_BIPED_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Biped reference tests require explicit client JAR and pinned library archives")
        endif()
        add_test(NAME reference.biped_construction COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/biped/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --native-probe "$<TARGET_FILE:mc_biped_probe>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-biped-construction")
        set_tests_properties(reference.biped_construction PROPERTIES
            LABELS "local;reference;client;ownership;entity;math" TIMEOUT 600)
    endif()

    add_executable(mc_notice_probe tests/client/gui/notice_probe.c)
    target_include_directories(mc_notice_probe PRIVATE src/client src/core/java src/platform/time)
    target_link_libraries(mc_notice_probe PRIVATE mc_client_notice mc_resources
        mc_platform_time mc_c_atomics mc_build_options)
    add_test(NAME client.achievement_notice COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/notice/run_native.py"
        --native-probe "$<TARGET_FILE:mc_notice_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/notice-contracts")
    set_tests_properties(client.achievement_notice PROPERTIES
        LABELS "public;integration;client;ownership" TIMEOUT 150)
    add_test(NAME tools.achievement_notice_capture COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/notice/test_capture.py")
    add_test(NAME tools.achievement_notice_capture_optimized COMMAND Python3::Interpreter
        -O "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/notice/test_capture.py")
    set_tests_properties(tools.achievement_notice_capture tools.achievement_notice_capture_optimized
        PROPERTIES LABELS "public;unit;tools;client" TIMEOUT 30)
    if(MC_ENABLE_NOTICE_REFERENCE_TESTS)
        if(NOT WIN32)
            message(FATAL_ERROR "The original notice reference profile currently qualifies Windows x64 only")
        endif()
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR OR
           NOT MC_REFERENCE_CLIENT_NATIVE_DIR OR NOT MC_REFERENCE_CLIENT_RESOURCE_DIR)
            message(FATAL_ERROR "Notice reference tests require explicit client JAR, libraries, natives and resources")
        endif()
        add_test(NAME reference.achievement_notice COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/notice/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --natives "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
            --resources "${MC_REFERENCE_CLIENT_RESOURCE_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --native-probe "$<TARGET_FILE:mc_notice_probe>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-achievement-notice")
        set_tests_properties(reference.achievement_notice PROPERTIES
            LABELS "local;reference;client;ownership" TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()

    if(MC_BUILD_CLIENT)
        add_executable(mc_test_render_engine tests/client/render_engine/test_render_engine.c)
        target_link_libraries(mc_test_render_engine PRIVATE mc_client_render_engine mc_build_options)
        add_test(NAME client.render_engine COMMAND mc_test_render_engine)
        set_tests_properties(client.render_engine PROPERTIES
            LABELS "public;unit;client;font;ownership" TIMEOUT 180)
        add_executable(mc_test_render_textures tests/client/render_engine/test_textures.c)
        target_link_libraries(mc_test_render_textures PRIVATE mc_client_render_textures mc_build_options)
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.render_textures COMMAND mc_test_render_textures)
            set_tests_properties(client.render_textures PROPERTIES
                LABELS "public;integration;client;graphics;ownership" TIMEOUT 180)
            foreach(mode IN ITEMS normal optimized)
                set(_mc_render_python_flags -B)
                if(mode STREQUAL "optimized")
                    list(APPEND _mc_render_python_flags -O)
                endif()
                add_test(NAME tools.render_engine_${mode} COMMAND Python3::Interpreter
                    ${_mc_render_python_flags}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_render_engine.py"
                    --constructor "$<TARGET_FILE:mc_test_render_engine>"
                    --textures "$<TARGET_FILE:mc_test_render_textures>"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-engine-${mode}")
                set_tests_properties(tools.render_engine_${mode} PROPERTIES
                    LABELS "public;tools;client;graphics;ownership" TIMEOUT 900)
            endforeach()
            unset(_mc_render_python_flags)
        endif()
        if(MC_ENABLE_RENDER_ENGINE_REFERENCE_TESTS OR MC_ENABLE_RENDER_TEXTURE_REFERENCE_TESTS)
            if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
                message(FATAL_ERROR "RenderEngine reference tests require explicit client JAR and libraries")
            endif()
            find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
            if(NOT Java_VERSION_MAJOR EQUAL 17)
                message(FATAL_ERROR "RenderEngine reference tests require matching JDK17 java and javac")
            endif()
        endif()
        if(MC_ENABLE_RENDER_ENGINE_REFERENCE_TESTS)
            add_test(NAME reference.render_engine COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_engine/compare.py"
                --client-jar "${MC_REFERENCE_CLIENT_JAR}"
                --constructor "$<TARGET_FILE:mc_test_render_engine>"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-render-engine")
            set_tests_properties(reference.render_engine PROPERTIES
                LABELS "reference;original;client;font" TIMEOUT 900)
        endif()
        if(MC_ENABLE_RENDER_TEXTURE_REFERENCE_TESTS)
            if(NOT MC_REFERENCE_CLIENT_NATIVE_DIR)
                message(FATAL_ERROR "Render texture reference tests require explicit Windows native libraries")
            endif()
            add_test(NAME reference.render_textures COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_engine/compare.py"
                --client-jar "${MC_REFERENCE_CLIENT_JAR}"
                --constructor "$<TARGET_FILE:mc_test_render_engine>"
                --textures "$<TARGET_FILE:mc_test_render_textures>"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
                --native-dir "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-render-textures")
            set_tests_properties(reference.render_textures PROPERTIES
                LABELS "reference;original;client;graphics" TIMEOUT 1200)
        endif()
        add_executable(mc_test_loading_source_flag tests/client/gui/test_loading_source_flag.c)
        target_link_libraries(mc_test_loading_source_flag PRIVATE
            mc_client_source_resources mc_client_application_owner mc_c_atomics mc_build_options)
        add_test(NAME client.loading_source_flag COMMAND mc_test_loading_source_flag)
        set_tests_properties(client.loading_source_flag PROPERTIES
            LABELS "public;unit;client;ownership" TIMEOUT 120)

        add_executable(mc_test_source_resources tests/client/render_engine/test_source_resources.c)
        target_link_libraries(mc_test_source_resources PRIVATE mc_client_source_resources mc_build_options)
        add_test(NAME client.render_source_resources COMMAND mc_test_source_resources)
        set_tests_properties(client.render_source_resources PROPERTIES
            LABELS "public;unit;client;ownership" TIMEOUT 120)

        add_executable(mc_test_application_quad tests/client/startup/test_application_quad.c)
        target_link_libraries(mc_test_application_quad PRIVATE mc_client_source_resources mc_build_options)
        add_test(NAME client.application_textured_rectangle_cpu COMMAND mc_test_application_quad)
        set_tests_properties(client.application_textured_rectangle_cpu PROPERTIES
            LABELS "public;unit;client;geometry" TIMEOUT 120)

        add_executable(mc_test_source_close_callbacks tests/client/render_engine/test_close_callbacks.c)
        target_link_libraries(mc_test_source_close_callbacks PRIVATE
            mc_client_source_resources mc_resources mc_build_options)
        foreach(mode IN ITEMS normal optimized)
            set(_mc_source_python_flags -B)
            if(mode STREQUAL "optimized")
                list(APPEND _mc_source_python_flags -O)
            endif()
            add_test(NAME tools.render_source_close_${mode} COMMAND Python3::Interpreter
                ${_mc_source_python_flags}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/run_close_callbacks.py"
                --executable "$<TARGET_FILE:mc_test_source_close_callbacks>"
                --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-close-${mode}")
            set_tests_properties(tools.render_source_close_${mode} PROPERTIES
                LABELS "public;tools;client;ownership" TIMEOUT 120)
            add_test(NAME tools.render_source_close_parser_${mode} COMMAND Python3::Interpreter
                ${_mc_source_python_flags}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_close_callbacks_driver.py")
            set_tests_properties(tools.render_source_close_parser_${mode} PROPERTIES
                LABELS "public;tools;client;ownership" TIMEOUT 60)
        endforeach()
        unset(_mc_source_python_flags)

        # The test includes the exact Display TU for access to the real GL table.
        # Link its dependencies without a second mc_client_display body.
        add_executable(mc_test_source_fp tests/client/render_engine/test_source_fp.c)
        target_include_directories(mc_test_source_fp PRIVATE src/client/display)
        target_link_libraries(mc_test_source_fp PRIVATE mc_image SDL3::SDL3-static
        mc_client_event_transport mc_client_sdk_runtime mc_build_options)
        if(UNIX)
            target_link_libraries(mc_test_source_fp PRIVATE m)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.render_source_fp COMMAND mc_test_source_fp)
            set_tests_properties(client.render_source_fp PROPERTIES
                LABELS "public;integration;client;graphics;ownership" TIMEOUT 180)
        endif()
        add_executable(mc_probe_render_source_cpu tests/client/render_engine/source_cpu_probe.c)
        # The probe includes exact nw.c. The ordinary static archive remains a link
        # dependency; its nw.o must not be extracted, because all nw exports are
        # already defined by that exact included TU. Verify the actual link map.
        target_link_libraries(mc_probe_render_source_cpu PRIVATE
            mc_client_render_textures mc_build_options)
        add_test(NAME client.render_source_cpu COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_source_cpu.py"
            --executable "$<TARGET_FILE:mc_probe_render_source_cpu>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-cpu-controls")
        set_tests_properties(client.render_source_cpu PROPERTIES
            LABELS "public;integration;client;ownership" TIMEOUT 240)

        add_executable(mc_test_render_source_font tests/client/render_engine/test_source_font.c)
        # This executable includes exact Display.c once. Its definitions satisfy all
        # Display imports before archive extraction; no second Display.o is allowed
        # in the actual executable's link map. All intercepted GL functions forward.
        target_link_libraries(mc_test_render_source_font PRIVATE
            mc_client_source_resources mc_client_application_owner mc_resources
            mc_c_atomics mc_build_options)
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.render_source_font COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_source_font.py"
                --executable "$<TARGET_FILE:mc_test_render_source_font>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-font-controls")
            set_tests_properties(client.render_source_font PROPERTIES
                LABELS "public;integration;client;graphics;ownership" TIMEOUT 300)
        endif()
        foreach(mode IN ITEMS normal optimized)
            set(_mc_resource_protocol_flags -B)
            if(mode STREQUAL "optimized")
                list(APPEND _mc_resource_protocol_flags -O)
            endif()
            add_test(NAME tools.render_source_protocol_${mode} COMMAND Python3::Interpreter
                ${_mc_resource_protocol_flags}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_resources/test_protocol.py")
            set_tests_properties(tools.render_source_protocol_${mode} PROPERTIES
                LABELS "public;unit;tools;client;ownership" TIMEOUT 60)
        endforeach()
        unset(_mc_resource_protocol_flags)

        # The matching option declaration is a separate global insertion. The selected
        # original profile initially retains its recorded Windows/JDK17 scope.
        if(MC_ENABLE_RENDER_RESOURCES_REFERENCE_TESTS)
            if(NOT WIN32)
                message(FATAL_ERROR "Original resource CPU profile requires qualified Windows x64")
            endif()
            if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
                message(FATAL_ERROR "Original resource CPU profile requires explicit JAR and libraries")
            endif()
            find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
            add_test(NAME reference.render_source_cpu COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_resources/compare.py"
                --executable "$<TARGET_FILE:mc_probe_render_source_cpu>"
                --client-jar "${MC_REFERENCE_CLIENT_JAR}"
                --lwjgl "${MC_REFERENCE_CLIENT_LIBRARY_DIR}/lwjgl-2.9.0.jar"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-cpu-original")
            set_tests_properties(reference.render_source_cpu PROPERTIES
                LABELS "local;reference;original;client;ownership" TIMEOUT 420)
        endif()

        if(MC_ENABLE_RESOURCE_LOOKUP_OBSERVERS)
            foreach(kind IN ITEMS color texture)
                add_executable(mc_observe_${kind}_lookup
                    tests/client/render_engine/observe_${kind}_lookup.c)
                target_include_directories(mc_observe_${kind}_lookup PRIVATE src src/client src/platform/time)
                target_link_libraries(mc_observe_${kind}_lookup PRIVATE
                    mc_client_application_constructor mc_sound_backend mc_client_render_textures
                    mc_resources mc_c_atomics mc_build_options)
                set_property(TARGET mc_observe_${kind}_lookup PROPERTY LINKER_LANGUAGE CXX)
                if(WIN32)
                    add_custom_command(TARGET mc_observe_${kind}_lookup POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:mc_observe_${kind}_lookup>")
                endif()
            endforeach()
            foreach(mode IN ITEMS normal optimized)
                set(_mc_observation_flags -B)
                if(mode STREQUAL "optimized")
                    list(APPEND _mc_observation_flags -O)
                endif()
                add_test(NAME tools.lookup_observation_${mode} COMMAND Python3::Interpreter
                    ${_mc_observation_flags}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/resource_lookup_common/test_protocol.py")
                set_tests_properties(tools.lookup_observation_${mode} PROPERTIES
                    LABELS "public;unit;tools;client;ownership" TIMEOUT 120)
                add_test(NAME tools.lookup_observation_admission_${mode} COMMAND Python3::Interpreter
                    ${_mc_observation_flags}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/resource_lookup_common/test_admission.py"
                    --implementation "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/resource_lookup_common")
                set_tests_properties(tools.lookup_observation_admission_${mode} PROPERTIES
                    LABELS "public;unit;tools;client;ownership" TIMEOUT 120)
            endforeach()
            unset(_mc_observation_flags)
            foreach(kind IN ITEMS color texture)
                if((kind STREQUAL "color" AND MC_ENABLE_COLOR_LOOKUP_REFERENCE_TESTS) OR
                   (kind STREQUAL "texture" AND MC_ENABLE_TEXTURE_LOOKUP_REFERENCE_TESTS))
                    set(_mc_lookup_profiles default zip priority frontiers)
                    if(kind STREQUAL "color")
                        list(APPEND _mc_lookup_profiles helpers statics first-setter)
                    endif()
                    foreach(profile IN LISTS _mc_lookup_profiles)
                        set(_mc_lookup_directory default)
                        if(profile STREQUAL "zip")
                            set(_mc_lookup_directory zip)
                        endif()
                        set(_mc_lookup_extra)
                        if(kind STREQUAL "texture")
                            list(APPEND _mc_lookup_extra --natives "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
                                --native-archive "${MC_LOOKUP_NATIVE_ARCHIVE}")
                        endif()
                        add_test(NAME reference.${kind}_lookup_${profile} COMMAND Python3::Interpreter -B
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/${kind}_lookup/compare.py"
                            --root "${CMAKE_CURRENT_SOURCE_DIR}"
                            --resources "${MC_LOOKUP_OBSERVATION_INPUT_DIR}/${kind}/resources"
                            --directory "${MC_LOOKUP_OBSERVATION_INPUT_DIR}/${kind}/${_mc_lookup_directory}"
                            --profile "${profile}" --native "$<TARGET_FILE:mc_observe_${kind}_lookup>"
                            --native-products "${MC_LOOKUP_NATIVE_PRODUCT_DIR}/${kind}.json"
                            --native-products-gate "${MC_LOOKUP_NATIVE_PRODUCT_DIR}/${kind}-gate.json"
                            --authority-game "${MC_LOOKUP_AUTHORITY_GAME}" --authority-sdk "${MC_LOOKUP_AUTHORITY_SDK}"
                            --authority-nests "${MC_LOOKUP_AUTHORITY_NESTS}" --authority-runtime "${MC_LOOKUP_AUTHORITY_RUNTIME}"
                            ${_mc_lookup_extra}
                            --output "${CMAKE_CURRENT_BINARY_DIR}/reference-${kind}-lookup-${profile}")
                        set_tests_properties(reference.${kind}_lookup_${profile} PROPERTIES
                            LABELS "local;reference;original;client;ownership" TIMEOUT 900)
                    endforeach()
                endif()
            endforeach()
        endif()

        add_executable(mc_test_color_arrays tests/client/render_engine/test_color_arrays.c)
        target_include_directories(mc_test_color_arrays PRIVATE src)
        target_link_libraries(mc_test_color_arrays PRIVATE mc_client_render_engine mc_build_options)
        add_test(NAME client.color_arrays COMMAND mc_test_color_arrays)
        set_tests_properties(client.color_arrays PROPERTIES
            LABELS "public;unit;client;ownership" TIMEOUT 120)

        add_executable(mc_test_texture_zip tests/client/texture_packs/test_zip.c)
        target_include_directories(mc_test_texture_zip PRIVATE src/client/texture_packs)
        target_link_libraries(mc_test_texture_zip PRIVATE mc_client_texture_packs mc_build_options)
        foreach(mode IN ITEMS normal optimized)
            set(_mc_lookup_flags -B)
            if(mode STREQUAL "optimized")
                list(APPEND _mc_lookup_flags -O)
            endif()
            add_test(NAME tools.texture_zip_native_${mode} COMMAND Python3::Interpreter
                ${_mc_lookup_flags}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_texture_zip.py"
                --executable "$<TARGET_FILE:mc_test_texture_zip>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/texture-zip-${mode}")
            set_tests_properties(tools.texture_zip_native_${mode} PROPERTIES
                LABELS "public;integration;client;ownership" TIMEOUT 180)
            add_test(NAME tools.texture_zip_parser_${mode} COMMAND Python3::Interpreter
                ${_mc_lookup_flags}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/texture_packs/test_driver.py")
            set_tests_properties(tools.texture_zip_parser_${mode} PROPERTIES
                LABELS "public;unit;tools;client;ownership" TIMEOUT 60)
        endforeach()
        unset(_mc_lookup_flags)

        add_executable(mc_test_texture_lookup tests/client/render_engine/test_texture_lookup.c)
        target_include_directories(mc_test_texture_lookup PRIVATE src src/client src/platform/time)
        target_link_libraries(mc_test_texture_lookup PRIVATE
            mc_client_render_textures mc_client_application_owner mc_client_notice
            mc_resources mc_simulation mc_c_atomics mc_build_options)
        add_executable(mc_test_color_lookup tests/client/render_engine/test_color_lookup.c)
        target_include_directories(mc_test_color_lookup PRIVATE src src/client src/platform/time)
        target_link_libraries(mc_test_color_lookup PRIVATE
            mc_client_render_textures mc_client_application_owner mc_client_notice
            mc_resources mc_simulation mc_c_atomics mc_build_options)
        if(MC_ENABLE_CLIENT_LOOKUP_DATA_TESTS)
            foreach(kind IN ITEMS default zip)
                add_test(NAME client.color_lookup_${kind} COMMAND mc_test_color_lookup
                    "${MC_CLIENT_LOOKUP_FIXTURE_DIR}/color/resources"
                    "${MC_CLIENT_LOOKUP_FIXTURE_DIR}/color/${kind}" "${kind}")
                set_tests_properties(client.color_lookup_${kind} PROPERTIES
                    LABELS "local;integration;client;ownership" TIMEOUT 180)
                if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
                    add_test(NAME client.texture_lookup_${kind} COMMAND mc_test_texture_lookup
                        "${MC_CLIENT_LOOKUP_FIXTURE_DIR}/texture/resources"
                        "${MC_CLIENT_LOOKUP_FIXTURE_DIR}/texture/${kind}" "${kind}")
                    set_tests_properties(client.texture_lookup_${kind} PROPERTIES
                        LABELS "local;integration;client;graphics;ownership" TIMEOUT 240)
                endif()
            endforeach()
        endif()

        add_executable(mc_test_image tests/resources/image/test_image.c)
        target_link_libraries(mc_test_image PRIVATE mc_image ZLIB::ZLIBSTATIC mc_build_options)
        add_test(NAME resources.image COMMAND mc_test_image)
        set_tests_properties(resources.image PROPERTIES LABELS "public;unit;resources" TIMEOUT 60)
        add_executable(mc_image_probe tests/resources/image/image_probe.c)
        target_link_libraries(mc_image_probe PRIVATE mc_image mc_build_options)
        add_executable(mc_gui_main_menu_probe tests/client/gui/main_menu_probe.c)
        target_link_libraries(mc_gui_main_menu_probe PRIVATE mc_client_gui_main_menu mc_build_options)
        add_executable(mc_test_scaled_resolution tests/client/gui/test_scaled_resolution.c)
        target_link_libraries(mc_test_scaled_resolution PRIVATE mc_client_gui mc_build_options)
        add_test(NAME client.scaled_resolution COMMAND mc_test_scaled_resolution)
        set_tests_properties(client.scaled_resolution PROPERTIES LABELS "public;unit;client" TIMEOUT 60)
        add_executable(mc_test_font_renderer tests/client/gui/test_font_renderer.c)
        target_link_libraries(mc_test_font_renderer PRIVATE mc_client_gui mc_build_options)
        add_test(NAME client.font_renderer COMMAND mc_test_font_renderer)
        set_tests_properties(client.font_renderer PROPERTIES LABELS "public;unit;client" TIMEOUT 60)
        add_executable(mc_test_font_commands tests/client/gui/test_font_commands.c)
        target_link_libraries(mc_test_font_commands PRIVATE mc_client_gui mc_build_options)
        add_test(NAME client.font_commands COMMAND mc_test_font_commands)
        set_tests_properties(client.font_commands PROPERTIES LABELS "public;unit;client;ownership" TIMEOUT 60)
        add_executable(mc_test_client_assets tests/client/resources/test_assets.c)
        target_include_directories(mc_test_client_assets PRIVATE src/client)
        target_link_libraries(mc_test_client_assets PRIVATE mc_client_assets ZLIB::ZLIBSTATIC mc_build_options)
        add_test(NAME client.assets COMMAND mc_test_client_assets)
        set_tests_properties(client.assets PROPERTIES LABELS "public;unit;client;resources;ownership" TIMEOUT 60)
        add_executable(mc_test_game_settings_labels tests/client/settings/test_game_settings_labels.c)
        target_link_libraries(mc_test_game_settings_labels PRIVATE mc_client_game_settings_labels mc_build_options)
        add_test(NAME client.game_settings_labels COMMAND mc_test_game_settings_labels)
        set_tests_properties(client.game_settings_labels PROPERTIES LABELS "public;unit;client;resources;ownership" TIMEOUT 60)
        add_executable(mc_test_timer tests/client/timer/test_timer.c)
        target_link_libraries(mc_test_timer PRIVATE mc_client_timer mc_build_options)
        add_test(NAME client.timer COMMAND mc_test_timer)
        set_tests_properties(client.timer PROPERTIES LABELS "public;unit;client;numeric" TIMEOUT 60)
        add_executable(mc_test_timer_construction tests/client/timer/test_construction.c)
        target_include_directories(mc_test_timer_construction PRIVATE src)
        target_link_libraries(mc_test_timer_construction PRIVATE
            mc_client_timer_construction mc_platform_time mc_build_options)
        add_test(NAME client.timer_construction COMMAND mc_test_timer_construction)
        set_tests_properties(client.timer_construction PROPERTIES
            LABELS "public;unit;client;numeric;ownership" TIMEOUT 180)
        add_executable(mc_test_display_sdk_transport tests/client/display/test_sdk_transport.c)
        target_link_libraries(mc_test_display_sdk_transport PRIVATE mc_client_event_transport mc_build_options)
        add_executable(mc_test_display_sdk_window tests/client/display/test_sdk_window.c)
        # This test includes exact events.c once, forwards the actual SDL pump,
        # then invokes the real SDL event-watch API. No second events.o may be
        # extracted; require the real link-map proof in qualification.
        target_link_libraries(mc_test_display_sdk_window PRIVATE mc_client_display mc_resources mc_build_options)
        if(WIN32)
            target_link_libraries(mc_test_display_sdk_window PRIVATE user32 gdi32)
        else()
            target_link_libraries(mc_test_display_sdk_window PRIVATE X11::X11)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            foreach(kind IN ITEMS transport window)
                add_test(NAME client.display_sdk_${kind} COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_display_sdk.py"
                    --executable "$<TARGET_FILE:mc_test_display_sdk_${kind}>" --kind ${kind}
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/display-sdk-${kind}")
                set_tests_properties(client.display_sdk_${kind} PROPERTIES
                    LABELS "public;integration;client;graphics;ownership" TIMEOUT 240)
            endforeach()
        endif()
        add_executable(mc_test_client_display tests/client/display/test_display.c)
        target_link_libraries(mc_test_client_display PRIVATE mc_client_display mc_build_options)
        add_executable(mc_test_world_renderer tests/client/world/test_renderer.c)
        target_link_libraries(mc_test_world_renderer PRIVATE mc_client_world_renderer mc_build_options)
        add_executable(mc_test_client_world_session tests/client/world/test_session.c)
        target_link_libraries(mc_test_client_world_session PRIVATE mc_client_world_session mc_resources mc_build_options)
        add_test(NAME client.world_session COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/world/check_session.py"
            --executable "$<TARGET_FILE:mc_test_client_world_session>"
            --output "${CMAKE_CURRENT_BINARY_DIR}/world-session-output")
        set_tests_properties(client.world_session PROPERTIES LABELS "public;integration;client;ownership" TIMEOUT 120)
        add_executable(mc_test_world_mesher tests/client/world/test_mesher.c)
        target_link_libraries(mc_test_world_mesher PRIVATE mc_client_world_mesher mc_build_options)
        add_test(NAME client.world_mesher COMMAND mc_test_world_mesher)
        set_tests_properties(client.world_mesher PROPERTIES LABELS "public;unit;client;graphics" TIMEOUT 60)
        if(MC_ENABLE_RENDERING_REFERENCE_TESTS)
            find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
            foreach(probe IN ITEMS faces ambient_occlusion families)
                add_executable(mc_rendering_${probe}_probe "tests/reference/client/rendering/${probe}.c")
                target_link_libraries(mc_rendering_${probe}_probe PRIVATE
                    mc_client_world_mesher mc_simulation mc_build_options)
            endforeach()
            add_test(NAME reference.client_rendering COMMAND Python3::Interpreter
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/rendering/compare.py"
                --client-jar "${MC_REFERENCE_CLIENT_JAR}"
                --library-dir "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --faces-native "$<TARGET_FILE:mc_rendering_faces_probe>"
                --ao-native "$<TARGET_FILE:mc_rendering_ambient_occlusion_probe>"
                --families-native "$<TARGET_FILE:mc_rendering_families_probe>"
                --output-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-client-rendering")
            set_tests_properties(reference.client_rendering PROPERTIES LABELS "reference;original;client;graphics" TIMEOUT 180)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.world_renderer COMMAND mc_test_world_renderer)
            set_tests_properties(client.world_renderer PROPERTIES LABELS "public;integration;client;graphics" TIMEOUT 60)
            add_test(NAME client.display COMMAND mc_test_client_display)
            set_tests_properties(client.display PROPERTIES LABELS "public;integration;client;graphics" TIMEOUT 60)
        endif()
    endif()

    add_executable(mc_test_server_construction tests/server/runtime/test_construction.c)
    target_link_libraries(mc_test_server_construction PRIVATE mc_simulation mc_build_options)
    add_test(NAME server.construction COMMAND mc_test_server_construction)
    set_tests_properties(server.construction PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_dedicated_entity_index tests/world/runtime/dedicated/test_entity_index.c)
    target_link_libraries(mc_test_dedicated_entity_index PRIVATE mc_simulation mc_build_options)
    add_test(NAME world.dedicated_entity_index COMMAND mc_test_dedicated_entity_index)
    set_tests_properties(world.dedicated_entity_index PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_dedicated_construction tests/world/runtime/dedicated/test_construction.c)
    target_link_libraries(mc_test_dedicated_construction PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_info tests/world/info/test_info.c)
    target_include_directories(mc_test_world_info PRIVATE src/platform/time)
    target_link_libraries(mc_test_world_info PRIVATE mc_world_info mc_platform_time mc_build_options)
    add_test(NAME world.info COMMAND mc_test_world_info)
    set_tests_properties(world.info PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_world_info_observation_probe tests/world/info/observation_probe.c)
    target_link_libraries(mc_world_info_observation_probe PRIVATE mc_world_info mc_build_options)

    add_executable(mc_test_material tests/block/material/test_material.c)
    target_link_libraries(mc_test_material PRIVATE mc_block mc_build_options)
    add_test(NAME block.material COMMAND mc_test_material)
    set_tests_properties(block.material PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_block_registry tests/block/registry/test_registry.c)
    target_link_libraries(mc_test_block_registry PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.registry_construction COMMAND mc_test_block_registry)
    set_tests_properties(block.registry_construction PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_test_registry_managed tests/block/registry/test_managed.c)
    target_link_libraries(mc_test_registry_managed PRIVATE mc_registry mc_build_options)
    add_test(NAME block.registry_managed COMMAND mc_test_registry_managed)
    set_tests_properties(block.registry_managed PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_redstone_set tests/block/registry/test_redstone_set.c)
    target_link_libraries(mc_test_redstone_set PRIVATE mc_registry mc_build_options)
    add_test(NAME block.redstone_position_set COMMAND mc_test_redstone_set)
    set_tests_properties(block.redstone_position_set PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_redstone_set_probe tests/block/registry/redstone_set_probe.c)
    target_link_libraries(mc_redstone_set_probe PRIVATE mc_registry mc_build_options)

    add_executable(mc_test_biome_registry tests/world/biome/test_registry.c)
    target_link_libraries(mc_test_biome_registry PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME world.biome_registry COMMAND mc_test_biome_registry)
    set_tests_properties(world.biome_registry PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_test_biome_sampling tests/world/biome/test_sampling.c)
    target_link_libraries(mc_test_biome_sampling PRIVATE mc_registry mc_build_options)
    add_test(NAME world.biome_reference_sampling COMMAND mc_test_biome_sampling)
    set_tests_properties(world.biome_reference_sampling PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_biome_registry_probe tests/world/biome/registry_probe.c)
    target_link_libraries(mc_biome_registry_probe PRIVATE mc_resources mc_registry mc_build_options)

    add_executable(mc_test_item_registry tests/item/registry/test_registry.c)
    target_link_libraries(mc_test_item_registry PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME item.registry_construction COMMAND mc_test_item_registry)
    set_tests_properties(item.registry_construction PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_test_item_registry_runtime tests/item/registry/test_runtime.c)
    target_link_libraries(mc_test_item_registry_runtime PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME item.registry_runtime COMMAND mc_test_item_registry_runtime)
    set_tests_properties(item.registry_runtime PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_registry_prefix_probe tests/block/registry/prefix_probe.c)
    target_link_libraries(mc_registry_prefix_probe PRIVATE mc_registry mc_build_options)

    add_executable(mc_item_registry_prefix_probe tests/item/registry/prefix_probe.c)
    target_link_libraries(mc_item_registry_prefix_probe PRIVATE mc_registry mc_build_options)

    add_executable(mc_material_probe tests/block/material/material_probe.c)
    target_link_libraries(mc_material_probe PRIVATE mc_block mc_build_options)

    add_executable(mc_test_millis tests/platform/time/test_millis.c)
    target_include_directories(mc_test_millis PRIVATE src/platform/time)
    target_link_libraries(mc_test_millis PRIVATE mc_platform_time mc_build_options)
    add_test(NAME platform.millis COMMAND mc_test_millis)
    set_tests_properties(platform.millis PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_nanotime tests/platform/time/test_nanotime.c)
    target_include_directories(mc_test_nanotime PRIVATE src/platform/time)
    target_link_libraries(mc_test_nanotime PRIVATE mc_platform_time mc_build_options)
    add_test(NAME platform.nanotime COMMAND mc_test_nanotime)
    set_tests_properties(platform.nanotime PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_nanotime_conversion tests/platform/time/test_nanotime_conversion.c)
    target_include_directories(mc_test_nanotime_conversion PRIVATE src/platform/time tests/reference/java)
    target_link_libraries(mc_test_nanotime_conversion PRIVATE mc_platform_time mc_build_options)
    add_test(NAME platform.nanotime_conversion COMMAND mc_test_nanotime_conversion)
    set_tests_properties(platform.nanotime_conversion PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_java_random_profile tests/core/java/test_random_profile.c)
    target_link_libraries(mc_test_java_random_profile PRIVATE mc_core mc_c_atomics Threads::Threads mc_build_options)
    add_executable(mc_test_java_random_profile_inputs tests/core/java/test_random_profile_inputs.c)
    target_link_libraries(mc_test_java_random_profile_inputs PRIVATE mc_core mc_c_atomics mc_build_options)
    add_test(NAME core.java_random_profile_inputs COMMAND mc_test_java_random_profile_inputs)
    set_tests_properties(core.java_random_profile_inputs PROPERTIES LABELS "public;unit" TIMEOUT 60)
    add_test(NAME core.java_random_profile COMMAND mc_test_java_random_profile)
    set_tests_properties(core.java_random_profile PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_random_constructor_observation tests/core/java/test_random_constructor_observation.c)
    target_link_libraries(mc_test_random_constructor_observation PRIVATE mc_core mc_c_atomics mc_build_options)
    add_test(NAME core.java_random_constructor_observation COMMAND mc_test_random_constructor_observation)
    set_tests_properties(core.java_random_constructor_observation PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_item_stack tests/item/stack/test_stack.c)
    target_link_libraries(mc_test_item_stack PRIVATE mc_item mc_build_options)
    add_test(NAME item.stack COMMAND mc_test_item_stack)
    set_tests_properties(item.stack PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_registry_stacks tests/item/registry/test_stacks.c)
    target_link_libraries(mc_test_registry_stacks PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME item.registry_stacks COMMAND mc_test_registry_stacks)
    set_tests_properties(item.registry_stacks PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_recipe_bootstrap_probe tests/item/recipes/bootstrap_probe.c)
    target_link_libraries(mc_recipe_bootstrap_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_test_recipes tests/item/recipes/test_recipes.c)
    target_link_libraries(mc_test_recipes PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME item.recipes COMMAND mc_test_recipes)
    set_tests_properties(item.recipes PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_crafting tests/inventory/crafting/test_crafting.c
        tests/item/recipes/test_matching.c)
    target_link_libraries(mc_test_crafting PRIVATE mc_inventory mc_resources mc_build_options)

    add_executable(mc_crafting_probe tests/inventory/crafting/trace_crafting.c)
    target_link_libraries(mc_crafting_probe PRIVATE mc_inventory mc_resources mc_build_options)

    add_executable(mc_stack_values_probe tests/item/registry/stack_values_probe.c)
    target_link_libraries(mc_stack_values_probe PRIVATE mc_registry mc_build_options)

    add_executable(mc_test_entity tests/entity/test_entity.c)
    target_include_directories(mc_test_entity PRIVATE src/entity src/core/java)
    target_link_libraries(mc_test_entity PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.construction COMMAND mc_test_entity)
    set_tests_properties(entity.construction PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_player_construction tests/entity/player/test_construction.c)
    target_link_libraries(mc_test_player_construction PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)

    add_executable(mc_player_construction_probe tests/entity/player/trace_construction.c)
    target_link_libraries(mc_player_construction_probe PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)

    add_executable(mc_test_entity_bounds tests/entity/bounds/test_bounds.c)
    target_link_libraries(mc_test_entity_bounds PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.bounds COMMAND mc_test_entity_bounds)
    set_tests_properties(entity.bounds PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_entity_bounds_values_probe tests/entity/bounds/values_probe.c)
    target_link_libraries(mc_entity_bounds_values_probe PRIVATE mc_entity_classes mc_c_atomics mc_build_options)

    add_executable(mc_test_falling_entity tests/entity/test_falling.c)
    target_include_directories(mc_test_falling_entity PRIVATE src/entity src/core/java)
    target_link_libraries(mc_test_falling_entity PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.falling COMMAND mc_test_falling_entity)
    set_tests_properties(entity.falling PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_entity_minecart tests/entity/minecart/test_minecart.c)
    target_link_libraries(mc_test_entity_minecart PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.minecart COMMAND mc_test_entity_minecart)
    set_tests_properties(entity.minecart PROPERTIES LABELS "public;unit;ownership;constructor")

    add_executable(mc_test_entity_minecart_inventory tests/entity/minecart/test_inventory.c)
    target_link_libraries(mc_test_entity_minecart_inventory PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.minecart_inventory COMMAND mc_test_entity_minecart_inventory)
    set_tests_properties(entity.minecart_inventory PROPERTIES LABELS "public;unit;ownership")

    add_executable(mc_test_entity_minecart_nbt tests/entity/minecart/test_nbt.c)
    target_link_libraries(mc_test_entity_minecart_nbt PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.minecart_nbt COMMAND mc_test_entity_minecart_nbt)
    set_tests_properties(entity.minecart_nbt PROPERTIES LABELS "public;unit;ownership")

    add_executable(mc_test_entity_minecart_storage tests/entity/minecart/test_storage.c)
    target_link_libraries(mc_test_entity_minecart_storage PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_entity_nbt tests/entity/nbt/test_nbt.c)
    target_include_directories(mc_test_entity_nbt PRIVATE src/entity src/core/java)
    target_link_libraries(mc_test_entity_nbt PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.nbt COMMAND mc_test_entity_nbt)
    set_tests_properties(entity.nbt PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_entity_classes tests/entity/classes/test_classes.c)
    target_link_libraries(mc_test_entity_classes PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.classes COMMAND mc_test_entity_classes)
    set_tests_properties(entity.classes PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_entity_factory tests/entity/nbt/test_factory.c)
    target_include_directories(mc_test_entity_factory PRIVATE src/entity src/core/java)
    target_link_libraries(mc_test_entity_factory PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.factory COMMAND mc_test_entity_factory)
    set_tests_properties(entity.factory PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_probe_entity_nbt tests/entity/nbt/nbt_probe.c)
    target_include_directories(mc_probe_entity_nbt PRIVATE src/entity src/core/java)
    target_link_libraries(mc_probe_entity_nbt PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_entity_batch tests/world/runtime/entity_batch/test_entity_batch.c)
    target_link_libraries(mc_test_world_entity_batch PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_entity_item_formula_probe tests/entity/item_formula_probe.c)
    target_link_libraries(mc_entity_item_formula_probe PRIVATE mc_simulation mc_build_options)

    add_executable(mc_test_scheduled_membership tests/world/schedule/test_membership.c)
    target_link_libraries(mc_test_scheduled_membership PRIVATE mc_world mc_build_options)
    add_test(NAME world.scheduled_membership COMMAND mc_test_scheduled_membership)
    set_tests_properties(world.scheduled_membership PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_scheduled_ordered tests/world/schedule/test_ordered.c)
    target_link_libraries(mc_test_scheduled_ordered PRIVATE mc_world mc_build_options)
    add_test(NAME world.scheduled_ordered COMMAND mc_test_scheduled_ordered)
    set_tests_properties(world.scheduled_ordered PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_scheduled_ordered_probe tests/world/schedule/ordered_probe.c)
    target_link_libraries(mc_scheduled_ordered_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_stored_chunk tests/world/storage/test_chunk.c)
    target_link_libraries(mc_test_stored_chunk PRIVATE mc_persistence mc_build_options)
    add_test(NAME world.stored_chunk COMMAND mc_test_stored_chunk)
    set_tests_properties(world.stored_chunk PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_stored_chunk_probe tests/world/storage/chunk_probe.c)
    target_link_libraries(mc_stored_chunk_probe PRIVATE mc_persistence mc_build_options)

    add_executable(mc_world_regions_probe tests/world/storage/regions_probe.c)
    target_link_libraries(mc_world_regions_probe PRIVATE mc_persistence mc_build_options)

    add_executable(mc_world_store_probe tests/world/storage/store_probe.c)
    target_link_libraries(mc_world_store_probe PRIVATE mc_persistence mc_build_options)

    add_executable(mc_test_world_runtime tests/world/runtime/test_runtime.c)
    target_link_libraries(mc_test_world_runtime PRIVATE mc_simulation mc_build_options)

    add_executable(mc_test_world_info_binding tests/world/runtime/test_info_binding.c)
    target_link_libraries(mc_test_world_info_binding PRIVATE mc_simulation mc_build_options)

    add_executable(mc_test_world_typed_info tests/world/storage/test_typed_info.c)
    target_include_directories(mc_test_world_typed_info PRIVATE src/world/storage)
    target_link_libraries(mc_test_world_typed_info PRIVATE mc_persistence mc_build_options)

    add_executable(mc_test_world_environment tests/world/runtime/test_environment.c)
    target_include_directories(mc_test_world_environment PRIVATE src/world/runtime)
    target_link_libraries(mc_test_world_environment PRIVATE mc_simulation mc_build_options)
    add_test(NAME world.environment COMMAND mc_test_world_environment)
    set_tests_properties(world.environment PROPERTIES LABELS "public;unit;math" TIMEOUT 30)

    add_executable(mc_test_world_construction tests/world/runtime/test_world_construction.c)
    target_link_libraries(mc_test_world_construction PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.construction COMMAND mc_test_world_construction)
    set_tests_properties(world.construction PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_game_world tests/world/runtime/test_game_world.c)
    target_link_libraries(mc_test_game_world PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_registry tests/world/runtime/test_world_registry.c)
    target_link_libraries(mc_test_world_registry PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_biome_runtime tests/world/dimension/test_biome_runtime.c)
    target_link_libraries(mc_test_world_biome_runtime PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_fixed_biome_runtime tests/world/dimension/test_fixed_biome_runtime.c)
    target_link_libraries(mc_test_fixed_biome_runtime PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_temperature tests/world/dimension/test_temperature.c)
    target_link_libraries(mc_test_temperature PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_lighting_runtime tests/world/runtime/test_lighting_runtime.c)
    target_link_libraries(mc_test_world_lighting_runtime PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_light_queue_runtime tests/world/runtime/test_light_queue_runtime.c)
    target_link_libraries(mc_test_world_light_queue_runtime PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_light_propagation tests/world/test_propagation.c)
    target_link_libraries(mc_test_world_light_propagation PRIVATE
        mc_client_world_session mc_resources mc_build_options)
    add_test(NAME world.light_propagation COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/world/check_propagation.py"
        --executable "$<TARGET_FILE:mc_test_world_light_propagation>"
        --output "${CMAKE_CURRENT_BINARY_DIR}/light-propagation-output")
    set_tests_properties(world.light_propagation PROPERTIES
        LABELS "public;integration;world;ownership;lighting" TIMEOUT 150)
    add_test(NAME tools.light_propagation_driver COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/world/test_check_propagation.py")
    set_tests_properties(tools.light_propagation_driver PROPERTIES
        LABELS "public;unit;tools;lighting" TIMEOUT 30)

    add_executable(mc_world_propagation_reference_probe
        tests/reference/world/propagation/native.c
        tests/reference/world/propagation/owner.c)
    target_include_directories(mc_world_propagation_reference_probe PRIVATE
        src/world/runtime src/world/storage src/platform/time)
    target_link_libraries(mc_world_propagation_reference_probe PRIVATE
        mc_simulation mc_resources mc_platform_time mc_c_atomics mc_build_options)
    add_test(NAME tools.propagation_processes COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_propagation_processes.py")
    set_tests_properties(tools.propagation_processes PROPERTIES
        LABELS "public;unit;tools;lighting" TIMEOUT 30)
    if(MC_ENABLE_PROPAGATION_REFERENCE_TESTS OR MC_ENABLE_STARTUP_GRID_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR)
            message(FATAL_ERROR "Propagation reference tests require MC_REFERENCE_CLIENT_JAR")
        endif()
        set(mc_propagation_server_arguments)
        if(MC_REFERENCE_SERVER_JAR)
            list(APPEND mc_propagation_server_arguments --server-jar "${MC_REFERENCE_SERVER_JAR}")
        endif()
        add_test(NAME reference.world_propagation COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/propagation/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" ${mc_propagation_server_arguments}
            --native-probe "$<TARGET_FILE:mc_world_propagation_reference_probe>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --timeout 240 --tested-source-id "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-world-propagation")
        set_tests_properties(reference.world_propagation PROPERTIES
            LABELS "local;reference;world;lighting;ownership" TIMEOUT 360 SKIP_RETURN_CODE 77)
        if(MC_ENABLE_STARTUP_GRID_REFERENCE_TESTS)
            add_test(NAME reference.world_startup_grid COMMAND Python3::Interpreter
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/propagation/compare.py"
                --client-jar "${MC_REFERENCE_CLIENT_JAR}" ${mc_propagation_server_arguments}
                --native-probe "$<TARGET_FILE:mc_world_propagation_reference_probe>"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --profiles fresh warm --timeout 1800
                --tested-source-id "${MC_REFERENCE_TESTED_COMMIT}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-world-startup-grid")
            set_tests_properties(reference.world_startup_grid PROPERTIES
                LABELS "local;reference;world;lighting;expensive" TIMEOUT 3900 SKIP_RETURN_CODE 77)
        endif()
    endif()

    add_executable(mc_test_world_chunk_objects tests/world/runtime/test_chunk_objects.c)
    target_link_libraries(mc_test_world_chunk_objects PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_chunk_provider_generation tests/world/runtime/test_chunk_provider_generation.c)
    target_link_libraries(mc_test_world_chunk_provider_generation PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_chunk_provider_activation tests/world/runtime/test_chunk_provider_activation.c)
    target_link_libraries(mc_test_world_chunk_provider_activation PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_mutations tests/world/runtime/test_mutations.c)
    target_link_libraries(mc_test_world_mutations PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_pressure_plate tests/world/runtime/test_pressure_plate.c)
    target_link_libraries(mc_test_world_pressure_plate PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_scheduling tests/world/runtime/test_scheduling.c)
    target_link_libraries(mc_test_world_scheduling PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_solid_support tests/world/runtime/test_solid_support.c)
    target_link_libraries(mc_test_world_solid_support PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_power tests/world/runtime/power/test_power.c)
    target_link_libraries(mc_test_world_power PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_redstone tests/world/runtime/redstone/test_redstone.c)
    target_link_libraries(mc_test_world_redstone PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_redstone_multichunk tests/world/runtime/redstone/test_multichunk.c)
    target_link_libraries(mc_test_world_redstone_multichunk PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_redstone_world_probe tests/world/runtime/redstone/world_probe.c)
    target_link_libraries(mc_redstone_world_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_executable(mc_test_world_access tests/world/runtime/world_access/test_world_access.c)
    target_link_libraries(mc_test_world_access PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_rails tests/world/runtime/rails/test_rails.c)
    target_link_libraries(mc_test_world_rails PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_executable(mc_test_world_minecart_detector
        tests/world/runtime/rails/test_minecart_detector.c)
    target_link_libraries(mc_test_world_minecart_detector PRIVATE
        mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_rail_errors tests/world/runtime/rails/test_error_sites.c)
    target_link_libraries(mc_test_world_rail_errors PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.rail_errors COMMAND mc_test_world_rail_errors)
    set_tests_properties(world.rail_errors PROPERTIES LABELS "public;unit;mutation;errors")

    add_executable(mc_test_world_queries tests/world/runtime/test_queries.c)
    target_link_libraries(mc_test_world_queries PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_nearby tests/world/runtime/collision/test_nearby.c)
    target_link_libraries(mc_test_world_nearby PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_class_entities tests/world/runtime/collision/test_class_entities.c)
    target_link_libraries(mc_test_world_class_entities PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_collision_boxes tests/world/runtime/collision/test_boxes.c)
    target_link_libraries(mc_test_world_collision_boxes PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_piston_geometry tests/world/runtime/collision/test_piston_geometry.c)
    target_link_libraries(mc_test_piston_geometry PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_piston_tile_entities tests/world/runtime/tile_entities/test_piston.c)
    target_link_libraries(mc_test_piston_tile_entities PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_piston_finalize tests/world/runtime/tile_entities/test_piston_finalize.c)
    target_link_libraries(mc_test_piston_finalize PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_piston_tick tests/world/runtime/tile_entities/test_piston_tick.c)
    target_link_libraries(mc_test_piston_tick PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_entity_movement tests/entity/movement/test_movement.c)
    target_link_libraries(mc_test_entity_movement PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_entity_environment tests/entity/movement/test_environment.c)
    target_link_libraries(mc_test_entity_environment PRIVATE mc_simulation mc_build_options)
    add_test(NAME entity.environment COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/environment/run_native.py"
        --executable "$<TARGET_FILE:mc_test_entity_environment>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/entity-environment-contracts")
    set_tests_properties(entity.environment PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 210)
    add_test(NAME tools.entity_environment_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/environment/test_controller.py")
    add_test(NAME tools.entity_environment_controller_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/environment/test_controller.py")
    set_tests_properties(tools.entity_environment_controller tools.entity_environment_controller_optimized
        PROPERTIES LABELS "public;unit;tools;entity" TIMEOUT 30)
    if(MC_ENABLE_ENTITY_ENVIRONMENT_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_SERVER_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Entity environment reference tests require both original JARs and client libraries")
        endif()
        add_test(NAME reference.entity_environment COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/environment/compare_environment.py"
            --executable "$<TARGET_FILE:mc_test_entity_environment>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --server-jar "${MC_REFERENCE_SERVER_JAR}"
            --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-entity-environment")
        set_tests_properties(reference.entity_environment PROPERTIES
            LABELS "local;reference;entity;world;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_player_movement tests/entity/movement/test_player_movement.c)
    target_link_libraries(mc_test_player_movement PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.player_movement COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/movement/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_movement>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-movement-contracts")
    set_tests_properties(entity.player_movement PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 210)
    add_test(NAME tools.player_movement_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/movement/test_controller.py")
    add_test(NAME tools.player_movement_controller_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/movement/test_controller.py")
    set_tests_properties(tools.player_movement_controller tools.player_movement_controller_optimized
        PROPERTIES LABELS "public;unit;tools;entity" TIMEOUT 30)
    if(MC_ENABLE_PLAYER_MOVEMENT_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Player movement reference tests require client JAR and library paths")
        endif()
        add_test(NAME reference.player_movement COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/movement/compare.py"
            --executable "$<TARGET_FILE:mc_test_player_movement>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}" --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-movement")
        set_tests_properties(reference.player_movement PROPERTIES
            LABELS "local;reference;entity;world;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_client_stats_file tests/client/statistics/test_writer.c)
    target_link_libraries(mc_test_client_stats_file PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME client.stats_file COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_stats_file.py"
        --executable "$<TARGET_FILE:mc_test_client_stats_file>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/stats-contracts")
    set_tests_properties(client.stats_file PROPERTIES
        LABELS "public;integration;client;storage;ownership" TIMEOUT 180)
    add_test(NAME tools.stats_capture COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_stats_capture.py")
    add_test(NAME tools.stats_capture_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_stats_capture.py")
    set_tests_properties(tools.stats_capture tools.stats_capture_optimized PROPERTIES
        LABELS "public;unit;tools;client" TIMEOUT 60)
    if(MC_ENABLE_CLIENT_STATS_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR)
            message(FATAL_ERROR "Client statistics reference tests require MC_REFERENCE_CLIENT_JAR")
        endif()
        add_test(NAME reference.client_stats_file COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/statistics/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --native "$<TARGET_FILE:mc_test_client_stats_file>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-client-statistics")
        set_tests_properties(reference.client_stats_file PROPERTIES
            LABELS "local;reference;client;storage;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_water_animation tests/client/texture_effects/test_water_animation.c)
    target_link_libraries(mc_test_water_animation PRIVATE
        mc_client_notice mc_resources mc_c_atomics mc_build_options)
    add_test(NAME client.water_animation COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_water_animation.py"
        --executable "$<TARGET_FILE:mc_test_water_animation>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/water-animation-contracts")
    set_tests_properties(client.water_animation PROPERTIES
        LABELS "public;integration;client;texture;ownership;numeric" TIMEOUT 240)
    add_test(NAME tools.water_animation_driver COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_water_animation_driver.py")
    add_test(NAME tools.water_animation_driver_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_water_animation_driver.py")
    set_tests_properties(tools.water_animation_driver tools.water_animation_driver_optimized
        PROPERTIES LABELS "public;unit;tools;client;texture" TIMEOUT 90)
    if(MC_ENABLE_WATER_ANIMATION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR)
            message(FATAL_ERROR "Water animation reference tests require MC_REFERENCE_CLIENT_JAR")
        endif()
        add_test(NAME reference.water_animation COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/water_animation/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --native "$<TARGET_FILE:mc_test_water_animation>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-water-animation")
        set_tests_properties(reference.water_animation PROPERTIES
            LABELS "local;reference;client;texture;numeric;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_lava_animation tests/client/texture_effects/test_lava_animation.c)
    target_link_libraries(mc_test_lava_animation PRIVATE
        mc_client_notice mc_resources mc_c_atomics mc_build_options)
    add_test(NAME client.lava_animation COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_lava_animation.py"
        --executable "$<TARGET_FILE:mc_test_lava_animation>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lava-animation-contracts")
    set_tests_properties(client.lava_animation PROPERTIES
        LABELS "public;integration;client;texture;ownership;numeric" TIMEOUT 240)
    add_test(NAME tools.lava_animation_driver COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_lava_animation_driver.py")
    add_test(NAME tools.lava_animation_driver_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_lava_animation_driver.py")
    set_tests_properties(tools.lava_animation_driver tools.lava_animation_driver_optimized
        PROPERTIES LABELS "public;unit;tools;client;texture" TIMEOUT 90)
    if(MC_ENABLE_LAVA_ANIMATION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR)
            message(FATAL_ERROR "Lava animation reference tests require MC_REFERENCE_CLIENT_JAR")
        endif()
        add_test(NAME reference.lava_animation COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lava_animation/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --native "$<TARGET_FILE:mc_test_lava_animation>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-lava-animation")
        set_tests_properties(reference.lava_animation PROPERTIES
            LABELS "local;reference;client;texture;numeric;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_player_nbt tests/entity/player/test_nbt.c)
    target_include_directories(mc_test_player_nbt PRIVATE src/platform/time)
    target_link_libraries(mc_test_player_nbt PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_executable(mc_test_player_nbt_coordinates tests/entity/player/test_nbt_coordinates.c)
    target_include_directories(mc_test_player_nbt_coordinates PRIVATE src/platform/time)
    target_link_libraries(mc_test_player_nbt_coordinates PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_executable(mc_test_player_nbt_errors tests/entity/player/test_nbt_errors.c)
    target_link_libraries(mc_test_player_nbt_errors PRIVATE mc_simulation mc_c_atomics mc_build_options)
    foreach(kind IN ITEMS nbt coordinates)
        if(kind STREQUAL "nbt")
            set(nbt_target mc_test_player_nbt)
            set(nbt_case entity.player_nbt)
        else()
            set(nbt_target mc_test_player_nbt_coordinates)
            set(nbt_case entity.player_nbt_coordinates)
        endif()
        add_test(NAME "${nbt_case}" COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_nbt.py"
            --executable "$<TARGET_FILE:${nbt_target}>" --kind "${kind}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-nbt-${kind}-contracts")
        set_tests_properties("${nbt_case}" PROPERTIES
            LABELS "public;integration;entity;storage;ownership" TIMEOUT 240)
    endforeach()
    add_test(NAME entity.player_nbt_errors COMMAND mc_test_player_nbt_errors)
    set_tests_properties(entity.player_nbt_errors PROPERTIES
        LABELS "public;unit;entity;ownership" TIMEOUT 30)
    foreach(controller IN ITEMS native original compare)
        if(controller STREQUAL "native")
            set(nbt_controller "tests/tools/entity/test_player_nbt_controller.py")
        elseif(controller STREQUAL "original")
            set(nbt_controller "tests/reference/entity/player_nbt/test_controller.py")
        else()
            set(nbt_controller "tests/reference/entity/player_nbt/test_compare.py")
        endif()
        add_test(NAME "tools.player_nbt_${controller}" COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/${nbt_controller}")
        add_test(NAME "tools.player_nbt_${controller}_optimized" COMMAND Python3::Interpreter -O
            "${CMAKE_CURRENT_SOURCE_DIR}/${nbt_controller}")
        set_tests_properties("tools.player_nbt_${controller}" "tools.player_nbt_${controller}_optimized"
            PROPERTIES LABELS "public;unit;tools;entity;storage" TIMEOUT 90)
    endforeach()
    if(MC_ENABLE_PLAYER_NBT_REFERENCE_TESTS)
        if(NOT WIN32)
            message(FATAL_ERROR "The Player NBT original profile currently requires Windows x64 natives")
        endif()
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR OR NOT MC_REFERENCE_CLIENT_NATIVE_DIR)
            message(FATAL_ERROR "Player NBT reference tests require the client JAR, SDK library directory and native directory")
        endif()
        add_test(NAME reference.player_nbt COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_nbt/run_reference.py"
            --executable "$<TARGET_FILE:mc_test_player_nbt>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --natives "${MC_REFERENCE_CLIENT_NATIVE_DIR}" --java "${Java_JAVA_EXECUTABLE}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-nbt")
        set_tests_properties(reference.player_nbt PROPERTIES
            LABELS "local;reference;entity;storage;ownership" TIMEOUT 900)
    endif()

    add_executable(mc_test_player_statistics tests/client/statistics/test_player_dispatch.c)
    target_include_directories(mc_test_player_statistics PRIVATE src/platform/time)
    target_link_libraries(mc_test_player_statistics PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_executable(mc_test_player_statistics_fault
        tests/client/statistics/test_player_dispatch.c tests/client/statistics/notice_clock_fault.c)
    target_compile_definitions(mc_test_player_statistics_fault PRIVATE MC_PLAYER_STATISTICS_FAULT_TEST)
    target_include_directories(mc_test_player_statistics_fault PRIVATE src/platform/time)
    target_link_libraries(mc_test_player_statistics_fault PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME client.player_statistics COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_statistics.py"
        --executable "$<TARGET_FILE:mc_test_player_statistics>"
        --fault-executable "$<TARGET_FILE:mc_test_player_statistics_fault>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-statistics-contracts")
    set_tests_properties(client.player_statistics PROPERTIES
        LABELS "public;integration;client;entity;storage;ownership" TIMEOUT 240)
    add_test(NAME tools.player_statistics_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_statistics_controller.py")
    add_test(NAME tools.player_statistics_controller_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_statistics_controller.py")
    set_tests_properties(tools.player_statistics_controller tools.player_statistics_controller_optimized
        PROPERTIES LABELS "public;unit;tools;client;entity" TIMEOUT 60)
    add_executable(mc_test_player_callsites tests/client/statistics/test_player_callsites.c)
    target_include_directories(mc_test_player_callsites PRIVATE src/platform/time)
    target_link_libraries(mc_test_player_callsites PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME client.player_statistic_callsites COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_callsites.py"
        --executable "$<TARGET_FILE:mc_test_player_callsites>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-callsite-contracts")
    set_tests_properties(client.player_statistic_callsites PROPERTIES
        LABELS "public;integration;client;entity;world;storage;ownership" TIMEOUT 240)
    if(MC_ENABLE_PLAYER_STATISTICS_REFERENCE_TESTS)
        if(NOT WIN32)
            message(FATAL_ERROR "The original Player statistic dispatch profile currently qualifies Windows x64 only")
        endif()
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR OR NOT MC_REFERENCE_CLIENT_NATIVE_DIR)
            message(FATAL_ERROR "Player statistic reference tests require client JAR, libraries and Windows natives")
        endif()
        add_test(NAME reference.player_statistics COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/player_statistics/compare.py"
            --native-probe "$<TARGET_FILE:mc_test_player_statistics>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --natives "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-statistics")
        set_tests_properties(reference.player_statistics PROPERTIES
            LABELS "local;reference;client;entity;storage;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_player_travel tests/entity/living/test_travel.c)
    target_link_libraries(mc_test_player_travel PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.player_travel COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/travel/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_travel>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-travel-contracts")
    set_tests_properties(entity.player_travel PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 210)
    add_test(NAME tools.player_travel_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/travel/test_controller.py")
    add_test(NAME tools.player_travel_controller_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/travel/test_controller.py")
    set_tests_properties(tools.player_travel_controller tools.player_travel_controller_optimized
        PROPERTIES LABELS "public;unit;tools;entity" TIMEOUT 30)
    if(MC_ENABLE_PLAYER_TRAVEL_REFERENCE_TESTS)
        if(NOT WIN32)
            message(FATAL_ERROR "The original Player travel reference profile currently qualifies Windows x64 only")
        endif()
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR OR NOT MC_REFERENCE_CLIENT_NATIVE_DIR)
            message(FATAL_ERROR "Player travel reference tests require client JAR, libraries and Windows natives")
        endif()
        add_test(NAME reference.player_travel COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/travel/compare.py"
            --executable "$<TARGET_FILE:mc_test_player_travel>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --native-dir "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-travel")
        set_tests_properties(reference.player_travel PROPERTIES
            LABELS "local;reference;entity;world;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_player_base_tick tests/entity/tick/test_environment_tick.c)
    target_link_libraries(mc_test_player_base_tick PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.player_base_tick COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/base_tick/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_base_tick>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-base-tick-contracts")
    set_tests_properties(entity.player_base_tick PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 210)
    add_test(NAME tools.player_base_tick_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/base_tick/test_controller.py")
    add_test(NAME tools.player_base_tick_controller_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/base_tick/test_controller.py")
    set_tests_properties(tools.player_base_tick_controller tools.player_base_tick_controller_optimized
        PROPERTIES LABELS "public;unit;tools;entity" TIMEOUT 30)
    if(MC_ENABLE_PLAYER_BASE_TICK_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Player environment reference tests require client JAR and libraries")
        endif()
        add_test(NAME reference.player_base_tick COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/base_tick/compare.py"
            --executable "$<TARGET_FILE:mc_test_player_base_tick>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}" --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-base-tick")
        set_tests_properties(reference.player_base_tick PROPERTIES
            LABELS "local;reference;entity;world;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_inventory_tick tests/entity/inventory/test_tick.c)
    target_link_libraries(mc_test_inventory_tick PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.inventory_tick COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/inventory_tick/run_native.py"
        --executable "$<TARGET_FILE:mc_test_inventory_tick>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/inventory-tick-contracts")
    set_tests_properties(entity.inventory_tick PROPERTIES
        LABELS "public;integration;entity;inventory;world;ownership" TIMEOUT 210)
    add_test(NAME tools.inventory_tick_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/inventory_tick/test_controller.py")
    add_test(NAME tools.inventory_tick_controller_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/inventory_tick/test_controller.py")
    set_tests_properties(tools.inventory_tick_controller tools.inventory_tick_controller_optimized
        PROPERTIES LABELS "public;unit;tools;inventory" TIMEOUT 30)
    if(MC_ENABLE_INVENTORY_TICK_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Inventory tick reference tests require client JAR and libraries")
        endif()
        add_test(NAME reference.inventory_tick COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/inventory_tick/compare.py"
            --executable "$<TARGET_FILE:mc_test_inventory_tick>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-inventory-tick")
        set_tests_properties(reference.inventory_tick PROPERTIES
            LABELS "local;reference;inventory;entity;world;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_player_action tests/entity/action/test_action.c)
    target_link_libraries(mc_test_player_action PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.player_action COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/action/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_action>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-action-contracts")
    set_tests_properties(entity.player_action PROPERTIES
        LABELS "public;integration;entity;world;ownership;math" TIMEOUT 240)
    add_test(NAME tools.player_action_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/action/test_controller.py")
    add_test(NAME tools.player_action_controller_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/action/test_controller.py")
    set_tests_properties(tools.player_action_controller tools.player_action_controller_optimized
        PROPERTIES LABELS "public;unit;tools;entity" TIMEOUT 60)
    if(MC_ENABLE_PLAYER_ACTION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Player action reference tests require client JAR and libraries")
        endif()
        add_executable(mc_action_math_probe tests/entity/action/math_probe.c)
        target_link_libraries(mc_action_math_probe PRIVATE mc_simulation mc_build_options)
        add_test(NAME reference.player_action COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/action/compare.py"
            --executable "$<TARGET_FILE:mc_test_player_action>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}" --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-action")
        add_test(NAME reference.player_action_math COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/action/compare_math.py"
            --executable "$<TARGET_FILE:mc_action_math_probe>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-action-math")
        add_test(NAME reference.player_action_dispatch COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/action/audit_dispatch.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --source-root "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(reference.player_action reference.player_action_math reference.player_action_dispatch
            PROPERTIES LABELS "local;reference;entity;world;math" TIMEOUT 480)
    endif()

    add_executable(mc_test_item_tick tests/entity/item/test_tick.c)
    target_include_directories(mc_test_item_tick PRIVATE src)
    target_link_libraries(mc_test_item_tick PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.item_tick COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_tick/run.py"
        --native "$<TARGET_FILE:mc_test_item_tick>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-tick-contracts")
    set_tests_properties(entity.item_tick PROPERTIES
        LABELS "public;integration;entity;world;ownership;math" TIMEOUT 240)
    add_test(NAME tools.item_tick_controller COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_tick/test_controller.py")
    add_test(NAME tools.item_tick_controller_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_tick/test_controller.py")
    set_tests_properties(tools.item_tick_controller tools.item_tick_controller_optimized PROPERTIES
        LABELS "public;unit;tools;entity" TIMEOUT 60)
    if(MC_ENABLE_ITEM_TICK_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Item tick reference requires original client JAR and libraries")
        endif()
        add_test(NAME reference.item_tick COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_tick/run.py"
            --native "$<TARGET_FILE:mc_test_item_tick>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --library-dir "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-item-tick")
        set_tests_properties(reference.item_tick PROPERTIES
            LABELS "local;reference;entity;world;ownership;math" TIMEOUT 480)
    endif()

    add_executable(mc_test_item_pickup tests/entity/item/test_pickup.c)
    target_include_directories(mc_test_item_pickup PRIVATE src/platform/time)
    target_link_libraries(mc_test_item_pickup PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.item_pickup COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_pickup/run_native.py"
        --executable "$<TARGET_FILE:mc_test_item_pickup>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-pickup-contracts")
    set_tests_properties(entity.item_pickup PROPERTIES
        LABELS "public;integration;entity;inventory;ownership;math" TIMEOUT 240)
    if(MC_ENABLE_ITEM_PICKUP_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Item pickup reference tests require client JAR and libraries")
        endif()
        add_test(NAME reference.item_pickup COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_pickup/compare.py"
            --executable "$<TARGET_FILE:mc_test_item_pickup>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-item-pickup")
        add_test(NAME reference.item_pickup_dispatch COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_pickup/audit_dispatch.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}")
        set_tests_properties(reference.item_pickup reference.item_pickup_dispatch PROPERTIES
            LABELS "local;reference;entity;inventory;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_player_active_tick tests/entity/player/test_tick.c)
    add_executable(mc_test_player_active_allocation tests/entity/player/test_tick_allocation.c)
    foreach(target mc_test_player_active_tick mc_test_player_active_allocation)
        target_include_directories(${target} PRIVATE src/platform/time)
        target_link_libraries(${target} PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_build_options)
    endforeach()
    add_test(NAME entity.player_active_tick COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/active_tick/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_active_tick>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-active-tick-contracts")
    add_test(NAME entity.player_active_allocation COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/active_tick/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_active_allocation>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-active-allocation-contracts")
    set_tests_properties(entity.player_active_tick entity.player_active_allocation PROPERTIES
        LABELS "public;integration;entity;world;ownership;math" TIMEOUT 240)
    add_test(NAME tools.player_active_tick_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/active_tick/test_controller.py")
    add_test(NAME tools.player_active_tick_controller_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/active_tick/test_controller.py")
    set_tests_properties(tools.player_active_tick_controller tools.player_active_tick_controller_optimized
        PROPERTIES LABELS "public;unit;tools;entity" TIMEOUT 60)
    if(MC_ENABLE_PLAYER_ACTIVE_TICK_REFERENCE_TESTS OR MC_ENABLE_JAVA_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_executable(mc_player_atan2_probe tests/entity/player/atan2_probe.c)
        target_link_libraries(mc_player_atan2_probe PRIVATE mc_simulation mc_build_options)
        add_test(NAME reference.player_active_tick_math COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/active_tick/compare_math.py"
            --executable "$<TARGET_FILE:mc_player_atan2_probe>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-active-math")
        set_tests_properties(reference.player_active_tick_math PROPERTIES
            LABELS "public;jdk;reference;entity;math" TIMEOUT 180)
    endif()
    if(MC_ENABLE_PLAYER_ACTIVE_TICK_REFERENCE_TESTS)
        if(NOT MC_REFERENCE_CLIENT_JAR)
            message(FATAL_ERROR "Player active tick dispatch audit requires client JAR")
        endif()
        add_test(NAME reference.player_active_tick_dispatch COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/active_tick/audit_dispatch.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --source-root "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(reference.player_active_tick_dispatch PROPERTIES
            LABELS "local;reference;entity;ownership" TIMEOUT 60)
        if(WIN32)
            if(NOT MC_REFERENCE_CLIENT_LIBRARY_DIR OR NOT MC_REFERENCE_CLIENT_NATIVE_DIR)
                message(FATAL_ERROR "Original Player active tick profile requires Windows client libraries/native")
            endif()
            add_test(NAME reference.player_active_tick COMMAND Python3::Interpreter
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/active_tick/compare.py"
                --executable "$<TARGET_FILE:mc_test_player_active_tick>"
                --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
                --natives "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --source-root "${CMAKE_CURRENT_SOURCE_DIR}" --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-active-tick")
            set_tests_properties(reference.player_active_tick PROPERTIES
                LABELS "local;reference;entity;world;ownership" TIMEOUT 480)
        endif()
    endif()

    add_executable(mc_test_player_spawn tests/entity/player/test_spawn.c)
    add_executable(mc_test_player_spawn_allocation tests/entity/player/test_spawn_allocation.c)
    add_executable(mc_position_probe tests/entity/player/position_probe.c)
    foreach(target mc_test_player_spawn mc_test_player_spawn_allocation mc_position_probe)
        target_include_directories(${target} PRIVATE src/platform/time)
        target_link_libraries(${target} PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_build_options)
    endforeach()
    add_test(NAME entity.player_spawn_reset COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/spawn_reset/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_spawn>" --mode spawn-reference
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-spawn-contracts")
    add_test(NAME entity.player_spawn_allocation COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/spawn_reset/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_spawn_allocation>" --mode spawn
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-spawn-allocation-contracts")
    add_test(NAME entity.position COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/spawn_reset/run_native.py"
        --executable "$<TARGET_FILE:mc_position_probe>" --mode position-probe
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/entity-position-contracts")
    set_tests_properties(entity.player_spawn_reset entity.player_spawn_allocation entity.position
        PROPERTIES LABELS "public;integration;entity;world;ownership;math" TIMEOUT 240)
    add_test(NAME tools.spawn_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/spawn_reset/test_reference.py")
    add_test(NAME tools.spawn_reference_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/spawn_reset/test_reference.py")
    add_test(NAME tools.position_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/position/test_position.py")
    add_test(NAME tools.position_reference_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/position/test_position.py")
    set_tests_properties(tools.spawn_reference tools.spawn_reference_optimized
        tools.position_reference tools.position_reference_optimized PROPERTIES
        LABELS "public;unit;tools;entity;math" TIMEOUT 60)
    if(MC_ENABLE_PLAYER_SPAWN_REFERENCE_TESTS OR MC_ENABLE_POSITION_REFERENCE_TESTS)
        if(NOT WIN32)
            message(FATAL_ERROR "Original spawn/reset and position profiles require qualified Windows x64")
        endif()
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR OR
                NOT MC_REFERENCE_CLIENT_NATIVE_DIR)
            message(FATAL_ERROR "Original spawn/reset and position profiles require client JAR/libraries/natives")
        endif()
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(MC_ENABLE_PLAYER_SPAWN_REFERENCE_TESTS)
            add_test(NAME reference.player_spawn_reset COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/spawn_reset/compare.py"
                --executable "$<TARGET_FILE:mc_test_player_spawn>" --mode spawn
                --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
                --natives "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-spawn")
            set_tests_properties(reference.player_spawn_reset PROPERTIES
                LABELS "local;reference;original;entity;world;ownership;math" TIMEOUT 480)
        endif()
        if(MC_ENABLE_POSITION_REFERENCE_TESTS)
            add_test(NAME reference.entity_position COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/spawn_reset/compare.py"
                --executable "$<TARGET_FILE:mc_position_probe>" --mode position
                --client-jar "${MC_REFERENCE_CLIENT_JAR}" --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
                --natives "${MC_REFERENCE_CLIENT_NATIVE_DIR}"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-entity-position")
            set_tests_properties(reference.entity_position PROPERTIES
                LABELS "local;reference;original;entity;world;math" TIMEOUT 480)
        endif()
    endif()

    add_executable(mc_test_pickup_particles tests/entity/particles/test_pickup.c)
    target_include_directories(mc_test_pickup_particles PRIVATE src)
    target_link_libraries(mc_test_pickup_particles PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.pickup_particles COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/particles/run.py"
        --native "$<TARGET_FILE:mc_test_pickup_particles>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/pickup-particle-contracts")
    set_tests_properties(entity.pickup_particles PROPERTIES
        LABELS "public;integration;entity;client;ownership;math" TIMEOUT 180)
    add_executable(mc_test_effect_renderer tests/client/particles/test_effect_renderer.c)
    target_link_libraries(mc_test_effect_renderer PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME client.effect_renderer COMMAND mc_test_effect_renderer)
    set_tests_properties(client.effect_renderer PROPERTIES
        LABELS "public;integration;client;entity;ownership" TIMEOUT 180)
    add_executable(mc_test_player_pickup_particles tests/client/particles/test_player_pickup.c)
    target_include_directories(mc_test_player_pickup_particles PRIVATE src/platform/time)
    target_link_libraries(mc_test_player_pickup_particles PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME client.player_pickup_particles COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_pickup_particles.py"
        --executable "$<TARGET_FILE:mc_test_player_pickup_particles>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-pickup-particle-contracts")
    set_tests_properties(client.player_pickup_particles PROPERTIES
        LABELS "public;integration;client;entity;inventory;ownership" TIMEOUT 240)
    add_test(NAME tools.particle_controller COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/particles/test_driver.py")
    add_test(NAME tools.particle_controller_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/particles/test_driver.py")
    set_tests_properties(tools.particle_controller tools.particle_controller_optimized PROPERTIES
        LABELS "public;unit;tools;entity" TIMEOUT 60)
    if(MC_ENABLE_PARTICLE_REFERENCE_TESTS OR MC_ENABLE_JAVA_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME tools.particle_observer_cleanup COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/particles/test_cleanup.py"
            --java "${Java_JAVA_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/particle-observer-cleanup")
        set_tests_properties(tools.particle_observer_cleanup PROPERTIES
            LABELS "public;jdk;tools;ownership" TIMEOUT 120)
    endif()
    if(MC_ENABLE_PARTICLE_REFERENCE_TESTS)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Pickup particle reference requires original client JAR and pinned libraries")
        endif()
        add_test(NAME reference.pickup_particles COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/particles/run.py"
            --native "$<TARGET_FILE:mc_test_pickup_particles>"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --library-dir "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-pickup-particles")
        set_tests_properties(reference.pickup_particles PROPERTIES
            LABELS "local;reference;entity;client;ownership;math" TIMEOUT 240)
    endif()

    add_executable(mc_test_world_movement tests/entity/movement/test_world_movement.c)
    target_link_libraries(mc_test_world_movement PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_entity_movement_probe tests/entity/movement/movement_probe.c)
    target_link_libraries(mc_entity_movement_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_piston_float_probe tests/world/runtime/tile_entities/piston_float_probe.c)
    target_link_libraries(mc_piston_float_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_directions tests/block/registry/test_directions.c)
    target_link_libraries(mc_test_directions PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.directions COMMAND mc_test_directions)
    set_tests_properties(block.directions PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_horizontal_directions tests/block/registry/test_horizontal_directions.c)
    target_link_libraries(mc_test_horizontal_directions PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.horizontal_directions COMMAND mc_test_horizontal_directions)
    set_tests_properties(block.horizontal_directions PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_world_nearby_values_probe tests/world/runtime/collision/nearby_probe.c)
    target_link_libraries(mc_world_nearby_values_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_world_block_shapes_probe tests/world/runtime/collision/block_shape_probe.c)
    target_link_libraries(mc_world_block_shapes_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_piston_geometry_probe tests/world/runtime/collision/piston_geometry_probe.c)
    target_link_libraries(mc_piston_geometry_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_fluids tests/world/runtime/test_fluids.c)
    target_link_libraries(mc_test_world_fluids PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_falling tests/world/runtime/test_falling.c)
    target_link_libraries(mc_test_world_falling PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_snow tests/world/runtime/test_snow.c)
    target_link_libraries(mc_test_world_snow PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_plant_lifecycle tests/world/runtime/plants/test_lifecycle.c)
    target_link_libraries(mc_test_plant_lifecycle PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_plant_lifecycle_probe tests/world/runtime/plants/reference_probe.c)
    target_link_libraries(mc_plant_lifecycle_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_entity_publication tests/world/runtime/test_entity_publication.c)
    target_link_libraries(mc_test_world_entity_publication PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_player_publication
        tests/world/runtime/player_publication/test_publication.c)
    target_link_libraries(mc_test_world_player_publication PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.player_publication COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_player_publication.py"
        --executable "$<TARGET_FILE:mc_test_world_player_publication>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-player-publication-contracts")
    set_tests_properties(world.player_publication PROPERTIES
        LABELS "public;integration;world;entity;ownership" TIMEOUT 120)
    if(MC_ENABLE_PLAYER_PUBLICATION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR)
            message(FATAL_ERROR "Player publication reference tests require MC_REFERENCE_CLIENT_JAR")
        endif()
        add_test(NAME reference.player_publication COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/player_publication/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --native "$<TARGET_FILE:mc_test_world_player_publication>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-publication")
        set_tests_properties(reference.player_publication PROPERTIES
            LABELS "local;reference;world;entity;ownership" TIMEOUT 240)
    endif()

    add_executable(mc_test_world_tile_entities tests/world/runtime/tile_entities/test_tile_entities.c)
    target_link_libraries(mc_test_world_tile_entities PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_executable(mc_test_tile_entity_class_literals tests/world/runtime/tile_entities/test_class_literals.c)
    target_link_libraries(mc_test_tile_entity_class_literals PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.tile_entity_class_literals COMMAND mc_test_tile_entity_class_literals)
    set_tests_properties(world.tile_entity_class_literals PROPERTIES LABELS "public;unit;world;ownership")

    add_executable(mc_test_world_saving tests/world/runtime/saving/test_saving.c)
    target_link_libraries(mc_test_world_saving PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_provider_maintenance tests/world/runtime/test_maintenance.c)
    target_link_libraries(mc_test_world_provider_maintenance PRIVATE
        mc_simulation mc_resources mc_build_options)
    add_test(NAME world.provider_maintenance COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/maintenance/run_native.py"
        --executable "$<TARGET_FILE:mc_test_world_provider_maintenance>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/provider-maintenance-contracts")
    set_tests_properties(world.provider_maintenance PROPERTIES
        LABELS "public;integration;world;ownership;storage" TIMEOUT 210)
    add_test(NAME tools.provider_maintenance_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/maintenance/test_controller.py")
    set_tests_properties(tools.provider_maintenance_controller PROPERTIES
        LABELS "public;unit;tools;storage" TIMEOUT 30)
    if(MC_ENABLE_MAINTENANCE_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Maintenance reference tests require client JAR and library paths")
        endif()
        add_test(NAME reference.provider_maintenance COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/maintenance/compare_maintenance.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --native "$<TARGET_FILE:mc_test_world_provider_maintenance>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-provider-maintenance")
        set_tests_properties(reference.provider_maintenance PROPERTIES
            LABELS "local;reference;world;storage;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_material_movement tests/world/runtime/test_material_movement.c)
    target_link_libraries(mc_test_material_movement PRIVATE mc_simulation mc_build_options)
    add_test(NAME world.material_movement COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/material_movement/run_native.py"
        --executable "$<TARGET_FILE:mc_test_material_movement>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/material-movement-contracts")
    set_tests_properties(world.material_movement PROPERTIES
        LABELS "public;integration;world;entity;ownership" TIMEOUT 210)
    add_test(NAME tools.material_movement_controller COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/material_movement/test_controller.py")
    set_tests_properties(tools.material_movement_controller PROPERTIES
        LABELS "public;unit;tools;world" TIMEOUT 30)
    if(MC_ENABLE_MATERIAL_MOVEMENT_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_CLIENT_LIBRARY_DIR)
            message(FATAL_ERROR "Material movement reference tests require client JAR and library paths")
        endif()
        add_test(NAME reference.material_movement COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/material_movement/compare_material_movement.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --libraries "${MC_REFERENCE_CLIENT_LIBRARY_DIR}"
            --native "$<TARGET_FILE:mc_test_material_movement>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-material-movement")
        set_tests_properties(reference.material_movement PROPERTIES
            LABELS "local;reference;world;entity;ownership" TIMEOUT 480)
    endif()

    add_executable(mc_test_game_entities tests/world/runtime/entities/test_game_entities.c)
    target_link_libraries(mc_test_game_entities PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_dungeon tests/world/runtime/population/test_dungeon.c)
    target_link_libraries(mc_test_dungeon PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_overworld_population tests/world/runtime/population/test_overworld.c)
    target_link_libraries(mc_test_overworld_population PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_natural_population_probe tests/world/runtime/population/natural_probe.c)
    target_link_libraries(mc_natural_population_probe PRIVATE mc_simulation mc_resources mc_c_atomics mc_build_options)

    add_executable(mc_test_registry_falling tests/block/registry/test_falling.c)
    target_link_libraries(mc_test_registry_falling PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.registry_falling COMMAND mc_test_registry_falling)
    set_tests_properties(block.registry_falling PROPERTIES LABELS "public;integration;registry" TIMEOUT 90)

    add_executable(mc_dungeon_loot_probe tests/world/runtime/population/dungeon_loot_probe.c)
    target_link_libraries(mc_dungeon_loot_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_dungeon_placement_probe tests/world/runtime/population/dungeon_placement_probe.c)
    target_link_libraries(mc_dungeon_placement_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_registry_runtime_literals tests/block/registry/test_runtime_literals.c)
    target_link_libraries(mc_test_registry_runtime_literals PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.registry_runtime_literals COMMAND mc_test_registry_runtime_literals)
    set_tests_properties(block.registry_runtime_literals PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_overworld_provider_construction tests/world/generation/test_overworld_provider_construction.c)
    target_link_libraries(mc_test_overworld_provider_construction PRIVATE mc_world mc_c_atomics mc_build_options)

    add_executable(mc_dimension_observation_probe tests/world/dimension/dimension_observation_probe.c)
    target_include_directories(mc_dimension_observation_probe PRIVATE src/world/dimension)
    target_link_libraries(mc_dimension_observation_probe PRIVATE mc_simulation mc_build_options)

    add_executable(mc_test_world_saved_data tests/world/saved_data/test_storage.c)
    target_include_directories(mc_test_world_saved_data PRIVATE src/world/storage)
    target_link_libraries(mc_test_world_saved_data PRIVATE mc_world_saved_data mc_build_options)

    add_executable(mc_test_saved_data_value tests/world/saved_data/test_value.c)
    target_link_libraries(mc_test_saved_data_value PRIVATE mc_world_saved_data mc_build_options)

    add_executable(mc_test_saved_data_named tests/world/saved_data/test_named.c)
    target_include_directories(mc_test_saved_data_named PRIVATE src/world/storage)
    target_link_libraries(mc_test_saved_data_named PRIVATE mc_world_saved_data mc_build_options)

    add_executable(mc_saved_data_id_observation_probe tests/world/saved_data/observation_probe.c)
    target_link_libraries(mc_saved_data_id_observation_probe PRIVATE mc_world_saved_data mc_build_options)

    add_executable(mc_world_lifecycle_probe tests/world/runtime/lifecycle_probe.c)
    target_link_libraries(mc_world_lifecycle_probe PRIVATE mc_simulation mc_build_options)

    add_executable(mc_level_probe tests/storage/level/level_probe.c)
    target_link_libraries(mc_level_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_level tests/storage/level/test_level.c)
    target_link_libraries(mc_test_level PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.level COMMAND mc_test_level)
    set_tests_properties(storage.level PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_region_writer_probe tests/storage/region/writer_probe.c)
    target_link_libraries(mc_region_writer_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_region_writer tests/storage/region/test_writer.c)
    target_link_libraries(mc_test_region_writer PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.region_writer COMMAND mc_test_region_writer)
    set_tests_properties(storage.region_writer PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_chunk_acquire tests/world/cache/test_acquire.c)
    target_link_libraries(mc_test_chunk_acquire PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_acquire COMMAND mc_test_chunk_acquire)
    set_tests_properties(world.chunk_acquire PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_chunk_acquire_probe tests/world/cache/acquire_probe.c)
    target_link_libraries(mc_chunk_acquire_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_chunk_callback_probe tests/world/cache/callback_probe.c)
    target_link_libraries(mc_chunk_callback_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_chunk_lifecycle_callbacks_probe tests/world/cache/lifecycle_callbacks_probe.c)
    target_link_libraries(mc_chunk_lifecycle_callbacks_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_chunk_lifecycle tests/world/cache/test_lifecycle.c)
    target_link_libraries(mc_test_chunk_lifecycle PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_lifecycle COMMAND mc_test_chunk_lifecycle)
    set_tests_properties(world.chunk_lifecycle PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_chunk_pending tests/world/cache/test_pending.c)
    target_link_libraries(mc_test_chunk_pending PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_pending COMMAND mc_test_chunk_pending)
    set_tests_properties(world.chunk_pending PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_region tests/storage/region/test_region.c)
    target_link_libraries(mc_test_region PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.region COMMAND mc_test_region)
    set_tests_properties(storage.region PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_region_probe tests/storage/region/region_probe.c)
    target_link_libraries(mc_region_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_overworld_generator tests/world/generation/test_overworld.c)
    target_link_libraries(mc_test_overworld_generator PRIVATE mc_world mc_build_options)
    add_test(NAME world.overworld_generator COMMAND mc_test_overworld_generator)
    set_tests_properties(world.overworld_generator PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_overworld_generation_probe tests/world/generation/overworld_probe.c)
    target_link_libraries(mc_overworld_generation_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_chunk_finish tests/world/chunk/test_finish.c)
    target_link_libraries(mc_test_chunk_finish PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_finish COMMAND mc_test_chunk_finish)
    set_tests_properties(world.chunk_finish PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_chunk_finish_probe tests/world/chunk/finish_probe.c)
    target_link_libraries(mc_chunk_finish_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_light_queue tests/world/lighting/test_queue.c)
    target_link_libraries(mc_test_light_queue PRIVATE mc_world mc_build_options)
    add_test(NAME world.light_queue COMMAND mc_test_light_queue)
    set_tests_properties(world.light_queue PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_light_queue_probe tests/world/lighting/queue_probe.c)
    target_link_libraries(mc_light_queue_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_java tests/core/java/test_java.c)
    target_link_libraries(mc_test_java PRIVATE mc_core mc_build_options)
    add_test(NAME core.java COMMAND mc_test_java)
    set_tests_properties(core.java PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_test_java_float_parse tests/core/java/test_float_parse.c)
    target_link_libraries(mc_test_java_float_parse PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_float_parse COMMAND mc_test_java_float_parse)
    set_tests_properties(core.java_float_parse PROPERTIES LABELS "public;unit;numeric" TIMEOUT 60)

    add_executable(mc_java_float_parse_probe tests/core/java/float_parse_probe.c)
    target_link_libraries(mc_java_float_parse_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_test_java_float_format tests/core/java/test_float_format.c)
    target_link_libraries(mc_test_java_float_format PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_float_format COMMAND mc_test_java_float_format)
    set_tests_properties(core.java_float_format PROPERTIES LABELS "public;unit;numeric" TIMEOUT 60)

    add_executable(mc_test_java_integer_parse tests/core/java/test_integer_parse.c)
    target_link_libraries(mc_test_java_integer_parse PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_integer_parse COMMAND mc_test_java_integer_parse)
    set_tests_properties(core.java_integer_parse PROPERTIES LABELS "public;unit;numeric" TIMEOUT 60)

    add_executable(mc_java_integer_parse_probe tests/core/java/integer_parse_probe.c)
    target_link_libraries(mc_java_integer_parse_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_java_float_format_probe tests/core/java/float_format_probe.c)
    target_link_libraries(mc_java_float_format_probe PRIVATE mc_core mc_build_options)
    if(UNIX)
        target_link_libraries(mc_java_float_format_probe PRIVATE m)
    endif()

    add_executable(mc_test_java_gaussian tests/core/java/test_gaussian.c)
    target_link_libraries(mc_test_java_gaussian PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_gaussian COMMAND mc_test_java_gaussian)
    set_tests_properties(core.java_gaussian PROPERTIES LABELS "public;unit;math" TIMEOUT 30)

    add_executable(mc_java_gaussian_probe tests/core/java/gaussian_probe.c)
    target_link_libraries(mc_java_gaussian_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_test_java_int_set tests/core/java/test_int_set.c)
    target_link_libraries(mc_test_java_int_set PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_int_set COMMAND mc_test_java_int_set)
    set_tests_properties(core.java_int_set PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_java_int_set_allocator tests/core/java/test_int_set_allocator.c)
    target_link_libraries(mc_test_java_int_set_allocator PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_int_set_allocator COMMAND mc_test_java_int_set_allocator)
    set_tests_properties(core.java_int_set_allocator PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_java_properties tests/core/java/test_properties.c)
    target_link_libraries(mc_test_java_properties PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_properties COMMAND mc_test_java_properties)
    set_tests_properties(core.java_properties PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_java_properties_probe tests/core/java/properties_probe.c)
    target_link_libraries(mc_java_properties_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_test_java_reader tests/core/java/test_reader.c)
    add_executable(mc_test_java_format tests/core/java/test_format.c)
    target_link_libraries(mc_test_java_format PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_format COMMAND mc_test_java_format)
    set_tests_properties(core.java_format PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)
    add_executable(mc_java_format_probe tests/core/java/format_probe.c)
    target_link_libraries(mc_java_format_probe PRIVATE mc_core mc_build_options)
    add_executable(mc_java_case_probe tests/core/java/case_probe.c)
    target_link_libraries(mc_java_case_probe PRIVATE mc_core mc_build_options)
    target_link_libraries(mc_test_java_reader PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_reader COMMAND mc_test_java_reader)
    set_tests_properties(core.java_reader PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_java_writer tests/core/java/test_writer.c)
    target_link_libraries(mc_test_java_writer PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_writer COMMAND mc_test_java_writer)
    set_tests_properties(core.java_writer PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_java_writer_file tests/core/java/test_writer_file.c)
    target_link_libraries(mc_test_java_writer_file PRIVATE mc_core mc_build_options)

    add_executable(mc_java_writer_probe tests/core/java/writer_probe.c)
    target_link_libraries(mc_java_writer_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_java_reader_probe tests/core/java/reader_probe.c)
    target_link_libraries(mc_java_reader_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_translation_file_probe tests/resources/translation/files_probe.c)
    target_link_libraries(mc_translation_file_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_test_translation tests/resources/translation/test_translation.c)
    target_link_libraries(mc_test_translation PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME resources.translation COMMAND mc_test_translation)
    set_tests_properties(resources.translation PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_statistic_keys tests/statistics/keys/test_keys.c)
    target_link_libraries(mc_test_statistic_keys PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME statistics.keys COMMAND mc_test_statistic_keys)
    set_tests_properties(statistics.keys PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_statistic_registry tests/statistics/registry/test_registry.c)
    target_link_libraries(mc_test_statistic_registry PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME statistics.registry COMMAND mc_test_statistic_registry)
    set_tests_properties(statistics.registry PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME statistics.locale_c_utf8
            COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C.UTF-8 $<TARGET_FILE:mc_test_statistic_registry> en)
        add_test(NAME statistics.locale_invalid_category
            COMMAND ${CMAKE_COMMAND} -E env LC_ALL= LANG=en_US.UTF-8 LC_CTYPE=C.UTF-8
                LC_NUMERIC=mc_invalid_locale $<TARGET_FILE:mc_test_statistic_registry> en_US)
        set_tests_properties(statistics.locale_c_utf8 statistics.locale_invalid_category
            PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)
    endif()

    add_executable(mc_statistic_registry_file_probe tests/statistics/registry/files_probe.c)
    target_link_libraries(mc_statistic_registry_file_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_statistic_arrays_probe tests/statistics/registry/arrays_probe.c)
    target_link_libraries(mc_statistic_arrays_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_test_statistic_arrays tests/statistics/registry/test_arrays.c)
    target_link_libraries(mc_test_statistic_arrays PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME statistics.arrays COMMAND mc_test_statistic_arrays)
    set_tests_properties(statistics.arrays PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_achievements tests/statistics/achievements/test_achievements.c)
    target_link_libraries(mc_test_achievements PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME statistics.achievements COMMAND mc_test_achievements)
    set_tests_properties(statistics.achievements PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_statistic_keys_file_probe tests/statistics/keys/files_probe.c)
    target_link_libraries(mc_statistic_keys_file_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_java_int_set_probe tests/core/java/int_set_probe.c)
    target_link_libraries(mc_java_int_set_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_math tests/core/math/test_math.c)
    target_link_libraries(mc_test_math PRIVATE mc_core mc_build_options)
    add_test(NAME core.math COMMAND mc_test_math)
    set_tests_properties(core.math PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_math_probe tests/core/math/math_probe.c)
    target_link_libraries(mc_math_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_test_mutf8 tests/core/text/test_mutf8.c)
    target_link_libraries(mc_test_mutf8 PRIVATE mc_core mc_build_options)
    add_test(NAME core.mutf8 COMMAND mc_test_mutf8)
    set_tests_properties(core.mutf8 PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_test_nbt tests/storage/nbt/test_nbt.c)
    target_link_libraries(mc_test_nbt PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.nbt COMMAND mc_test_nbt)
    set_tests_properties(storage.nbt PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_nbt_probe tests/storage/nbt/nbt_probe.c)
    target_link_libraries(mc_nbt_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_nbt_overlay tests/storage/nbt/test_overlay.c)
    target_link_libraries(mc_test_nbt_overlay PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.nbt_overlay COMMAND mc_test_nbt_overlay)
    set_tests_properties(storage.nbt_overlay PROPERTIES LABELS "public;unit;storage" TIMEOUT 30)

    add_executable(mc_test_compression tests/storage/compression/test_compression.c)
    target_link_libraries(mc_test_compression PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.compression COMMAND mc_test_compression)
    set_tests_properties(storage.compression PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_compression_probe tests/storage/compression/compression_probe.c)
    target_link_libraries(mc_compression_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_chunk_data tests/world/chunk/test_data.c)
    target_link_libraries(mc_test_chunk_data PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_data COMMAND mc_test_chunk_data)
    set_tests_properties(world.chunk_data PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_test_chunk_nibbles tests/world/chunk/test_nibbles.c)
    target_link_libraries(mc_test_chunk_nibbles PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_nibbles COMMAND mc_test_chunk_nibbles)
    set_tests_properties(world.chunk_nibbles PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_test_chunk_reference_data tests/world/chunk/test_reference_data.c)
    target_link_libraries(mc_test_chunk_reference_data PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_reference_data COMMAND mc_test_chunk_reference_data)
    set_tests_properties(world.chunk_reference_data PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_chunk_data_probe tests/world/chunk/data_probe.c)
    target_link_libraries(mc_chunk_data_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_chunk_nibbles_probe tests/world/chunk/nibbles_probe.c)
    target_link_libraries(mc_chunk_nibbles_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_noise tests/world/noise/test_noise.c)
    target_link_libraries(mc_test_noise PRIVATE mc_world mc_build_options)
    add_test(NAME world.noise COMMAND mc_test_noise)
    set_tests_properties(world.noise PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_noise_probe tests/world/noise/noise_probe.c)
    target_link_libraries(mc_noise_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_simplex tests/world/noise/test_simplex.c)
    target_link_libraries(mc_test_simplex PRIVATE mc_world mc_build_options)
    add_test(NAME world.simplex COMMAND mc_test_simplex)
    set_tests_properties(world.simplex PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_simplex_probe tests/world/noise/simplex_probe.c)
    target_link_libraries(mc_simplex_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_biome tests/world/biome/test_biome.c)
    target_link_libraries(mc_test_biome PRIVATE mc_world mc_build_options)
    add_test(NAME world.biome COMMAND mc_test_biome)
    set_tests_properties(world.biome PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_biome_probe tests/world/biome/biome_probe.c)
    target_link_libraries(mc_biome_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_terrain tests/world/terrain/test_terrain.c)
    target_link_libraries(mc_test_terrain PRIVATE mc_world mc_build_options)
    add_test(NAME world.terrain COMMAND mc_test_terrain)
    set_tests_properties(world.terrain PROPERTIES LABELS "public;unit" TIMEOUT 90)

    add_executable(mc_terrain_probe tests/world/terrain/terrain_probe.c)
    target_link_libraries(mc_terrain_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_surface tests/world/terrain/test_surface.c)
    target_link_libraries(mc_test_surface PRIVATE mc_world mc_build_options)
    add_test(NAME world.surface COMMAND mc_test_surface)
    set_tests_properties(world.surface PROPERTIES LABELS "public;unit" TIMEOUT 90)

    add_executable(mc_surface_probe tests/world/terrain/surface_probe.c)
    target_link_libraries(mc_surface_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_caves tests/world/terrain/test_caves.c)
    target_link_libraries(mc_test_caves PRIVATE mc_world mc_build_options)
    add_test(NAME world.caves COMMAND mc_test_caves)
    set_tests_properties(world.caves PROPERTIES LABELS "public;unit" TIMEOUT 90)

    add_executable(mc_caves_probe tests/world/terrain/caves_probe.c)
    target_link_libraries(mc_caves_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_caves_geometry tests/world/terrain/test_caves_geometry.c)
    target_include_directories(mc_test_caves_geometry PRIVATE src/world/terrain)
    target_link_libraries(mc_test_caves_geometry PRIVATE mc_world mc_build_options)
    add_test(NAME world.caves_geometry COMMAND mc_test_caves_geometry)
    set_tests_properties(world.caves_geometry PROPERTIES LABELS "public;unit" TIMEOUT 90)

    add_executable(mc_caves_geometry_probe tests/world/terrain/caves_geometry_probe.c)
    target_include_directories(mc_caves_geometry_probe PRIVATE src/world/terrain)
    target_link_libraries(mc_caves_geometry_probe PRIVATE mc_world mc_build_options)

    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(NAME entity.player_construction COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_construction.py"
        --executable "$<TARGET_FILE:mc_test_player_construction>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-construction-contracts")
    set_tests_properties(entity.player_construction PROPERTIES
        LABELS "public;integration;entity;ownership" TIMEOUT 90)
    add_executable(mc_test_player_input tests/entity/player/test_input.c)
    target_link_libraries(mc_test_player_input PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_input COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_input.py"
        --executable "$<TARGET_FILE:mc_test_player_input>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-input-contracts")
    set_tests_properties(entity.player_input PROPERTIES
        LABELS "public;integration;entity;input;ownership" TIMEOUT 120)
    if(MC_ENABLE_PLAYER_INPUT_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        if(NOT MC_REFERENCE_CLIENT_JAR)
            message(FATAL_ERROR "Player input reference tests require MC_REFERENCE_CLIENT_JAR")
        endif()
        add_test(NAME reference.player_input COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_input/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --native "$<TARGET_FILE:mc_test_player_input>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-input")
        set_tests_properties(reference.player_input PROPERTIES
            LABELS "local;reference;entity;input;ownership" TIMEOUT 240)
    endif()
    add_test(NAME tools.player_construction_probe_inputs COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_construction_probe.py"
        --executable "$<TARGET_FILE:mc_player_construction_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-construction-protocol")
    set_tests_properties(tools.player_construction_probe_inputs PROPERTIES
        LABELS "public;tools;entity;ownership" TIMEOUT 180)
    if(MC_ENABLE_PLAYER_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.player_construction COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player/compare_construction.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --native "$<TARGET_FILE:mc_player_construction_probe>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-player-construction")
        set_tests_properties(reference.player_construction PROPERTIES
            LABELS "reference;original;entity;constructor" TIMEOUT 480)
    endif()

    add_test(NAME inventory.crafting COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/inventory/test_crafting.py"
        --executable "$<TARGET_FILE:mc_test_crafting>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/crafting-contracts")
    set_tests_properties(inventory.crafting PROPERTIES
        LABELS "public;integration;ownership;crafting" TIMEOUT 100)
    add_test(NAME tools.crafting_probe_inputs COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/inventory/test_crafting_probe.py"
        --executable "$<TARGET_FILE:mc_crafting_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/crafting-protocol")
    set_tests_properties(tools.crafting_probe_inputs PROPERTIES
        LABELS "public;tools;crafting" TIMEOUT 120)
    if(MC_ENABLE_CRAFTING_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.crafting COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/crafting/compare.py"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}" --server-jar "${MC_REFERENCE_SERVER_JAR}"
            --native "$<TARGET_FILE:mc_crafting_probe>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-crafting")
        set_tests_properties(reference.crafting PROPERTIES
            LABELS "reference;original;crafting" TIMEOUT 360)
    endif()

    add_executable(mc_test_sound_pool tests/client/sound/test_sound_pool.c)
    target_link_libraries(mc_test_sound_pool PRIVATE mc_client_sound_pool mc_build_options)
    add_test(NAME client.sound_pool COMMAND mc_test_sound_pool)
    set_tests_properties(client.sound_pool PROPERTIES LABELS "public;unit;client;ownership" TIMEOUT 60)

    add_executable(mc_test_wav_codec tests/client/sound/test_wav_codec.c)
    target_link_libraries(mc_test_wav_codec PRIVATE mc_sound_wav mc_build_options)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/wav-codec-output")
    add_test(NAME client.wav_codec COMMAND mc_test_wav_codec "${CMAKE_CURRENT_BINARY_DIR}/wav-codec-output")
    set_tests_properties(client.wav_codec PROPERTIES LABELS "public;integration;client;ownership" TIMEOUT 60)
    add_test(NAME tools.wav_reference_protocol COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/sound/wav/test_protocol.py")
    set_tests_properties(tools.wav_reference_protocol PROPERTIES LABELS "public;tools;client;audio" TIMEOUT 60)
    if(MC_ENABLE_WAV_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.wav_policy COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/sound/wav/compare.py"
            --jar "${MC_REFERENCE_CLIENT_JAR}" --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}" --native "$<TARGET_FILE:mc_test_wav_codec>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-wav"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.wav_policy PROPERTIES LABELS "reference;original;client;audio"
            TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    add_executable(mc_test_sound_manager tests/client/sound/test_sound_manager.c)
    target_link_libraries(mc_test_sound_manager PRIVATE mc_game_settings mc_build_options)
    add_test(NAME client.sound_manager COMMAND mc_test_sound_manager)
    set_tests_properties(client.sound_manager PROPERTIES LABELS "public;unit;client;ownership" TIMEOUT 60)

    add_executable(mc_test_settings_sound tests/client/sound/test_settings_sound.c)
    target_link_libraries(mc_test_settings_sound PRIVATE mc_game_settings mc_build_options)
    add_test(NAME client.settings_sound COMMAND mc_test_settings_sound
        "${CMAKE_CURRENT_BINARY_DIR}/settings-sound-missing-parent/child")
    set_tests_properties(client.settings_sound PROPERTIES LABELS "public;integration;client;ownership" TIMEOUT 60)

    add_executable(mc_test_sound_file tests/client/sound/test_sound_file.c)
    target_link_libraries(mc_test_sound_file PRIVATE mc_client_sound_pool mc_build_options)
    add_test(NAME client.sound_file
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_sound_file.py"
            --executable "$<TARGET_FILE:mc_test_sound_file>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(client.sound_file PROPERTIES LABELS "public;integration;client;ownership" TIMEOUT 45)

    if(MC_BUILD_CLIENT_SOUND)
        if(MC_BUILD_CLIENT_AUDIO)
            add_executable(mc_test_sound_backend tests/client/sound/backend/test_backend.c)
            target_link_libraries(mc_test_sound_backend PRIVATE mc_sound_backend mc_build_options)
            set_property(TARGET mc_test_sound_backend PROPERTY LINKER_LANGUAGE CXX)
            if(WIN32)
                add_custom_command(TARGET mc_test_sound_backend POST_BUILD COMMAND ${CMAKE_COMMAND}
                    -E copy_if_different "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:mc_test_sound_backend>")
            endif()
            add_test(NAME client.sound_backend COMMAND mc_test_sound_backend
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/sound/fixtures/vorbis")
            set_tests_properties(client.sound_backend PROPERTIES LABELS "public;integration;client;audio;ownership" TIMEOUT 60)
            add_library(mc_sound_failure_owner OBJECT src/client/sound/backend/owner.c)
            target_compile_definitions(mc_sound_failure_owner PRIVATE MC_SOUND_BACKEND_CALLOC=mc_test_sound_calloc)
            target_include_directories(mc_sound_failure_owner PRIVATE include)
            target_link_libraries(mc_sound_failure_owner PRIVATE OpenAL::OpenAL mc_c_atomics mc_build_options)
            add_executable(mc_test_sound_allocation tests/client/sound/backend/test_allocation.c
                $<TARGET_OBJECTS:mc_sound_failure_owner>)
            target_include_directories(mc_test_sound_allocation PRIVATE src/client/sound/backend)
            target_link_libraries(mc_test_sound_allocation PRIVATE mc_sound_backend OpenAL::OpenAL
                mc_c_atomics mc_build_options)
            set_property(TARGET mc_test_sound_allocation PROPERTY LINKER_LANGUAGE CXX)
            if(WIN32)
                add_custom_command(TARGET mc_test_sound_allocation POST_BUILD COMMAND ${CMAKE_COMMAND}
                    -E copy_if_different "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:mc_test_sound_allocation>")
            endif()
            add_test(NAME client.sound_allocation COMMAND mc_test_sound_allocation
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/sound/fixtures/vorbis")
            set_tests_properties(client.sound_allocation PROPERTIES LABELS "public;integration;client;audio;ownership" TIMEOUT 60)
        endif()
        add_executable(mc_test_vorbis_codec tests/client/sound/test_vorbis_codec.c)
        target_link_libraries(mc_test_vorbis_codec PRIVATE mc_sound_decoder mc_build_options)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/vorbis-codec-output")
        add_test(NAME client.vorbis_codec COMMAND mc_test_vorbis_codec
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/sound/fixtures/vorbis"
            "${CMAKE_CURRENT_BINARY_DIR}/vorbis-codec-output")
        set_tests_properties(client.vorbis_codec PROPERTIES LABELS "public;integration;client;ownership" TIMEOUT 60)

        if(MC_ENABLE_SOUND_REFERENCE_TESTS)
            find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
            if(NOT EXISTS "${MC_REFERENCE_CLIENT_JAR}")
                message(FATAL_ERROR "Sound reference comparison requires the local verified client JAR")
            endif()
            add_test(NAME reference.vorbis_policy COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/compare_vorbis_policy.py"
                --jar "${MC_REFERENCE_CLIENT_JAR}"
                --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
                --native "$<TARGET_FILE:mc_test_vorbis_codec>"
                --fixtures "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/sound/fixtures/vorbis"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/vorbis-policy-reference")
            set_tests_properties(reference.vorbis_policy PROPERTIES LABELS "local;reference;client;audio" TIMEOUT 180)
        endif()

        add_executable(mc_test_vorbis_decoder tests/client/sound/test_vorbis_decoder.c)
        target_include_directories(mc_test_vorbis_decoder PRIVATE src/client/sound)
        target_compile_definitions(mc_test_vorbis_decoder PRIVATE OV_EXCLUDE_STATIC_CALLBACKS)
        target_link_libraries(mc_test_vorbis_decoder PRIVATE mc_sound_decoder vorbisenc mc_build_options)
        add_test(NAME client.vorbis_decoder
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_vorbis_decoder.py"
                --executable "$<TARGET_FILE:mc_test_vorbis_decoder>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
        set_tests_properties(client.vorbis_decoder PROPERTIES
            LABELS "public;integration;client;ownership" TIMEOUT 120)

        add_executable(mc_test_vorbis_nonzero tests/client/sound/test_vorbis_nonzero.c)
        target_link_libraries(mc_test_vorbis_nonzero PRIVATE mc_sound_decoder vorbisenc mc_build_options)
        add_test(NAME client.vorbis_nonzero
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_vorbis_nonzero.py"
                --executable "$<TARGET_FILE:mc_test_vorbis_nonzero>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
        set_tests_properties(client.vorbis_nonzero PROPERTIES
            LABELS "public;integration;client;ownership" TIMEOUT 120)
    endif()

    add_executable(mc_test_entity_turn tests/entity/movement/test_turn.c)
    target_include_directories(mc_test_entity_turn PRIVATE src/entity)
    target_link_libraries(mc_test_entity_turn PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.turn COMMAND mc_test_entity_turn)
    set_tests_properties(entity.turn PROPERTIES LABELS "public;unit;ownership;numeric" TIMEOUT 60)
    add_executable(mc_entity_turn_probe tests/entity/movement/turn_probe.c)
    target_include_directories(mc_entity_turn_probe PRIVATE src/entity)
    target_link_libraries(mc_entity_turn_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)
    if(MC_ENABLE_TURN_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.entity_turn COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/turn/compare.py"
            --jar "${MC_REFERENCE_CLIENT_JAR}" --native "$<TARGET_FILE:mc_entity_turn_probe>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-turn")
        set_tests_properties(reference.entity_turn PROPERTIES LABELS "reference;original;entity;numeric" TIMEOUT 120)
    endif()

    add_executable(mc_test_movement_input tests/client/input/test_movement_input.c)
    target_link_libraries(mc_test_movement_input PRIVATE mc_movement_input mc_build_options)
    add_test(NAME client.movement_input COMMAND mc_test_movement_input)
    set_tests_properties(client.movement_input PROPERTIES LABELS "public;unit;client;input;ownership" TIMEOUT 60)
    add_executable(mc_movement_input_probe tests/client/input/movement_input_probe.c)
    target_link_libraries(mc_movement_input_probe PRIVATE mc_movement_input mc_build_options)
    if(MC_ENABLE_INPUT_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.movement_input COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/input/compare.py"
            --jar "${MC_REFERENCE_CLIENT_JAR}" --native "$<TARGET_FILE:mc_movement_input_probe>"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-input")
        set_tests_properties(reference.movement_input PROPERTIES LABELS "reference;original;client;input" TIMEOUT 120)
    endif()

    add_executable(mc_test_game_option tests/client/settings/test_game_option.c)
    target_link_libraries(mc_test_game_option PRIVATE mc_game_settings mc_build_options)
    add_test(NAME client.game_option COMMAND mc_test_game_option)
    set_tests_properties(client.game_option PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_client_game_directory tests/client/startup/test_game_directory.c)
    target_link_libraries(mc_test_client_game_directory PRIVATE mc_client_game_directory mc_build_options)
    add_test(NAME client.game_directory
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_game_directory_file.py"
            --executable "$<TARGET_FILE:mc_test_client_game_directory>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(client.game_directory PROPERTIES
        LABELS "public;integration;client;ownership" TIMEOUT 45)

    add_executable(mc_test_game_settings tests/client/settings/test_game_settings.c)
    target_link_libraries(mc_test_game_settings PRIVATE mc_game_settings mc_build_options)
    add_executable(mc_test_game_settings_borrow tests/client/settings/test_borrow.c)
    target_link_libraries(mc_test_game_settings_borrow PRIVATE mc_game_settings mc_build_options)
    add_test(NAME client.game_settings_borrow COMMAND mc_test_game_settings_borrow)
    set_tests_properties(client.game_settings_borrow PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 60)
    add_test(NAME client.game_settings
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_game_settings_file.py"
            --executable "$<TARGET_FILE:mc_test_game_settings>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(client.game_settings PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 60)

    add_executable(mc_test_game_settings_actions tests/client/settings/test_game_settings_actions.c)
    target_link_libraries(mc_test_game_settings_actions PRIVATE mc_game_settings mc_build_options)
    add_test(NAME client.game_settings_actions
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_game_settings_actions_file.py"
            --executable "$<TARGET_FILE:mc_test_game_settings_actions>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(client.game_settings_actions PROPERTIES
        LABELS "public;integration;client;ownership" TIMEOUT 65)

    add_executable(mc_game_settings_probe tests/client/settings/game_settings_probe.c)
    target_link_libraries(mc_game_settings_probe PRIVATE mc_game_settings mc_build_options)
    add_test(NAME tools.client_game_settings_probe
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_game_settings_probe_inputs.py"
            --executable "$<TARGET_FILE:mc_game_settings_probe>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(tools.client_game_settings_probe PROPERTIES
        LABELS "public;tools;ownership" TIMEOUT 120)

    add_test(NAME core.java_writer_file
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/test_writer_file.py"
            --executable "$<TARGET_FILE:mc_test_java_writer_file>")
    set_tests_properties(core.java_writer_file PROPERTIES LABELS "public;integration;ownership" TIMEOUT 30)

    add_test(NAME tools.java_writer_probe_inputs
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/test_writer_probe_inputs.py"
            --executable "$<TARGET_FILE:mc_java_writer_probe>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(tools.java_writer_probe_inputs PROPERTIES
        LABELS "public;tools;ownership;transport" TIMEOUT 90)

    add_test(NAME tools.java_float_format_probe
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/test_float_format_probe.py"
        --executable "$<TARGET_FILE:mc_java_float_format_probe>")
    set_tests_properties(tools.java_float_format_probe PROPERTIES LABELS "public;tools;numeric" TIMEOUT 60)
    if(MC_BUILD_CLIENT)
        add_test(NAME tools.client_main_menu_probe
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_main_menu_probe.py"
            --executable "$<TARGET_FILE:mc_gui_main_menu_probe>")
        set_tests_properties(tools.client_main_menu_probe PROPERTIES LABELS "public;tools;client" TIMEOUT 60)
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.startup
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_startup.py"
                    --executable "$<TARGET_FILE:minecraft-client>"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
            set_tests_properties(client.startup PROPERTIES
                LABELS "public;integration;client;graphics" TIMEOUT 240)
            add_test(NAME client.world_inspection COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_world_inspection.py"
                --executable "$<TARGET_FILE:minecraft-client>" --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
            set_tests_properties(client.world_inspection PROPERTIES
                LABELS "public;integration;client;graphics;ownership" TIMEOUT 210)
        endif()
    endif()

    add_test(NAME entity.nbt_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_nbt.py"
        --executable "$<TARGET_FILE:mc_probe_entity_nbt>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/entity-nbt-observation")
    set_tests_properties(entity.nbt_observation PROPERTIES LABELS "public;fixture;integration" TIMEOUT 120)

    add_test(NAME world.entity_batch
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_entity_batch>"
        --scenario world-entity-batch
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-entity-batch-contracts")
    set_tests_properties(world.entity_batch PROPERTIES LABELS "public;integration;ownership" TIMEOUT 90)

    add_executable(mc_text_stdout_probe tests/platform/text/stdout_probe.c)
    target_link_libraries(mc_text_stdout_probe PRIVATE mc_platform_text mc_build_options)
    add_test(NAME platform.text_stdout
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/platform/test_stdout.py"
            --executable "$<TARGET_FILE:mc_text_stdout_probe>"
            --jdk-fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/java/println/bytes.json")
    set_tests_properties(platform.text_stdout PROPERTIES LABELS "public;unit;platform" TIMEOUT 60)

    add_test(NAME core.java_format_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_format.py"
            --executable "$<TARGET_FILE:mc_java_format_probe>")
    set_tests_properties(core.java_format_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME core.java_string_case_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_string_case.py"
            --executable "$<TARGET_FILE:mc_java_case_probe>")
    set_tests_properties(core.java_string_case_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME statistics.arrays_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/statistics/compare_arrays.py"
            --executable "$<TARGET_FILE:mc_statistic_arrays_probe>")
    set_tests_properties(statistics.arrays_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME item.recipes_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/compare_recipes.py"
            --executable "$<TARGET_FILE:mc_recipe_bootstrap_probe>")
    set_tests_properties(item.recipes_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME world.biome_registry_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/check_biome_registry.py"
            --executable "$<TARGET_FILE:mc_biome_registry_probe>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/biome-registry-replay")
    set_tests_properties(world.biome_registry_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME item.stack_values_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/compare_stack_values.py"
            --executable "$<TARGET_FILE:mc_stack_values_probe>")
    set_tests_properties(item.stack_values_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME core.java_properties_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/compare_properties.py"
            "$<TARGET_FILE:mc_java_properties_probe>")
    set_tests_properties(core.java_properties_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME core.java_reader_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/compare_reader.py"
            "$<TARGET_FILE:mc_java_reader_probe>")
    set_tests_properties(core.java_reader_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME resources.imported_files
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/resources/test_files.py"
            "$<TARGET_FILE:mc_translation_file_probe>")
    set_tests_properties(resources.imported_files PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 60)

    add_test(NAME statistics.imported_keys
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/statistics/test_keys_files.py"
            "$<TARGET_FILE:mc_statistic_keys_file_probe>")
    set_tests_properties(statistics.imported_keys PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 60)

    add_test(NAME world.info_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/check_world_info.py"
            --probe "$<TARGET_FILE:mc_world_info_observation_probe>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-info-observation")
    set_tests_properties(world.info_observation PROPERTIES LABELS "public;replay" TIMEOUT 180)

    add_test(NAME entity.item_formula_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/compare_formula.py"
        --executable "$<TARGET_FILE:mc_entity_item_formula_probe>"
        --fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_formula_observation.tsv")
    set_tests_properties(entity.item_formula_observation PROPERTIES
        LABELS "public;fixture" TIMEOUT 210)

    add_test(NAME block.material_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/material/compare_observation.py"
        --executable "$<TARGET_FILE:mc_material_probe>"
        --fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/material/material_observation.tsv")
    set_tests_properties(block.material_observation PROPERTIES
        LABELS "public;fixture" TIMEOUT 30)

    add_test(NAME block.registry_base_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/block/compare_registry_prefix.py"
        --executable "$<TARGET_FILE:mc_registry_prefix_probe>"
        --fixtures "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/block/registry-prefix")
    set_tests_properties(block.registry_base_observation PROPERTIES
        LABELS "public;fixture" TIMEOUT 60)

    add_test(NAME item.registry_base_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/compare_registry_prefix.py"
        --executable "$<TARGET_FILE:mc_item_registry_prefix_probe>"
        --fixtures "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/registry-prefix")
    set_tests_properties(item.registry_base_observation PROPERTIES
        LABELS "public;fixture" TIMEOUT 60)

    add_test(NAME core.java_gaussian_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_gaussian.py"
        --executable "$<TARGET_FILE:mc_java_gaussian_probe>")
    set_tests_properties(core.java_gaussian_replay PROPERTIES
        LABELS "public;fixture;java;math" TIMEOUT 90)

    add_test(NAME tools.java_gaussian_protocol
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/test_gaussian_probe.py"
        --executable "$<TARGET_FILE:mc_java_gaussian_probe>")
    set_tests_properties(tools.java_gaussian_protocol PROPERTIES
        LABELS "public;tools;java;math" TIMEOUT 180)

    add_test(NAME world.scheduled_ordered_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_scheduled_ordered.py"
        --executable "$<TARGET_FILE:mc_scheduled_ordered_probe>")
    set_tests_properties(world.scheduled_ordered_replay PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME tools.scheduled_ordered_protocol
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_scheduled_ordered_probe.py"
        --executable "$<TARGET_FILE:mc_scheduled_ordered_probe>")
    set_tests_properties(tools.scheduled_ordered_protocol PROPERTIES
        LABELS "public;tools;world" TIMEOUT 180)

    foreach(MC_RUNTIME_CASE ownership-and-runtime generation generation-copy-limit profile-and-entity-bindings)
        add_test(NAME world.runtime_${MC_RUNTIME_CASE}
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
            --executable "$<TARGET_FILE:mc_test_world_runtime>"
            --scenario "${MC_RUNTIME_CASE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/runtime-contracts")
        set_tests_properties(world.runtime_${MC_RUNTIME_CASE} PROPERTIES
            LABELS "public;integration;storage" TIMEOUT 90)
    endforeach()

    add_test(NAME world.runtime_info_bindings
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_info_binding>"
        --scenario world-info-bindings
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/runtime-info-contracts")
    set_tests_properties(world.runtime_info_bindings PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 90)

    add_test(NAME world.typed_world_info
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_typed_info>"
        --scenario typed-world-info
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/typed-world-info-contracts")
    set_tests_properties(world.typed_world_info PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 90)

    add_test(NAME world.game_construction
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_game_world>"
        --scenario game-world
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/game-world-contracts")
    set_tests_properties(world.game_construction PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.dedicated_construction
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_dedicated_construction>"
        --scenario world-dedicated-construction
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dedicated-construction-contracts")
    set_tests_properties(world.dedicated_construction PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.registry_bindings
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_registry>"
        --scenario world-registry
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-registry-contracts")
    set_tests_properties(world.registry_bindings PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.biome_runtime
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_biome_runtime>"
        --scenario world-biome-runtime
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-biome-runtime-contracts")
    set_tests_properties(world.biome_runtime PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.fixed_biome_runtime COMMAND mc_test_fixed_biome_runtime)
    set_tests_properties(world.fixed_biome_runtime PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 90)

    add_test(NAME world.temperature_runtime
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_temperature>"
        --scenario temperature-runtime
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/temperature-runtime-contracts")
    set_tests_properties(world.temperature_runtime PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 90)

    add_test(NAME world.lighting_runtime
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_lighting_runtime>"
        --scenario world-lighting-runtime
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-lighting-runtime-contracts")
    set_tests_properties(world.lighting_runtime PROPERTIES
        LABELS "public;integration;lighting" TIMEOUT 90)

    add_test(NAME world.light_queue_runtime
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_light_queue_runtime>"
        --scenario world-light-queue-runtime
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-light-queue-runtime-contracts")
    set_tests_properties(world.light_queue_runtime PROPERTIES
        LABELS "public;integration;lighting" TIMEOUT 90)

    add_test(NAME world.chunk_objects
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_chunk_objects>"
        --scenario world-chunk-objects
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-chunk-object-contracts")
    set_tests_properties(world.chunk_objects PROPERTIES
        LABELS "public;integration;ownership;lighting;storage" TIMEOUT 90)

    add_test(NAME world.chunk_provider_generation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_chunk_provider_generation>"
        --scenario world-chunk-provider-generation
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-chunk-provider-generation-contracts")
    set_tests_properties(world.chunk_provider_generation PROPERTIES
        LABELS "public;integration;ownership;generation" TIMEOUT 90)

    add_test(NAME world.chunk_provider_activation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_chunk_provider_activation>"
        --scenario world-chunk-provider-activation
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-chunk-provider-activation-contracts")
    set_tests_properties(world.chunk_provider_activation PROPERTIES
        LABELS "public;integration;ownership;generation;storage" TIMEOUT 90)

    add_test(NAME world.mutations
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_mutations>"
        --scenario world-mutations
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-mutation-contracts")
    set_tests_properties(world.mutations PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.pressure_plate
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_pressure_plate>"
        --scenario world-pressure-plate
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-pressure-plate-contracts")
    set_tests_properties(world.pressure_plate PROPERTIES
        LABELS "public;integration;ownership;mutation;scheduling" TIMEOUT 90)

    add_test(NAME world.scheduling
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_scheduling>"
        --scenario world-scheduling
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-scheduling-contracts")
    set_tests_properties(world.scheduling PROPERTIES
        LABELS "public;integration;ownership;scheduling" TIMEOUT 90)

    add_test(NAME world.solid_support
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_solid_support>"
        --scenario world-solid-support
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-solid-support-contracts")
    set_tests_properties(world.solid_support PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.power
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_power>"
        --scenario world-power
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-power-contracts")
    set_tests_properties(world.power PROPERTIES
        LABELS "public;integration;ownership;power" TIMEOUT 90)

    add_test(NAME world.redstone_lifecycle COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/run_redstone.py"
        --executable "$<TARGET_FILE:mc_test_world_redstone>")
    set_tests_properties(world.redstone_lifecycle PROPERTIES
        LABELS "public;integration;ownership;mutation;power" TIMEOUT 90)

    add_test(NAME world.redstone_multichunk COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/run_redstone.py"
        --executable "$<TARGET_FILE:mc_test_world_redstone_multichunk>"
        --scenario world-redstone-multichunk)
    set_tests_properties(world.redstone_multichunk PROPERTIES
        LABELS "public;integration;ownership;mutation;power;dedicated" TIMEOUT 90)

    add_test(NAME world.world_access COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/run_world_access.py"
        --executable "$<TARGET_FILE:mc_test_world_access>")
    set_tests_properties(world.world_access PROPERTIES
        LABELS "public;integration;ownership;mutation;power" TIMEOUT 90)

    add_test(NAME world.redstone_probe_inputs COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_redstone_probe_inputs.py"
        --executable "$<TARGET_FILE:mc_redstone_world_probe>")
    set_tests_properties(world.redstone_probe_inputs PROPERTIES
        LABELS "public;integration;ownership;mutation;random" TIMEOUT 240)

    add_test(NAME world.rails
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_rails>"
        --scenario world-rails
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-rail-contracts")
    set_tests_properties(world.rails PROPERTIES
        LABELS "public;integration;ownership;mutation;power" TIMEOUT 90)

    add_test(NAME world.minecart_detector
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_minecart_detector>"
        --scenario world-minecart-detector
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-minecart-detector-contracts")
    set_tests_properties(world.minecart_detector PROPERTIES
        LABELS "public;integration;ownership;entity;mutation")

    add_test(NAME world.entity_publication
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_entity_publication>"
        --scenario world-entity-publication
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-entity-publication-contracts")
    set_tests_properties(world.entity_publication PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.nearby_entities
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_nearby>"
        --scenario world-nearby
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-nearby-contracts")
    set_tests_properties(world.nearby_entities PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.class_entities
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_class_entities>"
        --scenario world-class-entities
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-class-entities-contracts")
    set_tests_properties(world.class_entities PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.collision_boxes
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_collision_boxes>"
        --scenario world-collision-boxes
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-collision-boxes-contracts")
    set_tests_properties(world.collision_boxes PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.piston_geometry
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_piston_geometry>"
        --scenario world-piston-geometry
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-piston-geometry-contracts")
    set_tests_properties(world.piston_geometry PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.piston_tile_entities
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_piston_tile_entities>"
        --scenario world-piston-tile-entities
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-piston-tile-entities-contracts")
    set_tests_properties(world.piston_tile_entities PROPERTIES
        LABELS "public;integration;ownership;storage" TIMEOUT 90)

    add_test(NAME world.piston_finalization
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_piston_finalize>"
        --scenario world-piston-finalization
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-piston-finalization-contracts")
    set_tests_properties(world.piston_finalization PROPERTIES
        LABELS "public;integration;ownership;storage" TIMEOUT 90)

    add_test(NAME world.piston_tick
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_piston_tick>"
        --scenario world-piston-tick
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-piston-tick-contracts")
    set_tests_properties(world.piston_tick PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME entity.movement
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_entity_movement>"
        --scenario world-entity-movement
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-entity-movement-contracts")
    set_tests_properties(entity.movement PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.movement_dependencies
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_movement>"
        --scenario world-movement-dependencies
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-movement-dependencies-contracts")
    set_tests_properties(world.movement_dependencies PROPERTIES
        LABELS "public;integration;ownership;environment" TIMEOUT 90)

    add_test(NAME world.tile_entities
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_tile_entities.py"
        --executable "$<TARGET_FILE:mc_test_world_tile_entities>"
        --mode all
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-tile-entities-contracts")
    set_tests_properties(world.tile_entities PROPERTIES
        LABELS "public;integration;ownership;mutation;storage" TIMEOUT 90)

    add_test(NAME world.saving
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_saving.py"
        --executable "$<TARGET_FILE:mc_test_world_saving>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-saving-contracts")
    set_tests_properties(world.saving PROPERTIES
        LABELS "public;integration;ownership;storage" TIMEOUT 120)

    add_test(NAME world.game_entities
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_game_entities.py"
        --executable "$<TARGET_FILE:mc_test_game_entities>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-game-entities-contracts")
    set_tests_properties(world.game_entities PROPERTIES
        LABELS "public;integration;ownership;storage" TIMEOUT 120)

    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c
                "import pathlib, sys; print(pathlib.Path(sys.executable).resolve(strict=True))"
        RESULT_VARIABLE MC_TEST_MINECART_STORAGE_PYTHON_RESULT
        OUTPUT_VARIABLE MC_TEST_MINECART_STORAGE_PYTHON
        ERROR_VARIABLE MC_TEST_MINECART_STORAGE_PYTHON_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT MC_TEST_MINECART_STORAGE_PYTHON_RESULT STREQUAL "0"
        OR MC_TEST_MINECART_STORAGE_PYTHON STREQUAL ""
        OR MC_TEST_MINECART_STORAGE_PYTHON MATCHES "[\r\n]")
        message(FATAL_ERROR
            "Cannot resolve the Minecart storage test Python executable "
            "(result: ${MC_TEST_MINECART_STORAGE_PYTHON_RESULT}): "
            "${MC_TEST_MINECART_STORAGE_PYTHON_ERROR}")
    endif()
    add_test(NAME entity.minecart_storage
        COMMAND "${MC_TEST_MINECART_STORAGE_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_minecart_storage.py"
        --executable "$<TARGET_FILE:mc_test_entity_minecart_storage>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(entity.minecart_storage PROPERTIES
        LABELS "public;integration;ownership;storage")

    add_test(NAME world.dungeon
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dungeon.py"
        --executable "$<TARGET_FILE:mc_test_dungeon>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-dungeon-contracts")
    set_tests_properties(world.dungeon PROPERTIES
        LABELS "public;integration;ownership;mutation;storage" TIMEOUT 160)

    add_test(NAME world.overworld_population
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_overworld_population.py"
        --executable "$<TARGET_FILE:mc_test_overworld_population>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-overworld-population-contracts")
    set_tests_properties(world.overworld_population PROPERTIES
        LABELS "public;integration;ownership;population;storage" TIMEOUT 120)

    add_test(NAME tools.natural_population
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world" -p test_natural_population.py)
    set_tests_properties(tools.natural_population PROPERTIES
        LABELS "public;tools;population" TIMEOUT 90)

    add_test(NAME world.natural_population_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/replay_natural_population.py"
        --executable "$<TARGET_FILE:mc_natural_population_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/natural-population-replay")
    set_tests_properties(world.natural_population_replay PROPERTIES
        LABELS "public;integration;population;replay" TIMEOUT 180)

    add_test(NAME world.natural_population_ring_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/replay_natural_population.py"
        --executable "$<TARGET_FILE:mc_natural_population_probe>"
        --profile ring
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/natural-population-ring-replay")
    set_tests_properties(world.natural_population_ring_replay PROPERTIES
        LABELS "public;integration;population;replay" TIMEOUT 240)

    add_test(NAME tools.population_persistence
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world" -p test_population_persistence.py)
    set_tests_properties(tools.population_persistence PROPERTIES
        LABELS "public;tools;population;storage" TIMEOUT 180)

    add_test(NAME world.population_save_readback_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/replay_population_persistence.py"
        --executable "$<TARGET_FILE:mc_natural_population_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/population-save-readback-replay")
    set_tests_properties(world.population_save_readback_replay PROPERTIES
        LABELS "public;integration;population;storage;replay" TIMEOUT 300)

    add_test(NAME tools.population_world_save
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world" -p test_population_world_save.py)
    set_tests_properties(tools.population_world_save PROPERTIES
        LABELS "public;tools;population;storage" TIMEOUT 180)

    add_test(NAME world.population_full_save_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/replay_population_world_save.py"
        --executable "$<TARGET_FILE:mc_natural_population_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/population-full-save-replay")
    set_tests_properties(world.population_full_save_replay PROPERTIES
        LABELS "public;integration;population;storage;replay" TIMEOUT 300)

    add_test(NAME tools.population_named_data
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world" -p test_population_named_data.py)
    set_tests_properties(tools.population_named_data PROPERTIES
        LABELS "public;tools;population;storage" TIMEOUT 240)

    add_test(NAME world.population_named_data_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/replay_population_named_data.py"
        --executable "$<TARGET_FILE:mc_natural_population_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/population-named-data-replay")
    set_tests_properties(world.population_named_data_replay PROPERTIES
        LABELS "public;integration;population;storage;replay" TIMEOUT 300)

    add_test(NAME tools.population_dedicated
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/dedicated" -p "test_*.py")
    set_tests_properties(tools.population_dedicated PROPERTIES
        LABELS "public;tools;population;storage" TIMEOUT 240)

    add_test(NAME world.population_dedicated_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/population/dedicated/replay.py"
        --executable "$<TARGET_FILE:mc_natural_population_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/population-dedicated-replay")
    set_tests_properties(world.population_dedicated_replay PROPERTIES
        LABELS "public;integration;population;storage;replay" TIMEOUT 300)

    if(MC_ENABLE_POPULATION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        foreach(MC_POPULATION_PROFILE IN ITEMS origin ring)
            if(MC_POPULATION_PROFILE STREQUAL "origin")
                set(MC_POPULATION_TEST reference.natural_population)
                set(MC_POPULATION_CAPTURE reference-natural-population)
            else()
                set(MC_POPULATION_TEST reference.natural_population_ring)
                set(MC_POPULATION_CAPTURE reference-natural-population-ring)
            endif()
            add_test(NAME "${MC_POPULATION_TEST}"
                COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/population/compare_natural.py"
                --executable "$<TARGET_FILE:mc_natural_population_probe>"
                --client "${MC_REFERENCE_CLIENT_JAR}"
                --server "${MC_REFERENCE_SERVER_JAR}"
                --java "${Java_JAVA_EXECUTABLE}"
                --javac "${Java_JAVAC_EXECUTABLE}"
                --profile "${MC_POPULATION_PROFILE}"
                --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/${MC_POPULATION_CAPTURE}")
            set_tests_properties("${MC_POPULATION_TEST}" PROPERTIES
                LABELS "reference;original;population;integration" TIMEOUT 600)
        endforeach()

        add_test(NAME reference.population_save_readback
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/population/compare_persistence.py"
            --executable "$<TARGET_FILE:mc_natural_population_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-population-save-readback")
        set_tests_properties(reference.population_save_readback PROPERTIES
            LABELS "reference;original;population;storage;integration" TIMEOUT 600)

        add_test(NAME reference.population_full_save
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/population/compare_world_save.py"
            --executable "$<TARGET_FILE:mc_natural_population_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-population-full-save")
        set_tests_properties(reference.population_full_save PROPERTIES
            LABELS "reference;original;population;storage;integration" TIMEOUT 600)

        add_test(NAME reference.population_named_data
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/population/compare_named_data.py"
            --executable "$<TARGET_FILE:mc_natural_population_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-population-named-data")
        set_tests_properties(reference.population_named_data PROPERTIES
            LABELS "reference;original;population;storage;integration" TIMEOUT 600)

        add_test(NAME reference.population_dedicated
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/population/dedicated/reference.py"
            --executable "$<TARGET_FILE:mc_natural_population_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-population-dedicated")
        set_tests_properties(reference.population_dedicated PROPERTIES
            LABELS "reference;original;population;storage;integration" TIMEOUT 600)
    endif()

    add_test(NAME world.queries
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_queries>"
        --scenario world-queries
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-query-contracts")
    set_tests_properties(world.queries PROPERTIES
        LABELS "public;integration;ownership;generation" TIMEOUT 90)

    add_test(NAME world.fluids
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_fluids>"
        --scenario world-fluids
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-fluid-contracts")
    set_tests_properties(world.fluids PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.falling
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_falling.py"
        --executable "$<TARGET_FILE:mc_test_world_falling>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-falling-contracts")
    set_tests_properties(world.falling PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.snow
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_snow.py"
        --executable "$<TARGET_FILE:mc_test_world_snow>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-snow-contracts")
    set_tests_properties(world.snow PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.plant_lifecycle
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_plant_lifecycle.py"
        --executable "$<TARGET_FILE:mc_test_plant_lifecycle>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-plant-contracts")
    set_tests_properties(world.plant_lifecycle PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.plant_lifecycle_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/tools/world/replay_plant_lifecycle.py"
        --executable "$<TARGET_FILE:mc_plant_lifecycle_probe>"
        --work-dir "${CMAKE_BINARY_DIR}/plant-lifecycle-replay")
    set_tests_properties(world.plant_lifecycle_replay PROPERTIES
        LABELS "public;integration;mutation;replay" TIMEOUT 240)

    if(MC_ENABLE_PLANT_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.plant_lifecycle
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/scripts/reference/world/plants/compare_lifecycle.py"
            --executable "$<TARGET_FILE:mc_plant_lifecycle_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}"
            --work-dir "${CMAKE_BINARY_DIR}/reference-plant-lifecycle")
        set_tests_properties(reference.plant_lifecycle PROPERTIES
            LABELS "reference;original;mutation" TIMEOUT 300)
    endif()

    add_test(NAME world.overworld_provider_construction
        COMMAND mc_test_overworld_provider_construction)
    set_tests_properties(world.overworld_provider_construction PROPERTIES
        LABELS "public;unit;generation" TIMEOUT 90)

    add_test(NAME world.dimension_reference
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/tools/world/test_dimension_reference.py" verify
        --fixture-dir "${CMAKE_SOURCE_DIR}/tests/reference/world/dimension"
        --provenance-sha256 5b620c091d1de9881f7c60b267706c693aa4726a99f67b7c99b63152118f813b
        --probe "$<TARGET_FILE:mc_dimension_observation_probe>"
        --work-dir "${CMAKE_BINARY_DIR}/dimension-reference")
    set_tests_properties(world.dimension_reference PROPERTIES
        LABELS "public;reference;math" TIMEOUT 120)

    add_test(NAME world.saved_data_map_value COMMAND mc_test_saved_data_value)
    set_tests_properties(world.saved_data_map_value PROPERTIES
        LABELS "public;unit;storage" TIMEOUT 90)

    add_test(NAME world.saved_data_named
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_saved_data_named>"
        --scenario saved-data-named
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/saved-data-named-contracts")
    set_tests_properties(world.saved_data_named PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 90)

    add_test(NAME world.saved_data_ids
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_saved_data>"
        --scenario saved-data-ids
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/saved-data-id-contracts")
    set_tests_properties(world.saved_data_ids PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 90)

    add_test(NAME world.saved_data_ids_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/check_saved_data_ids.py"
        --executable "$<TARGET_FILE:mc_saved_data_id_observation_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/saved-data-memory-replay")
    set_tests_properties(world.saved_data_ids_replay PROPERTIES
        LABELS "public;reference;storage" TIMEOUT 90)

    add_test(NAME world.lifecycle_protocol
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_lifecycle_probe.py"
        --executable "$<TARGET_FILE:mc_world_lifecycle_probe>"
        --prepare-executable "$<TARGET_FILE:mc_test_world_runtime>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lifecycle-protocol-contracts")
    set_tests_properties(world.lifecycle_protocol PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 180)

    add_test(NAME world.lifecycle_callbacks_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/compare_lifecycle_callbacks.py"
        --executable "$<TARGET_FILE:mc_chunk_lifecycle_callbacks_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lifecycle-callbacks-replay")
    set_tests_properties(world.lifecycle_callbacks_replay PROPERTIES
        LABELS "public;fixture;cache" TIMEOUT 420)

    add_test(NAME core.java_int_set_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_int_set.py"
        --executable "$<TARGET_FILE:mc_java_int_set_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-java-int-set-replay")
    set_tests_properties(core.java_int_set_replay PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME world.chunk_pending_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_int_set.py"
        --executable "$<TARGET_FILE:mc_java_int_set_probe>" --pending
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-pending-replay")
    set_tests_properties(world.chunk_pending_replay PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME tools.world_storage_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage" -p test_world_storage.py)
    set_tests_properties(tools.world_storage_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 180)

    add_test(NAME tools.world_store_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_store.py"
        --executable "$<TARGET_FILE:mc_world_store_probe>")
    set_tests_properties(tools.world_store_interop PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 180)

    add_test(NAME tools.regions_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage" -p test_regions.py)
    set_tests_properties(tools.regions_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    add_test(NAME tools.regions_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_regions.py"
        --executable "$<TARGET_FILE:mc_world_regions_probe>")
    set_tests_properties(tools.regions_interop PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    add_test(NAME tools.chunk_storage_reference
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_chunk_storage.py"
        --executable "$<TARGET_FILE:mc_stored_chunk_probe>")
    set_tests_properties(tools.chunk_storage_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 180)

    add_test(NAME tools.level_reference
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_level.py"
        --executable "$<TARGET_FILE:mc_level_probe>")
    set_tests_properties(tools.level_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    add_test(NAME tools.region_write_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage" -p test_region_write.py)
    set_tests_properties(tools.region_write_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    add_test(NAME tools.region_write_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_region_write.py"
        --executable "$<TARGET_FILE:mc_region_writer_probe>")
    set_tests_properties(tools.region_write_interop PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    if(MC_ENABLE_STORAGE_REFERENCE_TESTS)
        find_package(Java 17 QUIET COMPONENTS Runtime Development)
        get_filename_component(MC_STORAGE_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        set(MC_STORAGE_SOURCE_ARGUMENTS)
        if(NOT MC_REFERENCE_TESTED_COMMIT STREQUAL "")
            list(APPEND MC_STORAGE_SOURCE_ARGUMENTS --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        endif()
        add_test(NAME reference.level
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/storage/compare_level.py"
            --executable "$<TARGET_FILE:mc_level_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_STORAGE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-level"
            ${MC_STORAGE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.level PROPERTIES
            LABELS "reference;original;storage" TIMEOUT 300 SKIP_RETURN_CODE 77)

        add_test(NAME reference.region_write
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/storage/compare_region_write.py"
            --executable "$<TARGET_FILE:mc_region_writer_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-region-write"
            ${MC_STORAGE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.region_write PROPERTIES
            LABELS "reference;original;storage" TIMEOUT 300 SKIP_RETURN_CODE 77)

        add_test(NAME reference.chunk_storage
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/storage/compare_chunk_storage.py"
            --executable "$<TARGET_FILE:mc_stored_chunk_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_STORAGE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-storage"
            ${MC_STORAGE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.chunk_storage PROPERTIES
            LABELS "reference;original;storage" TIMEOUT 600 SKIP_RETURN_CODE 77)

        add_test(NAME reference.world_storage
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/storage/compare_world_storage.py"
            --executable "$<TARGET_FILE:mc_world_store_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-world-storage"
            ${MC_STORAGE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.world_storage PROPERTIES
            LABELS "reference;original;storage" TIMEOUT 900 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_LIFECYCLE_REFERENCE_TESTS)
        find_package(Java 17 QUIET COMPONENTS Runtime Development)
        set(MC_LIFECYCLE_SOURCE_ARGUMENTS)
        if(NOT MC_REFERENCE_TESTED_COMMIT STREQUAL "")
            list(APPEND MC_LIFECYCLE_SOURCE_ARGUMENTS --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        endif()
        foreach(MC_LIFECYCLE_CONTEXT client base dedicated)
            foreach(MC_LIFECYCLE_SCENARIO cohort session-world detach-owned)
                add_test(NAME reference.lifecycle_${MC_LIFECYCLE_CONTEXT}_${MC_LIFECYCLE_SCENARIO}
                    COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/run_lifecycle.py"
                    --executable "$<TARGET_FILE:mc_world_lifecycle_probe>"
                    --callback-executable "$<TARGET_FILE:mc_chunk_lifecycle_callbacks_probe>"
                    --client "${MC_REFERENCE_CLIENT_JAR}"
                    --server "${MC_REFERENCE_SERVER_JAR}"
                    --java "${Java_JAVA_EXECUTABLE}"
                    --javac "${Java_JAVAC_EXECUTABLE}"
                    --context "${MC_LIFECYCLE_CONTEXT}" --scenario "${MC_LIFECYCLE_SCENARIO}"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-lifecycle-${MC_LIFECYCLE_CONTEXT}-${MC_LIFECYCLE_SCENARIO}"
                    ${MC_LIFECYCLE_SOURCE_ARGUMENTS})
                set_tests_properties(reference.lifecycle_${MC_LIFECYCLE_CONTEXT}_${MC_LIFECYCLE_SCENARIO} PROPERTIES
                    LABELS "reference;original;lifecycle" TIMEOUT 1800 RUN_SERIAL TRUE SKIP_RETURN_CODE 77)
            endforeach()
        endforeach()
        add_test(NAME reference.lifecycle_dedicated_unload101
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/run_lifecycle.py"
            --executable "$<TARGET_FILE:mc_world_lifecycle_probe>"
            --callback-executable "$<TARGET_FILE:mc_chunk_lifecycle_callbacks_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --context dedicated --scenario unload101
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-lifecycle-dedicated-unload101"
            ${MC_LIFECYCLE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.lifecycle_dedicated_unload101 PROPERTIES
            LABELS "reference;original;lifecycle" TIMEOUT 1800 RUN_SERIAL TRUE SKIP_RETURN_CODE 77)
    endif()

    add_test(NAME tools.acquire_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache" -p test_acquire_reference.py)
    set_tests_properties(tools.acquire_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.acquire_fixture
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache" -p test_acquire_interop.py)
    set_tests_properties(tools.acquire_fixture PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.acquire_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache/test_acquire_interop.py"
        --executable "$<TARGET_FILE:mc_chunk_acquire_probe>")
    set_tests_properties(tools.acquire_interop PROPERTIES LABELS "public;tools" TIMEOUT 120)

    add_test(NAME tools.callbacks_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache" -p test_callbacks_reference.py)
    # Failure controls repeatedly fingerprint the growing source tree; allow
    # filesystem headroom for mounted workspaces and concurrent public checks.
    set_tests_properties(tools.callbacks_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 180)

    add_test(NAME tools.callbacks_fixture
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache" -p test_callbacks_interop.py)
    set_tests_properties(tools.callbacks_fixture PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.callbacks_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache/test_callbacks_interop.py"
        --executable "$<TARGET_FILE:mc_chunk_callback_probe>")
    set_tests_properties(tools.callbacks_interop PROPERTIES LABELS "public;tools" TIMEOUT 120)

    if(MC_ENABLE_CACHE_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_CACHE_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.chunk_acquire
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/compare_acquire.py"
            --executable "$<TARGET_FILE:mc_chunk_acquire_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_CACHE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-acquire"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.chunk_acquire PROPERTIES
            LABELS "reference;original;cache" TIMEOUT 900 SKIP_RETURN_CODE 77)

        add_test(NAME reference.chunk_callbacks
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/compare_callbacks.py"
            --executable "$<TARGET_FILE:mc_chunk_callback_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_CACHE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-callbacks"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.chunk_callbacks PROPERTIES
            LABELS "reference;original;cache" TIMEOUT 900 SKIP_RETURN_CODE 77)
    endif()

    add_test(NAME tools.region_inspect
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/region/test_inspect.py"
        --executable "$<TARGET_FILE:mc_region_inspect>")
    set_tests_properties(tools.region_inspect PROPERTIES LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.region_capture
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/region/test_capture.py"
        --executable "$<TARGET_FILE:mc_region_probe>")
    set_tests_properties(tools.region_capture PROPERTIES LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.region_probe
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/region/test_probe.py"
        --executable "$<TARGET_FILE:mc_region_probe>")
    set_tests_properties(tools.region_probe PROPERTIES LABELS "public;tools" TIMEOUT 30)

    add_test(NAME tools.region_live
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/region" -p test_live.py)
    set_tests_properties(tools.region_live PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        # Several failure controls fingerprint the complete growing source tree.
        # Leave filesystem headroom for mounted workspaces and parallel checks.
        LABELS "public;tools" TIMEOUT 90)

    if(MC_ENABLE_REGION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_REGION_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.region
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/region/compare_region.py"
            --executable "$<TARGET_FILE:mc_region_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_REGION_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-region"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.region PROPERTIES
            LABELS "reference;original;region" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_REGION_CAPTURE_TESTS)
        add_test(NAME reference.region_capture
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/region/compare_capture.py"
            --executable "$<TARGET_FILE:mc_region_probe>"
            --capture "${MC_REGION_CAPTURE_MANIFEST}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-region-capture"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.region_capture PROPERTIES
            LABELS "reference;captured;region" TIMEOUT 120 SKIP_RETURN_CODE 77)
    endif()

    add_test(NAME tools.overworld_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/generation" -p test_overworld_reference.py)
    set_tests_properties(tools.overworld_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 30)

    add_test(NAME tools.overworld_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/generation/test_overworld_interop.py"
        --executable "$<TARGET_FILE:mc_overworld_generation_probe>")
    set_tests_properties(tools.overworld_interop PROPERTIES LABELS "public;tools" TIMEOUT 90)

    if(MC_ENABLE_GENERATION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_GENERATION_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.overworld_generation
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/generation/compare_overworld.py"
            --executable "$<TARGET_FILE:mc_overworld_generation_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_GENERATION_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-overworld-generation"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.overworld_generation PROPERTIES
            LABELS "reference;original;generation" TIMEOUT 1900 SKIP_RETURN_CODE 77)
    endif()

    add_test(NAME tools.finish_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/chunk" -p test_finish_reference.py)
    set_tests_properties(tools.finish_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 30)

    add_test(NAME tools.finish_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/chunk/test_finish_interop.py"
        --executable "$<TARGET_FILE:mc_chunk_finish_probe>")
    set_tests_properties(tools.finish_interop PROPERTIES LABELS "public;tools" TIMEOUT 60)

    if(MC_ENABLE_FINISH_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_FINISH_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.chunk_finish
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/chunk/compare_finish.py"
            --executable "$<TARGET_FILE:mc_chunk_finish_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_FINISH_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-finish"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.chunk_finish PROPERTIES
            LABELS "reference;original;chunk" TIMEOUT 600 SKIP_RETURN_CODE 77)
    endif()
    add_test(NAME tools.light_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/lighting" -p test_reference.py)
    set_tests_properties(tools.light_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 30)

    if(MC_ENABLE_LIGHT_REFERENCE_TESTS)
        find_package(Java 11 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_LIGHT_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.light_queue
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/lighting/compare_light.py"
            --executable "$<TARGET_FILE:mc_light_queue_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_LIGHT_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-light-queue"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.light_queue PROPERTIES
            LABELS "reference;original;lighting" TIMEOUT 240 SKIP_RETURN_CODE 77)
    endif()
    add_test(NAME tools.import_assets
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/resources" -p test_import_assets.py)
    set_tests_properties(tools.import_assets PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.compression_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/compression/test_interop.py"
        --executable "$<TARGET_FILE:mc_compression_probe>")
    set_tests_properties(tools.compression_interop PROPERTIES LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.noise_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/noise" -p test_reference.py)
    set_tests_properties(tools.noise_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.simplex_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/simplex" -p test_reference.py)
    set_tests_properties(tools.simplex_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.biome_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/biome" -p test_reference.py)
    set_tests_properties(tools.biome_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.density_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/density" -p test_reference.py)
    set_tests_properties(tools.density_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.surface_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/surface" -p test_reference.py)
    set_tests_properties(tools.surface_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.math_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/math" -p test_reference.py)
    set_tests_properties(tools.math_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.caves_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/caves" -p test_reference.py)
    set_tests_properties(tools.caves_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.caves_geometry_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/caves" -p test_geometry_reference.py)
    set_tests_properties(tools.caves_geometry_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.nibbles_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/chunk" -p test_nibbles_reference.py)
    set_tests_properties(tools.nibbles_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.chunk_data_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/chunk" -p test_data_reference.py)
    set_tests_properties(tools.chunk_data_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    if(MC_ENABLE_NIBBLES_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.nibbles
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/chunk/compare_nibbles.py"
            --executable "$<TARGET_FILE:mc_chunk_nibbles_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-nibbles"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.nibbles PROPERTIES
            LABELS "reference;original;chunk" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_CHUNK_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.chunk_data
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/chunk/compare_chunk.py"
            --executable "$<TARGET_FILE:mc_chunk_data_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-data"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.chunk_data PROPERTIES
            LABELS "reference;original;chunk" TIMEOUT 600 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_JAVA_REFERENCE_TESTS)
        find_package(Java 8 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.java_semantics
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_java.py"
            --executable "$<TARGET_FILE:mc_test_java>"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-java")
        set_tests_properties(reference.java_semantics PROPERTIES
            LABELS "reference;java" TIMEOUT 90 SKIP_RETURN_CODE 77)

        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.java_int_set
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_int_set.py"
            --executable "$<TARGET_FILE:mc_java_int_set_probe>"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-java-int-set")
        set_tests_properties(reference.java_int_set PROPERTIES
            LABELS "reference;java" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_NBT_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.nbt
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/nbt/compare_nbt.py"
            --executable "$<TARGET_FILE:mc_nbt_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-nbt"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.nbt PROPERTIES
            LABELS "reference;original;nbt" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_MATH_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.math
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/math/compare_math.py"
            --executable "$<TARGET_FILE:mc_math_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-math"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.math PROPERTIES
            LABELS "reference;original;math" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_COMPRESSION_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.compression
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/compression/compare_compression.py"
            --executable "$<TARGET_FILE:mc_compression_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-compression"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.compression PROPERTIES
            LABELS "reference;original;compression" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_NOISE_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.noise
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/noise/compare_noise.py"
            --executable "$<TARGET_FILE:mc_noise_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-noise"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.noise PROPERTIES
            LABELS "reference;original;noise" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_SIMPLEX_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.simplex
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/simplex/compare_simplex.py"
            --executable "$<TARGET_FILE:mc_simplex_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-simplex"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.simplex PROPERTIES
            LABELS "reference;original;simplex" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_BIOME_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.biome
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/biome/compare_biome.py"
            --executable "$<TARGET_FILE:mc_biome_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-biome"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.biome PROPERTIES
            LABELS "reference;original;biome" TIMEOUT 240 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_DENSITY_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.density
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/density/compare_density.py"
            --executable "$<TARGET_FILE:mc_terrain_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-density"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.density PROPERTIES
            LABELS "reference;original;density" TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_CAVES_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        get_filename_component(MC_REFERENCE_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.caves
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/caves/compare_caves.py"
            --executable "$<TARGET_FILE:mc_caves_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_REFERENCE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-caves"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.caves PROPERTIES
            LABELS "reference;original;caves" TIMEOUT 300 SKIP_RETURN_CODE 77)

        add_test(NAME reference.caves_geometry
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/caves/compare_geometry.py"
            --executable "$<TARGET_FILE:mc_caves_geometry_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_REFERENCE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-caves-geometry"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.caves_geometry PROPERTIES
            LABELS "reference;original;caves;geometry" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_SURFACE_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.surface
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/surface/compare_surface.py"
            --executable "$<TARGET_FILE:mc_surface_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-surface"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.surface PROPERTIES
            LABELS "reference;original;surface" TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()
endif()


# Canonical typed Pack and reached Engine/Font startup continuations.
if(TARGET mc_client_texture_packs)
    target_sources(mc_client_texture_packs PRIVATE
        src/client/texture_packs/source_files.c
        src/client/texture_packs/source_archive.c
        src/client/texture_packs/source_owner.c)
    target_link_libraries(mc_client_texture_packs PUBLIC mc_java_files)
endif()
if(TARGET mc_client_render_engine)
    target_link_libraries(mc_client_render_engine PUBLIC mc_c_atomics)
endif()
if(TARGET mc_client_application_startup AND TARGET mc_client_application_constructor)
    target_sources(mc_client_application_startup PRIVATE
        src/client/startup/application_startup_pack.c
        src/client/startup/application_startup_render_engine.c
        src/client/startup/application_startup_font.c
        src/client/startup/application_startup_colors.c)
    target_link_libraries(mc_client_application_startup PUBLIC
        mc_client_texture_packs mc_client_render_engine mc_client_source_resources)
endif()
if(TARGET mc_test_render_engine)
    target_include_directories(mc_test_render_engine PRIVATE src)
endif()

# PROPOSAL ONLY: append after canonical libraries and existing tests are declared.
# Production an adds no TU/target; its four native bodies stay canonical.
if(BUILD_TESTING)
    add_executable(mc_test_file_attributes tests/world/storage/test_file_attributes.c)
    target_link_libraries(mc_test_file_attributes PRIVATE mc_java_files mc_build_options)
    add_library(mc_file_attributes_fault_native OBJECT src/platform/files/directory.c)
    target_link_libraries(mc_file_attributes_fault_native PRIVATE mc_build_options)
    if(WIN32)
        target_compile_definitions(mc_file_attributes_fault_native PRIVATE
            MC_FILE_NATIVE_GET_TIME=mc_test_file_time
            MC_FILE_NATIVE_CLOSE_HANDLE=mc_test_file_close)
    else()
        target_compile_definitions(mc_file_attributes_fault_native PRIVATE
            MC_FILE_NATIVE_STAT=mc_test_file_stat)
    endif()
    add_executable(mc_test_file_attributes_fault
        tests/world/storage/test_file_attributes.c
        $<TARGET_OBJECTS:mc_file_attributes_fault_native>)
    target_compile_definitions(mc_test_file_attributes_fault PRIVATE MC_FILE_ATTRIBUTE_FAULT_CONTROL=1)
    target_link_libraries(mc_test_file_attributes_fault PRIVATE mc_java_files mc_build_options)
    foreach(_mc_attr_kind IN ITEMS ordinary fault)
        set(_mc_attr_target mc_test_file_attributes)
        if(_mc_attr_kind STREQUAL "fault")
            set(_mc_attr_target mc_test_file_attributes_fault)
        endif()
        foreach(_mc_attr_mode IN ITEMS normal optimized)
            set(_mc_attr_flags -B)
            if(_mc_attr_mode STREQUAL "optimized")
                list(APPEND _mc_attr_flags -O)
            endif()
            add_test(NAME world.file_attributes_${_mc_attr_kind}_${_mc_attr_mode}
                COMMAND Python3::Interpreter ${_mc_attr_flags}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_file_attributes.py"
                    --executable "$<TARGET_FILE:${_mc_attr_target}>"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/file-attributes-${_mc_attr_kind}-${_mc_attr_mode}")
            set_tests_properties(world.file_attributes_${_mc_attr_kind}_${_mc_attr_mode} PROPERTIES
                LABELS "public;unit;storage;filesystem;ownership" TIMEOUT 180)
        endforeach()
    endforeach()
    unset(_mc_attr_kind)
    unset(_mc_attr_target)
    unset(_mc_attr_mode)
    unset(_mc_attr_flags)
endif()

if(BUILD_TESTING AND MC_ENABLE_APPLICATION_CONSTRUCTOR_TESTS AND
   TARGET mc_client_application_constructor AND TARGET mc_client_application_startup)
    add_executable(mc_test_texture_pack_file tests/client/texture_packs/test_source_construction.c)
    target_include_directories(mc_test_texture_pack_file PRIVATE src)
    target_link_libraries(mc_test_texture_pack_file PRIVATE
        mc_client_application_constructor mc_game_settings mc_client_texture_packs
        mc_sound_backend mc_client_assets mc_resources mc_c_atomics mc_build_options)
    set_property(TARGET mc_test_texture_pack_file PROPERTY LINKER_LANGUAGE CXX)
    if(WIN32)
        add_custom_command(TARGET mc_test_texture_pack_file POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_texture_pack_file>")
    endif()
endif()

# Explicit fixture profile: generation is a separately admitted runner phase,
# once into a fresh owned directory. Never execute fixture code at configure.
option(MC_ENABLE_TEXTURE_PACK_FILE_TESTS "Register actual typed File Pack cases" OFF)
set(MC_TEXTURE_PACK_FILE_FIXTURE_ROOT "" CACHE PATH "Fresh authored typed Pack fixture root")
if(MC_ENABLE_TEXTURE_PACK_FILE_TESTS)
    if(NOT BUILD_TESTING OR NOT MC_ENABLE_APPLICATION_CONSTRUCTOR_TESTS OR
       NOT TARGET mc_test_texture_pack_file OR "${MC_TEXTURE_PACK_FILE_FIXTURE_ROOT}" STREQUAL "")
        message(FATAL_ERROR "Typed Pack controls require actual constructor/test target and explicit authored fixtures")
    endif()
    if(NOT "${MC_REFERENCE_CLIENT_RESOURCE_DIR}" STREQUAL "")
        set(_mc_pack_constructor_root "${MC_REFERENCE_CLIENT_RESOURCE_DIR}")
    else()
        set(_mc_pack_constructor_root "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/startup/fixtures")
    endif()
    foreach(_mc_pack_case IN ITEMS empty absent non-directory mkdir-false stored deflated
            metadata-limit removed null-app null-settings missing-icon bound)
        set(_mc_pack_resources "${MC_TEXTURE_PACK_FILE_FIXTURE_ROOT}/resources")
        if(_mc_pack_case STREQUAL "missing-icon")
            set(_mc_pack_resources "${MC_TEXTURE_PACK_FILE_FIXTURE_ROOT}/missing-resources")
        endif()
        add_test(NAME "client.texture_pack_file_${_mc_pack_case}"
            COMMAND mc_test_texture_pack_file "${_mc_pack_constructor_root}" "${_mc_pack_resources}"
                "${MC_TEXTURE_PACK_FILE_FIXTURE_ROOT}/${_mc_pack_case}" "${_mc_pack_case}")
        set_tests_properties("client.texture_pack_file_${_mc_pack_case}" PROPERTIES
            LABELS "public;integration;client;texture;filesystem;ownership" TIMEOUT 180)
    endforeach()
    unset(_mc_pack_constructor_root)
    unset(_mc_pack_resources)
    unset(_mc_pack_case)
endif()


if(MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS)
    if(NOT TARGET mc_test_application_startup OR NOT TARGET mc_client_application_startup OR
       NOT MC_ENABLE_CLIENT_DISPLAY_TESTS)
        message(FATAL_ERROR "Source Engine/Font startup modes require the actual startup and Display targets")
    endif()
    foreach(_mc_startup_mode IN ITEMS render-engine-limit render-font-limit bitmap-font-limit font-shared-owner
            color-lookup-limit colorizer-limit)
        set(_mc_startup_mode_labels "integration;client;graphics;startup;filesystem;font;ownership")
        if(_mc_startup_mode STREQUAL "color-lookup-limit" OR _mc_startup_mode STREQUAL "colorizer-limit")
            set(_mc_startup_mode_labels "integration;client;graphics;startup;filesystem;color;ownership")
        endif()
        add_test(NAME "client.application_startup_${_mc_startup_mode}"
            COMMAND mc_test_application_startup "${_mc_startup_resource_root}"
                "window-${_mc_startup_mode}" "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}")
        set_tests_properties("client.application_startup_${_mc_startup_mode}" PROPERTIES
            LABELS "${_mc_startup_mode_labels}" TIMEOUT 180)
    endforeach()
    unset(_mc_startup_mode_labels)
    unset(_mc_startup_mode)
endif()


# Retain actual native/fault/included-TU link extraction evidence.
foreach(_mc_map_target IN ITEMS
        mc_test_file_attributes_fault mc_test_sound_construction_allocation
        mc_test_sound_allocation mc_test_source_fp mc_probe_render_source_cpu
        mc_test_render_source_font mc_test_display_sdk_window)
    if(TARGET ${_mc_map_target})
        if(MSVC)
            target_link_options(${_mc_map_target} PRIVATE
                "/MAP:${CMAKE_CURRENT_BINARY_DIR}/${_mc_map_target}.map")
        else()
            target_link_options(${_mc_map_target} PRIVATE
                "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${_mc_map_target}.map")
        endif()
    endif()
endforeach()
unset(_mc_map_target)

if(BUILD_TESTING)
    include(cmake/tests/java_storage.cmake)
    include(cmake/tests/java_source_encoder.cmake)
    include(cmake/tests/statistics.cmake)
    include(cmake/tests/application_ownership.cmake)
    include(cmake/tests/client_input.cmake)
    include(cmake/tests/client_gl_diagnostics.cmake)
    include(cmake/tests/texture_effects.cmake)
    include(cmake/tests/client_cx.cmake)
    include(cmake/tests/gui_ingame.cmake)
    include(cmake/tests/java_file_writer.cmake)
    include(cmake/tests/game_settings_source_save.cmake)
    include(cmake/tests/render_global.cmake)
    if(MC_BUILD_CLIENT)
        include(cmake/tests/client_controllers.cmake)
        include(cmake/tests/client_gl_setup.cmake)
    endif()
    if(MC_BUILD_CLIENT)
        include(cmake/tests/client_rendering.cmake)
        include(cmake/tests/client_render_manager_frame.cmake)
        include(cmake/tests/client_tile_entity_renderer.cmake)
    endif()
endif()

# Canonical base PrintStream and selected diagnostic builder controls.
include(cmake/tests/java_print_stream.cmake)

# Reviewed startup-tail boundary observer after ordinary target links.
include(cmake/tests/client_startup_tail_boundary.cmake)

include(cmake/tests/startup_effects.cmake)
include(cmake/tests/startup_rendering.cmake)
include(cmake/tests/resource_download.cmake)
include(cmake/tests/client_source_screens.cmake)
include(cmake/client_source_screen_drawing.cmake)
include(cmake/client_source_screen_events.cmake)
include(cmake/client_effect_upload.cmake)
include(cmake/tests/source_screen_events.cmake)
include(cmake/client_runtime.cmake)
include(cmake/platform_date_format.cmake)
if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE
        src/client/gui/source_world_selection.c src/client/gui/source_world_list.c)
    target_link_libraries(mc_client_source_screens PRIVATE mc_client_saves mc_platform_date_format)
endif()
if(TARGET mc_client_source_screen_events AND TARGET mc_client_application_startup)
    add_library(mc_client_screen_transitions STATIC src/client/gui/source_screen_transitions.c)
    target_include_directories(mc_client_screen_transitions PUBLIC include src/client/gui
        PRIVATE src src/client src/platform/time)
    target_link_libraries(mc_client_screen_transitions PUBLIC mc_client_application_startup
        mc_client_source_screens mc_client_singleplayer_controller PRIVATE mc_build_options)
    target_link_libraries(mc_client_source_screen_events PUBLIC mc_client_screen_transitions)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_world_selection_drawing.c)
    target_link_libraries(mc_client_source_screen_drawing PUBLIC mc_client_screen_transitions mc_platform_date_format)
endif()
if(BUILD_TESTING)
    include(cmake/tests/platform_date_format.cmake)
    include(cmake/tests/client_source_world_selection.cmake)
    include(cmake/tests/startup_fingerprint.cmake)
    include(cmake/tests/client_world_selection_loop.cmake)
endif()
include(cmake/tests/platform_calendar.cmake)
include(cmake/tests/application_resource_services.cmake)

# Canonical source SaveHandler, SavedData and region transport inside GAME Worlds.
include(cmake/source_chunk_loader.cmake)
include(cmake/world_source_construction.cmake)
if(TARGET mc_client_screen_transitions)
    add_library(mc_client_world_load STATIC
        src/client/world/load_world.c src/client/world/prepare_world.c)
    target_include_directories(mc_client_world_load PUBLIC src/client/world
        PRIVATE src include src/platform/time)
    target_link_libraries(mc_client_world_load PUBLIC mc_client_application_startup
        mc_client_singleplayer_controller mc_simulation mc_c_atomics PRIVATE mc_build_options)
    target_link_libraries(mc_client_screen_transitions PUBLIC mc_client_world_load)
endif()
if(BUILD_TESTING)
    include(cmake/tests/source_saved_data.cmake)
    include(cmake/tests/source_regions.cmake)
    target_link_libraries(mc_test_source_handler PRIVATE mc_simulation)
endif()

include(cmake/entity_appearance.cmake)
include(cmake/world_player_load.cmake)
include(cmake/client_image_buffer.cmake)
include(cmake/entity_hit_result.cmake)
include(cmake/world_raytrace.cmake)
include(cmake/tests/world_raytrace.cmake)
include(cmake/entity_living_view.cmake)
include(cmake/client_controller_tick.cmake)
include(cmake/client_input_fields.cmake)
include(cmake/world_brightness.cmake)
include(cmake/client_held_item_tick.cmake)
include(cmake/client_renderer_rain.cmake)
include(cmake/client_renderer_tick.cmake)
include(cmake/client_renderer_focus.cmake)
include(cmake/entity_mouse_filter.cmake)
include(cmake/client_pause_screen.cmake)
include(cmake/client_world_frame.cmake)
include(cmake/world_tick.cmake)
include(cmake/client_sky_color.cmake)
include(cmake/world_colors.cmake)
include(cmake/client_renderer_camera.cmake)
include(cmake/client_frustum.cmake)
include(cmake/world_sky.cmake)
include(cmake/client_sky_frame.cmake)
include(cmake/client_controller_interpolation.cmake)
include(cmake/world_client_tick_state.cmake)
if(MC_BUILD_CLIENT)
    target_sources(mc_client_entity_renderer PRIVATE src/client/world/entity_renderer/picking.c)
    target_link_libraries(mc_client_entity_renderer PRIVATE mc_client_singleplayer_controller)
    target_sources(mc_client_render_textures PRIVATE src/client/render_engine/bounds_draw.c)
    target_sources(mc_client_render_manager_cpu PRIVATE src/client/render/manager/world.c)
    include(cmake/client_render_global_world.cmake)
    include(cmake/client_render_global_recenter.cmake)
    include(cmake/client_render_global_access.cmake)
    include(cmake/tests/client_source_lists.cmake)
    include(cmake/tests/render_global_access.cmake)
    include(cmake/tests/render_global_world.cmake)
    include(cmake/client_image_download.cmake)
    include(cmake/tests/client_image_download.cmake)
    include(cmake/tests/client_picking.cmake)
    include(cmake/client_render_block_colors.cmake)
    include(cmake/tests/client_render_blocks_ao.cmake)
    include(cmake/tests/client_render_blocks_faces.cmake)
    include(cmake/tests/client_render_blocks_flat.cmake)
    include(cmake/tests/client_render_batch.cmake)
    include(cmake/tests/client_occlusion_queries.cmake)
    include(cmake/tests/client_render_global_recenter.cmake)
    include(cmake/client_render_global_submit_tests.cmake)
    include(cmake/tests/client_render_global_submit_cpu.cmake)
    include(cmake/tests/client_lighting_enable.cmake)
    include(cmake/tests/client_lighting_raster.cmake)
endif()
