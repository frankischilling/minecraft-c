include_guard(GLOBAL)

target_sources(mc_inventory PRIVATE
    src/inventory/dispenser.c
    src/inventory/dispenser_container.c)
target_sources(mc_simulation PRIVATE src/inventory/dispenser_tile.c)

if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_dispenser_screen.c)
endif()
if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_dispenser_drawing.c)
endif()

if(BUILD_TESTING)
    add_executable(mc_test_dispenser_activation tests/world/runtime/dispenser/test_activation.c)
    target_include_directories(mc_test_dispenser_activation PRIVATE src src/platform/time)
    target_link_libraries(mc_test_dispenser_activation PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.dispenser_activation COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dispenser_activation.py"
        --executable "$<TARGET_FILE:mc_test_dispenser_activation>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dispenser-activation-controls")
    set_tests_properties(world.dispenser_activation PROPERTIES
        LABELS "public;integration;world;inventory;ownership" TIMEOUT 180)
    add_executable(mc_test_dispenser_storage tests/world/runtime/dispenser/test_storage.c)
    target_include_directories(mc_test_dispenser_storage PRIVATE src src/platform/time)
    target_link_libraries(mc_test_dispenser_storage PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.dispenser_storage COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dispenser_storage.py"
        --executable "$<TARGET_FILE:mc_test_dispenser_storage>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dispenser-storage-controls")
    set_tests_properties(world.dispenser_storage PROPERTIES
        LABELS "public;integration;world;inventory;storage;ownership" TIMEOUT 180)
    add_executable(mc_test_dispenser_container tests/inventory/dispenser_container.c)
    add_executable(mc_test_dispenser_container_projection
        tests/inventory/dispenser_container_projection.c)
    target_include_directories(mc_test_dispenser_container_projection PRIVATE src src/platform/time)
    target_link_libraries(mc_test_dispenser_container_projection PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    target_include_directories(mc_test_dispenser_container PRIVATE src src/platform/time)
    target_link_libraries(mc_test_dispenser_container PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME inventory.dispenser_container COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dispenser.py"
        --executable "$<TARGET_FILE:mc_test_dispenser_container>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dispenser-container-controls")
    set_tests_properties(inventory.dispenser_container PROPERTIES
        LABELS "public;integration;inventory;ownership" TIMEOUT 240)

    add_executable(mc_test_dispenser_dirty tests/world/runtime/dispenser/test_dirty.c)
    target_include_directories(mc_test_dispenser_dirty PRIVATE src src/platform/time)
    target_link_libraries(mc_test_dispenser_dirty PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.dispenser_dirty COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dispenser_dirty.py"
        --executable "$<TARGET_FILE:mc_test_dispenser_dirty>" --profile producer
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dispenser-dirty-controls")
    set_tests_properties(world.dispenser_dirty PROPERTIES
        LABELS "public;integration;world;inventory;ownership" TIMEOUT 180)

    if(TARGET mc_client_source_screens)
        add_executable(mc_test_source_dispenser_dirty
            tests/client/gui/test_source_dispenser_dirty.c)
        target_include_directories(mc_test_source_dispenser_dirty PRIVATE src src/platform/time)
        target_link_libraries(mc_test_source_dispenser_dirty PRIVATE
            mc_client_source_screens mc_client_application_owner mc_simulation
            mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_source_dispenser_dirty PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_source_dispenser_dirty POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_source_dispenser_dirty>")
        endif()
        add_test(NAME client.dispenser_dirty COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dispenser_dirty.py"
            --executable "$<TARGET_FILE:mc_test_source_dispenser_dirty>" --profile screen
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dispenser-screen-dirty-controls")
        set_tests_properties(client.dispenser_dirty PROPERTIES
            LABELS "public;integration;client;world;inventory;ownership" TIMEOUT 180)
    endif()
endif()
