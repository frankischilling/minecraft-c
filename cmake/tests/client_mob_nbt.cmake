if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_client_mob_nbt tests/world/runtime/saving/test_mob_nbt.c)
    target_include_directories(mc_test_client_mob_nbt PRIVATE src)
    target_link_libraries(mc_test_client_mob_nbt PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_test(NAME entity.client_mob_nbt COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_nbt.py"
        --executable "$<TARGET_FILE:mc_test_client_mob_nbt>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/client-mob-nbt-controls")
    set_tests_properties(entity.client_mob_nbt PROPERTIES
        LABELS "public;integration;entity;world;storage;ownership;filesystem" TIMEOUT 600)
    add_test(NAME entity.client_mob_nbt_wolf_owner COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_nbt.py"
        --executable "$<TARGET_FILE:mc_test_client_mob_nbt>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/client-mob-wolf-owner-controls"
        --profile wolf-owner)
    set_tests_properties(entity.client_mob_nbt_wolf_owner PROPERTIES
        LABELS "public;integration;entity;world;storage;ownership;filesystem" TIMEOUT 600)
endif()
