if(TARGET mc_client_application_constructor)
    add_executable(mc_test_application_resource_services
        tests/client/startup/test_resource_thread_services.c)
    target_include_directories(mc_test_application_resource_services PRIVATE
        src src/client src/platform/time)
    target_link_libraries(mc_test_application_resource_services PRIVATE
        mc_client_application_constructor mc_client_notice mc_c_atomics mc_build_options)
    add_test(NAME client.application_resource_services COMMAND mc_test_application_resource_services)
    set_tests_properties(client.application_resource_services PROPERTIES
        LABELS "public;unit" TIMEOUT 60)
endif()
