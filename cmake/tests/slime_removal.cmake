include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_slime_removal tests/world/runtime/test_slime_removal.c)
    target_include_directories(mc_test_slime_removal PRIVATE src src/platform/time)
    target_link_libraries(mc_test_slime_removal PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    foreach(_mc_slime_mode IN ITEMS split negative-health direct-split size-one size-zero odd-size
            signed-size negative-size remote positive-health null-world watcher-type
            callback-pose callback-size callback-reseed callback-null-world)
        add_test(NAME "entity.slime_removal_${_mc_slime_mode}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_slime_removal.py"
            --mode "${_mc_slime_mode}" --executable "$<TARGET_FILE:mc_test_slime_removal>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/slime-removal-controls")
        set_tests_properties("entity.slime_removal_${_mc_slime_mode}" PROPERTIES
            LABELS "public;integration;entity;world;ownership" TIMEOUT 300)
    endforeach()
    unset(_mc_slime_mode)
    foreach(_mc_slime_python IN ITEMS normal optimized)
        set(_mc_slime_reference_flags -B)
        if(_mc_slime_python STREQUAL "optimized")
            list(APPEND _mc_slime_reference_flags -O)
        endif()
        add_test(NAME "tools.slime_removal_reference_${_mc_slime_python}" COMMAND
            Python3::Interpreter ${_mc_slime_reference_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/slime_removal/test_packet.py")
        set_tests_properties("tools.slime_removal_reference_${_mc_slime_python}" PROPERTIES
            LABELS "public;unit;tools;entity;replay" TIMEOUT 60)
    endforeach()
    unset(_mc_slime_reference_flags)
    unset(_mc_slime_python)
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(_mc_slime_python_mode IN ITEMS normal optimized)
        set(_mc_slime_python_flags -B)
        if(_mc_slime_python_mode STREQUAL "optimized")
            list(APPEND _mc_slime_python_flags -O)
        endif()
        add_test(NAME "tools.slime_removal_protocol_${_mc_slime_python_mode}" COMMAND
            Python3::Interpreter ${_mc_slime_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_slime_removal.py" --self-test)
        set_tests_properties("tools.slime_removal_protocol_${_mc_slime_python_mode}" PROPERTIES
            LABELS "public;unit;tools;entity;world;ownership" TIMEOUT 60)
    endforeach()
    unset(_mc_slime_python_flags)
    unset(_mc_slime_python_mode)
endif()
