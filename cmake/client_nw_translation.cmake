include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_client_render_textures AND Python3_Interpreter_FOUND)
    add_executable(mc_test_nw_translation tests/client/render_engine/test_nw_translation.c)
    target_include_directories(mc_test_nw_translation PRIVATE src)
    target_link_libraries(mc_test_nw_translation PRIVATE
        mc_client_render_textures mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_nw_translation PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.nw_translation COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/nw_translation/run_native.py"
        --executable "$<TARGET_FILE:mc_test_nw_translation>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/nw-translation-controls")
    set_tests_properties(client.nw_translation PROPERTIES
        LABELS "public;unit;client;graphics;ownership;reference;replay" TIMEOUT 60)
    add_test(NAME tools.nw_translation_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/nw_translation" -p test_packet.py)
    set_tests_properties(tools.nw_translation_packet PROPERTIES
        LABELS "public;unit;tools;client;graphics;reference" TIMEOUT 60)
endif()
