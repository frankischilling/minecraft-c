include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/entity/player/mouse_filter.c)
if(BUILD_TESTING)
    add_executable(mc_test_mouse_filter tests/client/input/test_mouse_filter.c)
    target_link_libraries(mc_test_mouse_filter PRIVATE mc_simulation mc_build_options)
    add_test(NAME client.mouse_filter COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/mouse_filter/run_native.py"
        --executable "$<TARGET_FILE:mc_test_mouse_filter>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mouse-filter")
    set_tests_properties(client.mouse_filter PROPERTIES
        LABELS "public;unit;client;input;replay" TIMEOUT 60)
endif()
