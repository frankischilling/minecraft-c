include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_executable(mc_test_piston_inventory_texture tests/client/render/test_piston_inventory_texture.c)
    target_include_directories(mc_test_piston_inventory_texture PRIVATE src)
    target_link_libraries(mc_test_piston_inventory_texture PRIVATE
        mc_registry mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_piston_inventory_texture PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.piston_inventory_texture COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/piston_inventory_texture/run_native.py"
        --executable "$<TARGET_FILE:mc_test_piston_inventory_texture>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/piston-inventory-texture-controls")
    set_tests_properties(client.piston_inventory_texture PROPERTIES
        LABELS "public;unit;client;graphics;ownership;reference;replay" TIMEOUT 60)
    add_test(NAME tools.piston_inventory_texture_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/piston_inventory_texture" -p test_packet.py)
    set_tests_properties(tools.piston_inventory_texture_packet PROPERTIES
        LABELS "public;unit;tools;client;graphics;reference" TIMEOUT 60)
endif()
