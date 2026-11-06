include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.torch_world_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/torch_world" -p test_packet.py)
    set_tests_properties(tools.torch_world_packet PROPERTIES
        LABELS "public;unit;tools;client;world;graphics;reference" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_torch_world tests/client/render/test_torch_world.c)
    target_include_directories(mc_test_torch_world PRIVATE src src/platform/time)
    target_link_libraries(mc_test_torch_world PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_torch_world PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.torch_world COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/torch_world/run_native.py"
        --executable "$<TARGET_FILE:mc_test_torch_world>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/torch-world-controls")
    set_tests_properties(client.torch_world PROPERTIES
        LABELS "public;integration;client;world;graphics;ownership;reference;replay" TIMEOUT 180)
endif()
