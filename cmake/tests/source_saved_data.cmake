add_executable(mc_test_source_saved_data tests/world/saved_data/test_source_storage.c)
target_link_libraries(mc_test_source_saved_data PRIVATE mc_world_saved_data mc_build_options)

add_executable(mc_test_source_saved_data_close tests/world/saved_data/test_source_storage.c
    $<TARGET_OBJECTS:mc_test_save_close_io>)
target_compile_definitions(mc_test_source_saved_data_close PRIVATE MC_SAVED_SOURCE_CLOSE_CONTROL=1)
target_link_libraries(mc_test_source_saved_data_close PRIVATE mc_world_saved_data mc_build_options)

foreach(_mc_saved_source_kind IN ITEMS ordinary close)
    set(_mc_saved_source_target mc_test_source_saved_data)
    if(_mc_saved_source_kind STREQUAL "close")
        set(_mc_saved_source_target mc_test_source_saved_data_close)
    endif()
    add_test(NAME world.saved_data_source_${_mc_saved_source_kind}
        COMMAND ${_mc_saved_source_target}
            "${CMAKE_CURRENT_BINARY_DIR}/source-saved-data-${_mc_saved_source_kind}")
    set_tests_properties(world.saved_data_source_${_mc_saved_source_kind} PROPERTIES
        LABELS "public;integration;world;saved-data;ownership;filesystem" TIMEOUT 60)
endforeach()
unset(_mc_saved_source_kind)
unset(_mc_saved_source_target)
