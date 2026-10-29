if(BUILD_TESTING AND TARGET mc_java_file_urls)
    add_executable(mc_test_download_leaves tests/client/resources/test_download_leaves.c)
    target_include_directories(mc_test_download_leaves PRIVATE include src)
    target_link_libraries(mc_test_download_leaves PRIVATE mc_java_file_urls mc_java_outputs
        mc_core mc_build_options)
    add_executable(mc_test_sound_file_registration tests/client/resources/test_sound_file_registration.c)
    target_include_directories(mc_test_sound_file_registration PRIVATE include src)
    target_link_libraries(mc_test_sound_file_registration PRIVATE mc_client_sound_pool
        mc_java_outputs Threads::Threads mc_build_options)
    add_executable(mc_test_info_name_concurrency tests/client/resources/test_info_name_concurrency.c)
    target_include_directories(mc_test_info_name_concurrency PRIVATE include src src/platform/time)
    target_link_libraries(mc_test_info_name_concurrency PRIVATE mc_java_files mc_storage
        mc_platform_time mc_c_atomics Threads::Threads mc_build_options)
    add_test(NAME core.info_name_concurrency COMMAND mc_test_info_name_concurrency)
    set_tests_properties(core.info_name_concurrency PROPERTIES LABELS "unit;ownership;thread" TIMEOUT 180)
    set(mc_resource_test_targets mc_test_download_leaves mc_test_sound_file_registration mc_test_info_name_concurrency)
    if(TARGET mc_client_resource_download)
        add_executable(mc_test_download_thread tests/client/resources/test_download_thread.c)
        add_executable(mc_test_download_dis tests/client/resources/test_download_dis.c)
        target_include_directories(mc_test_download_dis PRIVATE include src)
        target_link_libraries(mc_test_download_dis PRIVATE mc_java_http_inputs mc_storage mc_build_options)
        add_test(NAME core.data_input_bulk COMMAND mc_test_download_dis)
        set_tests_properties(core.data_input_bulk PROPERTIES LABELS "unit;ownership;stream" TIMEOUT 180)
        add_executable(mc_test_download_http tests/client/resources/test_download_http.c)
        foreach(target mc_test_download_thread mc_test_download_http)
            target_include_directories(${target} PRIVATE include src src/platform/time)
            target_link_libraries(${target} PRIVATE mc_client_resource_download mc_build_options)
        endforeach()
        list(APPEND mc_resource_test_targets mc_test_download_thread mc_test_download_http mc_test_download_dis)
        add_test(NAME client.resource_download_owner COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/resources/run_contract.py"
            --executable "$<TARGET_FILE:mc_test_download_thread>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/resource-contracts")
        set_tests_properties(client.resource_download_owner PROPERTIES
            LABELS "unit;client;ownership;thread;filesystem" TIMEOUT 180)
        add_test(NAME client.resource_download_http COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/resources/run_contract.py"
            --executable "$<TARGET_FILE:mc_test_download_http>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/resource-contracts" --http)
        set_tests_properties(client.resource_download_http PROPERTIES
            LABELS "integration;client;ownership;filesystem;loopback" TIMEOUT 120)
    endif()
    foreach(target IN LISTS mc_resource_test_targets)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET ${target} PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MSVC)
            target_link_options(${target} PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/${target}.map")
        elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_link_options(${target} PRIVATE "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${target}.map")
        endif()
    endforeach()
    add_test(NAME core.resource_download_leaves COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/resources/run_contract.py"
        --executable "$<TARGET_FILE:mc_test_download_leaves>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/resource-contracts")
    add_test(NAME client.resource_sound_registration COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/resources/run_contract.py"
        --executable "$<TARGET_FILE:mc_test_sound_file_registration>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/resource-contracts")
    set_tests_properties(core.resource_download_leaves client.resource_sound_registration PROPERTIES
        LABELS "unit;ownership;filesystem" TIMEOUT 180)
    # Transport qualification binds only an ephemeral loopback port. The literal
    # public resource endpoint in source run() is never redirected by this test.
endif()
