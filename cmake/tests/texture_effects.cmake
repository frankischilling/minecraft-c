if(BUILD_TESTING AND MC_BUILD_CLIENT)
    foreach(_mode IN ITEMS normal optimized)
        set(_optimization)
        if(_mode STREQUAL "optimized")
            set(_optimization -O)
        endif()
        add_test(NAME tools.texture_effect_output_${_mode}
            COMMAND Python3::Interpreter -B ${_optimization}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_texture_effects_output.py")
        set_tests_properties(tools.texture_effect_output_${_mode} PROPERTIES
            LABELS "public;unit;tools;client" TIMEOUT 30)
    endforeach()
    unset(_mode)
    unset(_optimization)
    foreach(_case IN ITEMS cpu registration allocation world spawn)
        if(_case STREQUAL "cpu")
            set(_source tests/client/texture_effects/test_effects.c)
        elseif(_case STREQUAL "registration")
            set(_source tests/client/texture_effects/test_effect_registration.c)
        elseif(_case STREQUAL "allocation")
            set(_source tests/client/texture_effects/test_effect_registration_allocation.c)
        elseif(_case STREQUAL "world")
            set(_source tests/client/texture_effects/test_effect_world.c)
        else()
            set(_source tests/client/texture_effects/test_spawn_factor.c)
        endif()
        add_executable(mc_test_texture_effect_${_case} ${_source})
        target_include_directories(mc_test_texture_effect_${_case} PRIVATE src src/platform/time)
        target_link_libraries(mc_test_texture_effect_${_case} PRIVATE
            mc_client_texture_effects mc_simulation mc_resources ZLIB::ZLIBSTATIC mc_build_options)
        if(_case STREQUAL "registration" OR _case STREQUAL "allocation")
            target_link_libraries(mc_test_texture_effect_${_case} PRIVATE mc_client_application_owner)
        endif()
        if(_case STREQUAL "allocation" OR _case STREQUAL "spawn")
            if(MSVC)
                target_link_options(mc_test_texture_effect_${_case} PRIVATE
                    "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_texture_effect_${_case}.map")
            else()
                target_link_options(mc_test_texture_effect_${_case} PRIVATE
                    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_texture_effect_${_case}.map")
            endif()
        endif()
        add_test(NAME client.texture_effect_${_case} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_texture_effects.py"
            --executable "$<TARGET_FILE:mc_test_texture_effect_${_case}>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/texture-effect-contracts" --profile "${_case}")
        set_tests_properties(client.texture_effect_${_case} PROPERTIES
            LABELS "public;unit;client;ownership" TIMEOUT 240)
    endforeach()
    unset(_case)
    unset(_source)
endif()
