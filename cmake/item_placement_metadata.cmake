include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_executable(mc_test_item_placement_metadata tests/item/use/placement_metadata.c)
    target_include_directories(mc_test_item_placement_metadata PRIVATE src)
    target_link_libraries(mc_test_item_placement_metadata PRIVATE
        mc_registry mc_c_atomics mc_build_options)
    add_test(NAME tools.item_placement_metadata_packet COMMAND Python3::Interpreter -B
        -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/placement_metadata"
        -p test_packet.py)
    set_tests_properties(tools.item_placement_metadata_packet PROPERTIES
        LABELS "public;unit;tools;item;reference" TIMEOUT 60)
    add_test(NAME item.placement_metadata_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_placement_metadata.py"
        --executable "$<TARGET_FILE:mc_test_item_placement_metadata>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-placement-metadata")
    set_tests_properties(item.placement_metadata_reference PROPERTIES
        LABELS "public;unit;item;reference" TIMEOUT 60)
endif()
