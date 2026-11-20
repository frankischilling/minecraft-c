# The control includes the canonical Writer body once. The linker map must
# exclude the archive's Writer object.
add_executable(mc_test_source_encoder tests/core/java/test_source_encoder.c)
target_include_directories(mc_test_source_encoder PRIVATE include src)
target_link_libraries(mc_test_source_encoder PRIVATE mc_core mc_build_options)
if(MSVC)
    target_link_options(mc_test_source_encoder PRIVATE
        "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_encoder.map")
else()
    target_link_options(mc_test_source_encoder PRIVATE
        "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_encoder.map")
endif()
add_test(NAME core.java_source_encoder COMMAND mc_test_source_encoder)
set_tests_properties(core.java_source_encoder PROPERTIES
    LABELS "public;unit;core;ownership" TIMEOUT 120)
