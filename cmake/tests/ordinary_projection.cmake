include_guard(GLOBAL)
if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_ordinary_projection tests/world/runtime/test_ordinary_projection.c)
    target_include_directories(mc_test_ordinary_projection PRIVATE src)
    target_link_libraries(mc_test_ordinary_projection PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.ordinary_projection COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_ordinary_projection.py"
        --executable "$<TARGET_FILE:mc_test_ordinary_projection>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/ordinary-projection-controls")
    set_tests_properties(world.ordinary_projection PROPERTIES
        LABELS "public;integration;world;storage;ownership" TIMEOUT 600)
endif()
