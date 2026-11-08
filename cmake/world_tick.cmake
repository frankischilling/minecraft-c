include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE
    src/world/runtime/world_tick.c
    src/world/runtime/nightmare.c
    src/world/runtime/spawn_postspawn.c
    src/world/runtime/world_tick_spawn.c
    src/world/runtime/world_tick_scheduled.c)

if(BUILD_TESTING)
    add_executable(mc_test_nightmare_eligibility tests/world/runtime/test_nightmare_eligibility.c)
    target_include_directories(mc_test_nightmare_eligibility PRIVATE src src/core/java)
    target_link_libraries(mc_test_nightmare_eligibility PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.nightmare_eligibility COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_nightmare_eligibility.py"
        --executable "$<TARGET_FILE:mc_test_nightmare_eligibility>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/nightmare-eligibility")
    set_tests_properties(world.nightmare_eligibility PROPERTIES
        LABELS "public;integration;world;entity;ownership" TIMEOUT 240)
    add_executable(mc_test_nightmare tests/world/runtime/test_nightmare.c)
    target_include_directories(mc_test_nightmare PRIVATE src src/core/java)
    target_link_libraries(mc_test_nightmare PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.nightmare_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/nightmare/run_native.py"
        --executable "$<TARGET_FILE:mc_test_nightmare>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/nightmare-reference")
    set_tests_properties(world.nightmare_reference PROPERTIES
        LABELS "public;integration;world;entity;ownership;reference;replay" TIMEOUT 1500)
    foreach(suffix IN ITEMS normal optimized)
        set(_mc_nightmare_python_options -B)
        if(suffix STREQUAL "optimized")
            list(APPEND _mc_nightmare_python_options -O)
        endif()
        add_test(NAME "tools.nightmare_packet_${suffix}" COMMAND Python3::Interpreter
            ${_mc_nightmare_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/nightmare/test_packet.py")
        set_tests_properties("tools.nightmare_packet_${suffix}" PROPERTIES
            LABELS "public;unit;tools;world;reference" TIMEOUT 90)
    endforeach()

    add_executable(mc_test_spawn_postspawn tests/world/runtime/test_spawn_postspawn.c)
    target_include_directories(mc_test_spawn_postspawn PRIVATE src src/core/java)
    target_link_libraries(mc_test_spawn_postspawn PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.spawn_postspawn COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_spawn_postspawn.py"
        --executable "$<TARGET_FILE:mc_test_spawn_postspawn>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/spawn-postspawn-contracts")
    set_tests_properties(world.spawn_postspawn PROPERTIES
        LABELS "public;integration;world;entity;ownership" TIMEOUT 180)

    add_executable(mc_test_world_tick tests/world/runtime/test_world_tick.c)
    target_link_libraries(mc_test_world_tick PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.tick COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/tick/run.py"
        --native $<TARGET_FILE:mc_test_world_tick>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-tick")
    set_tests_properties(world.tick PROPERTIES LABELS "public;unit;world;ownership" TIMEOUT 120)
endif()
