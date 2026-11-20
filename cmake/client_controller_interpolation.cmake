include_guard(GLOBAL)
if(TARGET mc_client_render_global)
    target_sources(mc_client_singleplayer_controller PRIVATE
        src/client/controller/render_interpolation.c)
    target_include_directories(mc_client_singleplayer_controller PRIVATE src src/platform/time)
    if(BUILD_TESTING)
        add_executable(mc_test_controller_interpolation
            tests/client/controller/test_render_interpolation.c)
        target_include_directories(mc_test_controller_interpolation PRIVATE src src/platform/time)
        target_link_libraries(mc_test_controller_interpolation PRIVATE
            mc_client_singleplayer_controller mc_client_gui_ingame mc_client_render_global
            mc_build_options)
        find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
        add_test(NAME client.controller_interpolation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/controller_interpolation/run_native.py"
            --executable "$<TARGET_FILE:mc_test_controller_interpolation>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/controller-interpolation")
        set_tests_properties(client.controller_interpolation PROPERTIES
            LABELS "public;unit;client;ownership;replay" TIMEOUT 60)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_controller_interpolation PROPERTY LINKER_LANGUAGE CXX)
        endif()
    endif()
endif()
