include_guard(GLOBAL)

if(TARGET mc_simulation)
    target_sources(mc_simulation PRIVATE src/entity/falling_sand/tick.c)
    if(BUILD_TESTING)
        add_executable(mc_test_falling_tick tests/entity/falling_sand/test_tick.c)
        target_include_directories(mc_test_falling_tick PRIVATE src)
        target_link_libraries(mc_test_falling_tick PRIVATE
            mc_simulation mc_c_atomics mc_build_options)
        add_test(NAME entity.falling_tick COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_falling_tick.py"
            --executable "$<TARGET_FILE:mc_test_falling_tick>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/falling-tick-contracts")
        set_tests_properties(entity.falling_tick PROPERTIES
            LABELS "public;integration;entity;world;ownership;math" TIMEOUT 180)
    endif()
endif()
