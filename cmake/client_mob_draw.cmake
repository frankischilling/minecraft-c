include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/entity/render/leaves.c)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE
        src/client/render/model_runtime/common.c
        src/client/render/model_runtime/quadruped.c
        src/client/render/living_draw/common.c
        src/client/render/living_draw/body.c
        src/client/render/living_draw/sheep.c
        src/client/render/living_draw/shadow.c
        src/client/render/manager/draw.c
        src/client/render/manager/draw_owner.c)
    target_link_libraries(mc_client_entity_renderer PRIVATE
        mc_client_render_manager_cpu mc_math_classes)
    if(BUILD_TESTING AND TARGET mc_test_application_startup)
        add_executable(mc_test_mob_draw tests/client/render/test_mob_draw.c)
        target_include_directories(mc_test_mob_draw PRIVATE src src/platform/time)
        get_target_property(_mc_mob_draw_links mc_test_application_startup LINK_LIBRARIES)
        target_link_libraries(mc_test_mob_draw PRIVATE
            mc_client_entity_renderer mc_simulation mc_math_classes ${_mc_mob_draw_links})
        unset(_mc_mob_draw_links)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_mob_draw PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_mob_draw POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_mob_draw>")
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.mob_draw COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_mob_draw.py"
                --executable "$<TARGET_FILE:mc_test_mob_draw>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mob-draw-contracts")
            set_tests_properties(client.mob_draw PROPERTIES
                LABELS "public;integration;client;world;graphics;ownership;replay" TIMEOUT 300)
        endif()
    endif()
endif()
