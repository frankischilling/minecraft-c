# Actual single-query GL diagnostic prefix; System.out13 remains a dependency.
if(TARGET mc_client_display)
    add_library(mc_client_gl_diagnostics STATIC
        src/client/startup/application_gl_diagnostics.c
        src/client/startup/glu_error_text.c)
    target_include_directories(mc_client_gl_diagnostics PUBLIC include src)
    target_link_libraries(mc_client_gl_diagnostics PUBLIC mc_client_display mc_core
        PRIVATE mc_build_options)
endif()

if(TARGET mc_client_gl_diagnostics)
    target_sources(mc_client_gl_diagnostics PRIVATE
        src/client/startup/diagnostic_builder.c
        src/client/startup/application_gl_output.c)
    target_link_libraries(mc_client_gl_diagnostics PUBLIC mc_simulation)
endif()
