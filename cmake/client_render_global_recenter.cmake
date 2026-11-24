if(TARGET mc_client_render_global)
    target_sources(mc_client_render_global PRIVATE
        src/client/world/render_global_recenter.c
        src/client/world/render_global_jo.c)
    target_link_libraries(mc_client_render_global PUBLIC mc_math_classes)
endif()
