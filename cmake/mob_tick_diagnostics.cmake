include_guard(GLOBAL)

if(BUILD_TESTING)
    add_library(mc_test_mob_workspace_workload OBJECT tests/entity/test_mob_workspace.c)
    target_include_directories(mc_test_mob_workspace_workload PRIVATE src src/platform/time)
    target_link_libraries(mc_test_mob_workspace_workload PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_executable(mc_test_mob_workspace $<TARGET_OBJECTS:mc_test_mob_workspace_workload>)
    target_link_libraries(mc_test_mob_workspace PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.mob_workspace COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_workspace.py"
        --executable "$<TARGET_FILE:mc_test_mob_workspace>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mob-workspace-controls")
    set_tests_properties(entity.mob_workspace PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 240)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_mob_diagnostics_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_mob_diagnostics_python_options -O)
        endif()
        add_test(NAME "tools.mob_tick_diagnostics_benchmark_${mode}"
            COMMAND Python3::Interpreter ${_mc_mob_diagnostics_python_options}
                -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity"
                -p test_benchmark_mob_tick_diagnostics.py)
        set_tests_properties("tools.mob_tick_diagnostics_benchmark_${mode}" PROPERTIES
            LABELS "public;unit;entity;world;performance;tools" TIMEOUT 90)
        add_test(NAME "tools.mob_workspace_controls_${mode}"
            COMMAND Python3::Interpreter ${_mc_mob_diagnostics_python_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_workspace.py" --self-test)
        set_tests_properties("tools.mob_workspace_controls_${mode}" PROPERTIES
            LABELS "public;unit;entity;world;ownership;tools" TIMEOUT 90)
    endforeach()
endif()

if(MC_BUILD_BENCHMARKS AND BUILD_TESTING)
    add_library(mc_test_mob_workspace_observer OBJECT
        tests/benchmarks/mob_tick/control_observer.c)
    target_link_libraries(mc_test_mob_workspace_observer PRIVATE mc_build_options)
    add_library(mc_benchmark_mob_tick_workload OBJECT
        tests/benchmarks/mob_tick_diagnostics.c)
    foreach(role IN ITEMS world dispatch)
        foreach(algorithm IN ITEMS legacy candidate)
            add_library("mc_benchmark_mob_tick_${role}_${algorithm}" OBJECT
                "tests/benchmarks/mob_tick/${role}_${algorithm}.c")
        endforeach()
    endforeach()
    foreach(component IN ITEMS workload world_legacy world_candidate dispatch_legacy dispatch_candidate)
        target_include_directories("mc_benchmark_mob_tick_${component}" PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}" src src/platform/time)
        target_link_libraries("mc_benchmark_mob_tick_${component}" PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    endforeach()

    foreach(variant IN ITEMS legacy tail workspace both)
        set(_mc_mob_world legacy)
        set(_mc_mob_dispatch legacy)
        if(variant STREQUAL "tail" OR variant STREQUAL "both")
            set(_mc_mob_world candidate)
        endif()
        if(variant STREQUAL "workspace" OR variant STREQUAL "both")
            set(_mc_mob_dispatch candidate)
        endif()
        set(_mc_mob_target "mc_benchmark_mob_tick_${variant}")
        add_executable("${_mc_mob_target}"
            $<TARGET_OBJECTS:mc_benchmark_mob_tick_workload>
            "$<TARGET_OBJECTS:mc_benchmark_mob_tick_world_${_mc_mob_world}>"
            "$<TARGET_OBJECTS:mc_benchmark_mob_tick_dispatch_${_mc_mob_dispatch}>")
        target_link_libraries("${_mc_mob_target}" PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
        if(MSVC)
            target_link_options("${_mc_mob_target}" PRIVATE
                "/MAP:$<TARGET_FILE_DIR:${_mc_mob_target}>/$<TARGET_FILE_BASE_NAME:${_mc_mob_target}>.map")
        elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_link_options("${_mc_mob_target}" PRIVATE
                "-Wl,-Map,$<TARGET_FILE_DIR:${_mc_mob_target}>/$<TARGET_FILE_BASE_NAME:${_mc_mob_target}>.map")
        endif()
        set(_mc_mob_control "mc_test_mob_workspace_${variant}")
        add_executable("${_mc_mob_control}"
            $<TARGET_OBJECTS:mc_test_mob_workspace_workload>
            $<TARGET_OBJECTS:mc_test_mob_workspace_observer>
            "$<TARGET_OBJECTS:mc_benchmark_mob_tick_world_${_mc_mob_world}>"
            "$<TARGET_OBJECTS:mc_benchmark_mob_tick_dispatch_${_mc_mob_dispatch}>")
        target_link_libraries("${_mc_mob_control}" PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
        add_test(NAME "entity.mob_workspace_${variant}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_workspace.py"
            --executable "$<TARGET_FILE:${_mc_mob_control}>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mob-workspace-${variant}-controls")
        set_tests_properties("entity.mob_workspace_${variant}" PROPERTIES
            LABELS "public;integration;entity;world;ownership;performance" TIMEOUT 240)
    endforeach()
endif()
