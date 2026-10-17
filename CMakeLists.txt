cmake_minimum_required(VERSION 3.24)
project(minecraft_c VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MC_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(MC_ENABLE_JAVA_REFERENCE_TESTS "Compare numerical helpers with a local JDK" OFF)
option(MC_ENABLE_NBT_REFERENCE_TESTS "Compare NBT with locally supplied original JARs" OFF)
option(MC_ENABLE_COMPRESSION_REFERENCE_TESTS "Compare compression with original helpers" OFF)
option(MC_ENABLE_NOISE_REFERENCE_TESTS "Compare terrain noise with original samplers" OFF)
option(MC_ENABLE_SIMPLEX_REFERENCE_TESTS "Compare 2D simplex noise with original samplers" OFF)
option(MC_ENABLE_BIOME_REFERENCE_TESTS "Compare climate and biomes with original methods" OFF)
option(MC_ENABLE_DENSITY_REFERENCE_TESTS "Compare normal density and base terrain with originals" OFF)
option(MC_ENABLE_SURFACE_REFERENCE_TESTS "Compare normal surface replacement with originals" OFF)
set(MC_REFERENCE_CLIENT_JAR "" CACHE FILEPATH "Local verified original client JAR")
set(MC_REFERENCE_SERVER_JAR "" CACHE FILEPATH "Local verified original server JAR")
set(MC_REFERENCE_TESTED_COMMIT "" CACHE STRING "Exact source commit for local reference reports")

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

# The shared core has no window, graphics, audio, JDK, or asset dependencies.
add_library(mc_core STATIC src/core/java/java.c src/core/text/mutf8.c)
target_include_directories(mc_core PUBLIC include)
target_link_libraries(mc_core PRIVATE mc_build_options)

add_library(mc_storage STATIC src/storage/nbt/nbt.c src/storage/compression/compression.c)
target_link_libraries(mc_storage PUBLIC mc_core PRIVATE mc_build_options ZLIB::ZLIBSTATIC)

add_library(mc_world STATIC
    src/world/noise/noise.c
    src/world/noise/simplex.c
    src/world/biome/biome.c
    src/world/terrain/terrain.c
    src/world/terrain/surface.c)
target_link_libraries(mc_world PUBLIC mc_core PRIVATE mc_build_options)

include(CTest)
if(BUILD_TESTING)
    add_executable(mc_test_java tests/core/java/test_java.c)
    target_link_libraries(mc_test_java PRIVATE mc_core mc_build_options)
    add_test(NAME core.java COMMAND mc_test_java)
    set_tests_properties(core.java PROPERTIES LABELS "public;unit" TIMEOUT 30)

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

    add_executable(mc_test_compression tests/storage/compression/test_compression.c)
    target_link_libraries(mc_test_compression PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.compression COMMAND mc_test_compression)
    set_tests_properties(storage.compression PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_compression_probe tests/storage/compression/compression_probe.c)
    target_link_libraries(mc_compression_probe PRIVATE mc_storage mc_build_options)

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

    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
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
