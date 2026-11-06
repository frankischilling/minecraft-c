if(BUILD_TESTING)
    add_executable(mc_test_world_raytrace tests/world/runtime/collision/test_raytrace.c)
    target_link_libraries(mc_test_world_raytrace PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.raytrace COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/run_raytrace.py"
        --executable "$<TARGET_FILE:mc_test_world_raytrace>")
    set_tests_properties(world.raytrace PROPERTIES
        LABELS "public;integration;ownership;collision;filesystem;replay" TIMEOUT 180)
endif()
