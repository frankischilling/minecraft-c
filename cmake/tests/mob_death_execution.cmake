include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_mob_death_execution tests/entity/mob/test_death_execution.c)
    target_include_directories(mc_test_mob_death_execution PRIVATE src src/core/java)
    target_link_libraries(mc_test_mob_death_execution PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_executable(mc_test_mob_death_execution_profiles
        tests/entity/mob/test_death_execution_profiles.c)
    target_include_directories(mc_test_mob_death_execution_profiles PRIVATE src src/core/java)
    target_link_libraries(mc_test_mob_death_execution_profiles PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.mob_death_execution_math-missing COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_death_execution.py"
        --mode math-missing --executable "$<TARGET_FILE:mc_test_mob_death_execution>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mob-death-execution-controls")
    set_tests_properties(entity.mob_death_execution_math-missing PROPERTIES
        LABELS "public;integration;entity;world;ownership;math" TIMEOUT 180)
    foreach(_mc_death_mode IN ITEMS profiles remote callback-1 callback-2 callback-3 callback-4
            native-stop admission)
        add_test(NAME "entity.mob_death_execution_${_mc_death_mode}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_death_execution.py"
            --mode "${_mc_death_mode}" --executable "$<TARGET_FILE:mc_test_mob_death_execution_profiles>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mob-death-execution-controls")
        set_tests_properties("entity.mob_death_execution_${_mc_death_mode}" PROPERTIES
            LABELS "public;integration;entity;world;ownership;math" TIMEOUT 180)
    endforeach()
    unset(_mc_death_mode)
endif()
