include_guard(GLOBAL)

target_sources(mc_simulation PRIVATE
    src/entity/living/fields.c
    src/entity/mob/damage.c
    src/entity/mob/references.c
    src/entity/mob/tick/common.c
    src/entity/mob/tick/helpers.c
    src/entity/mob/tick/ray.c
    src/entity/mob/tick/behavior.c
    src/entity/mob/tick/creeper.c
    src/entity/mob/tick/monster_attack.c
    src/entity/mob/tick/path.c
    src/entity/mob/tick/path_follow.c
    src/entity/mob/tick/tick.c
    src/entity/mob/tick/living.c
    src/entity/mob/tick/slime_squid.c
    src/entity/mob/tick/wolf.c
    src/entity/mob/tick/wolf_setter.c
    src/entity/mob/tick/wrappers.c)

if(BUILD_TESTING)
    add_executable(mc_test_zombie_null_damage tests/entity/test_zombie_null_damage.c)
    target_include_directories(mc_test_zombie_null_damage PRIVATE src src/core/java)
    target_link_libraries(mc_test_zombie_null_damage PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.zombie_null_damage COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_zombie_null_damage.py"
        --executable $<TARGET_FILE:mc_test_zombie_null_damage>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/zombie-null-damage-contracts")
    set_tests_properties(entity.zombie_null_damage PROPERTIES
        LABELS "public;integration;entity;ownership;replay" TIMEOUT 300)

    add_executable(mc_test_wolf_interest tests/entity/test_wolf_interest.c)
    target_include_directories(mc_test_wolf_interest PRIVATE src src/core/java)
    target_link_libraries(mc_test_wolf_interest PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.wolf_interest COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_wolf_interest.py"
        --executable $<TARGET_FILE:mc_test_wolf_interest>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/wolf-interest-contracts")
    set_tests_properties(entity.wolf_interest PROPERTIES
        LABELS "public;integration;entity;ownership" TIMEOUT 240)

    add_executable(mc_test_mob_tick tests/entity/test_mob_tick.c)
    target_include_directories(mc_test_mob_tick PRIVATE src src/core/java)
    target_link_libraries(mc_test_mob_tick PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.mob_tick COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_tick.py"
        --executable $<TARGET_FILE:mc_test_mob_tick>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/entity-mob-tick-contracts")
    set_tests_properties(entity.mob_tick PROPERTIES
        LABELS "public;unit;entity;ownership" TIMEOUT 420)
    add_executable(mc_test_creeper_attack tests/entity/test_creeper_attack.c)
    target_include_directories(mc_test_creeper_attack PRIVATE src src/core/java)
    target_link_libraries(mc_test_creeper_attack PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.creeper_attack COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_creeper_attack.py"
        --executable $<TARGET_FILE:mc_test_creeper_attack>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/creeper-attack-contracts")
    set_tests_properties(entity.creeper_attack PROPERTIES
        LABELS "public;integration;entity;ownership;replay" TIMEOUT 180)
    add_executable(mc_test_monster_attack tests/entity/test_monster_attack.c)
    target_include_directories(mc_test_monster_attack PRIVATE src src/core/java)
    target_link_libraries(mc_test_monster_attack PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.monster_attack COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_monster_attack.py"
        --executable $<TARGET_FILE:mc_test_monster_attack>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/monster-attack-contracts")
    set_tests_properties(entity.monster_attack PROPERTIES
        LABELS "public;integration;entity;ownership;replay" TIMEOUT 180)
endif()
