include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_entity_drop_execution tests/entity/drop/test_execution.c)
    target_include_directories(mc_test_entity_drop_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_entity_drop_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    if(Python3_Interpreter_FOUND)
        foreach(_mc_drop_mode IN ITEMS math-missing math-limit success absent nullable
                callback-failure callback-capture native-close preflight capture-capacity)
        add_test(NAME "entity.drop_execution_${_mc_drop_mode}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_drop_execution.py"
            --mode "${_mc_drop_mode}" --executable "$<TARGET_FILE:mc_test_entity_drop_execution>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/entity-drop-execution-controls")
        set_tests_properties("entity.drop_execution_${_mc_drop_mode}" PROPERTIES
            LABELS "public;integration;entity;world;ownership" TIMEOUT 420)
        endforeach()
        unset(_mc_drop_mode)
    endif()
endif()
