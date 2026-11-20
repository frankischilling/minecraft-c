include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_executable(mc_test_item_flint_and_steel_reference
        tests/item/use/flint_and_steel_reference.c)
    target_include_directories(mc_test_item_flint_and_steel_reference PRIVATE src src/platform/time)
    target_link_libraries(mc_test_item_flint_and_steel_reference PRIVATE
        mc_client_singleplayer_controller mc_resources mc_c_atomics mc_build_options)
    add_test(NAME item.flint_and_steel_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/flint_and_steel/run_native.py"
        --executable "$<TARGET_FILE:mc_test_item_flint_and_steel_reference>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-flint-and-steel-reference")
    set_tests_properties(item.flint_and_steel_reference PROPERTIES
        LABELS "public;integration;item;world;client;statistics;ownership;reference;replay" TIMEOUT 600)
    foreach(_mc_flint_mode IN ITEMS normal optimized)
        set(_mc_flint_options -B)
        if(_mc_flint_mode STREQUAL "optimized")
            list(APPEND _mc_flint_options -O)
        endif()
        foreach(_mc_flint_reader IN ITEMS packet native)
            add_test(NAME "tools.flint_and_steel_${_mc_flint_reader}_${_mc_flint_mode}"
                COMMAND Python3::Interpreter ${_mc_flint_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/flint_and_steel/test_${_mc_flint_reader}.py")
            set_tests_properties("tools.flint_and_steel_${_mc_flint_reader}_${_mc_flint_mode}" PROPERTIES
                LABELS "public;unit;tools;item;world;reference" TIMEOUT 240)
        endforeach()
    endforeach()
endif()
