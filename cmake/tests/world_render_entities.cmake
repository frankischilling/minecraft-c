if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_world_render_entities
        tests/world/runtime/render_entities/test_render_entities.c)
    target_link_libraries(mc_test_world_render_entities PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.render_entities COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/world/runtime/render_entities/run_native.py"
        --executable "$<TARGET_FILE:mc_test_world_render_entities>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-render-entities-contracts")
    set_tests_properties(world.render_entities PROPERTIES
        LABELS "public;integration;world;entity;ownership" TIMEOUT 120)
endif()
