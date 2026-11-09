add_executable(mc_test_java_random_record_concurrency
    tests/core/java/test_random_profile_record_overlap.c)
target_link_libraries(mc_test_java_random_record_concurrency PRIVATE
    mc_core mc_c_atomics Threads::Threads mc_build_options)
# Exit 0 certifies Source/invariant validity. Genuine overlap requires both
# separately printed qualification flags to be 1; the test never forces or retries it.
add_test(NAME core.java_random_record_concurrency_source_valid
    COMMAND mc_test_java_random_record_concurrency)
set_tests_properties(core.java_random_record_concurrency_source_valid PROPERTIES
    LABELS "public;unit;core;threading" TIMEOUT 120)

if(TARGET mc_client_runtime)
    add_executable(mc_test_bootstrap_random_admission
        tests/client/startup/test_bootstrap_random_admission.c)
    target_link_libraries(mc_test_bootstrap_random_admission PRIVATE
        mc_client_runtime mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_bootstrap_random_admission PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32)
        add_custom_command(TARGET mc_test_bootstrap_random_admission POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_bootstrap_random_admission>")
    endif()
    add_test(NAME client.bootstrap_random_admission COMMAND mc_test_bootstrap_random_admission)
    set_tests_properties(client.bootstrap_random_admission PROPERTIES
        LABELS "public;unit;client;ownership;admission" TIMEOUT 60)
endif()
