include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_world_notification_execution
        tests/world/runtime/world_access/test_notification_execution.c)
    target_include_directories(mc_test_world_notification_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_notification_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    if(Python3_Interpreter_FOUND)
        foreach(_mc_notification_mode IN ITEMS lease-cell lease-range live-cell live-range
                equal-range empty-cell empty-range admission nodes capture-refusal
                native-stop-cell native-stop-range scope-drift)
            add_test(NAME "world.notification_execution_${_mc_notification_mode}" COMMAND
                Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_notification_execution.py"
                --mode "${_mc_notification_mode}"
                --executable "$<TARGET_FILE:mc_test_world_notification_execution>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-notification-controls")
            set_tests_properties("world.notification_execution_${_mc_notification_mode}" PROPERTIES
                LABELS "public;integration;world;storage;ownership;filesystem"
                RESOURCE_LOCK world_notification_execution TIMEOUT 240)
        endforeach()
        unset(_mc_notification_mode)
    endif()
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(_mc_notification_python_mode IN ITEMS normal optimized)
        set(_mc_notification_python_flags -B)
        if(_mc_notification_python_mode STREQUAL "optimized")
            list(APPEND _mc_notification_python_flags -O)
        endif()
        add_test(NAME "tools.world_notification_execution_${_mc_notification_python_mode}" COMMAND
            Python3::Interpreter ${_mc_notification_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_notification_execution.py" --self-test)
        set_tests_properties("tools.world_notification_execution_${_mc_notification_python_mode}" PROPERTIES
            LABELS "public;unit;tools;world;ownership" TIMEOUT 90)
    endforeach()
    unset(_mc_notification_python_flags)
    unset(_mc_notification_python_mode)
endif()
