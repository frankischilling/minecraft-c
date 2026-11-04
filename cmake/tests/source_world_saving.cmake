if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_source_world_saving tests/world/runtime/saving/test_source_saving.c)
    target_include_directories(mc_test_source_world_saving PRIVATE src)
    target_link_libraries(mc_test_source_world_saving PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_test(NAME world.source_saving COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_source_saving.py"
        --executable "$<TARGET_FILE:mc_test_source_world_saving>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-world-saving-controls")
    set_tests_properties(world.source_saving PROPERTIES
        LABELS "public;integration;world;storage;ownership;filesystem" TIMEOUT 600)
endif()

if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_source_owned_cache tests/world/runtime/saving/test_owned_cache.c)
    target_include_directories(mc_test_source_owned_cache PRIVATE src)
    target_link_libraries(mc_test_source_owned_cache PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_test(NAME world.source_owned_cache COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_owned_cache.py"
        --executable "$<TARGET_FILE:mc_test_source_owned_cache>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-owned-cache-controls")
    set_tests_properties(world.source_owned_cache PROPERTIES
        LABELS "public;integration;world;storage;ownership;filesystem" TIMEOUT 300)
endif()

option(MC_BUILD_BENCHMARKS "Build optional fixed-work CPU benchmarks" OFF)
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.client_frame_observations COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/benchmarks" -p test_client_frames_run.py)
    set_tests_properties(tools.client_frame_observations PROPERTIES
        LABELS "public;unit;tools;client;performance" TIMEOUT 30)
    add_test(NAME tools.world_random_tick_observations COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/benchmarks" -p test_world_random_tick.py)
    set_tests_properties(tools.world_random_tick_observations PROPERTIES
        LABELS "public;unit;tools;world;performance" TIMEOUT 30)
endif()
if(MC_BUILD_BENCHMARKS AND BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_benchmark_world_queries tests/benchmarks/world_queries.c)
    target_include_directories(mc_benchmark_world_queries PRIVATE src)
    target_link_libraries(mc_benchmark_world_queries PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_executable(mc_benchmark_light_drain tests/benchmarks/light_drain.c)
    target_include_directories(mc_benchmark_light_drain PRIVATE src)
    target_link_libraries(mc_benchmark_light_drain PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_executable(mc_benchmark_world_random_tick tests/benchmarks/world_random_tick.c)
    target_include_directories(mc_benchmark_world_random_tick PRIVATE src src/core/java)
    target_link_libraries(mc_benchmark_world_random_tick PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_build_options)
    add_library(mc_benchmark_chunk_cache_workload OBJECT tests/benchmarks/chunk_cache.c)
    target_include_directories(mc_benchmark_chunk_cache_workload PRIVATE src)
    target_link_libraries(mc_benchmark_chunk_cache_workload PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    foreach(_mc_cache_adapter IN ITEMS public failure)
        add_executable("mc_benchmark_chunk_cache_${_mc_cache_adapter}"
            "tests/benchmarks/chunk_cache_${_mc_cache_adapter}.c"
            $<TARGET_OBJECTS:mc_benchmark_chunk_cache_workload>)
        target_include_directories("mc_benchmark_chunk_cache_${_mc_cache_adapter}" PRIVATE src)
        target_link_libraries("mc_benchmark_chunk_cache_${_mc_cache_adapter}" PRIVATE
            mc_simulation mc_persistence mc_c_atomics mc_build_options)
    endforeach()
    unset(_mc_cache_adapter)
endif()
