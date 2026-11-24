include_guard(GLOBAL)

if(BUILD_TESTING)
    add_test(NAME tools.named_entity_factory_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/named_factory/test_packet.py")
    set_tests_properties(tools.named_entity_factory_packet PROPERTIES
        LABELS "public;unit;tools;entity;reference" TIMEOUT 90)
    add_executable(mc_test_named_entity_factory tests/entity/factory/test_named.c)
    target_include_directories(mc_test_named_entity_factory PRIVATE src src/core/java)
    target_link_libraries(mc_test_named_entity_factory PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.named_factory COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/named_factory/run_native.py"
        --executable "$<TARGET_FILE:mc_test_named_entity_factory>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/named-entity-factory")
    set_tests_properties(entity.named_factory PROPERTIES
        LABELS "public;integration;entity;reference;ownership" TIMEOUT 180)
endif()
