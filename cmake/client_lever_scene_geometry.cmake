include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.lever_scene_geometry_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lever_scene_geometry" -p test_packet.py)
    set_tests_properties(tools.lever_scene_geometry_packet PROPERTIES
        LABELS "public;unit;tools;client;world;graphics;reference" TIMEOUT 60)
    add_test(NAME tools.lever_geometry_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_lever_geometry.py")
    set_tests_properties(tools.lever_geometry_controls PROPERTIES
        LABELS "public;unit;tools;client;world;graphics;reference" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_lever_scene_geometry tests/client/render/test_lever_scene_geometry.c)
    target_include_directories(mc_test_lever_scene_geometry PRIVATE src src/platform/time)
    target_link_libraries(mc_test_lever_scene_geometry PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_lever_scene_geometry PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.lever_scene_geometry COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lever_scene_geometry/run_native.py"
        --executable "$<TARGET_FILE:mc_test_lever_scene_geometry>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lever-scene-geometry")
    set_tests_properties(client.lever_scene_geometry PROPERTIES
        LABELS "public;integration;client;world;graphics;reference;replay" TIMEOUT 120)
endif()
