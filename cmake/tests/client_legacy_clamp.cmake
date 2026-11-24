include_guard(GLOBAL)
if(BUILD_TESTING AND TARGET mc_test_texture_lookup)
    add_executable(mc_test_legacy_clamp tests/client/display/test_legacy_clamp.c)
    target_include_directories(mc_test_legacy_clamp PRIVATE src src/client src/platform/time)
    get_target_property(_mc_clamp_links mc_test_texture_lookup LINK_LIBRARIES)
    target_link_libraries(mc_test_legacy_clamp PRIVATE ${_mc_clamp_links})
    unset(_mc_clamp_links)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_legacy_clamp PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.legacy_clamp COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_legacy_clamp.py"
            --executable "$<TARGET_FILE:mc_test_legacy_clamp>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/legacy-clamp-contracts")
        set_tests_properties(client.legacy_clamp PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 300)
    endif()
endif()
