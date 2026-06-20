cmake_minimum_required(VERSION 3.20)
project(tclsqlite C)

# =====================================================================
# Pinned SQLite version.  To upgrade, bump these two values only.
# Find the current version and its release year at:
#     https://sqlite.org/download.html
# =====================================================================
set(SQLITE_VERSION 3.53.2)
set(SQLITE_YEAR    2026)
# =====================================================================

# Derive the pieces the SQLite download/build naming needs:
#   download code : 3.53.2 -> 3530200   (used in the tarball filename)
#   windows nodots: 3.53.2 -> 3532      (used in the .dll name)
string(REPLACE "." ";" _sv "${SQLITE_VERSION}")
list(GET _sv 0 SQLITE_MAJOR)
list(GET _sv 1 SQLITE_MINOR)
list(GET _sv 2 SQLITE_PATCH)
math(EXPR SQLITE_CODE
    "${SQLITE_MAJOR}*1000000 + ${SQLITE_MINOR}*10000 + ${SQLITE_PATCH}*100")
set(SQLITE_NODOTS "${SQLITE_MAJOR}${SQLITE_MINOR}${SQLITE_PATCH}")

# ---------------------------------------------------------------------
# Fetch the autoconf amalgamation tarball (source only).  It ships both
# the SQLite amalgamation (sqlite3.c/.h) and the Tcl binding
# (tea/generic/tclsqlite3.c, which #includes sqlite3.c).  No CMakeLists,
# so MakeAvailable just populates the tree.
# ---------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(sqlite_amalg
    URL https://www.sqlite.org/${SQLITE_YEAR}/sqlite-autoconf-${SQLITE_CODE}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite_amalg)

set(SQLITE_TCL_SRC ${sqlite_amalg_SOURCE_DIR}/tea/generic/tclsqlite3.c)
if(NOT EXISTS ${SQLITE_TCL_SRC})
    message(FATAL_ERROR "tclsqlite3.c not found in fetched sqlite tarball: ${SQLITE_TCL_SRC}")
endif()

# ---------------------------------------------------------------------
# Build the Tcl sqlite3 extension (single translation unit; the binding
# pulls in the amalgamation via #include "sqlite3.c").
# Feature set mirrors the previously-shipped binary.
# ---------------------------------------------------------------------
add_library(tclsqlite SHARED ${SQLITE_TCL_SRC})

target_compile_definitions(tclsqlite PRIVATE
    USE_TCL_STUBS
    SQLITE_ENABLE_FTS3
    SQLITE_ENABLE_FTS4
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_GEOPOLY
    SQLITE_ENABLE_MATH_FUNCTIONS
    SQLITE_ENABLE_DBSTAT_VTAB
    SQLITE_THREADSAFE=1
)

target_include_directories(tclsqlite PRIVATE ${sqlite_amalg_SOURCE_DIR})

# Tcl stub headers from the in-tree build
if(tcl_SOURCE_DIR)
    target_include_directories(tclsqlite PRIVATE
        ${tcl_SOURCE_DIR}/generic
        ${tcl_SOURCE_DIR}/libtommath
    )
    if(WIN32)
        target_include_directories(tclsqlite PRIVATE ${tcl_SOURCE_DIR}/win)
    else()
        target_include_directories(tclsqlite PRIVATE ${tcl_SOURCE_DIR}/unix)
    endif()
endif()

target_link_libraries(tclsqlite PRIVATE tclstub)

set_target_properties(tclsqlite PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Filenames must match what pkgIndex.tcl loads:
#   Windows : tcl9sqlite<nodots>.dll        (e.g. tcl9sqlite3532.dll)
#   Unix    : libtcl9sqlite<version>.{so,dylib}  (e.g. libtcl9sqlite3.53.2.dylib)
if(WIN32)
    set_target_properties(tclsqlite PROPERTIES
        OUTPUT_NAME "tcl9sqlite${SQLITE_NODOTS}"
        PREFIX ""
    )
else()
    set_target_properties(tclsqlite PROPERTIES
        OUTPUT_NAME "tcl9sqlite${SQLITE_VERSION}"
        PREFIX "lib"
    )
    if(APPLE)
        set_target_properties(tclsqlite PROPERTIES LINK_FLAGS "-dynamiclib")
    endif()
endif()

# ---------------------------------------------------------------------
# Generate the version-stamped pkgIndex.tcl so a version bump needs no
# other edits.  Exposed to the parent scope for install_tcl_package().
# ---------------------------------------------------------------------
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/pkgIndex.tcl.in
    ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl
    @ONLY
)
set(SQLITE_PKGINDEX ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl PARENT_SCOPE)
