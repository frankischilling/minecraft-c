include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.native_product_capture COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client" -p test_native*captur*.py)
    set_tests_properties(tools.native_product_capture PROPERTIES
        LABELS "public;unit;tools;client;ownership" TIMEOUT 60)
endif()
