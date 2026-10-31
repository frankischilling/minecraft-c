add_executable(mc_test_entity_renderer tests/client/world/test_entity_renderer.c)
target_include_directories(mc_test_entity_renderer PRIVATE src/core/java)
target_link_libraries(mc_test_entity_renderer PRIVATE mc_client_entity_renderer mc_build_options)
add_test(NAME client.entity_renderer_constructor COMMAND mc_test_entity_renderer)
set_tests_properties(client.entity_renderer_constructor PROPERTIES
    LABELS "public;unit;client;constructor;ownership" TIMEOUT 90)

add_executable(mc_test_render_manager tests/client/render/test_manager.c)
target_include_directories(mc_test_render_manager PRIVATE src/core/java)
target_link_libraries(mc_test_render_manager PRIVATE
    mc_client_render_manager_cpu mc_resources mc_c_atomics mc_build_options)
if(CMAKE_CXX_COMPILER_LOADED)
    set_property(TARGET mc_test_entity_renderer mc_test_render_manager PROPERTY LINKER_LANGUAGE CXX)
endif()
if(MC_REFERENCE_CLIENT_RESOURCE_DIR)
    set(_mc_manager_resources "${MC_REFERENCE_CLIENT_RESOURCE_DIR}")
else()
    set(_mc_manager_resources "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/startup/fixtures")
endif()
add_test(NAME client.render_manager_constructor COMMAND mc_test_render_manager "${_mc_manager_resources}")
set_tests_properties(client.render_manager_constructor PROPERTIES
    LABELS "public;unit;client;constructor;ownership" TIMEOUT 180)
unset(_mc_manager_resources)

add_executable(mc_test_loading_draw
    tests/client/gui/test_loading_draw.c
    tests/client/gui/test_loading_event_pump.c
    tests/client/gui/test_loading_yield.c)
target_link_libraries(mc_test_loading_draw PRIVATE
    mc_client_source_resources mc_client_application_owner mc_resources mc_c_atomics mc_build_options)
if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
    add_test(NAME client.loading_draw COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_loading_draw.py"
        --executable "$<TARGET_FILE:mc_test_loading_draw>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/loading-draw-controls")
    set_tests_properties(client.loading_draw PROPERTIES
        LABELS "public;integration;client;graphics;ownership" TIMEOUT 3600)
endif()
foreach(_mc_loading_mode IN ITEMS normal optimized)
    set(_mc_loading_flags -B)
    if(_mc_loading_mode STREQUAL "optimized")
        list(APPEND _mc_loading_flags -O)
    endif()
    add_test(NAME tools.loading_draw_protocol_${_mc_loading_mode} COMMAND Python3::Interpreter
        ${_mc_loading_flags}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_loading_draw_protocol.py")
    set_tests_properties(tools.loading_draw_protocol_${_mc_loading_mode} PROPERTIES
        LABELS "public;unit;tools;client" TIMEOUT 60)
endforeach()
unset(_mc_loading_mode)
unset(_mc_loading_flags)

if(TARGET mc_test_application_startup)
    add_test(NAME client.entity_renderer_app_null_fields COMMAND mc_test_application_startup
        "${_mc_startup_resource_root}" entity-renderer-cpu)
    set_tests_properties(client.entity_renderer_app_null_fields PROPERTIES
        LABELS "public;unit;client;constructor;ownership" TIMEOUT 180)
    if(WIN32)
        target_link_libraries(mc_test_application_startup PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(mc_test_application_startup PRIVATE OpenGL::GL)
    endif()
    if(MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS)
        foreach(_mc_entity_mode IN ITEMS entity-map-array-limit entity-map-image-limit)
            add_test(NAME client.application_startup_${_mc_entity_mode}
                COMMAND mc_test_application_startup "${_mc_startup_resource_root}"
                    "window-${_mc_entity_mode}" "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}")
            set_tests_properties(client.application_startup_${_mc_entity_mode} PROPERTIES
                LABELS "integration;client;graphics;startup;ownership" TIMEOUT 180)
        endforeach()
        unset(_mc_entity_mode)
    endif()
endif()

# Direct private callee over the genuine prefix; no skipped startup calls.
if(BUILD_TESTING AND TARGET mc_client_application_startup AND TARGET mc_sound_backend AND
   TARGET mc_client_assets AND TARGET SDL3::SDL3-static)
    add_executable(mc_test_application_logo_prepared
        tests/client/startup/test_application_logo_prepared.c)
    target_include_directories(mc_test_application_logo_prepared PRIVATE src src/core/java)
    target_link_libraries(mc_test_application_logo_prepared PRIVATE
        mc_client_application_startup mc_sound_backend mc_client_assets mc_resources
        SDL3::SDL3-static mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_application_logo_prepared PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32)
        target_link_libraries(mc_test_application_logo_prepared PRIVATE opengl32)
        add_custom_command(TARGET mc_test_application_logo_prepared POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:mc_test_application_logo_prepared>")
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(mc_test_application_logo_prepared PRIVATE OpenGL::GL)
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS AND
       NOT "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}" STREQUAL "")
        add_test(NAME client.application_logo_prepared_callee
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_application_logo_prepared.py"
                --executable "$<TARGET_FILE:mc_test_application_logo_prepared>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/logo-prepared-controls")
        set_tests_properties(client.application_logo_prepared_callee PROPERTIES
            LABELS "local;integration;client;graphics;prepared-callee;ownership" TIMEOUT 1200)
    endif()
    if(MSVC)
        target_link_options(mc_test_application_logo_prepared PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_logo_prepared.map")
    else()
        target_link_options(mc_test_application_logo_prepared PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_logo_prepared.map")
    endif()
endif()

if(BUILD_TESTING AND TARGET mc_test_application_startup)
    foreach(_mc_tail_mode IN ITEMS statistics-admission statistics-null-z)
        string(REPLACE "-" "_" _mc_tail_name "${_mc_tail_mode}")
        add_test(NAME "client.application_startup_${_mc_tail_name}"
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_application_startup_tail.py"
                --executable "$<TARGET_FILE:mc_test_application_startup>"
                --resources "${_mc_startup_resource_root}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/startup-tail-${_mc_tail_mode}"
                --mode "${_mc_tail_mode}")
        set_tests_properties("client.application_startup_${_mc_tail_name}" PROPERTIES
            LABELS "public;integration;client;startup;statistics;ownership" TIMEOUT 240)
    endforeach()
    if(MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS)
        foreach(_mc_tail_mode IN ITEMS window-session window-statistics-foreign-ready)
            string(REGEX REPLACE "^window-" "" _mc_tail_name "${_mc_tail_mode}")
            string(REPLACE "-" "_" _mc_tail_name "${_mc_tail_name}")
            add_test(NAME "client.application_startup_${_mc_tail_name}"
                COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_application_startup_tail.py"
                    --executable "$<TARGET_FILE:mc_test_application_startup>"
                    --resources "${_mc_startup_resource_root}"
                    --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/startup-tail-${_mc_tail_mode}"
                    --mode "${_mc_tail_mode}")
            set_tests_properties("client.application_startup_${_mc_tail_name}" PROPERTIES
                LABELS "integration;client;startup;statistics;input;window;ownership" TIMEOUT 240)
        endforeach()
    endif()
    unset(_mc_tail_mode)
    unset(_mc_tail_name)
endif()
