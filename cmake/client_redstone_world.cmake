include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.redstone_world_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/redstone_world" -p test_packet.py)
    set_tests_properties(tools.redstone_world_packet PROPERTIES
        LABELS "public;unit;tools;client;world;graphics;reference" TIMEOUT 60)
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND AND TARGET mc_client_render_global
   AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_redstone_world tests/client/render/test_redstone_world.c)
    target_include_directories(mc_test_redstone_world PRIVATE src src/platform/time)
    target_link_libraries(mc_test_redstone_world PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_redstone_world PROPERTY LINKER_LANGUAGE CXX)
    endif()
    foreach(cohort IN ITEMS flat vertical capability signed nullable)
        set(redstone_owner_args)
        if(cohort STREQUAL "signed")
            set(redstone_owner_args --owners
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/redstone_world/signed/owners.tsv")
        endif()
        add_test(NAME client.redstone_world_${cohort} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_redstone_world.py"
            --executable "$<TARGET_FILE:mc_test_redstone_world>"
            --profiles "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/redstone_world/${cohort}/profiles.txt"
            --reference "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/redstone_world/${cohort}/selected.txt"
            --layout "${cohort}" --work-dir "${CMAKE_CURRENT_BINARY_DIR}/redstone-world-${cohort}"
            ${redstone_owner_args})
        set_tests_properties(client.redstone_world_${cohort} PROPERTIES
            LABELS "public;integration;client;world;graphics;ownership;reference;replay" TIMEOUT 180)
    endforeach()
    add_test(NAME client.redstone_world_callbacks COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_redstone_world.py"
        --executable "$<TARGET_FILE:mc_test_redstone_world>" --layout callbacks
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/redstone-world-callbacks")
    set_tests_properties(client.redstone_world_callbacks PROPERTIES
        LABELS "public;integration;client;world;graphics;ownership" TIMEOUT 180)
endif()
