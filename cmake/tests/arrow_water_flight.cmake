include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_arrow_water_flight tests/world/runtime/test_arrow_water_flight.c)
    target_include_directories(mc_test_arrow_water_flight PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_water_flight PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
endif()

if(BUILD_TESTING AND TARGET mc_test_arrow_water_flight AND Python3_Interpreter_FOUND)
    foreach(_mc_arrow_water_mode IN ITEMS
            water-first water-multi dry-to-water water-to-air particle-failure null-callback)
        string(REPLACE "-" "_" _mc_arrow_water_name "${_mc_arrow_water_mode}")
        add_test(NAME world.arrow_water_${_mc_arrow_water_name}
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_arrow_water_flight.py"
                --mode "${_mc_arrow_water_mode}"
                --executable "$<TARGET_FILE:mc_test_arrow_water_flight>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-water-flight-scenes")
        set_tests_properties(world.arrow_water_${_mc_arrow_water_name} PROPERTIES
            TIMEOUT 300 RESOURCE_LOCK world_query_controls
            LABELS "public;integration;world;entity;ownership")
    endforeach()
    unset(_mc_arrow_water_mode)
    unset(_mc_arrow_water_name)
    add_test(NAME tools.arrow_water_flight_protocol
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_arrow_water_flight.py" --self-test)
    add_test(NAME tools.arrow_water_flight_protocol_optimized
        COMMAND Python3::Interpreter -B -O
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_arrow_water_flight.py" --self-test)
    set_tests_properties(tools.arrow_water_flight_protocol
        tools.arrow_water_flight_protocol_optimized PROPERTIES
        TIMEOUT 90 LABELS "public;unit;tools;world;entity")
endif()
