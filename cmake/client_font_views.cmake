# Additive borrowed String view admission; existing sj kernels/legacy APIs remain.
if(TARGET mc_client_source_resources)
    target_sources(mc_client_source_resources PRIVATE src/client/gui/bitmap_font_view.c)
endif()
