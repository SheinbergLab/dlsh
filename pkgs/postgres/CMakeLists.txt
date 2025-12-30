cmake_minimum_required(VERSION 3.15)
project(postgres)

set(CMAKE_BUILD_TYPE Release)
add_definitions(-fPIC -DUSE_TCL_STUBS)

# Platform definitions
if(WIN32)
    # Windows-specific
elseif(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX)
else()
    add_definitions(-DLINUX)
endif()

###############################
# postgres
###############################
add_library(postgres SHARED src/postgres.c)

# Use in-tree tclstub and jansson from parent build
target_link_libraries(postgres PRIVATE tclstub jansson)

# TCL includes from in-tree build
if(tcl_SOURCE_DIR)
    target_include_directories(postgres PRIVATE 
        ${tcl_SOURCE_DIR}/generic
        ${tcl_SOURCE_DIR}/libtommath
    )
    if(WIN32)
        target_include_directories(postgres PRIVATE ${tcl_SOURCE_DIR}/win)
    else()
        target_include_directories(postgres PRIVATE ${tcl_SOURCE_DIR}/unix)
    endif()
endif()

# Jansson includes from in-tree build
if(jansson_SOURCE_DIR)
    target_include_directories(postgres PRIVATE 
        ${jansson_SOURCE_DIR}/src
        ${jansson_BINARY_DIR}/include
    )
endif()

###############################
# Find libpq (PostgreSQL client library)
###############################
# Try pkg-config first (works with cross-compilation if properly configured)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBPQ QUIET libpq)
endif()

if(NOT LIBPQ_FOUND)
    # Manual search - check common locations
    find_path(LIBPQ_INCLUDE_DIR libpq-fe.h
        HINTS
            ${CMAKE_SYSROOT}/usr/include/postgresql
            ${CMAKE_SYSROOT}/usr/include
        PATHS
            /usr/include/postgresql
            /usr/local/include
            /opt/homebrew/include/postgresql@17
            /opt/homebrew/include/postgresql
    )
    find_library(LIBPQ_LIBRARY NAMES pq
        HINTS
            ${CMAKE_SYSROOT}/usr/lib
            ${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
        PATHS
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
            /usr/local/lib
            /opt/homebrew/lib/postgresql@17
            /opt/homebrew/lib
    )
    
    if(LIBPQ_INCLUDE_DIR AND LIBPQ_LIBRARY)
        set(LIBPQ_FOUND TRUE)
        set(LIBPQ_INCLUDE_DIRS ${LIBPQ_INCLUDE_DIR})
        set(LIBPQ_LIBRARIES ${LIBPQ_LIBRARY})
        message(STATUS "Found libpq: ${LIBPQ_LIBRARY}")
    else()
        message(FATAL_ERROR "libpq not found. Install with: apt install libpq-dev (or libpq-dev:amd64 for cross)")
    endif()
endif()

target_include_directories(postgres PRIVATE ${LIBPQ_INCLUDE_DIRS})
target_link_libraries(postgres PRIVATE ${LIBPQ_LIBRARIES})

# Windows-specific linker settings
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_target_properties(postgres PROPERTIES
            LINK_FLAGS "/NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT"
        )
    endif()
elseif(APPLE)
    set_target_properties(postgres PROPERTIES
        LINK_FLAGS "-dynamiclib"
    )
endif()

# Install
install(TARGETS postgres 
    LIBRARY DESTINATION lib/postgres/${TCL_OS}/${TCL_MACHINE} 
    COMPONENT dlsh-zip
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/ 
    DESTINATION lib/postgres 
    COMPONENT dlsh-zip 
    FILES_MATCHING PATTERN *.tcl
    PATTERN "src" EXCLUDE
)
