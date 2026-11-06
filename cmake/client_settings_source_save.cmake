if(NOT TARGET mc_game_settings OR NOT TARGET mc_java_file_writers)
    message(FATAL_ERROR "Typed Settings saves require the canonical FileWriter owner")
endif()
target_link_libraries(mc_game_settings PUBLIC mc_java_file_writers)
