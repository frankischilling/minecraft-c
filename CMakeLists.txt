cmake_minimum_required(VERSION 3.24)
project(minecraft_c VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MC_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(MC_ENABLE_JAVA_REFERENCE_TESTS "Compare numerical helpers with a local JDK" OFF)
option(MC_ENABLE_NBT_REFERENCE_TESTS "Compare NBT with locally supplied original JARs" OFF)
option(MC_ENABLE_COMPRESSION_REFERENCE_TESTS "Compare compression with original helpers" OFF)
option(MC_ENABLE_REGION_CAPTURE_TESTS "Compare region reader with recorded observations" OFF)
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

# The shared core has no window, graphics, audio, JDK, or asset dependencies.
add_library(mc_core STATIC src/core/java/java.c src/core/text/mutf8.c)
target_include_directories(mc_core PUBLIC include)
target_link_libraries(mc_core PRIVATE mc_build_options)

add_library(mc_storage STATIC
    src/storage/nbt/nbt.c
    src/storage/compression/compression.c
    src/storage/region/region.c)
target_link_libraries(mc_storage PUBLIC mc_core PRIVATE mc_build_options ZLIB::ZLIBSTATIC)

add_executable(mc_region_inspect src/tools/storage/region_inspect.c)
target_link_libraries(mc_region_inspect PRIVATE mc_storage mc_build_options)
if(MINGW)
    target_link_options(mc_region_inspect PRIVATE -municode)
endif()

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

    add_executable(mc_test_region tests/storage/region/test_region.c)
    target_link_libraries(mc_test_region PRIVATE mc_storage mc_build_options)
    add_test(NAME storage.region COMMAND mc_test_region)
    set_tests_properties(storage.region PROPERTIES LABELS "public;unit" TIMEOUT 60)

    add_executable(mc_region_probe tests/storage/region/region_probe.c)
    target_link_libraries(mc_region_probe PRIVATE mc_storage mc_build_options)

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
endif()
