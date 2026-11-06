include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_BED_RESPAWN_TESTS "Run local imported-resource saved-Bed respawn scenes" OFF)

if(BUILD_TESTING)
    foreach(packet IN ITEMS world/bed_spawn client/bed_world entity/player_coordinates client/gui_ingame_chat)
        string(REPLACE "/" ";" parts "${packet}")
        list(GET parts 1 name)
        add_test(NAME "tools.${name}_packet" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/${packet}/test_packet.py")
        add_test(NAME "tools.${name}_packet_optimized" COMMAND Python3::Interpreter -B -O
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/${packet}/test_packet.py")
        set_tests_properties("tools.${name}_packet" "tools.${name}_packet_optimized" PROPERTIES
            LABELS "public;unit;tools;client;reference;ownership" TIMEOUT 90)
    endforeach()
    add_executable(mc_test_world_bed_spawn tests/world/runtime/test_bed_spawn.c)
    target_include_directories(mc_test_world_bed_spawn PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_bed_spawn PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.bed_spawn COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_bed_spawn.py"
        --executable "$<TARGET_FILE:mc_test_world_bed_spawn>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/bed-spawn-contracts")
    set_tests_properties(world.bed_spawn PROPERTIES
        LABELS "public;integration;world;entity;ownership;reference;replay" TIMEOUT 180)
    add_test(NAME entity.player_coordinates COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_coordinates/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_nbt_coordinates>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-coordinates-replay")
    set_tests_properties(entity.player_coordinates PROPERTIES
        LABELS "public;integration;entity;ownership;reference;replay" TIMEOUT 180)
endif()

if(MC_ENABLE_CLIENT_BED_RESPAWN_TESTS)
    if(NOT WIN32 OR NOT BUILD_TESTING OR NOT TARGET mc_client_runtime OR
       NOT MC_ENABLE_CLIENT_DISPLAY_TESTS OR NOT MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        message(FATAL_ERROR "Saved-Bed scenes require native Windows client/display tests and imported resources")
    endif()
    add_executable(mc_test_bed_respawn tests/client/startup/test_world_selection_loop.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_compile_definitions(mc_test_bed_respawn PRIVATE MC_TEST_DEATH_RESPAWN=1 MC_TEST_BED_RESPAWN=1)
    target_include_directories(mc_test_bed_respawn PRIVATE src src/platform/time)
    target_link_libraries(mc_test_bed_respawn PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_bed_respawn PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_custom_command(TARGET mc_test_bed_respawn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
            "$<TARGET_FILE_DIR:mc_test_bed_respawn>")
    foreach(profile IN ITEMS valid missing)
        add_test(NAME client.bed_respawn_${profile} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_bed_respawn.py"
            --executable "$<TARGET_FILE:mc_test_bed_respawn>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/bed-respawn-scenes" --profile ${profile})
        set_tests_properties(client.bed_respawn_${profile} PROPERTIES
            LABELS "local;integration;client;input;graphics;entity;ownership;storage"
            RESOURCE_LOCK mc_desktop_focus TIMEOUT 300)
    endforeach()
endif()

if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_bed_world tests/client/render/test_bed_world.c)
    target_include_directories(mc_test_bed_world PRIVATE src src/platform/time)
    target_link_libraries(mc_test_bed_world PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_bed_world PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.bed_world COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/bed_world/run_native.py"
        --executable "$<TARGET_FILE:mc_test_bed_world>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/bed-world-controls")
    set_tests_properties(client.bed_world PROPERTIES
        LABELS "public;integration;client;world;graphics;reference;replay" TIMEOUT 180)
endif()

if(BUILD_TESTING AND TARGET mc_test_gui_ingame_draw)
    get_target_property(_mc_chat_links mc_test_gui_ingame_draw LINK_LIBRARIES)
    foreach(kind IN ITEMS cpu device)
        if(kind STREQUAL "cpu")
            set(chat_target mc_test_gui_ingame_chat_cpu)
            set(chat_case client.gui_ingame_chat_cpu)
            set(chat_source tests/client/gui/test_gui_ingame_chat_cpu.c)
            set(chat_options --cpu)
        else()
            set(chat_target mc_test_gui_ingame_chat)
            set(chat_case client.gui_ingame_chat)
            set(chat_source tests/client/gui/test_gui_ingame_chat.c)
            set(chat_options)
        endif()
        add_executable(${chat_target} ${chat_source})
        target_link_libraries(${chat_target} PRIVATE ${_mc_chat_links})
        target_include_directories(${chat_target} PRIVATE src src/platform/time)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET ${chat_target} PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET ${chat_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:${chat_target}>")
        endif()
        if(kind STREQUAL "cpu" OR MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME ${chat_case} COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_gui_ingame_chat.py"
                --executable "$<TARGET_FILE:${chat_target}>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/${chat_target}-contracts" ${chat_options})
            set_tests_properties(${chat_case} PROPERTIES
                LABELS "public;integration;client;gui;ownership;reference;replay" TIMEOUT 180)
            if(kind STREQUAL "device")
                set_tests_properties(${chat_case} PROPERTIES RESOURCE_LOCK mc_desktop_focus)
            endif()
        endif()
    endforeach()
    unset(_mc_chat_links)
endif()
