include_guard(GLOBAL)
target_sources(mc_inventory PRIVATE src/inventory/player_current.c)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE
        src/client/world/entity_renderer/held_item_tick.c)
    target_link_libraries(mc_client_entity_renderer PRIVATE mc_inventory mc_simulation)
    if(BUILD_TESTING)
        add_executable(mc_test_held_item_tick tests/client/world/test_held_item_tick.c)
        target_include_directories(mc_test_held_item_tick PRIVATE src src/platform/time)
        target_link_libraries(mc_test_held_item_tick PRIVATE
            mc_client_entity_renderer mc_build_options)
        find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
        add_test(NAME client.held_item_tick COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/held_item_tick/run_native.py"
            --executable "$<TARGET_FILE:mc_test_held_item_tick>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/held-item-tick")
        set_tests_properties(client.held_item_tick PROPERTIES
            LABELS "public;unit;client;ownership;replay" TIMEOUT 60)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_held_item_tick PROPERTY LINKER_LANGUAGE CXX)
        endif()
    endif()
endif()
