cmake_minimum_required(VERSION 3.10)
project(nginxui VERSION 1.0.0 LANGUAGES C)

# Set install paths
set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install path prefix")

include(GNUInstallDirs)

# Find pkg-config
find_package(PkgConfig REQUIRED)

# Find GTK4
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

# Find GtkSourceView (optional - try different versions)
set(GTKSOURCEVIEW_FOUND FALSE)
# Try version 5 first
pkg_check_modules(GTKSOURCEVIEW5 gtksourceview-5 QUIET)
if(GTKSOURCEVIEW5_FOUND)
    set(GTKSOURCEVIEW_FOUND TRUE)
    set(GTKSOURCEVIEW_LIBRARIES ${GTKSOURCEVIEW5_LIBRARIES})
    set(GTKSOURCEVIEW_INCLUDE_DIRS ${GTKSOURCEVIEW5_INCLUDE_DIRS})
    set(GTKSOURCEVIEW_CFLAGS_OTHER ${GTKSOURCEVIEW5_CFLAGS_OTHER})
else()
    # Try version 4
    pkg_check_modules(GTKSOURCEVIEW4 gtksourceview-4 QUIET)
    if(GTKSOURCEVIEW4_FOUND)
        set(GTKSOURCEVIEW_FOUND TRUE)
        set(GTKSOURCEVIEW_LIBRARIES ${GTKSOURCEVIEW4_LIBRARIES})
        set(GTKSOURCEVIEW_INCLUDE_DIRS ${GTKSOURCEVIEW4_INCLUDE_DIRS})
        set(GTKSOURCEVIEW_CFLAGS_OTHER ${GTKSOURCEVIEW4_CFLAGS_OTHER})
    else()
        # Try plain name
        pkg_check_modules(GTKSOURCEVIEW_PLAIN gtksourceview QUIET)
        if(GTKSOURCEVIEW_PLAIN_FOUND)
            set(GTKSOURCEVIEW_FOUND TRUE)
            set(GTKSOURCEVIEW_LIBRARIES ${GTKSOURCEVIEW_PLAIN_LIBRARIES})
            set(GTKSOURCEVIEW_INCLUDE_DIRS ${GTKSOURCEVIEW_PLAIN_INCLUDE_DIRS})
            set(GTKSOURCEVIEW_CFLAGS_OTHER ${GTKSOURCEVIEW_PLAIN_CFLAGS_OTHER})
        endif()
    endif()
endif()

if(NOT GTKSOURCEVIEW_FOUND)
    message(STATUS "GtkSourceView not found - syntax highlighting will be disabled")
endif()

set(CMAKE_C_STANDARD 23)

add_executable(nginxui 
    src/main.c
    src/nginx_ui.c
    src/nginx_file.c
    src/nginx_hosts.c
)

# Link GTK4
target_link_libraries(nginxui
        PRIVATE
        PkgConfig::GTK4
)

# Link GtkSourceView if available
if(GTKSOURCEVIEW_FOUND)
    target_link_libraries(nginxui PRIVATE ${GTKSOURCEVIEW_LIBRARIES})
    target_include_directories(nginxui PRIVATE ${GTKSOURCEVIEW_INCLUDE_DIRS})
    target_compile_options(nginxui PRIVATE ${GTKSOURCEVIEW_CFLAGS_OTHER})
    add_definitions(-DHAVE_GTKSOURCEVIEW)
endif()

# Optional: warnings
target_compile_options(nginxui PRIVATE -Wall -Wextra)

# Install target
install(TARGETS nginxui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)