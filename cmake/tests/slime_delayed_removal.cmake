include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_slime_delayed_removal tests/world/runtime/test_slime_delayed_removal.c)
    target_include_directories(mc_test_slime_delayed_removal PRIVATE src src/platform/time)
    target_link_libraries(mc_test_slime_delayed_removal PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.slime_delayed_removal COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_slime_delayed_removal.py"
        --executable "$<TARGET_FILE:mc_test_slime_delayed_removal>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/slime-delayed-removal-controls")
    set_tests_properties(entity.slime_delayed_removal PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 300)
endif()
