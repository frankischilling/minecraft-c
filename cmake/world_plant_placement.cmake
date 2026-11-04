include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.plant_placement_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/plant_placement" -p test_packet.py)
    set_tests_properties(tools.plant_placement_packet PROPERTIES
        LABELS "public;unit;tools;world;reference" TIMEOUT 60)
    if(TARGET mc_test_item_use)
        add_executable(mc_test_plant_placement_reference tests/world/runtime/plant_placement_reference.c)
        target_include_directories(mc_test_plant_placement_reference PRIVATE src src/platform/time)
        get_target_property(_plant_placement_links mc_test_item_use LINK_LIBRARIES)
        target_link_libraries(mc_test_plant_placement_reference PRIVATE ${_plant_placement_links})
        unset(_plant_placement_links)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_plant_placement_reference PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_plant_placement_reference POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_plant_placement_reference>")
        endif()
        add_test(NAME world.plant_placement_reference COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_plant_placement.py"
            --executable "$<TARGET_FILE:mc_test_plant_placement_reference>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/plant-placement-reference")
        set_tests_properties(world.plant_placement_reference PROPERTIES
            LABELS "public;integration;world;ownership;reference" TIMEOUT 180)
    endif()
endif()
