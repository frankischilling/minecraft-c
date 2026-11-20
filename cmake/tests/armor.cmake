include_guard(GLOBAL)

if(BUILD_TESTING)
    add_executable(mc_test_armor tests/inventory/player/test_armor.c)
    target_include_directories(mc_test_armor PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_armor PRIVATE mc_simulation mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME inventory.armor COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/inventory/test_armor.py"
        --executable "$<TARGET_FILE:mc_test_armor>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/armor-controls")
    set_tests_properties(inventory.armor PROPERTIES
        LABELS "public;integration;inventory;entity;ownership" TIMEOUT 180)
endif()
