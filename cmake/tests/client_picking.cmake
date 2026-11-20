if(BUILD_TESTING AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_client_picking tests/client/world/test_picking.c)
    target_include_directories(mc_test_client_picking PRIVATE src src/platform/time)
    target_link_libraries(mc_test_client_picking PRIVATE
        mc_client_entity_renderer mc_client_singleplayer_controller mc_simulation
        mc_resources mc_math_classes mc_c_atomics mc_build_options)
    add_test(NAME client.picking COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_picking.py"
        --executable "$<TARGET_FILE:mc_test_client_picking>"
        --observations "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/picking/finite.json"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/picking-contracts")
    set_tests_properties(client.picking PROPERTIES
        LABELS "public;integration;client;entity;world;ownership" TIMEOUT 180)
endif()
