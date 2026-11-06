if(BUILD_TESTING AND TARGET mc_game_settings)
    add_executable(mc_test_game_settings_source_save
        tests/client/settings/test_source_save.c)
    target_link_libraries(mc_test_game_settings_source_save PRIVATE
        mc_game_settings mc_build_options)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/typed-settings-save")
    add_test(NAME client.game_settings_source_save
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_game_settings_source_save.py"
            --executable "$<TARGET_FILE:mc_test_game_settings_source_save>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/typed-settings-save")
    set_tests_properties(client.game_settings_source_save PROPERTIES
        LABELS "public;unit;client;settings;ownership;io" TIMEOUT 90)
endif()
