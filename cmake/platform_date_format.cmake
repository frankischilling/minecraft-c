# Selected native SimpleDateFormat() constructor and SHORT/SHORT format leaf.
add_library(mc_platform_date_format STATIC src/platform/time/date_format.c)
target_link_libraries(mc_platform_date_format PUBLIC mc_platform_time mc_platform_text
    PRIVATE mc_build_options)
