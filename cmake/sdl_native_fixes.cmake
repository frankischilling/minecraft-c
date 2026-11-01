include_guard(GLOBAL)

# Compile corrected copies; offline inputs and historical source observations
# keep the untouched, hash-checked SDL archive contents.
function(mc_sdl_native_source_fix relative before after)
    set(original "${mc_sdl3_SOURCE_DIR}/${relative}")
    file(READ "${original}" source)
    string(FIND "${source}" "${before}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned SDL native correction does not match ${relative}")
    endif()
    string(REPLACE "${before}" "${after}" source "${source}")
    get_filename_component(name "${original}" NAME)
    get_filename_component(directory "${original}" DIRECTORY)
    set(corrected "${CMAKE_CURRENT_BINARY_DIR}/mc-sdl3-native/${name}")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/mc-sdl3-native")
    # Avoid rebuilding an unchanged copy during every CMake regeneration.
    set(write_copy TRUE)
    if(EXISTS "${corrected}")
        file(READ "${corrected}" existing)
        if(existing STREQUAL source)
            set(write_copy FALSE)
        endif()
    endif()
    if(write_copy)
        file(WRITE "${corrected}" "${source}")
    endif()
    get_target_property(sources SDL3-static SOURCES)
    list(FIND sources "${original}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "Pinned SDL target does not compile ${relative}")
    endif()
    list(REMOVE_AT sources ${index})
    list(INSERT sources ${index} "${corrected}")
    set_property(TARGET SDL3-static PROPERTY SOURCES "${sources}")
    # Keep other SDL compile commands and their compiler-cache keys unchanged.
    set_source_files_properties("${corrected}" TARGET_DIRECTORY SDL3-static PROPERTIES
        INCLUDE_DIRECTORIES "${directory}" SKIP_PRECOMPILE_HEADERS ON)
endfunction()

if(WIN32)
    # GetForegroundWindow names the top-level Frame, never its embedded Canvas.
    # Preserve the existing top-level path; only real native child windows need
    # the focused HWND under their foreground root during SDL dispatch.
    mc_sdl_native_source_fix("src/video/windows/SDL_windowsevents.c"
        "bool has_focus = (GetForegroundWindow() == hwnd);"
        "bool has_focus = (GetForegroundWindow() == hwnd);\n    if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) {\n        has_focus = (GetFocus() == hwnd && GetForegroundWindow() == GetAncestor(hwnd, GA_ROOT));\n    }")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # ConfigureNotify queries can return child storage for the real Frame.
    # XQueryTree transfers that allocation even when only its parent is needed.
    mc_sdl_native_source_fix("src/video/x11/SDL_x11events.c"
        "X11_XQueryTree(data->videodata->display, xevent->xconfigure.window, &Root, &Parent, &Children, &NumChildren);"
        "Children = NULL;\n            if (!X11_XQueryTree(data->videodata->display, xevent->xconfigure.window, &Root, &Parent, &Children, &NumChildren)) {\n                X11_XSync(display, False);\n                X11_XSetErrorHandler(prev_handler);\n                break;\n            }\n            if (Children) {\n                X11_XFree(Children);\n            }")
endif()
