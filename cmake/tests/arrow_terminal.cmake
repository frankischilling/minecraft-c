include_guard(GLOBAL)

if(BUILD_TESTING)
    add_executable(mc_test_arrow_terminal tests/world/runtime/test_arrow_terminal.c)
    target_include_directories(mc_test_arrow_terminal PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_terminal PRIVATE
        mc_simulation mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.arrow_terminal COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_arrow_terminal.py"
        --executable "$<TARGET_FILE:mc_test_arrow_terminal>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-terminal-controls")
    set_tests_properties(world.arrow_terminal PROPERTIES
        LABELS "public;integration;entity;world;ownership;terminal" TIMEOUT 120)
endif()
