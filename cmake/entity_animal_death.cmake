include_guard(GLOBAL)

if(TARGET mc_simulation)
    target_sources(mc_simulation PRIVATE src/entity/drop/drop.c src/entity/mob/death.c)
endif()

if(BUILD_TESTING)
    add_executable(mc_test_entity_drop tests/entity/drop/test_drop.c)
    target_include_directories(mc_test_entity_drop PRIVATE src src/core/java)
    target_link_libraries(mc_test_entity_drop PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.drop COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_drop.py"
        --executable "$<TARGET_FILE:mc_test_entity_drop>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/entity-drop-controls")
    set_tests_properties(entity.drop PROPERTIES
        LABELS "public;integration;entity;world;ownership;math" TIMEOUT 240)
    add_executable(mc_test_animal_death tests/entity/mob/test_animal_death.c)
    target_include_directories(mc_test_animal_death PRIVATE src src/core/java)
    target_link_libraries(mc_test_animal_death PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.animal_death COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_animal_death.py"
        --executable "$<TARGET_FILE:mc_test_animal_death>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/animal-death-controls")
    set_tests_properties(entity.animal_death PROPERTIES
        LABELS "public;integration;entity;world;ownership;math" TIMEOUT 240)
    add_test(NAME entity.animal_death_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/animal_death/run_native.py"
        --executable "$<TARGET_FILE:mc_test_animal_death>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/animal-death-reference")
    set_tests_properties(entity.animal_death_reference PROPERTIES
        LABELS "public;integration;entity;ownership;replay" TIMEOUT 240)
    add_test(NAME tools.animal_death_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/animal_death/test_packet.py"
        AnimalPacket.test_baseline_and_actual_entropy_census
        AnimalPacket.test_identity_inventory_types_and_categories
        AnimalPacket.test_constructor_formula_and_entropy_progress_mutations
        AnimalPacket.test_deterministic_stack_item_and_damage_mutations
        AnimalPacket.test_membership_owner_callback_and_repeat_mutations
        AnimalPacket.test_independent_private_clock_context_is_preserved_separately
        AnimalPacket.test_source_closure_duplicate_keys_and_coercion)
    set_tests_properties(tools.animal_death_packet PROPERTIES
        LABELS "public;unit;tools;entity;replay" TIMEOUT 60)
endif()
