# Include in the existing BUILD_TESTING client block, after client_controllers.
include_guard(GLOBAL)
if(NOT BUILD_TESTING OR NOT MC_BUILD_CLIENT)
    return()
endif()
if(NOT TARGET mc_client_sdk_runtime OR NOT _mc_controller_config_path)
    message(FATAL_ERROR "Controllers test wiring requires the production module first")
endif()
add_executable(mc_test_sdk_controllers_cold
    tests/client/display/test_sdk_controllers_cold.c)
add_executable(mc_test_sdk_controllers_window
    tests/client/display/test_sdk_controllers_window.c)
foreach(_mc_controller_test IN ITEMS mc_test_sdk_controllers_cold mc_test_sdk_controllers_window)
    target_include_directories(${_mc_controller_test} PRIVATE src "${_mc_controller_config_path}")
    target_link_libraries(${_mc_controller_test} PRIVATE
        mc_client_sdk_runtime mc_client_display mc_resources mc_build_options)
    # Tests include the exact production native TU; window additionally includes
    # the exact canonical Controllers TU. A later qualification must prove no
    # archive copy of either included body was extracted into these products.
    if(MSVC)
        target_link_options(${_mc_controller_test} PRIVATE
            "/MAP:$<TARGET_FILE_DIR:${_mc_controller_test}>/${_mc_controller_test}.map")
    else()
        target_link_options(${_mc_controller_test} PRIVATE
            "-Wl,-Map,$<TARGET_FILE_DIR:${_mc_controller_test}>/${_mc_controller_test}.map")
    endif()
endforeach()
add_test(NAME client.controllers_cold COMMAND mc_test_sdk_controllers_cold)
set_tests_properties(client.controllers_cold PROPERTIES
    LABELS "public;unit;client;ownership" TIMEOUT 60)
option(MC_ENABLE_CONTROLLER_DEVICE_TESTS
    "Run real SDL Controllers discovery with actual imported SDK icon files" OFF)
if(MC_ENABLE_CONTROLLER_DEVICE_TESTS)
    if(NOT MC_ENABLE_CLIENT_DISPLAY_TESTS)
        message(FATAL_ERROR "Controllers device tests require actual desktop Display tests")
    endif()
    if(NOT EXISTS "${MC_CLIENT_ASSET_IMPORT_DIR}/sdk/lwjgl-icon-16.rgba" OR
       NOT EXISTS "${MC_CLIENT_ASSET_IMPORT_DIR}/sdk/lwjgl-icon-32.rgba")
        message(FATAL_ERROR "Controllers device tests require the actual SDK icon import root")
    endif()
    add_test(NAME client.controllers_discovery COMMAND mc_test_sdk_controllers_window
        "${MC_CLIENT_ASSET_IMPORT_DIR}")
    add_test(NAME client.controllers_virtual_frontier COMMAND mc_test_sdk_controllers_window
        "${MC_CLIENT_ASSET_IMPORT_DIR}" --virtual)
    add_test(NAME client.controllers_enumeration_failure COMMAND mc_test_sdk_controllers_window
        "${MC_CLIENT_ASSET_IMPORT_DIR}" --enumeration-failure)
    add_test(NAME client.controllers_retire_retry COMMAND mc_test_sdk_controllers_window
        "${MC_CLIENT_ASSET_IMPORT_DIR}" --retire-failure)
    set_tests_properties(client.controllers_discovery client.controllers_virtual_frontier
        client.controllers_enumeration_failure client.controllers_retire_retry PROPERTIES
        LABELS "local;integration;client;graphics;ownership" RUN_SERIAL TRUE TIMEOUT 240)
endif()
