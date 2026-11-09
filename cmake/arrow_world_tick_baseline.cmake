include_guard(GLOBAL)

# Single-interval CPU workload only. No CTest/driver, Source overlay or allocator probe.
if(MC_BUILD_BENCHMARKS AND BUILD_TESTING)
    add_library(mc_benchmark_arrow_world_tick_workload OBJECT
        tests/benchmarks/arrow_world_tick_baseline.c)
    target_include_directories(mc_benchmark_arrow_world_tick_workload PRIVATE
        src src/platform/time)
    target_link_libraries(mc_benchmark_arrow_world_tick_workload PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_executable(mc_benchmark_arrow_world_tick_baseline
        $<TARGET_OBJECTS:mc_benchmark_arrow_world_tick_workload>)
    target_link_libraries(mc_benchmark_arrow_world_tick_baseline PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    if(MSVC)
        target_link_options(mc_benchmark_arrow_world_tick_baseline PRIVATE
            "/MAP:$<TARGET_FILE_DIR:mc_benchmark_arrow_world_tick_baseline>/$<TARGET_FILE_BASE_NAME:mc_benchmark_arrow_world_tick_baseline>.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(mc_benchmark_arrow_world_tick_baseline PRIVATE
            "-Wl,-Map,$<TARGET_FILE_DIR:mc_benchmark_arrow_world_tick_baseline>/$<TARGET_FILE_BASE_NAME:mc_benchmark_arrow_world_tick_baseline>.map")
    endif()
endif()
