include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_monster_death tests/entity/mob/test_monster_death.c)
    target_include_directories(mc_test_monster_death PRIVATE src src/core/java)
    target_link_libraries(mc_test_monster_death PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.monster_death COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_monster_death.py"
        --executable "$<TARGET_FILE:mc_test_monster_death>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/monster-death-controls")
    set_tests_properties(entity.monster_death PROPERTIES
        LABELS "public;integration;entity;world;ownership;math" TIMEOUT 180)
endif()
