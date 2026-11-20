if(BUILD_TESTING AND TARGET mc_java_file_writers)
    # Actual FOS and bridge TUs included once for concrete native IO/guard
    # arrangements; the link map must exclude both canonical archive members.
    add_executable(mc_test_java_file_writer tests/world/storage/test_java_file_writer.c)
    target_include_directories(mc_test_java_file_writer PRIVATE include src)
    target_link_libraries(mc_test_java_file_writer PRIVATE mc_java_file_writers
        mc_platform_files Threads::Threads mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_java_file_writer PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MSVC)
        target_link_options(mc_test_java_file_writer PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_java_file_writer.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(mc_test_java_file_writer PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_java_file_writer.map")
    endif()
    add_test(NAME world.typed_file_writer COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_java_file_writer.py"
        --executable "$<TARGET_FILE:mc_test_java_file_writer>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/typed-file-writer")
    set_tests_properties(world.typed_file_writer PROPERTIES
        LABELS "unit;storage;ownership;filesystem" TIMEOUT 180)
endif()
