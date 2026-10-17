cmake_minimum_required(VERSION 3.24)
project(minecraft_c VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MC_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(MC_ENABLE_JAVA_REFERENCE_TESTS "Compare numerical helpers with a local JDK" OFF)

# These options apply to project-owned code, not to future dependencies.
add_library(mc_build_options INTERFACE)
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
    target_compile_options(mc_build_options INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mc_build_options INTERFACE -fsanitize=address,undefined)
endif()

# The shared core has no window, graphics, audio, JDK, or asset dependencies.
add_library(mc_core STATIC src/core/java/java.c)
target_include_directories(mc_core PUBLIC include)
target_link_libraries(mc_core PRIVATE mc_build_options)

include(CTest)
if(BUILD_TESTING)
    add_executable(mc_test_java tests/core/java/test_java.c)
    target_link_libraries(mc_test_java PRIVATE mc_core mc_build_options)
    add_test(NAME core.java COMMAND mc_test_java)
    set_tests_properties(core.java PROPERTIES LABELS "public;unit" TIMEOUT 30)

    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(NAME tools.import_assets
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/resources" -p test_import_assets.py)
    set_tests_properties(tools.import_assets PROPERTIES
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
endif()
