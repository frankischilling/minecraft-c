include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_executable(mc_test_redstone_torch_texture tests/block/registry/redstone_torch/texture.c)
    add_executable(mc_test_redstone_torch_texture_detail tests/block/registry/redstone_torch/texture_detail.c)
    foreach(target IN ITEMS mc_test_redstone_torch_texture mc_test_redstone_torch_texture_detail)
        target_include_directories(${target} PRIVATE src)
        target_link_libraries(${target} PRIVATE mc_registry mc_c_atomics mc_build_options)
    endforeach()
    add_test(NAME tools.redstone_torch_texture_packet COMMAND Python3::Interpreter -B
        -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/redstone_torch_texture"
        -p test_packet.py)
    add_test(NAME tools.redstone_torch_texture_comparator COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/block/test_redstone_torch_texture.py" --controls-only)
    add_test(NAME block.redstone_torch_texture_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/block/test_redstone_torch_texture.py"
        --execute --executable "$<TARGET_FILE:mc_test_redstone_torch_texture>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/redstone-torch-texture")
    add_test(NAME block.redstone_torch_texture_detail COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/block/test_redstone_torch_texture.py"
        --execute --detail --executable "$<TARGET_FILE:mc_test_redstone_torch_texture_detail>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/redstone-torch-texture-detail")
    set_tests_properties(tools.redstone_torch_texture_packet tools.redstone_torch_texture_comparator
        block.redstone_torch_texture_reference block.redstone_torch_texture_detail PROPERTIES
        LABELS "public;unit;block;reference" TIMEOUT 60)
endif()
