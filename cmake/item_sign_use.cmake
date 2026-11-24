if(BUILD_TESTING AND TARGET mc_item_use)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_executable(mc_test_item_sign_use tests/item/use/sign_controls.c)
    target_include_directories(mc_test_item_sign_use PRIVATE src src/platform/time)
    target_link_libraries(mc_test_item_sign_use PRIVATE mc_item_use mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME item.sign_use COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_sign_use.py"
        --executable "$<TARGET_FILE:mc_test_item_sign_use>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-sign-use")
    set_tests_properties(item.sign_use PROPERTIES
        LABELS "public;integration;item;world;ownership" TIMEOUT 240)
endif()
