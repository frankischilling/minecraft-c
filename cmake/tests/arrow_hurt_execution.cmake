include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_arrow_hurt_execution tests/world/runtime/test_arrow_hurt_execution.c)
    target_include_directories(mc_test_arrow_hurt_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_hurt_execution PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    foreach(_mc_hurt_mode IN ITEMS pig-impact natural-pig)
    add_test(NAME "world.arrow_hurt_execution_${_mc_hurt_mode}" COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_arrow_hurt_execution.py"
        --mode "${_mc_hurt_mode}" --executable "$<TARGET_FILE:mc_test_arrow_hurt_execution>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-hurt-execution-controls")
    set_tests_properties("world.arrow_hurt_execution_${_mc_hurt_mode}" PROPERTIES
        LABELS "public;integration;world;entity;ownership" TIMEOUT 300)
    endforeach()
    unset(_mc_hurt_mode)
endif()
