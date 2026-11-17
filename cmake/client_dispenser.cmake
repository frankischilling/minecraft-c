include_guard(GLOBAL)

target_sources(mc_inventory PRIVATE
    src/inventory/dispenser.c
    src/inventory/dispenser_container.c)
target_sources(mc_simulation PRIVATE src/inventory/dispenser_tile.c
    src/world/runtime/mutation/dispenser/dispenser.c)

if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_dispenser_screen.c)
endif()
if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_dispenser_drawing.c)
endif()

if(BUILD_TESTING)
    foreach(_dispenser_profile IN ITEMS orientation placed-by registry-failure)
        if(_dispenser_profile STREQUAL "orientation")
            set(_dispenser_target orientation)
            set(_dispenser_source test_placement.c)
        elseif(_dispenser_profile STREQUAL "placed-by")
            set(_dispenser_target placed_by)
            set(_dispenser_source test_placed_by.c)
        else()
            set(_dispenser_target registry_failure)
            set(_dispenser_source test_registry_failure.c)
        endif()
        add_executable(mc_test_dispenser_${_dispenser_target}
            tests/world/runtime/dispenser/${_dispenser_source})
        target_include_directories(mc_test_dispenser_${_dispenser_target} PRIVATE src src/platform/time)
        target_link_libraries(mc_test_dispenser_${_dispenser_target} PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_build_options)
        add_test(NAME world.dispenser_${_dispenser_target} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dispenser_placement.py"
            --executable "$<TARGET_FILE:mc_test_dispenser_${_dispenser_target}>"
            --profile "${_dispenser_profile}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dispenser-placement-cpu-controls")
        set_tests_properties(world.dispenser_${_dispenser_target} PROPERTIES
            # The child keeps its 150-second bound. Two complete protected
            # input passes can take over a minute on a mounted source tree.
            LABELS "public;integration;world;inventory;ownership" TIMEOUT 300)
    endforeach()
    unset(_dispenser_profile)
    unset(_dispenser_target)
    unset(_dispenser_source)
    add_executable(mc_test_dispenser_registry_projection tests/inventory/dispenser_registry_projection.c)
    target_include_directories(mc_test_dispenser_registry_projection PRIVATE src src/platform/time)
    target_link_libraries(mc_test_dispenser_registry_projection PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME inventory.dispenser_registry_projection COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dispenser_placement.py"
        --executable "$<TARGET_FILE:mc_test_dispenser_registry_projection>" --profile registry-projection
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dispenser-registry-projection-controls")
    set_tests_properties(inventory.dispenser_registry_projection PROPERTIES
        LABELS "public;integration;inventory;world;ownership" TIMEOUT 300)
    foreach(_dispenser_mode IN ITEMS normal optimized)
        if(_dispenser_mode STREQUAL "optimized")
            set(_dispenser_options -O -B)
        else()
            set(_dispenser_options -B)
        endif()
        add_test(NAME tools.dispenser_inventory_packet_${_dispenser_mode}
            COMMAND Python3::Interpreter ${_dispenser_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/dispenser_container/test_packet.py")
        add_test(NAME tools.dispenser_placement_cpu_${_dispenser_mode}
            COMMAND Python3::Interpreter ${_dispenser_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dispenser_placement.py" --self-test)
        set_tests_properties(tools.dispenser_inventory_packet_${_dispenser_mode}
            tools.dispenser_placement_cpu_${_dispenser_mode} PROPERTIES
            LABELS "public;unit;tools;inventory;world" TIMEOUT 60)
    endforeach()
    unset(_dispenser_mode)
    unset(_dispenser_options)
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
