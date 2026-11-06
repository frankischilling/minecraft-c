include_guard(GLOBAL)

if(BUILD_TESTING)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_sleep_nbt_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_sleep_nbt_python_options -O)
        endif()
        add_test(NAME "tools.player_sleep_nbt_parent_packet_${mode}" COMMAND Python3::Interpreter
            ${_mc_sleep_nbt_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_sleep_nbt_parent/test_packet.py")
        set_tests_properties("tools.player_sleep_nbt_parent_packet_${mode}" PROPERTIES
            LABELS "public;unit;entity;world;storage;reference;tools" TIMEOUT 90)
    endforeach()
endif()

if(BUILD_TESTING AND TARGET mc_test_player_nbt)
    add_test(NAME entity.player_sleep_nbt_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_sleep_nbt_controls.py"
        --executable "$<TARGET_FILE:mc_test_player_nbt>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-sleep-nbt-controls")
    set_tests_properties(entity.player_sleep_nbt_controls PROPERTIES
        LABELS "public;integration;entity;world;storage;ownership" TIMEOUT 240)
    add_test(NAME entity.player_sleep_nbt_parent COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_sleep_nbt_parent.py"
        --executable "$<TARGET_FILE:mc_test_player_nbt>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-sleep-nbt-parent")
    set_tests_properties(entity.player_sleep_nbt_parent PROPERTIES
        LABELS "public;integration;entity;world;storage;reference;replay;ownership" TIMEOUT 240)
endif()

option(MC_ENABLE_CLIENT_PLAYER_SLEEP_NBT_TESTS
    "Enable local saved sleeping-Player Load/Respawn SDK scenes" OFF)
if(MC_ENABLE_CLIENT_PLAYER_SLEEP_NBT_TESTS)
    if(NOT WIN32 OR NOT BUILD_TESTING OR NOT TARGET mc_client_runtime OR
       NOT MC_ENABLE_CLIENT_DISPLAY_TESTS OR NOT MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        message(FATAL_ERROR
            "Sleeping-Player scenes require native Windows display tests and imported resources")
    endif()
    add_executable(mc_test_player_sleep_nbt_load
        tests/client/startup/test_world_selection_loop.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_compile_definitions(mc_test_player_sleep_nbt_load PRIVATE MC_TEST_PLAYER_SLEEP_NBT=1)
    target_include_directories(mc_test_player_sleep_nbt_load PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_sleep_nbt_load PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_player_sleep_nbt_load PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_custom_command(TARGET mc_test_player_sleep_nbt_load POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
            "$<TARGET_FILE_DIR:mc_test_player_sleep_nbt_load>")
    foreach(profile IN ITEMS success load-failure)
        if(profile STREQUAL "success")
            set(_mc_sleep_nbt_case client.player_sleep_nbt_load)
        else()
            set(_mc_sleep_nbt_case client.player_sleep_nbt_load_failure)
        endif()
        add_test(NAME "${_mc_sleep_nbt_case}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_sleep_nbt_load.py"
            --executable "$<TARGET_FILE:mc_test_player_sleep_nbt_load>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-sleep-nbt-load" --profile ${profile})
        set_tests_properties("${_mc_sleep_nbt_case}" PROPERTIES
            LABELS "local;integration;client;input;graphics;entity;ownership;storage"
            RESOURCE_LOCK mc_desktop_focus TIMEOUT 300)
    endforeach()
endif()
