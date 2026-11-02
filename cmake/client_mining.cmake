include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_MINING_TESTS "Run the native saved-World mining/pickup cycle" OFF)

target_sources(mc_inventory PRIVATE src/inventory/mining.c src/item/mining/mining.c)
target_sources(mc_simulation PRIVATE src/entity/player/mining.c
    src/entity/particles/digging.c src/world/runtime/auxiliary.c)
target_sources(mc_client_singleplayer_controller PRIVATE src/client/controller/mining.c)
if(TARGET mc_client_render_global)
    target_sources(mc_client_render_global PRIVATE src/client/render/blocks/inventory.c)
    target_sources(mc_client_entity_renderer PRIVATE src/client/render/item_draw.c)
endif()

if(BUILD_TESTING)
    add_test(NAME tools.item_mining_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/mining/test_packet.py")
    set_tests_properties(tools.item_mining_packet PROPERTIES
        LABELS "public;unit;tools;item;inventory;reference" TIMEOUT 60)
    add_executable(mc_test_item_mining tests/item/mining/test_mining.c)
    target_include_directories(mc_test_item_mining PRIVATE src src/platform/time)
    target_link_libraries(mc_test_item_mining PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME item.mining COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_mining.py"
        --executable "$<TARGET_FILE:mc_test_item_mining>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-mining-controls")
    set_tests_properties(item.mining PROPERTIES
        LABELS "public;unit;item;inventory;reference" TIMEOUT 180)
    add_executable(mc_test_mining_tools tests/client/controller/test_mining_tools.c)
    target_include_directories(mc_test_mining_tools PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_mining_tools PRIVATE
        mc_client_singleplayer_controller mc_resources mc_c_atomics mc_build_options)
    add_test(NAME client.mining_tools COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_mining_tools.py"
        --executable "$<TARGET_FILE:mc_test_mining_tools>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mining-tools-controls")
    set_tests_properties(client.mining_tools PROPERTIES
        LABELS "public;integration;client;item;world;ownership;reference" TIMEOUT 180)
    add_executable(mc_test_mining_controller tests/client/controller/test_mining.c)
    target_include_directories(mc_test_mining_controller PRIVATE src src/platform/time)
    target_link_libraries(mc_test_mining_controller PRIVATE
        mc_client_singleplayer_controller mc_resources mc_c_atomics mc_build_options)
    add_executable(mc_test_world_auxiliary tests/world/runtime/world_access/test_auxiliary.c)
    target_include_directories(mc_test_world_auxiliary PRIVATE src)
    target_link_libraries(mc_test_world_auxiliary PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    foreach(_mode IN ITEMS controller auxiliary)
        if(_mode STREQUAL "controller")
            set(_target mc_test_mining_controller)
        else()
            set(_target mc_test_world_auxiliary)
        endif()
        add_test(NAME "client.mining_${_mode}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_mining_components.py"
            --executable "$<TARGET_FILE:${_target}>" --mode "${_mode}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mining-${_mode}-controls")
        set_tests_properties("client.mining_${_mode}" PROPERTIES
            LABELS "public;integration;client;entity;world;ownership" TIMEOUT 180)
    endforeach()
    unset(_mode)
    unset(_target)
    if(TARGET mc_test_render_blocks_faces)
        get_target_property(_face_links mc_test_render_blocks_faces LINK_LIBRARIES)
        add_executable(mc_test_inventory_block tests/client/render/test_inventory_block.c)
        target_include_directories(mc_test_inventory_block PRIVATE src src/platform/time)
        target_link_libraries(mc_test_inventory_block PRIVATE mc_client_entity_renderer ${_face_links})
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_inventory_block PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.inventory_block COMMAND mc_test_inventory_block)
        set_tests_properties(client.inventory_block PROPERTIES
            LABELS "public;unit;client;graphics;ownership" TIMEOUT 60)
        unset(_face_links)
    endif()
    if(TARGET mc_test_item_particle)
        get_target_property(_links mc_test_item_particle LINK_LIBRARIES)
        add_executable(mc_test_digging_particle tests/entity/particles/test_digging.c)
        target_include_directories(mc_test_digging_particle PRIVATE src src/platform/time)
        target_link_libraries(mc_test_digging_particle PRIVATE ${_links} mc_client_runtime)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_digging_particle PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_digging_particle POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_digging_particle>")
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME entity.digging_particle COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_digging.py"
                --executable "$<TARGET_FILE:mc_test_digging_particle>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/digging-particle-controls")
            set_tests_properties(entity.digging_particle PROPERTIES
                LABELS "public;integration;client;entity;world;graphics;ownership" TIMEOUT 600)
        endif()
        unset(_links)
    endif()
    if(TARGET mc_test_world_selection_loop)
        add_executable(mc_test_mining_loop tests/client/gameplay/test_mining.c
            tests/client/world/test_load_world_foreign_loading.c
            tests/client/world/test_render_global_entities.c
            tests/client/world/test_refresh_entity.c
            tests/client/world/test_renderer_focus.c)
        target_include_directories(mc_test_mining_loop PRIVATE src src/platform/time)
        target_link_libraries(mc_test_mining_loop PRIVATE
            mc_client_runtime mc_client_saves mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_mining_loop PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_mining_loop POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_mining_loop>")
        endif()
        if(WIN32 AND MC_ENABLE_CLIENT_MINING_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
           AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
            add_test(NAME client.mining_loop COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_mining_loop.py"
                --executable "$<TARGET_FILE:mc_test_mining_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/mining-loop-captures")
            set_tests_properties(client.mining_loop PROPERTIES
                LABELS "local;integration;client;input;graphics;storage;ownership"
                TIMEOUT 960 RUN_SERIAL TRUE)
        endif()
    endif()
endif()
