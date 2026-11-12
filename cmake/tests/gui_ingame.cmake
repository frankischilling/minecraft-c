if(BUILD_TESTING AND TARGET mc_client_gui_ingame)
    add_executable(mc_test_gui_ingame tests/client/gui/test_gui_ingame.c)
    target_include_directories(mc_test_gui_ingame PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_gui_ingame PRIVATE mc_client_gui_ingame mc_c_atomics mc_build_options)
    add_test(NAME client.gui_ingame_constructor COMMAND mc_test_gui_ingame)
    set_tests_properties(client.gui_ingame_constructor PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 180)
endif()
