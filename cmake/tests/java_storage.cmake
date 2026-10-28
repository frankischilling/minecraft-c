add_executable(mc_test_java_logging tests/core/java/test_logging.c)
target_link_libraries(mc_test_java_logging PRIVATE mc_core Threads::Threads mc_build_options)
add_test(NAME core.java_logging COMMAND mc_test_java_logging)
set_tests_properties(core.java_logging PROPERTIES
    LABELS "public;unit;core;ownership;threading" TIMEOUT 120)

# This control includes the canonical production body under its native allocator.
add_executable(mc_test_java_logging_allocation tests/core/java/test_logging_allocation.c)
target_include_directories(mc_test_java_logging_allocation PRIVATE include)
target_link_libraries(mc_test_java_logging_allocation PRIVATE
    Threads::Threads mc_c_atomics mc_build_options)
add_test(NAME core.java_logging_allocation COMMAND mc_test_java_logging_allocation)
set_tests_properties(core.java_logging_allocation PROPERTIES
    LABELS "public;unit;core;ownership;allocation" TIMEOUT 120)

add_executable(mc_test_java_output
    tests/world/storage/test_java_output.c tests/world/storage/java_output_native.c)
target_link_libraries(mc_test_java_output PRIVATE mc_java_outputs mc_build_options)
add_test(NAME storage.java_binary_output COMMAND Python3::Interpreter -B
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/test_java_output.py"
    --executable "$<TARGET_FILE:mc_test_java_output>"
    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/java-binary-output-controls")
set_tests_properties(storage.java_binary_output PROPERTIES
    LABELS "public;integration;storage;ownership" TIMEOUT 240)

add_executable(mc_test_source_handler
    tests/world/storage/test_source_handler.c tests/world/storage/java_output_native.c)
target_include_directories(mc_test_source_handler PRIVATE src)
target_link_libraries(mc_test_source_handler PRIVATE
    mc_client_saves mc_persistence mc_java_outputs mc_platform_time mc_build_options)
add_test(NAME storage.source_handler_constructor COMMAND Python3::Interpreter -B
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_source_handler.py"
    --executable "$<TARGET_FILE:mc_test_source_handler>"
    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-handler-controls")
set_tests_properties(storage.source_handler_constructor PROPERTIES
    LABELS "public;integration;storage;ownership;filesystem" TIMEOUT 240)

foreach(_mc_storage_map IN ITEMS mc_test_source_handler mc_test_java_output)
    if(MSVC)
        target_link_options(${_mc_storage_map} PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/${_mc_storage_map}.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(${_mc_storage_map} PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${_mc_storage_map}.map")
    endif()
endforeach()
unset(_mc_storage_map)

add_executable(mc_test_source_reads tests/world/storage/test_source_reads.c)
target_link_libraries(mc_test_source_reads PRIVATE mc_persistence mc_build_options)
add_executable(mc_test_source_reads_close tests/world/storage/test_source_reads.c
    $<TARGET_OBJECTS:mc_test_save_close_io>)
target_compile_definitions(mc_test_source_reads_close PRIVATE MC_SOURCE_READ_CLOSE_CONTROL=1)
target_link_libraries(mc_test_source_reads_close PRIVATE mc_persistence mc_build_options)
foreach(_mc_reads_kind IN ITEMS ordinary close)
    set(_mc_reads_target mc_test_source_reads)
    if(_mc_reads_kind STREQUAL "close")
        set(_mc_reads_target mc_test_source_reads_close)
    endif()
    if(MSVC)
        target_link_options(${_mc_reads_target} PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/${_mc_reads_target}.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(${_mc_reads_target} PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${_mc_reads_target}.map")
    endif()
    add_test(NAME storage.source_handler_reads_${_mc_reads_kind}
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_source_reads.py"
            --executable "$<TARGET_FILE:${_mc_reads_target}>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-reads-${_mc_reads_kind}")
    set_tests_properties(storage.source_handler_reads_${_mc_reads_kind} PROPERTIES
        LABELS "public;integration;storage;ownership;filesystem" TIMEOUT 240)
endforeach()
unset(_mc_reads_kind)
unset(_mc_reads_target)
