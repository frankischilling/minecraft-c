# source_input.inc is included once by the canonical game_settings.c owner.
# It is not a separate translation unit and introduces no Settings layout copy.
include_guard(GLOBAL)
if(TARGET mc_inventory)
    target_sources(mc_inventory PRIVATE src/inventory/player_selection.c)
endif()

if(BUILD_TESTING)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_executable(mc_test_client_input_fields tests/client/settings/test_input_fields.c)
    target_include_directories(mc_test_client_input_fields PRIVATE src/client/settings)
    target_link_libraries(mc_test_client_input_fields PRIVATE
        mc_inventory mc_game_settings mc_build_options)
    add_test(NAME client.input_fields
        COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/input_fields/run_native.py"
            --executable "$<TARGET_FILE:mc_test_client_input_fields>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/input-fields")
    set_tests_properties(client.input_fields PROPERTIES
        LABELS "public;unit;client;inventory;replay" TIMEOUT 60)
endif()
