include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_BED_PLACEMENT_TESTS
    "Run local imported-resource Bed placement and daytime interaction scene" OFF)

target_sources(mc_entity_classes PRIVATE src/entity/player/sleep_result.c)
target_sources(mc_simulation PRIVATE
    src/entity/player/sleep.c
    src/world/runtime/mutation/bed.c)
if(TARGET mc_client_source_resources)
    target_sources(mc_client_source_resources PRIVATE src/client/entity/player_message.c)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_bed_placement
        tests/client/gameplay/test_bed_placement.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_bed_placement PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" src src/platform/time)
    target_link_libraries(mc_test_bed_placement PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_bed_placement PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_bed_placement POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_bed_placement>")
    endif()
endif()

if(MC_ENABLE_CLIENT_BED_PLACEMENT_TESTS)
    if(NOT WIN32 OR NOT BUILD_TESTING OR NOT TARGET mc_test_bed_placement OR
       NOT MC_ENABLE_CLIENT_DISPLAY_TESTS OR NOT MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        message(FATAL_ERROR "Bed placement scene requires native Windows client/display tests and imported resources")
    endif()
    add_test(NAME client.bed_placement COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_bed_placement.py"
        --executable "$<TARGET_FILE:mc_test_bed_placement>"
        --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/bed-placement-scenes")
    set_tests_properties(client.bed_placement PROPERTIES
        LABELS "local;integration;client;input;graphics;entity;ownership;storage"
        RESOURCE_LOCK mc_desktop_focus TIMEOUT 300)
endif()

if(BUILD_TESTING)
    foreach(suffix IN ITEMS normal optimized)
        set(_mc_bed_fixture_options -B)
        if(suffix STREQUAL "optimized")
            list(APPEND _mc_bed_fixture_options -O)
        endif()
        add_test(NAME "tools.bed_placement_fixture_${suffix}" COMMAND Python3::Interpreter
            ${_mc_bed_fixture_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_bed_placement.py" --self-test)
        set_tests_properties("tools.bed_placement_fixture_${suffix}" PROPERTIES
            LABELS "public;unit;tools;client;storage" TIMEOUT 90)
    endforeach()

    foreach(packet IN ITEMS item/bed_use world/bed_interaction entity/player_sleep)
        string(REPLACE "/" ";" parts "${packet}")
        list(GET parts 1 name)
        add_test(NAME "tools.${name}_packet" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/${packet}/test_packet.py")
        add_test(NAME "tools.${name}_packet_optimized" COMMAND Python3::Interpreter -B -O
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/${packet}/test_packet.py")
        set_tests_properties("tools.${name}_packet" "tools.${name}_packet_optimized" PROPERTIES
            LABELS "public;unit;tools;item;world;entity;reference" TIMEOUT 90)
    endforeach()

    add_executable(mc_test_item_bed_use tests/item/use/bed_reference.c)
    target_include_directories(mc_test_item_bed_use PRIVATE src src/platform/time)
    target_link_libraries(mc_test_item_bed_use PRIVATE mc_item_use mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME item.bed_use COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_bed_use.py"
        --executable "$<TARGET_FILE:mc_test_item_bed_use>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-bed-use")
    set_tests_properties(item.bed_use PROPERTIES
        LABELS "public;integration;item;world;ownership;reference;replay" TIMEOUT 240)

    add_executable(mc_test_world_bed_interaction tests/world/runtime/test_bed_interaction.c)
    target_include_directories(mc_test_world_bed_interaction PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_bed_interaction PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.bed_interaction COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_bed_interaction.py"
        --executable "$<TARGET_FILE:mc_test_world_bed_interaction>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/bed-interaction-contracts")
    set_tests_properties(world.bed_interaction PROPERTIES
        LABELS "public;integration;world;entity;ownership;reference;replay" TIMEOUT 300)

    add_executable(mc_test_player_sleep tests/entity/player/test_sleep.c)
    target_include_directories(mc_test_player_sleep PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_sleep PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_sleep COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_sleep.py"
        --executable "$<TARGET_FILE:mc_test_player_sleep>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-sleep-controls")
    set_tests_properties(entity.player_sleep PROPERTIES
        LABELS "public;integration;entity;world;ownership;reference;replay" TIMEOUT 180)
    foreach(suffix IN ITEMS normal optimized)
        set(_mc_sleep_python_options -B)
        if(suffix STREQUAL "optimized")
            list(APPEND _mc_sleep_python_options -O)
        endif()
        add_test(NAME "tools.player_sleep_native_${suffix}" COMMAND Python3::Interpreter
            ${_mc_sleep_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_sleep.py" --controls)
        set_tests_properties("tools.player_sleep_native_${suffix}" PROPERTIES
            LABELS "public;unit;tools;entity;reference" TIMEOUT 90)
    endforeach()
endif()
