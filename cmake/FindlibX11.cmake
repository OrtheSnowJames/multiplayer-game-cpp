

include(FindPackageHandleStandardArgs)

if (UNIX)
    # Try to find X11 include directory
    find_path(X11_INCLUDE_DIR
        NAMES X11/Xlib.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/X11/include
    )

    # Try to find X11 library
    find_library(X11_LIBRARY
        NAMES X11
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/X11/lib
            /usr/lib/x86_64-linux-gnu
    )

    # Handle the QUIETLY and REQUIRED arguments
    find_package_handle_standard_args(libX11
        REQUIRED_VARS
            X11_LIBRARY
            X11_INCLUDE_DIR
    )

    if(LIBX11_FOUND)
        # Create an imported target
        if(NOT TARGET X11::X11)
            add_library(X11::X11 UNKNOWN IMPORTED)
            set_target_properties(X11::X11 PROPERTIES
                IMPORTED_LOCATION "${X11_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${X11_INCLUDE_DIR}"
            )
        endif()
    endif()
endif()

mark_as_advanced(X11_INCLUDE_DIR X11_LIBRARY)