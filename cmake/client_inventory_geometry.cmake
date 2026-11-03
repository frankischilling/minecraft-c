include_guard(GLOBAL)
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.inventory_geometry_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/inventory_geometry" -p "test_*.py")
    set_tests_properties(tools.inventory_geometry_packet PROPERTIES
        LABELS "public;unit;tools;client;graphics;reference" TIMEOUT 60)
    add_test(NAME tools.inventory_side_chains_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/inventory_geometry/side_chains" -p test_packet.py)
    set_tests_properties(tools.inventory_side_chains_packet PROPERTIES
        LABELS "public;unit;tools;client;graphics;reference" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_test_render_blocks_faces)
    get_target_property(_inventory_geometry_links mc_test_render_blocks_faces LINK_LIBRARIES)
    add_executable(mc_test_inventory_geometry tests/client/render/test_inventory_geometry.c)
    target_include_directories(mc_test_inventory_geometry PRIVATE src src/platform/time)
    target_link_libraries(mc_test_inventory_geometry PRIVATE ${_inventory_geometry_links})
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_inventory_geometry PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_inventory_geometry POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_inventory_geometry>")
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.inventory_geometry COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_inventory_geometry.py"
            --executable "$<TARGET_FILE:mc_test_inventory_geometry>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/inventory-geometry-controls")
        set_tests_properties(client.inventory_geometry PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 240)
    endif()
    unset(_inventory_geometry_links)
endif()
