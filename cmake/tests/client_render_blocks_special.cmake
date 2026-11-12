include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_crossed_plant tests/client/render/test_crossed_plant.c)
    target_include_directories(mc_test_crossed_plant PRIVATE src src/platform/time)
    target_link_libraries(mc_test_crossed_plant PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_crossed_plant PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.crossed_plant COMMAND mc_test_crossed_plant)
    set_tests_properties(client.crossed_plant PROPERTIES
        LABELS "public;unit;client;world;ownership" TIMEOUT 180)
    add_test(NAME client.crossed_plant_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_blocks_special/run_native.py"
        --executable "$<TARGET_FILE:mc_test_crossed_plant>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/crossed-plant-reference")
    set_tests_properties(client.crossed_plant_reference PROPERTIES
        LABELS "public;integration;client;world;ownership;replay" TIMEOUT 180)
    add_test(NAME tools.crossed_plant_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_blocks_special/test_packet.py"
        PlantPacket.test_unchanged_baseline
        PlantPacket.test_row_identity_and_extent_mutations
        PlantPacket.test_semantic_words_and_retained_fields_are_compared
        PlantPacket.test_capture_input_identity_mutations
        PlantPacket.test_duplicate_metadata_rejected
        PlantPacket.test_numeric_boolean_identity_is_distinct)
    set_tests_properties(tools.crossed_plant_packet PROPERTIES
        LABELS "public;unit;tools;client;replay" TIMEOUT 60)
    add_executable(mc_test_fluid tests/client/render/test_fluid.c)
    target_include_directories(mc_test_fluid PRIVATE src src/platform/time)
    target_link_libraries(mc_test_fluid PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_fluid PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.fluid COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_fluid.py"
        --executable "$<TARGET_FILE:mc_test_fluid>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/fluid-controls")
    set_tests_properties(client.fluid PROPERTIES
        LABELS "public;integration;client;world;ownership" TIMEOUT 180)
endif()
