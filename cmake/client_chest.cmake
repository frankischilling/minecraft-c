include_guard(GLOBAL)

if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_chest_screen.c)
endif()
if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_chest_drawing.c)
endif()

if(BUILD_TESTING AND TARGET mc_client_source_screens)
    add_executable(mc_test_source_chest tests/client/gui/test_source_chest.c)
    target_include_directories(mc_test_source_chest PRIVATE src)
    target_link_libraries(mc_test_source_chest PRIVATE mc_client_source_screens
        mc_client_source_screen_events mc_client_application_owner mc_resources
        mc_math_classes mc_simulation mc_build_options)
    if(TARGET mc_client_source_screen_drawing)
        target_link_libraries(mc_test_source_chest PRIVATE mc_client_source_screen_drawing)
        target_compile_definitions(mc_test_source_chest PRIVATE MC_TEST_CHEST_DRAWING=1)
    endif()
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_chest PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.chest_screen_cpu COMMAND mc_test_source_chest)
    set_tests_properties(client.chest_screen_cpu PROPERTIES
        LABELS "public;unit;client;inventory;ownership" TIMEOUT 60)
endif()
