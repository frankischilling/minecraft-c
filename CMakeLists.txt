cmake_minimum_required(VERSION 3.24)
project(minecraft_c VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MC_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(MC_ENABLE_JAVA_REFERENCE_TESTS "Compare numeric and collection helpers with a local JDK" OFF)
option(MC_ENABLE_MATH_REFERENCE_TESTS "Compare float lookup and floor helpers with originals" OFF)
option(MC_ENABLE_NBT_REFERENCE_TESTS "Compare NBT with locally supplied original JARs" OFF)
option(MC_ENABLE_COMPRESSION_REFERENCE_TESTS "Compare compression with original helpers" OFF)
option(MC_ENABLE_NOISE_REFERENCE_TESTS "Compare terrain noise with original samplers" OFF)
option(MC_ENABLE_SIMPLEX_REFERENCE_TESTS "Compare 2D simplex noise with original samplers" OFF)
option(MC_ENABLE_BIOME_REFERENCE_TESTS "Compare climate and biomes with original methods" OFF)
option(MC_ENABLE_DENSITY_REFERENCE_TESTS "Compare normal density and base terrain with originals" OFF)
option(MC_ENABLE_SURFACE_REFERENCE_TESTS "Compare normal surface replacement with originals" OFF)
option(MC_ENABLE_CAVES_REFERENCE_TESTS "Compare normal cave scheduling and carving with originals" OFF)
option(MC_ENABLE_NIBBLES_REFERENCE_TESTS "Compare packed nibble access with original classes" OFF)
option(MC_ENABLE_CHUNK_REFERENCE_TESTS "Compare local chunk initialization with original methods" OFF)
option(MC_ENABLE_LIGHT_REFERENCE_TESTS "Compare light regions and ordered queues with originals" OFF)
option(MC_ENABLE_FINISH_REFERENCE_TESTS "Compare full normal chunk finishing with original traces" OFF)
option(MC_ENABLE_GENERATION_REFERENCE_TESTS "Compare complete normal generation with original traces" OFF)
option(MC_ENABLE_REGION_CAPTURE_TESTS "Compare region reader with recorded observations" OFF)
option(MC_ENABLE_REGION_REFERENCE_TESTS "Compare region inspection with untouched original readers" OFF)
option(MC_ENABLE_CACHE_REFERENCE_TESTS "Compare synchronous acquisition with retained original providers" OFF)
option(MC_ENABLE_STORAGE_REFERENCE_TESTS "Compare world persistence with original storage methods" OFF)
option(MC_ENABLE_LIFECYCLE_REFERENCE_TESTS "Compare copied-world save/unload lifecycle with original controls" OFF)
set(MC_REFERENCE_CLIENT_JAR "" CACHE FILEPATH "Local verified original client JAR")
set(MC_REFERENCE_SERVER_JAR "" CACHE FILEPATH "Local verified original server JAR")
set(MC_REFERENCE_TESTED_COMMIT "" CACHE STRING "Exact source commit for local reference reports")
set(MC_REGION_CAPTURE_MANIFEST "" CACHE FILEPATH "Local recorded region observation manifest")

# Strict diagnostics apply to project-owned code; dependencies share sanitizers only.
add_library(mc_build_options INTERFACE)
add_library(mc_sanitizers INTERFACE)
target_link_libraries(mc_build_options INTERFACE mc_sanitizers)
if(MSVC)
    target_compile_options(mc_build_options INTERFACE /W4 /WX /fp:strict)
else()
    target_compile_options(mc_build_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
        -fno-fast-math -ffp-contract=off)
endif()
if(MC_ENABLE_SANITIZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR MSVC)
        message(FATAL_ERROR "MC_ENABLE_SANITIZERS requires GCC or Clang with ASan/UBSan")
    endif()
    target_compile_options(mc_sanitizers INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mc_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

# Official C source archive, verified against the release site's SHA-256.
# For offline builds set FETCHCONTENT_SOURCE_DIR_MC_ZLIB to an unpacked copy.
include(FetchContent)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mc_zlib
    URL https://zlib.net/fossils/zlib-1.3.2.tar.gz
    URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(mc_zlib)
target_link_libraries(zlibstatic PRIVATE mc_sanitizers)

add_library(mc_platform_time STATIC src/platform/time/nanotime.c src/platform/time/millis.c)
target_link_libraries(mc_platform_time PRIVATE mc_build_options)

add_library(mc_platform_text STATIC src/platform/text/charset.c src/platform/text/locale.c
    src/platform/text/stdout.c)
target_include_directories(mc_platform_text PRIVATE include)
target_link_libraries(mc_platform_text PRIVATE Threads::Threads mc_build_options)

find_package(Threads REQUIRED)
add_library(mc_c_atomics INTERFACE)
if(MSVC)
    target_compile_options(mc_c_atomics INTERFACE /experimental:c11atomics)
endif()

# The shared core has no window, graphics, audio, JDK, or asset dependencies.
add_library(mc_core STATIC
    src/core/java/int_set.c
    src/core/java/java.c
    src/core/java/format.c
    src/core/java/format_case.c
    src/core/java/properties.c
    src/core/java/reader.c
    src/core/java/random_profile.c
    src/core/math/math.c
    src/core/math/strict.c
    src/core/math/fdlibm/log.c
    src/core/text/mutf8.c)
target_include_directories(mc_core PUBLIC include)
target_link_libraries(mc_core PRIVATE mc_platform_time mc_platform_text Threads::Threads mc_c_atomics mc_build_options)
if(UNIX)
    target_link_libraries(mc_core PRIVATE m)
endif()

add_library(mc_storage STATIC
    src/storage/nbt/nbt.c
    src/storage/nbt/tree.c
    src/storage/nbt/overlay.c
    src/storage/level/level.c
    src/storage/compression/compression.c
    src/storage/region/region.c
    src/storage/region/writer.c)
target_link_libraries(mc_storage PUBLIC mc_core PRIVATE mc_build_options ZLIB::ZLIBSTATIC)

add_library(mc_item STATIC src/item/stack/stack.c)
target_link_libraries(mc_item PUBLIC mc_storage PRIVATE mc_build_options)

add_library(mc_block STATIC src/block/material/material.c)
target_include_directories(mc_block PUBLIC include)
target_link_libraries(mc_block PRIVATE mc_build_options)

# Passive Entity class/literal/namespace ownership stays below the Registry.
# Concrete Entity construction and World/NBT methods remain in simulation.
add_library(mc_entity_classes STATIC
    src/entity/nbt/errors.c
    src/entity/classes/classes.c
    src/entity/bounds/bounds.c
    src/entity/registry/registry.c)
target_link_libraries(mc_entity_classes PUBLIC mc_core PRIVATE mc_build_options)

add_library(mc_registry STATIC
    src/block/registry/registry.c
    src/block/registry/runtime.c
    src/block/registry/directions.c
    src/block/registry/falling.c
    src/block/registry/class_initializers.c
    src/block/registry/constructors.c
    src/block/registry/registration.c
    src/block/registry/opacity.c
    src/block/registry/texture.c
    src/block/registry/bootstrap.c
    src/item/registry/registry.c
    src/item/registry/runtime.c
    src/item/registry/class_initializers.c
    src/item/registry/constructors.c
    src/item/registry/registration.c
    src/item/registry/stacks.c
    src/item/recipes/registration.c
    src/item/recipes/families.c
    src/item/recipes/explicit.c
    src/item/recipes/managers.c
    src/item/recipes/smelting.c
    src/resources/translation/translation.c
    src/resources/translation/format.c
    src/statistics/keys/keys.c
    src/statistics/achievements/achievements.c
    src/statistics/registry/base.c
    src/statistics/registry/arrays.c
    src/statistics/registry/names.c
    src/statistics/registry/format.c
    src/statistics/registry/initializer.c
    src/statistics/registry/registration.c
    src/world/biome/registry.c
    src/world/biome/sampling.c
    src/world/biome/fixed_sampling.c)
target_link_libraries(mc_registry PUBLIC mc_block mc_core mc_item mc_world mc_entity_classes PRIVATE mc_platform_text mc_build_options)

add_library(mc_resources STATIC src/resources/files/files.c)
target_link_libraries(mc_resources PUBLIC mc_registry PRIVATE mc_build_options)

add_library(mc_world_info STATIC src/world/info/info.c)
target_include_directories(mc_world_info PRIVATE src/platform/time)
target_link_libraries(mc_world_info PUBLIC mc_storage PRIVATE mc_platform_time mc_build_options)

add_executable(mc_region_inspect src/tools/storage/region_inspect.c)
target_link_libraries(mc_region_inspect PRIVATE mc_storage mc_build_options)
if(MINGW)
    target_link_options(mc_region_inspect PRIVATE -municode)
endif()

add_library(mc_world STATIC
    src/world/runtime/loading/errors.c
    src/world/runtime/tiles/errors.c
    src/world/schedule/entry.c
    src/world/schedule/membership.c
    src/world/schedule/ordered.c
    src/world/generation/normal.c
    src/world/cache/cache.c
    src/world/cache/acquire.c
    src/world/cache/lifecycle.c
    src/world/chunk/finish.c
    src/world/lighting/world.c
    src/world/lighting/queue.c
    src/world/chunk/data.c
    src/world/chunk/object.c
    src/world/chunk/nibbles.c
    src/world/noise/noise.c
    src/world/noise/simplex.c
    src/world/biome/biome.c
    src/world/terrain/terrain.c
    src/world/terrain/surface.c
    src/world/terrain/caves.c)
target_link_libraries(mc_world PUBLIC mc_core mc_entity_classes PRIVATE mc_build_options)

add_library(mc_persistence STATIC
    src/world/storage/status.c
    src/world/storage/files.c
    src/world/storage/regions.c
    src/world/storage/store.c
    src/world/storage/chunk.c)
target_link_libraries(mc_persistence PUBLIC mc_world mc_storage mc_world_info PRIVATE mc_build_options)

add_library(mc_world_saved_data STATIC src/world/saved_data/storage.c)
target_include_directories(mc_world_saved_data PRIVATE src/world/storage)
target_link_libraries(mc_world_saved_data PUBLIC mc_persistence PRIVATE mc_build_options)

add_library(mc_simulation STATIC
    src/entity/movement/movement.c
    src/entity/movement/effects.c
    src/entity/nbt/factory.c
    src/entity/nbt/nbt.c
    src/entity/nbt/storage.c
    src/world/runtime/tiles/tiles.c
    src/world/runtime/tiles/piston_math.c
    src/world/runtime/loading/loading.c
    src/world/runtime/saving/saving.c
    src/world/runtime/runtime.c
    src/world/runtime/activation.c
    src/world/runtime/registry.c
    src/world/runtime/construction.c
    src/world/runtime/environment.c
    src/world/runtime/movement.c
    src/world/runtime/contact.c
    src/world/runtime/blocks.c
    src/world/runtime/height.c
    src/world/runtime/queries.c
    src/world/runtime/plants.c
    src/world/runtime/population/plants.c
    src/world/runtime/population/trees.c
    src/world/runtime/population/ore_clay.c
    src/world/runtime/population/lake_spring.c
    src/world/runtime/population/dungeon.c
    src/world/runtime/population/normal.c
    src/world/runtime/lighting.c
    src/world/runtime/chunks.c
    src/world/runtime/presence.c
    src/world/runtime/solid_support.c
    src/world/runtime/presentation.c
    src/world/runtime/entity_publication.c
    src/world/runtime/collision/entities.c
    src/world/runtime/collision/boxes.c
    src/world/runtime/collision/blocks.c
    src/world/chunk/entities.c
    src/world/runtime/scheduling.c
    src/world/runtime/mutation/support.c
    src/world/runtime/mutation/chunk.c
    src/world/runtime/mutation/height.c
    src/world/runtime/mutation/observers.c
    src/world/runtime/mutation/hooks.c
    src/world/runtime/mutation/ticks.c
    src/world/runtime/mutation/fluids.c
    src/world/runtime/mutation/falling.c
    src/world/runtime/mutation/snow.c
    src/world/runtime/mutation/drops.c
    src/world/runtime/mutation/world.c
    src/world/runtime/provider.c
    src/world/runtime/generation/generation.c
    src/world/runtime/generation/surface.c
    src/world/runtime/generation/caves.c
    src/world/dimension/dimension.c
    src/world/dimension/temperature.c
    src/entity/entity.c)
target_include_directories(mc_simulation PRIVATE src/platform/time)
target_link_libraries(mc_simulation PUBLIC mc_persistence mc_world_saved_data mc_item mc_registry
    PRIVATE mc_platform_time mc_platform_text mc_build_options)

include(CTest)
if(BUILD_TESTING)
    add_executable(mc_test_world_info tests/world/info/test_info.c)
    target_include_directories(mc_test_world_info PRIVATE src/platform/time)
    target_link_libraries(mc_test_world_info PRIVATE mc_world_info mc_platform_time mc_build_options)
    add_test(NAME world.info COMMAND mc_test_world_info)
    set_tests_properties(world.info PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_world_info_observation_probe tests/world/info/observation_probe.c)
    target_link_libraries(mc_world_info_observation_probe PRIVATE mc_world_info mc_build_options)

    add_executable(mc_test_material tests/block/material/test_material.c)
    target_link_libraries(mc_test_material PRIVATE mc_block mc_build_options)
    add_test(NAME block.material COMMAND mc_test_material)
    set_tests_properties(block.material PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_block_registry tests/block/registry/test_registry.c)
    target_link_libraries(mc_test_block_registry PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.registry_construction COMMAND mc_test_block_registry)
    set_tests_properties(block.registry_construction PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_test_biome_registry tests/world/biome/test_registry.c)
    target_link_libraries(mc_test_biome_registry PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME world.biome_registry COMMAND mc_test_biome_registry)
    set_tests_properties(world.biome_registry PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_test_biome_sampling tests/world/biome/test_sampling.c)
    target_link_libraries(mc_test_biome_sampling PRIVATE mc_registry mc_build_options)
    add_test(NAME world.biome_reference_sampling COMMAND mc_test_biome_sampling)
    set_tests_properties(world.biome_reference_sampling PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_biome_registry_probe tests/world/biome/registry_probe.c)
    target_link_libraries(mc_biome_registry_probe PRIVATE mc_resources mc_registry mc_build_options)

    add_executable(mc_test_item_registry tests/item/registry/test_registry.c)
    target_link_libraries(mc_test_item_registry PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME item.registry_construction COMMAND mc_test_item_registry)
    set_tests_properties(item.registry_construction PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_test_item_registry_runtime tests/item/registry/test_runtime.c)
    target_link_libraries(mc_test_item_registry_runtime PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME item.registry_runtime COMMAND mc_test_item_registry_runtime)
    set_tests_properties(item.registry_runtime PROPERTIES LABELS "public;unit;ownership" TIMEOUT 90)

    add_executable(mc_registry_prefix_probe tests/block/registry/prefix_probe.c)
    target_link_libraries(mc_registry_prefix_probe PRIVATE mc_registry mc_build_options)

    add_executable(mc_item_registry_prefix_probe tests/item/registry/prefix_probe.c)
    target_link_libraries(mc_item_registry_prefix_probe PRIVATE mc_registry mc_build_options)

    add_executable(mc_material_probe tests/block/material/material_probe.c)
    target_link_libraries(mc_material_probe PRIVATE mc_block mc_build_options)

    add_executable(mc_test_millis tests/platform/time/test_millis.c)
    target_include_directories(mc_test_millis PRIVATE src/platform/time)
    target_link_libraries(mc_test_millis PRIVATE mc_platform_time mc_build_options)
    add_test(NAME platform.millis COMMAND mc_test_millis)
    set_tests_properties(platform.millis PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_nanotime tests/platform/time/test_nanotime.c)
    target_include_directories(mc_test_nanotime PRIVATE src/platform/time)
    target_link_libraries(mc_test_nanotime PRIVATE mc_platform_time mc_build_options)
    add_test(NAME platform.nanotime COMMAND mc_test_nanotime)
    set_tests_properties(platform.nanotime PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_nanotime_conversion tests/platform/time/test_nanotime_conversion.c)
    target_include_directories(mc_test_nanotime_conversion PRIVATE src/platform/time tests/reference/java)
    target_link_libraries(mc_test_nanotime_conversion PRIVATE mc_platform_time mc_build_options)
    add_test(NAME platform.nanotime_conversion COMMAND mc_test_nanotime_conversion)
    set_tests_properties(platform.nanotime_conversion PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_java_random_profile tests/core/java/test_random_profile.c)
    target_link_libraries(mc_test_java_random_profile PRIVATE mc_core mc_c_atomics Threads::Threads mc_build_options)
    add_test(NAME core.java_random_profile COMMAND mc_test_java_random_profile)
    set_tests_properties(core.java_random_profile PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_random_constructor_observation tests/core/java/test_random_constructor_observation.c)
    target_link_libraries(mc_test_random_constructor_observation PRIVATE mc_core mc_c_atomics mc_build_options)
    add_test(NAME core.java_random_constructor_observation COMMAND mc_test_random_constructor_observation)
    set_tests_properties(core.java_random_constructor_observation PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_item_stack tests/item/stack/test_stack.c)
    target_link_libraries(mc_test_item_stack PRIVATE mc_item mc_build_options)
    add_test(NAME item.stack COMMAND mc_test_item_stack)
    set_tests_properties(item.stack PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_registry_stacks tests/item/registry/test_stacks.c)
    target_link_libraries(mc_test_registry_stacks PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME item.registry_stacks COMMAND mc_test_registry_stacks)
    set_tests_properties(item.registry_stacks PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_recipe_bootstrap_probe tests/item/recipes/bootstrap_probe.c)
    target_link_libraries(mc_recipe_bootstrap_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_test_recipes tests/item/recipes/test_recipes.c)
    target_link_libraries(mc_test_recipes PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME item.recipes COMMAND mc_test_recipes)
    set_tests_properties(item.recipes PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_stack_values_probe tests/item/registry/stack_values_probe.c)
    target_link_libraries(mc_stack_values_probe PRIVATE mc_registry mc_build_options)

    add_executable(mc_test_entity tests/entity/test_entity.c)
    target_include_directories(mc_test_entity PRIVATE src/entity src/core/java)
    target_link_libraries(mc_test_entity PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.construction COMMAND mc_test_entity)
    set_tests_properties(entity.construction PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_entity_bounds tests/entity/bounds/test_bounds.c)
    target_link_libraries(mc_test_entity_bounds PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.bounds COMMAND mc_test_entity_bounds)
    set_tests_properties(entity.bounds PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_entity_bounds_values_probe tests/entity/bounds/values_probe.c)
    target_link_libraries(mc_entity_bounds_values_probe PRIVATE mc_entity_classes mc_c_atomics mc_build_options)

    add_executable(mc_test_falling_entity tests/entity/test_falling.c)
    target_include_directories(mc_test_falling_entity PRIVATE src/entity src/core/java)
    target_link_libraries(mc_test_falling_entity PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.falling COMMAND mc_test_falling_entity)
    set_tests_properties(entity.falling PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_entity_nbt tests/entity/nbt/test_nbt.c)
    target_include_directories(mc_test_entity_nbt PRIVATE src/entity src/core/java)
    target_link_libraries(mc_test_entity_nbt PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.nbt COMMAND mc_test_entity_nbt)
    set_tests_properties(entity.nbt PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_entity_classes tests/entity/classes/test_classes.c)
    target_link_libraries(mc_test_entity_classes PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.classes COMMAND mc_test_entity_classes)
    set_tests_properties(entity.classes PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_entity_factory tests/entity/nbt/test_factory.c)
    target_include_directories(mc_test_entity_factory PRIVATE src/entity src/core/java)
    target_link_libraries(mc_test_entity_factory PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.factory COMMAND mc_test_entity_factory)
    set_tests_properties(entity.factory PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_probe_entity_nbt tests/entity/nbt/nbt_probe.c)
    target_include_directories(mc_probe_entity_nbt PRIVATE src/entity src/core/java)
    target_link_libraries(mc_probe_entity_nbt PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_entity_batch tests/world/runtime/entity_batch/test_entity_batch.c)
    target_link_libraries(mc_test_world_entity_batch PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_entity_item_formula_probe tests/entity/item_formula_probe.c)
    target_link_libraries(mc_entity_item_formula_probe PRIVATE mc_simulation mc_build_options)

    add_executable(mc_test_scheduled_membership tests/world/schedule/test_membership.c)
    target_link_libraries(mc_test_scheduled_membership PRIVATE mc_world mc_build_options)
    add_test(NAME world.scheduled_membership COMMAND mc_test_scheduled_membership)
    set_tests_properties(world.scheduled_membership PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_scheduled_ordered tests/world/schedule/test_ordered.c)
    target_link_libraries(mc_test_scheduled_ordered PRIVATE mc_world mc_build_options)
    add_test(NAME world.scheduled_ordered COMMAND mc_test_scheduled_ordered)
    set_tests_properties(world.scheduled_ordered PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_scheduled_ordered_probe tests/world/schedule/ordered_probe.c)
    target_link_libraries(mc_scheduled_ordered_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_stored_chunk tests/world/storage/test_chunk.c)
    target_link_libraries(mc_test_stored_chunk PRIVATE mc_persistence mc_build_options)
    add_test(NAME world.stored_chunk COMMAND mc_test_stored_chunk)
    set_tests_properties(world.stored_chunk PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_stored_chunk_probe tests/world/storage/chunk_probe.c)
    target_link_libraries(mc_stored_chunk_probe PRIVATE mc_persistence mc_build_options)

    add_executable(mc_world_regions_probe tests/world/storage/regions_probe.c)
    target_link_libraries(mc_world_regions_probe PRIVATE mc_persistence mc_build_options)

    add_executable(mc_world_store_probe tests/world/storage/store_probe.c)
    target_link_libraries(mc_world_store_probe PRIVATE mc_persistence mc_build_options)

    add_executable(mc_test_world_runtime tests/world/runtime/test_runtime.c)
    target_link_libraries(mc_test_world_runtime PRIVATE mc_simulation mc_build_options)

    add_executable(mc_test_world_info_binding tests/world/runtime/test_info_binding.c)
    target_link_libraries(mc_test_world_info_binding PRIVATE mc_simulation mc_build_options)

    add_executable(mc_test_world_typed_info tests/world/storage/test_typed_info.c)
    target_include_directories(mc_test_world_typed_info PRIVATE src/world/storage)
    target_link_libraries(mc_test_world_typed_info PRIVATE mc_persistence mc_build_options)

    add_executable(mc_test_world_environment tests/world/runtime/test_environment.c)
    target_include_directories(mc_test_world_environment PRIVATE src/world/runtime)
    target_link_libraries(mc_test_world_environment PRIVATE mc_simulation mc_build_options)
    add_test(NAME world.environment COMMAND mc_test_world_environment)
    set_tests_properties(world.environment PROPERTIES LABELS "public;unit;math" TIMEOUT 30)

    add_executable(mc_test_world_construction tests/world/runtime/test_world_construction.c)
    target_link_libraries(mc_test_world_construction PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.construction COMMAND mc_test_world_construction)
    set_tests_properties(world.construction PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_game_world tests/world/runtime/test_game_world.c)
    target_link_libraries(mc_test_game_world PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_registry tests/world/runtime/test_world_registry.c)
    target_link_libraries(mc_test_world_registry PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_biome_runtime tests/world/dimension/test_biome_runtime.c)
    target_link_libraries(mc_test_world_biome_runtime PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_fixed_biome_runtime tests/world/dimension/test_fixed_biome_runtime.c)
    target_link_libraries(mc_test_fixed_biome_runtime PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_temperature tests/world/dimension/test_temperature.c)
    target_link_libraries(mc_test_temperature PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_lighting_runtime tests/world/runtime/test_lighting_runtime.c)
    target_link_libraries(mc_test_world_lighting_runtime PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_light_queue_runtime tests/world/runtime/test_light_queue_runtime.c)
    target_link_libraries(mc_test_world_light_queue_runtime PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_chunk_objects tests/world/runtime/test_chunk_objects.c)
    target_link_libraries(mc_test_world_chunk_objects PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_provider_generation tests/world/runtime/test_provider_generation.c)
    target_link_libraries(mc_test_world_provider_generation PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_provider_activation tests/world/runtime/test_provider_activation.c)
    target_link_libraries(mc_test_world_provider_activation PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_mutations tests/world/runtime/test_mutations.c)
    target_link_libraries(mc_test_world_mutations PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_scheduling tests/world/runtime/test_scheduling.c)
    target_link_libraries(mc_test_world_scheduling PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_solid_support tests/world/runtime/test_solid_support.c)
    target_link_libraries(mc_test_world_solid_support PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_queries tests/world/runtime/test_queries.c)
    target_link_libraries(mc_test_world_queries PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_nearby tests/world/runtime/collision/test_nearby.c)
    target_link_libraries(mc_test_world_nearby PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_collision_boxes tests/world/runtime/collision/test_boxes.c)
    target_link_libraries(mc_test_world_collision_boxes PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_piston_geometry tests/world/runtime/collision/test_piston_geometry.c)
    target_link_libraries(mc_test_piston_geometry PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_piston_tiles tests/world/runtime/tiles/test_piston.c)
    target_link_libraries(mc_test_piston_tiles PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_entity_movement tests/entity/movement/test_movement.c)
    target_link_libraries(mc_test_entity_movement PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_movement tests/entity/movement/test_world_movement.c)
    target_link_libraries(mc_test_world_movement PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_piston_float_probe tests/world/runtime/tiles/piston_float_probe.c)
    target_link_libraries(mc_piston_float_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_directions tests/block/registry/test_directions.c)
    target_link_libraries(mc_test_directions PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.directions COMMAND mc_test_directions)
    set_tests_properties(block.directions PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_world_nearby_values_probe tests/world/runtime/collision/nearby_probe.c)
    target_link_libraries(mc_world_nearby_values_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_world_block_shapes_probe tests/world/runtime/collision/block_shape_probe.c)
    target_link_libraries(mc_world_block_shapes_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_piston_geometry_probe tests/world/runtime/collision/piston_geometry_probe.c)
    target_link_libraries(mc_piston_geometry_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_fluids tests/world/runtime/test_fluids.c)
    target_link_libraries(mc_test_world_fluids PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_falling tests/world/runtime/test_falling.c)
    target_link_libraries(mc_test_world_falling PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_snow tests/world/runtime/test_snow.c)
    target_link_libraries(mc_test_world_snow PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_entity_publication tests/world/runtime/test_entity_publication.c)
    target_link_libraries(mc_test_world_entity_publication PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_tiles tests/world/runtime/tiles/test_tiles.c)
    target_link_libraries(mc_test_world_tiles PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_world_saving tests/world/runtime/saving/test_saving.c)
    target_link_libraries(mc_test_world_saving PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_game_entities tests/world/runtime/entities/test_game_entities.c)
    target_link_libraries(mc_test_game_entities PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_dungeon tests/world/runtime/population/test_dungeon.c)
    target_link_libraries(mc_test_dungeon PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_normal_population tests/world/runtime/population/test_normal.c)
    target_link_libraries(mc_test_normal_population PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_registry_falling tests/block/registry/test_falling.c)
    target_link_libraries(mc_test_registry_falling PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.registry_falling COMMAND mc_test_registry_falling)
    set_tests_properties(block.registry_falling PROPERTIES LABELS "public;integration;registry" TIMEOUT 90)

    add_executable(mc_dungeon_loot_probe tests/world/runtime/population/dungeon_loot_probe.c)
    target_link_libraries(mc_dungeon_loot_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_dungeon_placement_probe tests/world/runtime/population/dungeon_placement_probe.c)
    target_link_libraries(mc_dungeon_placement_probe PRIVATE mc_simulation mc_c_atomics mc_build_options)

    add_executable(mc_test_registry_runtime_literals tests/block/registry/test_runtime_literals.c)
    target_link_libraries(mc_test_registry_runtime_literals PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME block.registry_runtime_literals COMMAND mc_test_registry_runtime_literals)
    set_tests_properties(block.registry_runtime_literals PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_normal_provider_construction tests/world/generation/test_provider_construction.c)
    target_link_libraries(mc_test_normal_provider_construction PRIVATE mc_world mc_c_atomics mc_build_options)

    add_executable(mc_dimension_observation_probe tests/world/dimension/dimension_observation_probe.c)
    target_include_directories(mc_dimension_observation_probe PRIVATE src/world/dimension)
    target_link_libraries(mc_dimension_observation_probe PRIVATE mc_simulation mc_build_options)

    add_executable(mc_test_world_saved_data tests/world/saved_data/test_storage.c)
    target_include_directories(mc_test_world_saved_data PRIVATE src/world/storage)
    target_link_libraries(mc_test_world_saved_data PRIVATE mc_world_saved_data mc_build_options)

    add_executable(mc_saved_data_id_observation_probe tests/world/saved_data/observation_probe.c)
    target_link_libraries(mc_saved_data_id_observation_probe PRIVATE mc_world_saved_data mc_build_options)

    add_executable(mc_world_lifecycle_probe tests/world/runtime/lifecycle_probe.c)
    target_link_libraries(mc_world_lifecycle_probe PRIVATE mc_simulation mc_build_options)

    add_executable(mc_level_probe tests/storage/level/level_probe.c)
    target_link_libraries(mc_level_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_level tests/storage/level/test_level.c)
    target_link_libraries(mc_test_level PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.level COMMAND mc_test_level)
    set_tests_properties(storage.level PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_region_writer_probe tests/storage/region/writer_probe.c)
    target_link_libraries(mc_region_writer_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_region_writer tests/storage/region/test_writer.c)
    target_link_libraries(mc_test_region_writer PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.region_writer COMMAND mc_test_region_writer)
    set_tests_properties(storage.region_writer PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_chunk_acquire tests/world/cache/test_acquire.c)
    target_link_libraries(mc_test_chunk_acquire PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_acquire COMMAND mc_test_chunk_acquire)
    set_tests_properties(world.chunk_acquire PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_chunk_acquire_probe tests/world/cache/acquire_probe.c)
    target_link_libraries(mc_chunk_acquire_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_chunk_callback_probe tests/world/cache/callback_probe.c)
    target_link_libraries(mc_chunk_callback_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_chunk_lifecycle_callbacks_probe tests/world/cache/lifecycle_callbacks_probe.c)
    target_link_libraries(mc_chunk_lifecycle_callbacks_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_chunk_lifecycle tests/world/cache/test_lifecycle.c)
    target_link_libraries(mc_test_chunk_lifecycle PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_lifecycle COMMAND mc_test_chunk_lifecycle)
    set_tests_properties(world.chunk_lifecycle PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_chunk_pending tests/world/cache/test_pending.c)
    target_link_libraries(mc_test_chunk_pending PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_pending COMMAND mc_test_chunk_pending)
    set_tests_properties(world.chunk_pending PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_region tests/storage/region/test_region.c)
    target_link_libraries(mc_test_region PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.region COMMAND mc_test_region)
    set_tests_properties(storage.region PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_region_probe tests/storage/region/region_probe.c)
    target_link_libraries(mc_region_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_normal_generator tests/world/generation/test_normal.c)
    target_link_libraries(mc_test_normal_generator PRIVATE mc_world mc_build_options)
    add_test(NAME world.normal_generator COMMAND mc_test_normal_generator)
    set_tests_properties(world.normal_generator PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_normal_generation_probe tests/world/generation/normal_probe.c)
    target_link_libraries(mc_normal_generation_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_chunk_finish tests/world/chunk/test_finish.c)
    target_link_libraries(mc_test_chunk_finish PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_finish COMMAND mc_test_chunk_finish)
    set_tests_properties(world.chunk_finish PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_chunk_finish_probe tests/world/chunk/finish_probe.c)
    target_link_libraries(mc_chunk_finish_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_light_queue tests/world/lighting/test_queue.c)
    target_link_libraries(mc_test_light_queue PRIVATE mc_world mc_build_options)
    add_test(NAME world.light_queue COMMAND mc_test_light_queue)
    set_tests_properties(world.light_queue PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_light_queue_probe tests/world/lighting/queue_probe.c)
    target_link_libraries(mc_light_queue_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_java tests/core/java/test_java.c)
    target_link_libraries(mc_test_java PRIVATE mc_core mc_build_options)
    add_test(NAME core.java COMMAND mc_test_java)
    set_tests_properties(core.java PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_test_java_gaussian tests/core/java/test_gaussian.c)
    target_link_libraries(mc_test_java_gaussian PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_gaussian COMMAND mc_test_java_gaussian)
    set_tests_properties(core.java_gaussian PROPERTIES LABELS "public;unit;math" TIMEOUT 30)

    add_executable(mc_java_gaussian_probe tests/core/java/gaussian_probe.c)
    target_link_libraries(mc_java_gaussian_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_test_java_int_set tests/core/java/test_int_set.c)
    target_link_libraries(mc_test_java_int_set PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_int_set COMMAND mc_test_java_int_set)
    set_tests_properties(core.java_int_set PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_test_java_int_set_allocator tests/core/java/test_int_set_allocator.c)
    target_link_libraries(mc_test_java_int_set_allocator PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_int_set_allocator COMMAND mc_test_java_int_set_allocator)
    set_tests_properties(core.java_int_set_allocator PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_java_properties tests/core/java/test_properties.c)
    target_link_libraries(mc_test_java_properties PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_properties COMMAND mc_test_java_properties)
    set_tests_properties(core.java_properties PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_java_properties_probe tests/core/java/properties_probe.c)
    target_link_libraries(mc_java_properties_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_test_java_reader tests/core/java/test_reader.c)
    add_executable(mc_test_java_format tests/core/java/test_format.c)
    target_link_libraries(mc_test_java_format PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_format COMMAND mc_test_java_format)
    set_tests_properties(core.java_format PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)
    add_executable(mc_java_format_probe tests/core/java/format_probe.c)
    target_link_libraries(mc_java_format_probe PRIVATE mc_core mc_build_options)
    add_executable(mc_java_case_probe tests/core/java/case_probe.c)
    target_link_libraries(mc_java_case_probe PRIVATE mc_core mc_build_options)
    target_link_libraries(mc_test_java_reader PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_reader COMMAND mc_test_java_reader)
    set_tests_properties(core.java_reader PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_java_reader_probe tests/core/java/reader_probe.c)
    target_link_libraries(mc_java_reader_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_translation_file_probe tests/resources/translation/files_probe.c)
    target_link_libraries(mc_translation_file_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_test_translation tests/resources/translation/test_translation.c)
    target_link_libraries(mc_test_translation PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME resources.translation COMMAND mc_test_translation)
    set_tests_properties(resources.translation PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_statistic_keys tests/statistics/keys/test_keys.c)
    target_link_libraries(mc_test_statistic_keys PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME statistics.keys COMMAND mc_test_statistic_keys)
    set_tests_properties(statistics.keys PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_statistic_registry tests/statistics/registry/test_registry.c)
    target_link_libraries(mc_test_statistic_registry PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME statistics.registry COMMAND mc_test_statistic_registry)
    set_tests_properties(statistics.registry PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME statistics.locale_c_utf8
            COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C.UTF-8 $<TARGET_FILE:mc_test_statistic_registry> en)
        add_test(NAME statistics.locale_invalid_category
            COMMAND ${CMAKE_COMMAND} -E env LC_ALL= LANG=en_US.UTF-8 LC_CTYPE=C.UTF-8
                LC_NUMERIC=mc_invalid_locale $<TARGET_FILE:mc_test_statistic_registry> en_US)
        set_tests_properties(statistics.locale_c_utf8 statistics.locale_invalid_category
            PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)
    endif()

    add_executable(mc_statistic_registry_file_probe tests/statistics/registry/files_probe.c)
    target_link_libraries(mc_statistic_registry_file_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_statistic_arrays_probe tests/statistics/registry/arrays_probe.c)
    target_link_libraries(mc_statistic_arrays_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_test_statistic_arrays tests/statistics/registry/test_arrays.c)
    target_link_libraries(mc_test_statistic_arrays PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME statistics.arrays COMMAND mc_test_statistic_arrays)
    set_tests_properties(statistics.arrays PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_test_achievements tests/statistics/achievements/test_achievements.c)
    target_link_libraries(mc_test_achievements PRIVATE mc_registry mc_c_atomics mc_build_options)
    add_test(NAME statistics.achievements COMMAND mc_test_achievements)
    set_tests_properties(statistics.achievements PROPERTIES LABELS "public;unit;ownership" TIMEOUT 60)

    add_executable(mc_statistic_keys_file_probe tests/statistics/keys/files_probe.c)
    target_link_libraries(mc_statistic_keys_file_probe PRIVATE mc_resources mc_build_options)

    add_executable(mc_java_int_set_probe tests/core/java/int_set_probe.c)
    target_link_libraries(mc_java_int_set_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_math tests/core/math/test_math.c)
    target_link_libraries(mc_test_math PRIVATE mc_core mc_build_options)
    add_test(NAME core.math COMMAND mc_test_math)
    set_tests_properties(core.math PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_math_probe tests/core/math/math_probe.c)
    target_link_libraries(mc_math_probe PRIVATE mc_core mc_build_options)

    add_executable(mc_test_mutf8 tests/core/text/test_mutf8.c)
    target_link_libraries(mc_test_mutf8 PRIVATE mc_core mc_build_options)
    add_test(NAME core.mutf8 COMMAND mc_test_mutf8)
    set_tests_properties(core.mutf8 PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_test_nbt tests/storage/nbt/test_nbt.c)
    target_link_libraries(mc_test_nbt PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.nbt COMMAND mc_test_nbt)
    set_tests_properties(storage.nbt PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_nbt_probe tests/storage/nbt/nbt_probe.c)
    target_link_libraries(mc_nbt_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_nbt_overlay tests/storage/nbt/test_overlay.c)
    target_link_libraries(mc_test_nbt_overlay PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.nbt_overlay COMMAND mc_test_nbt_overlay)
    set_tests_properties(storage.nbt_overlay PROPERTIES LABELS "public;unit;storage" TIMEOUT 30)

    add_executable(mc_test_compression tests/storage/compression/test_compression.c)
    target_link_libraries(mc_test_compression PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.compression COMMAND mc_test_compression)
    set_tests_properties(storage.compression PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_compression_probe tests/storage/compression/compression_probe.c)
    target_link_libraries(mc_compression_probe PRIVATE mc_storage mc_build_options)

    add_executable(mc_test_chunk_data tests/world/chunk/test_data.c)
    target_link_libraries(mc_test_chunk_data PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_data COMMAND mc_test_chunk_data)
    set_tests_properties(world.chunk_data PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_test_chunk_nibbles tests/world/chunk/test_nibbles.c)
    target_link_libraries(mc_test_chunk_nibbles PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_nibbles COMMAND mc_test_chunk_nibbles)
    set_tests_properties(world.chunk_nibbles PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_test_chunk_reference_data tests/world/chunk/test_reference_data.c)
    target_link_libraries(mc_test_chunk_reference_data PRIVATE mc_world mc_build_options)
    add_test(NAME world.chunk_reference_data COMMAND mc_test_chunk_reference_data)
    set_tests_properties(world.chunk_reference_data PROPERTIES LABELS "public;unit" TIMEOUT 30)

    add_executable(mc_chunk_data_probe tests/world/chunk/data_probe.c)
    target_link_libraries(mc_chunk_data_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_chunk_nibbles_probe tests/world/chunk/nibbles_probe.c)
    target_link_libraries(mc_chunk_nibbles_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_noise tests/world/noise/test_noise.c)
    target_link_libraries(mc_test_noise PRIVATE mc_world mc_build_options)
    add_test(NAME world.noise COMMAND mc_test_noise)
    set_tests_properties(world.noise PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_noise_probe tests/world/noise/noise_probe.c)
    target_link_libraries(mc_noise_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_simplex tests/world/noise/test_simplex.c)
    target_link_libraries(mc_test_simplex PRIVATE mc_world mc_build_options)
    add_test(NAME world.simplex COMMAND mc_test_simplex)
    set_tests_properties(world.simplex PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_simplex_probe tests/world/noise/simplex_probe.c)
    target_link_libraries(mc_simplex_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_biome tests/world/biome/test_biome.c)
    target_link_libraries(mc_test_biome PRIVATE mc_world mc_build_options)
    add_test(NAME world.biome COMMAND mc_test_biome)
    set_tests_properties(world.biome PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_biome_probe tests/world/biome/biome_probe.c)
    target_link_libraries(mc_biome_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_terrain tests/world/terrain/test_terrain.c)
    target_link_libraries(mc_test_terrain PRIVATE mc_world mc_build_options)
    add_test(NAME world.terrain COMMAND mc_test_terrain)
    set_tests_properties(world.terrain PROPERTIES LABELS "public;unit" TIMEOUT 90)

    add_executable(mc_terrain_probe tests/world/terrain/terrain_probe.c)
    target_link_libraries(mc_terrain_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_surface tests/world/terrain/test_surface.c)
    target_link_libraries(mc_test_surface PRIVATE mc_world mc_build_options)
    add_test(NAME world.surface COMMAND mc_test_surface)
    set_tests_properties(world.surface PROPERTIES LABELS "public;unit" TIMEOUT 90)

    add_executable(mc_surface_probe tests/world/terrain/surface_probe.c)
    target_link_libraries(mc_surface_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_caves tests/world/terrain/test_caves.c)
    target_link_libraries(mc_test_caves PRIVATE mc_world mc_build_options)
    add_test(NAME world.caves COMMAND mc_test_caves)
    set_tests_properties(world.caves PROPERTIES LABELS "public;unit" TIMEOUT 90)

    add_executable(mc_caves_probe tests/world/terrain/caves_probe.c)
    target_link_libraries(mc_caves_probe PRIVATE mc_world mc_build_options)

    add_executable(mc_test_caves_geometry tests/world/terrain/test_caves_geometry.c)
    target_include_directories(mc_test_caves_geometry PRIVATE src/world/terrain)
    target_link_libraries(mc_test_caves_geometry PRIVATE mc_world mc_build_options)
    add_test(NAME world.caves_geometry COMMAND mc_test_caves_geometry)
    set_tests_properties(world.caves_geometry PROPERTIES LABELS "public;unit" TIMEOUT 90)

    add_executable(mc_caves_geometry_probe tests/world/terrain/caves_geometry_probe.c)
    target_include_directories(mc_caves_geometry_probe PRIVATE src/world/terrain)
    target_link_libraries(mc_caves_geometry_probe PRIVATE mc_world mc_build_options)

    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

    add_test(NAME entity.nbt_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_nbt.py"
        --executable "$<TARGET_FILE:mc_probe_entity_nbt>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/entity-nbt-observation")
    set_tests_properties(entity.nbt_observation PROPERTIES LABELS "public;fixture;integration" TIMEOUT 120)

    add_test(NAME world.entity_batch
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_entity_batch>"
        --scenario world-entity-batch
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-entity-batch-contracts")
    set_tests_properties(world.entity_batch PROPERTIES LABELS "public;integration;ownership" TIMEOUT 90)

    add_executable(mc_text_stdout_probe tests/platform/text/stdout_probe.c)
    target_link_libraries(mc_text_stdout_probe PRIVATE mc_platform_text mc_build_options)
    add_test(NAME platform.text_stdout
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/platform/test_stdout.py"
            --executable "$<TARGET_FILE:mc_text_stdout_probe>"
            --jdk-fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/java/println/bytes.json")
    set_tests_properties(platform.text_stdout PROPERTIES LABELS "public;unit;platform" TIMEOUT 60)

    add_test(NAME core.java_format_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_format.py"
            --executable "$<TARGET_FILE:mc_java_format_probe>")
    set_tests_properties(core.java_format_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME core.java_string_case_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_string_case.py"
            --executable "$<TARGET_FILE:mc_java_case_probe>")
    set_tests_properties(core.java_string_case_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME statistics.arrays_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/statistics/compare_arrays.py"
            --executable "$<TARGET_FILE:mc_statistic_arrays_probe>")
    set_tests_properties(statistics.arrays_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME item.recipes_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/compare_recipes.py"
            --executable "$<TARGET_FILE:mc_recipe_bootstrap_probe>")
    set_tests_properties(item.recipes_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME world.biome_registry_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/check_biome_registry.py"
            --executable "$<TARGET_FILE:mc_biome_registry_probe>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/biome-registry-replay")
    set_tests_properties(world.biome_registry_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME item.stack_values_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/compare_stack_values.py"
            --executable "$<TARGET_FILE:mc_stack_values_probe>")
    set_tests_properties(item.stack_values_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME core.java_properties_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/compare_properties.py"
            "$<TARGET_FILE:mc_java_properties_probe>")
    set_tests_properties(core.java_properties_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 60)

    add_test(NAME core.java_reader_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/compare_reader.py"
            "$<TARGET_FILE:mc_java_reader_probe>")
    set_tests_properties(core.java_reader_observation PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME resources.imported_files
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/resources/test_files.py"
            "$<TARGET_FILE:mc_translation_file_probe>")
    set_tests_properties(resources.imported_files PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 60)

    add_test(NAME statistics.imported_keys
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/statistics/test_keys_files.py"
            "$<TARGET_FILE:mc_statistic_keys_file_probe>")
    set_tests_properties(statistics.imported_keys PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 60)

    add_test(NAME world.info_observation
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/check_world_info.py"
            --probe "$<TARGET_FILE:mc_world_info_observation_probe>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-info-observation")
    set_tests_properties(world.info_observation PROPERTIES LABELS "public;replay" TIMEOUT 180)

    add_test(NAME entity.item_formula_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/compare_formula.py"
        --executable "$<TARGET_FILE:mc_entity_item_formula_probe>"
        --fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/item_formula_observation.tsv")
    set_tests_properties(entity.item_formula_observation PROPERTIES
        LABELS "public;fixture" TIMEOUT 210)

    add_test(NAME block.material_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/material/compare_observation.py"
        --executable "$<TARGET_FILE:mc_material_probe>"
        --fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/material/material_observation.tsv")
    set_tests_properties(block.material_observation PROPERTIES
        LABELS "public;fixture" TIMEOUT 30)

    add_test(NAME block.registry_base_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/block/compare_registry_prefix.py"
        --executable "$<TARGET_FILE:mc_registry_prefix_probe>"
        --fixtures "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/block/registry-prefix")
    set_tests_properties(block.registry_base_observation PROPERTIES
        LABELS "public;fixture" TIMEOUT 60)

    add_test(NAME item.registry_base_observation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/compare_registry_prefix.py"
        --executable "$<TARGET_FILE:mc_item_registry_prefix_probe>"
        --fixtures "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/item/registry-prefix")
    set_tests_properties(item.registry_base_observation PROPERTIES
        LABELS "public;fixture" TIMEOUT 60)

    add_test(NAME core.java_gaussian_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_gaussian.py"
        --executable "$<TARGET_FILE:mc_java_gaussian_probe>")
    set_tests_properties(core.java_gaussian_replay PROPERTIES
        LABELS "public;fixture;java;math" TIMEOUT 90)

    add_test(NAME tools.java_gaussian_protocol
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/java/test_gaussian_probe.py"
        --executable "$<TARGET_FILE:mc_java_gaussian_probe>")
    set_tests_properties(tools.java_gaussian_protocol PROPERTIES
        LABELS "public;tools;java;math" TIMEOUT 180)

    add_test(NAME world.scheduled_ordered_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_scheduled_ordered.py"
        --executable "$<TARGET_FILE:mc_scheduled_ordered_probe>")
    set_tests_properties(world.scheduled_ordered_replay PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME tools.scheduled_ordered_protocol
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_scheduled_ordered_probe.py"
        --executable "$<TARGET_FILE:mc_scheduled_ordered_probe>")
    set_tests_properties(tools.scheduled_ordered_protocol PROPERTIES
        LABELS "public;tools;world" TIMEOUT 180)

    foreach(MC_RUNTIME_CASE ownership-and-runtime generation generation-copy-limit profile-and-entity-bindings)
        add_test(NAME world.runtime_${MC_RUNTIME_CASE}
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
            --executable "$<TARGET_FILE:mc_test_world_runtime>"
            --scenario "${MC_RUNTIME_CASE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/runtime-contracts")
        set_tests_properties(world.runtime_${MC_RUNTIME_CASE} PROPERTIES
            LABELS "public;integration;storage" TIMEOUT 90)
    endforeach()

    add_test(NAME world.runtime_info_bindings
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_info_binding>"
        --scenario world-info-bindings
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/runtime-info-contracts")
    set_tests_properties(world.runtime_info_bindings PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 90)

    add_test(NAME world.typed_world_info
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_typed_info>"
        --scenario typed-world-info
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/typed-world-info-contracts")
    set_tests_properties(world.typed_world_info PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 90)

    add_test(NAME world.game_construction
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_game_world>"
        --scenario game-world
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/game-world-contracts")
    set_tests_properties(world.game_construction PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.registry_bindings
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_registry>"
        --scenario world-registry
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-registry-contracts")
    set_tests_properties(world.registry_bindings PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.biome_runtime
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_biome_runtime>"
        --scenario world-biome-runtime
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-biome-runtime-contracts")
    set_tests_properties(world.biome_runtime PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.fixed_biome_runtime COMMAND mc_test_fixed_biome_runtime)
    set_tests_properties(world.fixed_biome_runtime PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 90)

    add_test(NAME world.temperature_runtime
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_temperature>"
        --scenario temperature-runtime
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/temperature-runtime-contracts")
    set_tests_properties(world.temperature_runtime PROPERTIES
        LABELS "public;unit;ownership" TIMEOUT 90)

    add_test(NAME world.lighting_runtime
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_lighting_runtime>"
        --scenario world-lighting-runtime
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-lighting-runtime-contracts")
    set_tests_properties(world.lighting_runtime PROPERTIES
        LABELS "public;integration;lighting" TIMEOUT 90)

    add_test(NAME world.light_queue_runtime
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_light_queue_runtime>"
        --scenario world-light-queue-runtime
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-light-queue-runtime-contracts")
    set_tests_properties(world.light_queue_runtime PROPERTIES
        LABELS "public;integration;lighting" TIMEOUT 90)

    add_test(NAME world.chunk_objects
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_chunk_objects>"
        --scenario world-chunk-objects
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-chunk-object-contracts")
    set_tests_properties(world.chunk_objects PROPERTIES
        LABELS "public;integration;ownership;lighting;storage" TIMEOUT 90)

    add_test(NAME world.provider_generation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_provider_generation>"
        --scenario world-provider-generation
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-provider-generation-contracts")
    set_tests_properties(world.provider_generation PROPERTIES
        LABELS "public;integration;ownership;generation" TIMEOUT 90)

    add_test(NAME world.provider_activation
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_provider_activation>"
        --scenario world-provider-activation
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-provider-activation-contracts")
    set_tests_properties(world.provider_activation PROPERTIES
        LABELS "public;integration;ownership;generation;storage" TIMEOUT 90)

    add_test(NAME world.mutations
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_mutations>"
        --scenario world-mutations
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-mutation-contracts")
    set_tests_properties(world.mutations PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.scheduling
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_scheduling>"
        --scenario world-scheduling
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-scheduling-contracts")
    set_tests_properties(world.scheduling PROPERTIES
        LABELS "public;integration;ownership;scheduling" TIMEOUT 90)

    add_test(NAME world.solid_support
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_solid_support>"
        --scenario world-solid-support
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-solid-support-contracts")
    set_tests_properties(world.solid_support PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.entity_publication
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_entity_publication>"
        --scenario world-entity-publication
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-entity-publication-contracts")
    set_tests_properties(world.entity_publication PROPERTIES
        LABELS "public;integration;ownership" TIMEOUT 90)

    add_test(NAME world.nearby_entities
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_nearby>"
        --scenario world-nearby
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-nearby-contracts")
    set_tests_properties(world.nearby_entities PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.collision_boxes
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_collision_boxes>"
        --scenario world-collision-boxes
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-collision-boxes-contracts")
    set_tests_properties(world.collision_boxes PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.piston_geometry
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_piston_geometry>"
        --scenario world-piston-geometry
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-piston-geometry-contracts")
    set_tests_properties(world.piston_geometry PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.piston_tiles
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_piston_tiles>"
        --scenario world-piston-tiles
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-piston-tiles-contracts")
    set_tests_properties(world.piston_tiles PROPERTIES
        LABELS "public;integration;ownership;storage" TIMEOUT 90)

    add_test(NAME entity.movement
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_entity_movement>"
        --scenario world-entity-movement
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-entity-movement-contracts")
    set_tests_properties(entity.movement PROPERTIES
        LABELS "public;integration;ownership;collision" TIMEOUT 90)

    add_test(NAME world.movement_dependencies
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_movement>"
        --scenario world-movement-dependencies
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-movement-dependencies-contracts")
    set_tests_properties(world.movement_dependencies PROPERTIES
        LABELS "public;integration;ownership;environment" TIMEOUT 90)

    add_test(NAME world.tiles
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_tiles.py"
        --executable "$<TARGET_FILE:mc_test_world_tiles>"
        --mode all
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-tiles-contracts")
    set_tests_properties(world.tiles PROPERTIES
        LABELS "public;integration;ownership;mutation;storage" TIMEOUT 90)

    add_test(NAME world.saving
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_saving.py"
        --executable "$<TARGET_FILE:mc_test_world_saving>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-saving-contracts")
    set_tests_properties(world.saving PROPERTIES
        LABELS "public;integration;ownership;storage" TIMEOUT 120)

    add_test(NAME world.game_entities
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_game_entities.py"
        --executable "$<TARGET_FILE:mc_test_game_entities>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-game-entities-contracts")
    set_tests_properties(world.game_entities PROPERTIES
        LABELS "public;integration;ownership;storage" TIMEOUT 120)

    add_test(NAME world.dungeon
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_dungeon.py"
        --executable "$<TARGET_FILE:mc_test_dungeon>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-dungeon-contracts")
    set_tests_properties(world.dungeon PROPERTIES
        LABELS "public;integration;ownership;mutation;storage" TIMEOUT 160)

    add_test(NAME world.normal_population
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_normal_population.py"
        --executable "$<TARGET_FILE:mc_test_normal_population>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-normal-population-contracts")
    set_tests_properties(world.normal_population PROPERTIES
        LABELS "public;integration;ownership;population;storage" TIMEOUT 120)

    add_test(NAME world.queries
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_queries>"
        --scenario world-queries
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-query-contracts")
    set_tests_properties(world.queries PROPERTIES
        LABELS "public;integration;ownership;generation" TIMEOUT 90)

    add_test(NAME world.fluids
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_fluids>"
        --scenario world-fluids
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-fluid-contracts")
    set_tests_properties(world.fluids PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.falling
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_falling.py"
        --executable "$<TARGET_FILE:mc_test_world_falling>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-falling-contracts")
    set_tests_properties(world.falling PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.snow
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_snow.py"
        --executable "$<TARGET_FILE:mc_test_world_snow>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-snow-contracts")
    set_tests_properties(world.snow PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 90)

    add_test(NAME world.normal_provider_construction
        COMMAND mc_test_normal_provider_construction)
    set_tests_properties(world.normal_provider_construction PROPERTIES
        LABELS "public;unit;generation" TIMEOUT 90)

    add_test(NAME world.dimension_reference
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/tests/tools/world/test_dimension_reference.py" verify
        --fixture-dir "${CMAKE_SOURCE_DIR}/tests/reference/world/dimension"
        --provenance-sha256 5b620c091d1de9881f7c60b267706c693aa4726a99f67b7c99b63152118f813b
        --probe "$<TARGET_FILE:mc_dimension_observation_probe>"
        --work-dir "${CMAKE_BINARY_DIR}/dimension-reference")
    set_tests_properties(world.dimension_reference PROPERTIES
        LABELS "public;reference;math" TIMEOUT 120)

    add_test(NAME world.saved_data_ids
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_saved_data>"
        --scenario saved-data-ids
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/saved-data-id-contracts")
    set_tests_properties(world.saved_data_ids PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 90)

    add_test(NAME world.saved_data_ids_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/world/check_saved_data_ids.py"
        --executable "$<TARGET_FILE:mc_saved_data_id_observation_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/saved-data-memory-replay")
    set_tests_properties(world.saved_data_ids_replay PROPERTIES
        LABELS "public;reference;storage" TIMEOUT 90)

    add_test(NAME world.lifecycle_protocol
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_lifecycle_probe.py"
        --executable "$<TARGET_FILE:mc_world_lifecycle_probe>"
        --prepare-executable "$<TARGET_FILE:mc_test_world_runtime>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lifecycle-protocol-contracts")
    set_tests_properties(world.lifecycle_protocol PROPERTIES
        LABELS "public;integration;storage" TIMEOUT 180)

    add_test(NAME world.lifecycle_callbacks_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/compare_lifecycle_callbacks.py"
        --executable "$<TARGET_FILE:mc_chunk_lifecycle_callbacks_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lifecycle-callbacks-replay")
    set_tests_properties(world.lifecycle_callbacks_replay PROPERTIES
        LABELS "public;fixture;cache" TIMEOUT 420)

    add_test(NAME core.java_int_set_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_int_set.py"
        --executable "$<TARGET_FILE:mc_java_int_set_probe>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-java-int-set-replay")
    set_tests_properties(core.java_int_set_replay PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME world.chunk_pending_replay
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_int_set.py"
        --executable "$<TARGET_FILE:mc_java_int_set_probe>" --pending
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-pending-replay")
    set_tests_properties(world.chunk_pending_replay PROPERTIES
        LABELS "public;fixture;java" TIMEOUT 90)

    add_test(NAME tools.world_storage_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage" -p test_world_storage.py)
    set_tests_properties(tools.world_storage_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 180)

    add_test(NAME tools.world_store_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_store.py"
        --executable "$<TARGET_FILE:mc_world_store_probe>")
    set_tests_properties(tools.world_store_interop PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 180)

    add_test(NAME tools.regions_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage" -p test_regions.py)
    set_tests_properties(tools.regions_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    add_test(NAME tools.regions_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_regions.py"
        --executable "$<TARGET_FILE:mc_world_regions_probe>")
    set_tests_properties(tools.regions_interop PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    add_test(NAME tools.chunk_storage_reference
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_chunk_storage.py"
        --executable "$<TARGET_FILE:mc_stored_chunk_probe>")
    set_tests_properties(tools.chunk_storage_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 180)

    add_test(NAME tools.level_reference
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_level.py"
        --executable "$<TARGET_FILE:mc_level_probe>")
    set_tests_properties(tools.level_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    add_test(NAME tools.region_write_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage" -p test_region_write.py)
    set_tests_properties(tools.region_write_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    add_test(NAME tools.region_write_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_region_write.py"
        --executable "$<TARGET_FILE:mc_region_writer_probe>")
    set_tests_properties(tools.region_write_interop PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools;storage" TIMEOUT 120)

    if(MC_ENABLE_STORAGE_REFERENCE_TESTS)
        find_package(Java 17 QUIET COMPONENTS Runtime Development)
        get_filename_component(MC_STORAGE_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        set(MC_STORAGE_SOURCE_ARGUMENTS)
        if(NOT MC_REFERENCE_TESTED_COMMIT STREQUAL "")
            list(APPEND MC_STORAGE_SOURCE_ARGUMENTS --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        endif()
        add_test(NAME reference.level
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/storage/compare_level.py"
            --executable "$<TARGET_FILE:mc_level_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_STORAGE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-level"
            ${MC_STORAGE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.level PROPERTIES
            LABELS "reference;original;storage" TIMEOUT 300 SKIP_RETURN_CODE 77)

        add_test(NAME reference.region_write
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/storage/compare_region_write.py"
            --executable "$<TARGET_FILE:mc_region_writer_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-region-write"
            ${MC_STORAGE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.region_write PROPERTIES
            LABELS "reference;original;storage" TIMEOUT 300 SKIP_RETURN_CODE 77)

        add_test(NAME reference.chunk_storage
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/storage/compare_chunk_storage.py"
            --executable "$<TARGET_FILE:mc_stored_chunk_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_STORAGE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-storage"
            ${MC_STORAGE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.chunk_storage PROPERTIES
            LABELS "reference;original;storage" TIMEOUT 600 SKIP_RETURN_CODE 77)

        add_test(NAME reference.world_storage
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/storage/compare_world_storage.py"
            --executable "$<TARGET_FILE:mc_world_store_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-world-storage"
            ${MC_STORAGE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.world_storage PROPERTIES
            LABELS "reference;original;storage" TIMEOUT 900 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_LIFECYCLE_REFERENCE_TESTS)
        find_package(Java 17 QUIET COMPONENTS Runtime Development)
        set(MC_LIFECYCLE_SOURCE_ARGUMENTS)
        if(NOT MC_REFERENCE_TESTED_COMMIT STREQUAL "")
            list(APPEND MC_LIFECYCLE_SOURCE_ARGUMENTS --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        endif()
        foreach(MC_LIFECYCLE_CONTEXT client base dedicated)
            foreach(MC_LIFECYCLE_SCENARIO cohort session-world detach-owned)
                add_test(NAME reference.lifecycle_${MC_LIFECYCLE_CONTEXT}_${MC_LIFECYCLE_SCENARIO}
                    COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/run_lifecycle.py"
                    --executable "$<TARGET_FILE:mc_world_lifecycle_probe>"
                    --callback-executable "$<TARGET_FILE:mc_chunk_lifecycle_callbacks_probe>"
                    --client "${MC_REFERENCE_CLIENT_JAR}"
                    --server "${MC_REFERENCE_SERVER_JAR}"
                    --java "${Java_JAVA_EXECUTABLE}"
                    --javac "${Java_JAVAC_EXECUTABLE}"
                    --context "${MC_LIFECYCLE_CONTEXT}" --scenario "${MC_LIFECYCLE_SCENARIO}"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-lifecycle-${MC_LIFECYCLE_CONTEXT}-${MC_LIFECYCLE_SCENARIO}"
                    ${MC_LIFECYCLE_SOURCE_ARGUMENTS})
                set_tests_properties(reference.lifecycle_${MC_LIFECYCLE_CONTEXT}_${MC_LIFECYCLE_SCENARIO} PROPERTIES
                    LABELS "reference;original;lifecycle" TIMEOUT 1800 RUN_SERIAL TRUE SKIP_RETURN_CODE 77)
            endforeach()
        endforeach()
        add_test(NAME reference.lifecycle_dedicated_unload101
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/run_lifecycle.py"
            --executable "$<TARGET_FILE:mc_world_lifecycle_probe>"
            --callback-executable "$<TARGET_FILE:mc_chunk_lifecycle_callbacks_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --context dedicated --scenario unload101
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-lifecycle-dedicated-unload101"
            ${MC_LIFECYCLE_SOURCE_ARGUMENTS})
        set_tests_properties(reference.lifecycle_dedicated_unload101 PROPERTIES
            LABELS "reference;original;lifecycle" TIMEOUT 1800 RUN_SERIAL TRUE SKIP_RETURN_CODE 77)
    endif()

    add_test(NAME tools.acquire_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache" -p test_acquire_reference.py)
    set_tests_properties(tools.acquire_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.acquire_fixture
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache" -p test_acquire_interop.py)
    set_tests_properties(tools.acquire_fixture PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.acquire_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache/test_acquire_interop.py"
        --executable "$<TARGET_FILE:mc_chunk_acquire_probe>")
    set_tests_properties(tools.acquire_interop PROPERTIES LABELS "public;tools" TIMEOUT 120)

    add_test(NAME tools.callbacks_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache" -p test_callbacks_reference.py)
    # Failure controls repeatedly fingerprint the growing source tree; allow
    # filesystem headroom for mounted workspaces and concurrent public checks.
    set_tests_properties(tools.callbacks_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 180)

    add_test(NAME tools.callbacks_fixture
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache" -p test_callbacks_interop.py)
    set_tests_properties(tools.callbacks_fixture PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.callbacks_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/cache/test_callbacks_interop.py"
        --executable "$<TARGET_FILE:mc_chunk_callback_probe>")
    set_tests_properties(tools.callbacks_interop PROPERTIES LABELS "public;tools" TIMEOUT 120)

    if(MC_ENABLE_CACHE_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_CACHE_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.chunk_acquire
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/compare_acquire.py"
            --executable "$<TARGET_FILE:mc_chunk_acquire_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_CACHE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-acquire"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.chunk_acquire PROPERTIES
            LABELS "reference;original;cache" TIMEOUT 900 SKIP_RETURN_CODE 77)

        add_test(NAME reference.chunk_callbacks
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/cache/compare_callbacks.py"
            --executable "$<TARGET_FILE:mc_chunk_callback_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_CACHE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-callbacks"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.chunk_callbacks PROPERTIES
            LABELS "reference;original;cache" TIMEOUT 900 SKIP_RETURN_CODE 77)
    endif()

    add_test(NAME tools.region_inspect
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/region/test_inspect.py"
        --executable "$<TARGET_FILE:mc_region_inspect>")
    set_tests_properties(tools.region_inspect PROPERTIES LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.region_capture
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/region/test_capture.py"
        --executable "$<TARGET_FILE:mc_region_probe>")
    set_tests_properties(tools.region_capture PROPERTIES LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.region_probe
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/region/test_probe.py"
        --executable "$<TARGET_FILE:mc_region_probe>")
    set_tests_properties(tools.region_probe PROPERTIES LABELS "public;tools" TIMEOUT 30)

    add_test(NAME tools.region_live
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/region" -p test_live.py)
    set_tests_properties(tools.region_live PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        # Several failure controls fingerprint the complete growing source tree.
        # Leave filesystem headroom for mounted workspaces and parallel checks.
        LABELS "public;tools" TIMEOUT 90)

    if(MC_ENABLE_REGION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_REGION_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.region
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/region/compare_region.py"
            --executable "$<TARGET_FILE:mc_region_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_REGION_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-region"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.region PROPERTIES
            LABELS "reference;original;region" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_REGION_CAPTURE_TESTS)
        add_test(NAME reference.region_capture
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/region/compare_capture.py"
            --executable "$<TARGET_FILE:mc_region_probe>"
            --capture "${MC_REGION_CAPTURE_MANIFEST}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-region-capture"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.region_capture PROPERTIES
            LABELS "reference;captured;region" TIMEOUT 120 SKIP_RETURN_CODE 77)
    endif()

    add_test(NAME tools.normal_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/generation" -p test_normal_reference.py)
    set_tests_properties(tools.normal_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 30)

    add_test(NAME tools.normal_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/generation/test_normal_interop.py"
        --executable "$<TARGET_FILE:mc_normal_generation_probe>")
    set_tests_properties(tools.normal_interop PROPERTIES LABELS "public;tools" TIMEOUT 90)

    if(MC_ENABLE_GENERATION_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_GENERATION_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.normal_generation
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/generation/compare_normal.py"
            --executable "$<TARGET_FILE:mc_normal_generation_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_GENERATION_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-normal-generation"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.normal_generation PROPERTIES
            LABELS "reference;original;generation" TIMEOUT 1900 SKIP_RETURN_CODE 77)
    endif()

    add_test(NAME tools.finish_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/chunk" -p test_finish_reference.py)
    set_tests_properties(tools.finish_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 30)

    add_test(NAME tools.finish_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/chunk/test_finish_interop.py"
        --executable "$<TARGET_FILE:mc_chunk_finish_probe>")
    set_tests_properties(tools.finish_interop PROPERTIES LABELS "public;tools" TIMEOUT 60)

    if(MC_ENABLE_FINISH_REFERENCE_TESTS)
        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_FINISH_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.chunk_finish
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/chunk/compare_finish.py"
            --executable "$<TARGET_FILE:mc_chunk_finish_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_FINISH_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-finish"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.chunk_finish PROPERTIES
            LABELS "reference;original;chunk" TIMEOUT 600 SKIP_RETURN_CODE 77)
    endif()
    add_test(NAME tools.light_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/lighting" -p test_reference.py)
    set_tests_properties(tools.light_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 30)

    if(MC_ENABLE_LIGHT_REFERENCE_TESTS)
        find_package(Java 11 REQUIRED COMPONENTS Runtime Development)
        get_filename_component(MC_LIGHT_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.light_queue
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/lighting/compare_light.py"
            --executable "$<TARGET_FILE:mc_light_queue_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_LIGHT_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-light-queue"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.light_queue PROPERTIES
            LABELS "reference;original;lighting" TIMEOUT 240 SKIP_RETURN_CODE 77)
    endif()
    add_test(NAME tools.import_assets
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/resources" -p test_import_assets.py)
    set_tests_properties(tools.import_assets PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.compression_interop
        COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/compression/test_interop.py"
        --executable "$<TARGET_FILE:mc_compression_probe>")
    set_tests_properties(tools.compression_interop PROPERTIES LABELS "public;tools" TIMEOUT 90)

    add_test(NAME tools.noise_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/noise" -p test_reference.py)
    set_tests_properties(tools.noise_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.simplex_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/simplex" -p test_reference.py)
    set_tests_properties(tools.simplex_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.biome_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/biome" -p test_reference.py)
    set_tests_properties(tools.biome_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.density_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/density" -p test_reference.py)
    set_tests_properties(tools.density_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.surface_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/surface" -p test_reference.py)
    set_tests_properties(tools.surface_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.math_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/math" -p test_reference.py)
    set_tests_properties(tools.math_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.caves_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/caves" -p test_reference.py)
    set_tests_properties(tools.caves_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.caves_geometry_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/caves" -p test_geometry_reference.py)
    set_tests_properties(tools.caves_geometry_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.nibbles_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/chunk" -p test_nibbles_reference.py)
    set_tests_properties(tools.nibbles_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    add_test(NAME tools.chunk_data_reference
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/chunk" -p test_data_reference.py)
    set_tests_properties(tools.chunk_data_reference PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "public;tools" TIMEOUT 60)

    if(MC_ENABLE_NIBBLES_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.nibbles
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/chunk/compare_nibbles.py"
            --executable "$<TARGET_FILE:mc_chunk_nibbles_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-nibbles"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.nibbles PROPERTIES
            LABELS "reference;original;chunk" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_CHUNK_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.chunk_data
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/chunk/compare_chunk.py"
            --executable "$<TARGET_FILE:mc_chunk_data_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-chunk-data"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.chunk_data PROPERTIES
            LABELS "reference;original;chunk" TIMEOUT 600 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_JAVA_REFERENCE_TESTS)
        find_package(Java 8 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.java_semantics
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_java.py"
            --executable "$<TARGET_FILE:mc_test_java>"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-java")
        set_tests_properties(reference.java_semantics PROPERTIES
            LABELS "reference;java" TIMEOUT 90 SKIP_RETURN_CODE 77)

        find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
        add_test(NAME reference.java_int_set
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/java/compare_int_set.py"
            --executable "$<TARGET_FILE:mc_java_int_set_probe>"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-java-int-set")
        set_tests_properties(reference.java_int_set PROPERTIES
            LABELS "reference;java" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_NBT_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.nbt
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/nbt/compare_nbt.py"
            --executable "$<TARGET_FILE:mc_nbt_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-nbt"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.nbt PROPERTIES
            LABELS "reference;original;nbt" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_MATH_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.math
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/math/compare_math.py"
            --executable "$<TARGET_FILE:mc_math_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-math"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.math PROPERTIES
            LABELS "reference;original;math" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_COMPRESSION_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.compression
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/compression/compare_compression.py"
            --executable "$<TARGET_FILE:mc_compression_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-compression"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.compression PROPERTIES
            LABELS "reference;original;compression" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_NOISE_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.noise
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/noise/compare_noise.py"
            --executable "$<TARGET_FILE:mc_noise_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-noise"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.noise PROPERTIES
            LABELS "reference;original;noise" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_SIMPLEX_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.simplex
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/simplex/compare_simplex.py"
            --executable "$<TARGET_FILE:mc_simplex_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-simplex"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.simplex PROPERTIES
            LABELS "reference;original;simplex" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_BIOME_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.biome
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/biome/compare_biome.py"
            --executable "$<TARGET_FILE:mc_biome_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-biome"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.biome PROPERTIES
            LABELS "reference;original;biome" TIMEOUT 240 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_DENSITY_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.density
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/density/compare_density.py"
            --executable "$<TARGET_FILE:mc_terrain_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-density"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.density PROPERTIES
            LABELS "reference;original;density" TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_CAVES_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        get_filename_component(MC_REFERENCE_JAVA_BIN "${Java_JAVA_EXECUTABLE}" DIRECTORY)
        add_test(NAME reference.caves
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/caves/compare_caves.py"
            --executable "$<TARGET_FILE:mc_caves_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_REFERENCE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-caves"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.caves PROPERTIES
            LABELS "reference;original;caves" TIMEOUT 300 SKIP_RETURN_CODE 77)

        add_test(NAME reference.caves_geometry
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/caves/compare_geometry.py"
            --executable "$<TARGET_FILE:mc_caves_geometry_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --javap "${MC_REFERENCE_JAVA_BIN}/javap${CMAKE_EXECUTABLE_SUFFIX}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-caves-geometry"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.caves_geometry PROPERTIES
            LABELS "reference;original;caves;geometry" TIMEOUT 180 SKIP_RETURN_CODE 77)
    endif()

    if(MC_ENABLE_SURFACE_REFERENCE_TESTS)
        find_package(Java 11 QUIET COMPONENTS Runtime Development)
        add_test(NAME reference.surface
            COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/reference/surface/compare_surface.py"
            --executable "$<TARGET_FILE:mc_surface_probe>"
            --client "${MC_REFERENCE_CLIENT_JAR}"
            --server "${MC_REFERENCE_SERVER_JAR}"
            --java "${Java_JAVA_EXECUTABLE}"
            --javac "${Java_JAVAC_EXECUTABLE}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-surface"
            --tested-commit "${MC_REFERENCE_TESTED_COMMIT}")
        set_tests_properties(reference.surface PROPERTIES
            LABELS "reference;original;surface" TIMEOUT 300 SKIP_RETURN_CODE 77)
    endif()
endif()
