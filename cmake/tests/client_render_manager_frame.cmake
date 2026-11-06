if(BUILD_TESTING AND TARGET mc_client_render_manager_cpu)
    if(MC_REFERENCE_CLIENT_RESOURCE_DIR)
        set(_mc_manager_frame_resources "${MC_REFERENCE_CLIENT_RESOURCE_DIR}")
    else()
        set(_mc_manager_frame_resources "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/startup/fixtures")
    endif()
    add_executable(mc_test_render_manager_frame tests/client/render/test_manager_frame.c)
    add_executable(mc_test_render_manager_sleeping tests/client/render/test_manager_sleeping.c)
    foreach(_mc_manager_frame_target IN ITEMS
            mc_test_render_manager_frame mc_test_render_manager_sleeping)
        target_include_directories(${_mc_manager_frame_target} PRIVATE src src/core/java)
        target_link_libraries(${_mc_manager_frame_target} PRIVATE
            mc_client_render_manager_cpu mc_resources mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET ${_mc_manager_frame_target} PROPERTY LINKER_LANGUAGE CXX)
        endif()
    endforeach()
    add_test(NAME client.render_manager_frame COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_manager_frame/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_manager_frame>"
        --resource-root "${_mc_manager_frame_resources}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-manager-frame")
    set_tests_properties(client.render_manager_frame PROPERTIES
        LABELS "public;unit;client;ownership;replay" TIMEOUT 540)
    add_test(NAME client.render_manager_sleeping COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_manager_frame/run_sleeping.py"
        --executable "$<TARGET_FILE:mc_test_render_manager_sleeping>"
        --resource-root "${_mc_manager_frame_resources}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-manager-sleeping")
    set_tests_properties(client.render_manager_sleeping PROPERTIES
        LABELS "public;integration;client;ownership;replay;storage" TIMEOUT 180)
    unset(_mc_manager_frame_resources)
endif()
