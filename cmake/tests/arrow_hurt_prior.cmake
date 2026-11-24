include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_arrow_hurt_prior tests/world/runtime/test_arrow_hurt_prior.c)
    target_include_directories(mc_test_arrow_hurt_prior PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_hurt_prior PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.arrow_hurt_prior_pending COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_arrow_hurt_prior.py"
        --executable "$<TARGET_FILE:mc_test_arrow_hurt_prior>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-hurt-prior-controls")
    set_tests_properties(world.arrow_hurt_prior_pending PROPERTIES
        LABELS "public;integration;world;entity;ownership" TIMEOUT 300)
endif()
