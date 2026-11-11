include_guard(GLOBAL)

# These Inventory children call actual Stack durability/statistic producers,
# so they live with gameplay rather than adding a reverse mc_inventory link.
target_sources(mc_simulation PRIVATE
    src/entity/player/damage.c
    src/entity/player/damage_alert.c
    src/entity/player/damage_wake.c
    src/entity/player/death.c
    src/inventory/runtime/armor.c
    src/inventory/runtime/scatter.c)

if(BUILD_TESTING)
    add_executable(mc_test_player_mob_attacker tests/entity/player/test_mob_attacker.c)
    target_include_directories(mc_test_player_mob_attacker PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_mob_attacker PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME entity.player_mob_attacker COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_mob_attacker.py"
        --executable "$<TARGET_FILE:mc_test_player_mob_attacker>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-mob-attacker")
    set_tests_properties(entity.player_mob_attacker PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 180)
    add_executable(mc_test_player_ally_alert_ownership tests/entity/player/test_ally_alert_ownership.c)
    target_include_directories(mc_test_player_ally_alert_ownership PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_ally_alert_ownership PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.player_ally_alert_ownership COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_ally_alert_ownership.py"
        --executable "$<TARGET_FILE:mc_test_player_ally_alert_ownership>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-ally-alert-ownership")
    set_tests_properties(entity.player_ally_alert_ownership PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 180)
    add_executable(mc_test_player_ally_alert_original tests/entity/player/test_ally_alert_original.c)
    target_include_directories(mc_test_player_ally_alert_original PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_ally_alert_original PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.player_ally_alert_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_ally_alert/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_ally_alert_original>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-ally-alert-original")
    set_tests_properties(entity.player_ally_alert_reference PROPERTIES
        LABELS "public;integration;entity;world;ownership;reference;replay" TIMEOUT 600)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_ally_alert_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_ally_alert_python_options -O)
        endif()
        add_test(NAME "tools.player_ally_alert_packet_${mode}" COMMAND Python3::Interpreter
            ${_mc_ally_alert_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_ally_alert/test_packet.py")
        set_tests_properties("tools.player_ally_alert_packet_${mode}" PROPERTIES
            LABELS "public;unit;entity;world;reference;tools" TIMEOUT 90)
        add_test(NAME "tools.player_ally_retire_benchmark_${mode}" COMMAND Python3::Interpreter
            ${_mc_ally_alert_python_options} -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity"
            -p test_benchmark_player_ally_retire.py)
        set_tests_properties("tools.player_ally_retire_benchmark_${mode}" PROPERTIES
            LABELS "public;unit;entity;ownership;performance;tools" TIMEOUT 30)
    endforeach()
    add_executable(mc_test_player_damage_wake tests/entity/player/test_damage_wake.c)
    target_include_directories(mc_test_player_damage_wake PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_damage_wake PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_damage_wake COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_damage_wake.py"
        --executable "$<TARGET_FILE:mc_test_player_damage_wake>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-damage-wake-contracts")
    add_test(NAME entity.block_drop_execution COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_damage_wake.py"
        --mode owned-block-drops --executable "$<TARGET_FILE:mc_test_player_damage_wake>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/block-drop-execution-contracts")
    set_tests_properties(entity.block_drop_execution PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 180)
    set_tests_properties(entity.player_damage_wake PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 180)
    foreach(wake_mode IN ITEMS recursion null-exit observer-error scope-drift null-chunk-world
                               foot foot-source foot-scope foot-math-missing)
        string(REPLACE "-" "_" wake_name "${wake_mode}")
        add_test(NAME "entity.player_damage_wake_owned_${wake_name}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_damage_wake.py"
            --mode "owned-wake-${wake_mode}" --executable "$<TARGET_FILE:mc_test_player_damage_wake>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-damage-wake-contracts")
        set_tests_properties("entity.player_damage_wake_owned_${wake_name}" PROPERTIES
            LABELS "public;integration;entity;world;ownership" TIMEOUT 180)
    endforeach()
    add_executable(mc_test_player_damage_wake_original tests/entity/player/test_damage_wake_original.c)
    target_include_directories(mc_test_player_damage_wake_original PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_damage_wake_original PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_damage_wake_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_damage_wake/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_damage_wake_original>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-damage-wake-original")
    set_tests_properties(entity.player_damage_wake_reference PROPERTIES
        LABELS "public;integration;entity;world;ownership;reference;replay" TIMEOUT 240)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_damage_wake_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_damage_wake_python_options -O)
        endif()
        add_test(NAME "tools.player_damage_wake_packet_${mode}" COMMAND Python3::Interpreter
            ${_mc_damage_wake_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_damage_wake/test_packet.py")
        set_tests_properties("tools.player_damage_wake_packet_${mode}" PROPERTIES
            LABELS "public;unit;entity;world;reference;tools" TIMEOUT 90)
        add_test(NAME "tools.player_recursive_wake_packet_${mode}" COMMAND Python3::Interpreter
            ${_mc_damage_wake_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_recursive_wake/packet.py")
        set_tests_properties("tools.player_recursive_wake_packet_${mode}" PROPERTIES
            LABELS "public;unit;entity;world;reference;tools" TIMEOUT 90)
        add_test(NAME "tools.bed_foot_wake_packet_${mode}" COMMAND Python3::Interpreter
            ${_mc_damage_wake_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/bed_foot_wake/test_packet.py")
        set_tests_properties("tools.bed_foot_wake_packet_${mode}" PROPERTIES
            LABELS "public;unit;entity;world;reference;tools" TIMEOUT 90)
    endforeach()
    add_executable(mc_test_player_death tests/entity/player/test_death.c)
    target_include_directories(mc_test_player_death PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_death PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_death COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_death.py"
        --executable "$<TARGET_FILE:mc_test_player_death>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-death-contracts")
    set_tests_properties(entity.player_death PROPERTIES
        LABELS "public;integration;entity;inventory;client;ownership" TIMEOUT 180)
    add_executable(mc_test_player_hurt_original tests/entity/player/test_hurt_original.c)
    target_include_directories(mc_test_player_hurt_original PRIVATE src/platform/time)
    target_link_libraries(mc_test_player_hurt_original PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_hurt COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_hurt/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_hurt_original>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-hurt-contracts")
    set_tests_properties(entity.player_hurt PROPERTIES
        LABELS "public;integration;entity;inventory;client;ownership" TIMEOUT 180)
    add_test(NAME tools.player_hurt_packet COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_hurt/test_packet.py")
    add_test(NAME tools.player_hurt_packet_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_hurt/test_packet.py")
    set_tests_properties(tools.player_hurt_packet tools.player_hurt_packet_optimized PROPERTIES
        LABELS "public;unit;tools;entity" TIMEOUT 60)
endif()

if(MC_BUILD_BENCHMARKS AND BUILD_TESTING)
    add_library(mc_benchmark_player_ally_retire_workload OBJECT tests/benchmarks/player_ally_retire.c)
    target_include_directories(mc_benchmark_player_ally_retire_workload PRIVATE src src/platform/time)
    target_link_libraries(mc_benchmark_player_ally_retire_workload PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    foreach(adapter IN ITEMS quadratic candidate fallback)
        add_executable("mc_benchmark_player_ally_retire_${adapter}"
            "tests/benchmarks/player_ally_retire_${adapter}.c"
            $<TARGET_OBJECTS:mc_benchmark_player_ally_retire_workload>)
        target_include_directories("mc_benchmark_player_ally_retire_${adapter}" PRIVATE
            src src/platform/time)
        target_link_libraries("mc_benchmark_player_ally_retire_${adapter}" PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    endforeach()
endif()
