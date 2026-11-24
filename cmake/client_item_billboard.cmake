include_guard(GLOBAL)

if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE src/client/render/item_billboard.c)
    if(BUILD_TESTING)
        add_executable(mc_test_item_billboard tests/client/render/test_item_billboard.c)
        target_include_directories(mc_test_item_billboard PRIVATE src)
        target_link_libraries(mc_test_item_billboard PRIVATE
            mc_client_entity_renderer mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_item_billboard PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.item_billboard_cpu COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/item_billboard/run_native.py"
            --executable "$<TARGET_FILE:mc_test_item_billboard>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-billboard-controls")
        set_tests_properties(client.item_billboard_cpu PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 180)
        add_test(NAME tools.item_billboard_packet COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/item_billboard/test_packet.py")
        set_tests_properties(tools.item_billboard_packet PROPERTIES
            LABELS "public;unit;tools;client;graphics" TIMEOUT 60)
    endif()
endif()
