# Actual source rj/qj transport stays below World composition. mx/ld callers
# depend on canonical provider/Registry/World owners and belong to simulation.
target_sources(mc_persistence PRIVATE
    src/world/storage/java_random_access.c
    src/world/storage/source_regions.c)
target_sources(mc_simulation PRIVATE src/world/storage/source_chunk_loader.c)
