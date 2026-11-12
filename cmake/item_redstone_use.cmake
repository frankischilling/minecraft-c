include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.redstone_use_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/redstone_use" -p test_packet.py)
    set_tests_properties(tools.redstone_use_packet PROPERTIES
        LABELS "public;unit;tools;item;world;reference" TIMEOUT 60)
    add_test(NAME tools.redstone_use_comparator COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_redstone_use.py" --self-test)
    set_tests_properties(tools.redstone_use_comparator PROPERTIES
        LABELS "public;unit;tools;item;world;reference" TIMEOUT 60)

    add_executable(mc_test_item_redstone_reference tests/item/use/redstone/reference.c)
    target_include_directories(mc_test_item_redstone_reference PRIVATE src src/platform/time)
    target_link_libraries(mc_test_item_redstone_reference PRIVATE mc_item_use mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_item_redstone_reference PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_item_redstone_reference POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_item_redstone_reference>")
    endif()
    add_test(NAME item.redstone_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_redstone_use.py" --execute
        --executable "$<TARGET_FILE:mc_test_item_redstone_reference>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-redstone-reference")
    set_tests_properties(item.redstone_reference PROPERTIES
        LABELS "public;integration;item;world;statistics;ownership;reference" TIMEOUT 240)
endif()
