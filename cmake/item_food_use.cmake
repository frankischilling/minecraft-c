include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.food_use_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/food_use" -p test_packet.py)
    set_tests_properties(tools.food_use_packet PROPERTIES
        LABELS "public;unit;tools;item;reference" TIMEOUT 60)
endif()

if(BUILD_TESTING)
    add_executable(mc_test_item_food_reference tests/item/use/food_reference.c)
    target_include_directories(mc_test_item_food_reference PRIVATE src src/platform/time)
    target_link_libraries(mc_test_item_food_reference PRIVATE mc_item_use
        mc_client_singleplayer_controller mc_resources mc_persistence mc_c_atomics
        mc_math_classes mc_build_options)
    add_test(NAME item.food_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_food_use.py"
        --executable "$<TARGET_FILE:mc_test_item_food_reference>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-food-reference")
    set_tests_properties(item.food_reference PROPERTIES
        LABELS "public;integration;item;world;statistics;ownership;reference" TIMEOUT 180)
endif()
