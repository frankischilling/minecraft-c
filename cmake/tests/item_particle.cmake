include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_test_application_startup)
    get_target_property(_mc_item_particle_links mc_test_application_startup LINK_LIBRARIES)
    add_executable(mc_test_item_particle tests/entity/test_item_particle.c)
    target_include_directories(mc_test_item_particle PRIVATE src src/platform/time)
    target_link_libraries(mc_test_item_particle PRIVATE
        mc_client_entity_renderer mc_simulation mc_math_classes ${_mc_item_particle_links})
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_item_particle PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_item_particle POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_item_particle>")
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME entity.item_particle COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_item_particle.py"
            --executable "$<TARGET_FILE:mc_test_item_particle>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-particle-contracts")
        set_tests_properties(entity.item_particle PROPERTIES
            LABELS "public;integration;entity;client;world;graphics;ownership;replay" TIMEOUT 600)
    endif()
    unset(_mc_item_particle_links)
endif()
