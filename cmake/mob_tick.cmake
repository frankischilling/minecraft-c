include_guard(GLOBAL)

target_sources(mc_simulation PRIVATE
    src/entity/living/fields.c
    src/entity/mob/references.c
    src/entity/mob/tick/common.c
    src/entity/mob/tick/helpers.c
    src/entity/mob/tick/ray.c
    src/entity/mob/tick/behavior.c
    src/entity/mob/tick/path.c
    src/entity/mob/tick/path_follow.c
    src/entity/mob/tick/tick.c
    src/entity/mob/tick/living.c
    src/entity/mob/tick/slime_squid.c
    src/entity/mob/tick/wolf.c
    src/entity/mob/tick/wrappers.c)

if(BUILD_TESTING)
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
endif()
