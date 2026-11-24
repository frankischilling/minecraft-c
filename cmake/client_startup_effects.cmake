# Include cmake/client_cx.cmake before application_owner is defined,
# then include this file after the canonical FX and startup targets exist.
if(TARGET mc_client_application_owner)
    if(NOT TARGET mc_client_cx_cpu)
        message(FATAL_ERROR "Actual MC namespace cleanup requires the canonical cx CPU owner")
    endif()
    target_link_libraries(mc_client_application_owner PRIVATE mc_client_cx_cpu)
endif()
# The FX archive already links mc_client_notice, the actual owner of
# application_access.c and its native effect-borrow functions. No extra FX to
# application_owner edge is required for those symbols.
if(MC_BUILD_CLIENT AND TARGET mc_client_application_startup)
    if(NOT TARGET mc_client_cx_cpu OR NOT TARGET mc_client_texture_effects)
        message(FATAL_ERROR "Actual startup effects require canonical cx CPU and FX owners")
    endif()
    target_sources(mc_client_application_startup PRIVATE
        src/client/startup/application_startup_effects.c)
    target_link_libraries(mc_client_application_startup PUBLIC
        mc_client_cx_cpu mc_client_texture_effects)
endif()
