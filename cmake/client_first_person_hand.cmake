include_guard(GLOBAL)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE
        src/client/world/entity_renderer/first_person_hand.c
        src/client/world/entity_renderer/held_item_draw.c
        src/client/render/model_draw.c)
    if(BUILD_TESTING AND TARGET mc_test_application_startup)
        add_executable(mc_test_first_person_hand tests/client/world/test_first_person_hand.c)
        get_target_property(_mc_hand_links mc_test_application_startup LINK_LIBRARIES)
        target_link_libraries(mc_test_first_person_hand PRIVATE
            mc_client_entity_renderer mc_simulation mc_math_classes ${_mc_hand_links})
        unset(_mc_hand_links)
        target_include_directories(mc_test_first_person_hand PRIVATE src src/platform/time)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_first_person_hand PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_first_person_hand POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_first_person_hand>")
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.first_person_hand COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_first_person_hand.py"
                --executable "$<TARGET_FILE:mc_test_first_person_hand>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/first-person-hand")
            set_tests_properties(client.first_person_hand PROPERTIES
                LABELS "public;integration;client;world;graphics;ownership;replay" TIMEOUT 180)
        endif()
    endif()
endif()
