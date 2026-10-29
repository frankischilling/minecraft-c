if(BUILD_TESTING AND TARGET mc_client_image_downloads)
    add_executable(mc_test_image_download tests/client/render_engine/test_image_download.c)
    target_include_directories(mc_test_image_download PRIVATE src)
    target_link_libraries(mc_test_image_download PRIVATE
        mc_client_image_downloads mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_image_download PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.image_download COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_image_download.py"
        --executable "$<TARGET_FILE:mc_test_image_download>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/image-download-contracts")
    set_tests_properties(client.image_download PROPERTIES
        LABELS "public;integration;client;network;ownership" TIMEOUT 180)
endif()
