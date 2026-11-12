if(BUILD_TESTING AND MC_BUILD_CLIENT AND TARGET mc_client_cx)
    # Include the exact CPU owner once for the arranged private flag/monitor
    # controls. Map proof must show cx.c archive body is not extracted again.
    add_executable(mc_test_cx tests/client/render/test_cx.c)
    target_include_directories(mc_test_cx PRIVATE include src)
    target_link_libraries(mc_test_cx PRIVATE mc_client_cx Threads::Threads mc_build_options)
    # Include the exact Display once; real CPU/device cx archives stay canonical.
    add_executable(mc_test_cx_device tests/client/render/test_cx_device.c)
    target_include_directories(mc_test_cx_device PRIVATE include src src/client/display)
    target_link_libraries(mc_test_cx_device PRIVATE mc_client_cx mc_core mc_image
        SDL3::SDL3-static mc_client_event_transport mc_client_sdk_runtime
        Threads::Threads mc_build_options)
    if(UNIX)
        target_link_libraries(mc_test_cx_device PRIVATE m)
    endif()
    foreach(_mc_cx_target IN ITEMS mc_test_cx mc_test_cx_device)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET ${_mc_cx_target} PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MSVC)
            target_link_options(${_mc_cx_target} PRIVATE
                "/MAP:${CMAKE_CURRENT_BINARY_DIR}/${_mc_cx_target}.map")
        elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_link_options(${_mc_cx_target} PRIVATE
                "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${_mc_cx_target}.map")
        endif()
    endforeach()
    unset(_mc_cx_target)
    add_test(NAME client.cx_constructor COMMAND mc_test_cx)
    set_tests_properties(client.cx_constructor PROPERTIES LABELS "unit;client;ownership" TIMEOUT 60)
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.cx_capability_device COMMAND mc_test_cx_device)
        set_tests_properties(client.cx_capability_device PROPERTIES
            LABELS "integration;client;graphics;ownership" TIMEOUT 180)
    endif()
endif()
