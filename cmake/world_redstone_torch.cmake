include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND AND TARGET mc_simulation)
    add_executable(mc_test_redstone_torch_reference
        tests/world/runtime/redstone_torch/current_caller.c
        tests/world/runtime/redstone_torch/emit_current.c
        tests/world/runtime/redstone_torch/native_main.c)
    target_include_directories(mc_test_redstone_torch_reference PRIVATE src)
    target_link_libraries(mc_test_redstone_torch_reference PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_build_options)
    set_property(TARGET mc_test_redstone_torch_reference PROPERTY MC_NO_SHELL32_DELAYLOAD TRUE)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_redstone_torch_reference PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME world.redstone_torch_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_redstone_torch.py"
        --executable "$<TARGET_FILE:mc_test_redstone_torch_reference>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/redstone-torch-reference")
    set_tests_properties(world.redstone_torch_reference PROPERTIES
        LABELS "public;integration;world;ownership;reference;filesystem" TIMEOUT 400)
    add_test(NAME tools.redstone_torch_controller COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world" -p test_redstone_torch_controller.py)
    set_tests_properties(tools.redstone_torch_controller PROPERTIES
        LABELS "public;unit;tools;world;reference" TIMEOUT 60)
    add_test(NAME tools.redstone_torch_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/redstone_torch" -p test_packet.py)
    set_tests_properties(tools.redstone_torch_packet PROPERTIES
        LABELS "public;unit;tools;world;reference" TIMEOUT 60)
endif()
