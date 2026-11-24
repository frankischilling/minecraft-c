include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_damage_native_close_execution
        tests/world/runtime/world_access/test_damage_close_execution.c)
    target_include_directories(mc_test_damage_native_close_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_damage_native_close_execution PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    foreach(_mc_damage_close_mode IN ITEMS sound-pop sound-capture ally-success ally-unreached)
        add_test(NAME "entity.damage_native_close_${_mc_damage_close_mode}"
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_damage_native_close.py"
                --mode "${_mc_damage_close_mode}"
                --executable "$<TARGET_FILE:mc_test_damage_native_close_execution>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/damage-native-close-controls")
        set_tests_properties("entity.damage_native_close_${_mc_damage_close_mode}" PROPERTIES
            LABELS "public;integration;entity;world;ownership;damage"
            RESOURCE_LOCK world_query_controls TIMEOUT 240)
    endforeach()
    unset(_mc_damage_close_mode)
    foreach(_mc_damage_close_optimized IN ITEMS normal optimized)
        if(_mc_damage_close_optimized STREQUAL optimized)
            set(_mc_damage_close_python_flags -B -O)
        else()
            set(_mc_damage_close_python_flags -B)
        endif()
        add_test(NAME "entity.damage_native_close_protocol_${_mc_damage_close_optimized}"
            COMMAND Python3::Interpreter ${_mc_damage_close_python_flags}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_damage_native_close.py"
                --self-test)
        set_tests_properties("entity.damage_native_close_protocol_${_mc_damage_close_optimized}"
            PROPERTIES LABELS "public;unit;entity;protocol" TIMEOUT 30)
    endforeach()
    unset(_mc_damage_close_optimized)
    unset(_mc_damage_close_python_flags)
endif()
