# Include after the canonical mc_java_outputs and mc_core targets exist.
# This storage-level owner prevents any core -> storage dependency cycle.
add_library(mc_java_file_writers STATIC src/world/storage/java_file_writer.c)
target_include_directories(mc_java_file_writers PUBLIC include)
target_link_libraries(mc_java_file_writers PUBLIC mc_java_outputs mc_core
    PRIVATE Threads::Threads mc_build_options)
