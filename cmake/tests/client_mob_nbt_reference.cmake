option(MC_ENABLE_CLIENT_MOB_NBT_REFERENCE_TESTS
    "Enable original client Mob NBT prepared reference tests" OFF)
if(MC_ENABLE_CLIENT_MOB_NBT_REFERENCE_TESTS)
    if(NOT MC_REFERENCE_CLIENT_JAR)
        message(FATAL_ERROR
            "Client Mob NBT reference tests require the verified original client JAR")
    endif()
    find_package(Java 17 EXACT REQUIRED COMPONENTS Runtime Development)
    get_filename_component(_mc_mob_nbt_java "${Java_JAVA_EXECUTABLE}" REALPATH)
    get_filename_component(_mc_mob_nbt_java_bin "${_mc_mob_nbt_java}" DIRECTORY)
    get_filename_component(_mc_mob_nbt_jdk "${_mc_mob_nbt_java_bin}" DIRECTORY)
    add_test(NAME reference.client_mob_nbt
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/mob_nbt/capture.py"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --jdk-home "${_mc_mob_nbt_jdk}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-client-mob-nbt")
    set_tests_properties(reference.client_mob_nbt PROPERTIES
        LABELS "reference;original;entity;storage" TIMEOUT 600)
    unset(_mc_mob_nbt_java)
    unset(_mc_mob_nbt_java_bin)
    unset(_mc_mob_nbt_jdk)
endif()
