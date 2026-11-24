include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.item_appearance_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/appearance" -p test_packet.py)
    set_tests_properties(tools.item_appearance_packet PROPERTIES
        LABELS "public;unit;tools;item;reference" TIMEOUT 60)
    if(TARGET mc_client_render_engine)
        add_executable(mc_test_item_appearance tests/item/render/appearance_reference.c)
        target_include_directories(mc_test_item_appearance PRIVATE src)
        target_link_libraries(mc_test_item_appearance PRIVATE
            mc_registry mc_client_render_engine mc_c_atomics mc_build_options)
        add_test(NAME item.appearance_reference COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_appearance.py"
            --executable "$<TARGET_FILE:mc_test_item_appearance>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-appearance")
        set_tests_properties(item.appearance_reference PROPERTIES
            LABELS "public;unit;item;reference" TIMEOUT 90)
    endif()
endif()
