add_executable(mc_test_stats_source_files tests/client/statistics/test_source_files.c)
target_link_libraries(mc_test_stats_source_files PRIVATE
    mc_simulation mc_resources mc_c_atomics mc_build_options)

add_executable(mc_test_stats_file_overlap
    tests/client/statistics/test_file_overlap.c
    tests/client/statistics/file_namespace_control.c
    tests/client/statistics/file_stream_control.c)
target_link_libraries(mc_test_stats_file_overlap PRIVATE
    mc_simulation mc_resources mc_c_atomics mc_build_options)

foreach(_mc_stats_kind IN ITEMS files overlap app)
    if(_mc_stats_kind STREQUAL "files")
        set(_mc_stats_target mc_test_stats_source_files)
    elseif(_mc_stats_kind STREQUAL "overlap")
        set(_mc_stats_target mc_test_stats_file_overlap)
    else()
        set(_mc_stats_target mc_test_player_statistics)
    endif()
    add_test(NAME "client.stats_source_${_mc_stats_kind}" COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_stats_source_files.py"
        --executable "$<TARGET_FILE:${_mc_stats_target}>" --kind "${_mc_stats_kind}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/stats-source-${_mc_stats_kind}")
    set_tests_properties("client.stats_source_${_mc_stats_kind}" PROPERTIES
        LABELS "public;integration;client;statistics;ownership" TIMEOUT 240)
    if(MSVC)
        target_link_options(${_mc_stats_target} PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/${_mc_stats_target}.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(${_mc_stats_target} PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${_mc_stats_target}.map")
    endif()
endforeach()
unset(_mc_stats_kind)
unset(_mc_stats_target)
