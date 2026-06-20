cmake_minimum_required(VERSION 3.15)
project(yajltcl C)

set(CMAKE_BUILD_TYPE Release)
add_definitions(-fPIC -DUSE_TCL_STUBS)

# yajltcl version (matches deps/yajl-tcl and the committed pkgIndex.tcl)
set(YAJLTCL_VERSION 1.8.1)

# yajl source comes from the top-level FetchContent (yajl_SOURCE_DIR).
# The yajl-tcl package binding lives in deps/yajl-tcl (a submodule).
set(YAJLTCL_SRC ${CMAKE_SOURCE_DIR}/deps/yajl-tcl)

if(NOT yajl_SOURCE_DIR)
    message(FATAL_ERROR "yajl_SOURCE_DIR not set - yajl FetchContent must run before pkgs/yajltcl")
endif()
if(NOT EXISTS ${YAJLTCL_SRC}/generic/yajltcl.c)
    message(FATAL_ERROR "deps/yajl-tcl not populated - run: git submodule update --init deps/yajl-tcl")
endif()

###############################
# Generate the <yajl/...> include tree
###############################
# yajl's sources and yajltcl both include <yajl/yajl_*.h>. Upstream yajl
# normally copies api/*.h into a yajl/ subdir during its own build; we do the
# same here and generate yajl_version.h from the template (pinned to 2.1.0).
set(YAJL_GEN_INC ${CMAKE_CURRENT_BINARY_DIR}/yajl_include)
file(MAKE_DIRECTORY ${YAJL_GEN_INC}/yajl)
file(COPY
    ${yajl_SOURCE_DIR}/src/api/yajl_common.h
    ${yajl_SOURCE_DIR}/src/api/yajl_gen.h
    ${yajl_SOURCE_DIR}/src/api/yajl_parse.h
    ${yajl_SOURCE_DIR}/src/api/yajl_tree.h
    DESTINATION ${YAJL_GEN_INC}/yajl
)
set(YAJL_MAJOR 2)
set(YAJL_MINOR 1)
set(YAJL_MICRO 0)
# The template uses ${YAJL_*} placeholders, so use the default (non-@ONLY) form.
configure_file(
    ${yajl_SOURCE_DIR}/src/api/yajl_version.h.cmake
    ${YAJL_GEN_INC}/yajl/yajl_version.h
)

###############################
# yajltcl module (yajl statically linked in)
###############################
file(GLOB YAJL_SRCS ${yajl_SOURCE_DIR}/src/*.c)

add_library(yajltcl SHARED
    ${YAJL_SRCS}
    ${YAJLTCL_SRC}/generic/yajltcl.c
    ${YAJLTCL_SRC}/generic/yajltcllex.c
    ${YAJLTCL_SRC}/generic/tclyajltcl.c
)

# PACKAGE_VERSION is normally provided by the TEA configure; define it here.
target_compile_definitions(yajltcl PRIVATE PACKAGE_VERSION="${YAJLTCL_VERSION}")

target_include_directories(yajltcl PRIVATE
    ${YAJL_GEN_INC}                 # <yajl/...>
    ${yajl_SOURCE_DIR}/src          # "api/..." and yajl-internal headers
    ${YAJLTCL_SRC}/generic
)

# Tcl stub headers from the in-tree build
if(tcl_SOURCE_DIR)
    target_include_directories(yajltcl PRIVATE
        ${tcl_SOURCE_DIR}/generic
        ${tcl_SOURCE_DIR}/libtommath
    )
    if(WIN32)
        target_include_directories(yajltcl PRIVATE ${tcl_SOURCE_DIR}/win)
    else()
        target_include_directories(yajltcl PRIVATE ${tcl_SOURCE_DIR}/unix)
    endif()
endif()

target_link_libraries(yajltcl PRIVATE tclstub)

# Match the filename the committed pkgIndex.tcl loads:
#   libtcl9yajltcl1.8.1[info sharedlibextension]
set_target_properties(yajltcl PROPERTIES
    OUTPUT_NAME "tcl9yajltcl${YAJLTCL_VERSION}"
    PREFIX "lib"
    POSITION_INDEPENDENT_CODE ON
)

if(APPLE)
    set_target_properties(yajltcl PROPERTIES LINK_FLAGS "-dynamiclib")
endif()
