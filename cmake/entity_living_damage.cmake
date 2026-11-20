include_guard(GLOBAL)

if(BUILD_TESTING)
    add_executable(mc_test_living_damage tests/entity/mob/test_living_damage.c)
    target_include_directories(mc_test_living_damage PRIVATE src src/core/java)
    target_link_libraries(mc_test_living_damage PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.living_damage COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_living_damage.py"
        --executable "$<TARGET_FILE:mc_test_living_damage>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/living-damage-controls")
    set_tests_properties(entity.living_damage PROPERTIES
        LABELS "public;integration;entity;world;ownership;math" TIMEOUT 180)
    add_test(NAME entity.living_damage_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/living_damage/run_native.py"
        --executable "$<TARGET_FILE:mc_test_living_damage>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/living-damage-reference")
    set_tests_properties(entity.living_damage_reference PROPERTIES
        LABELS "public;integration;entity;ownership;replay" TIMEOUT 180)
    add_test(NAME tools.living_damage_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/living_damage/test_packet.py"
        LivingPacket.test_unchanged_baseline_and_explicit_scope_pairs
        LivingPacket.test_count_identity_schema_and_type_mutations
        LivingPacket.test_exact_primitive_and_random_comparison
        LivingPacket.test_sound_and_owner_mutations
        LivingPacket.test_death_checkpoint_and_distinct_failure_categories
        LivingPacket.test_capture_pins_duplicate_keys_and_boolean_coercion)
    set_tests_properties(tools.living_damage_packet PROPERTIES
        LABELS "public;unit;tools;entity;replay" TIMEOUT 60)
endif()
