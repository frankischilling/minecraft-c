include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.dye_icon_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/dye_icon" -p "test_*.py")
    set_tests_properties(tools.dye_icon_packet PROPERTIES
        LABELS "public;unit;tools;item;reference" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_test_inventory_block)
    add_test(NAME item.dye_icon_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/dye_icon/run_native.py"
        --executable "$<TARGET_FILE:mc_test_inventory_block>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dye-icon-reference")
    set_tests_properties(item.dye_icon_reference PROPERTIES
        LABELS "public;integration;item;reference" TIMEOUT 180)
endif()
