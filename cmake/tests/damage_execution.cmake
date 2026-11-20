include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_damage_execution
        tests/world/runtime/world_access/test_damage_execution.c)
    target_include_directories(mc_test_damage_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_damage_execution PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    foreach(_mc_damage_mode IN ITEMS sound-receipt unrelated-death unknown-receipt)
    add_test(NAME "entity.damage_execution_${_mc_damage_mode}"
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_damage_execution.py"
            --mode "${_mc_damage_mode}" --executable "$<TARGET_FILE:mc_test_damage_execution>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/damage-execution-controls")
    set_tests_properties("entity.damage_execution_${_mc_damage_mode}" PROPERTIES
        LABELS "public;integration;entity;world;ownership;damage"
        RESOURCE_LOCK world_query_controls TIMEOUT 240)
    endforeach()
    add_executable(mc_test_damage_execution_profiles
        tests/world/runtime/world_access/test_damage_execution_profiles.c)
    target_include_directories(mc_test_damage_execution_profiles PRIVATE src src/platform/time)
    target_link_libraries(mc_test_damage_execution_profiles PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    foreach(_mc_damage_mode IN ITEMS victims early math-missing native-stop older-math-no-in
            sound-null-list sound-scope-drift sound-scope-drift-bare wolf-query
            wolf-query-math wolf-owner player-live player-wake player-fatal player-fatal-math
            player-statistic-null
            mob-fatal pending admission)
        add_test(NAME "entity.damage_execution_${_mc_damage_mode}"
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_damage_execution.py"
                --mode "${_mc_damage_mode}" --executable "$<TARGET_FILE:mc_test_damage_execution_profiles>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/damage-execution-controls")
        set_tests_properties("entity.damage_execution_${_mc_damage_mode}" PROPERTIES
            LABELS "public;integration;entity;world;ownership;damage"
            RESOURCE_LOCK world_query_controls TIMEOUT 240)
    endforeach()
    unset(_mc_damage_mode)
endif()
