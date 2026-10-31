# CPU-only GuiIngame class/instance construction. Include after the canonical
# ItemRenderer, Simulation and Threads targets. No Display/application-owner link.
add_library(mc_client_gui_ingame STATIC src/client/gui/gui_ingame.c)
target_include_directories(mc_client_gui_ingame PUBLIC include PRIVATE src/client src/platform/time)
target_link_libraries(mc_client_gui_ingame PUBLIC mc_client_notice mc_simulation mc_c_atomics
    PRIVATE Threads::Threads mc_build_options)
