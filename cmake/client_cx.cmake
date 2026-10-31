# Actual cx namespace/Object constructor is CPU-only, including headless MC
# namespace teardown. The device method is a separate archive/TU, not dead-strip
# reliance on an unresolved Display reference in the CPU object.
add_library(mc_client_cx_cpu STATIC src/client/render/cx.c)
target_include_directories(mc_client_cx_cpu PUBLIC include src)
target_link_libraries(mc_client_cx_cpu PRIVATE Threads::Threads mc_build_options)

if(TARGET mc_client_display)
    add_library(mc_client_cx STATIC src/client/render/cx_supported.c)
    target_include_directories(mc_client_cx PUBLIC include src)
    target_link_libraries(mc_client_cx PUBLIC mc_client_cx_cpu mc_client_display
        PRIVATE Threads::Threads mc_build_options)
endif()
