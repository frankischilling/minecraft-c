include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/entity/render/leaves.c)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE
        src/client/render/model_runtime/common.c
        src/client/render/model_runtime/quadruped.c
        src/client/render/model_runtime/cow.c
        src/client/render/model_runtime/chicken.c
        src/client/render/living_draw/common.c
        src/client/render/living_draw/body.c
        src/client/render/living_draw/animals.c
        src/client/render/living_draw/sheep.c
        src/client/render/living_draw/shadow.c
        src/client/render/manager/draw.c
        src/client/render/manager/draw_owner.c)
    target_link_libraries(mc_client_entity_renderer PRIVATE
        mc_client_render_manager_cpu mc_math_classes)
    if(BUILD_TESTING AND TARGET mc_test_application_startup)
        get_target_property(_mc_mob_draw_links mc_test_application_startup LINK_LIBRARIES)
        foreach(_mc_draw_kind IN ITEMS mob animal)
            set(_mc_draw_target "mc_test_${_mc_draw_kind}_draw")
            add_executable(${_mc_draw_target} "tests/client/render/test_${_mc_draw_kind}_draw.c")
            target_include_directories(${_mc_draw_target} PRIVATE src src/platform/time)
            target_link_libraries(${_mc_draw_target} PRIVATE
                mc_client_entity_renderer mc_simulation mc_math_classes ${_mc_mob_draw_links})
            if(CMAKE_CXX_COMPILER_LOADED)
                set_property(TARGET ${_mc_draw_target} PROPERTY LINKER_LANGUAGE CXX)
            endif()
            if(WIN32 AND TARGET OpenAL)
                add_custom_command(TARGET ${_mc_draw_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                        "$<TARGET_FILE_DIR:${_mc_draw_target}>")
            endif()
            if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
                set(_mc_draw_timeout 300)
                if(_mc_draw_kind STREQUAL "animal")
                    set(_mc_draw_timeout 600)
                endif()
                add_test(NAME "client.${_mc_draw_kind}_draw" COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_${_mc_draw_kind}_draw.py"
                    --executable "$<TARGET_FILE:${_mc_draw_target}>"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/${_mc_draw_kind}-draw-contracts")
                set_tests_properties("client.${_mc_draw_kind}_draw" PROPERTIES
                    LABELS "public;integration;client;world;graphics;ownership;replay"
                    TIMEOUT ${_mc_draw_timeout})
            endif()
        endforeach()
        unset(_mc_draw_kind)
        unset(_mc_draw_target)
        unset(_mc_draw_timeout)
        unset(_mc_mob_draw_links)
    endif()
endif()
