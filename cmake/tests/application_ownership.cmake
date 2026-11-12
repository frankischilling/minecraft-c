# Include after the existing constructor-test resource-root selection.
# The main CMakeLists include remains coordinator-owned.
if(BUILD_TESTING AND MC_ENABLE_APPLICATION_CONSTRUCTOR_TESTS AND
   TARGET mc_client_application_constructor)
    add_executable(mc_test_inventory_callback_ownership
        tests/client/startup/test_inventory_callback_ownership.c)
    target_include_directories(mc_test_inventory_callback_ownership PRIVATE src/core/java)
    target_link_libraries(mc_test_inventory_callback_ownership PRIVATE
        mc_client_application_constructor mc_sound_backend mc_client_assets
        mc_resources mc_c_atomics mc_build_options)
    set_property(TARGET mc_test_inventory_callback_ownership PROPERTY LINKER_LANGUAGE CXX)
    add_test(NAME client.inventory_callback_ownership
        COMMAND mc_test_inventory_callback_ownership "${_mc_application_constructor_resource_root}")
    set_tests_properties(client.inventory_callback_ownership PROPERTIES
        LABELS "public;unit;client;constructor;ownership" TIMEOUT 120)
    if(WIN32)
        add_custom_command(TARGET mc_test_inventory_callback_ownership POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_inventory_callback_ownership>")
    endif()
endif()
