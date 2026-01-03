cmake_minimum_required(VERSION 3.20)

# Allow older cmake_minimum_required in FetchContent dependencies (jansson, etc.)
# CMake 4.x is stricter about this
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

if(NOT DEFINED PROJECT_VERSION)
  set(PROJECT_VERSION 0.0.0)
endif()
project(dlsh VERSION ${PROJECT_VERSION})

include(FetchContent)

# Force position-independent code for all targets (needed for static libs in shared lib)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build static libs by default for FetchContent dependencies
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

###############################
# FetchContent Dependencies
###############################

# Option to use system libraries vs building in-tree
option(USE_SYSTEM_LIBS "Use system-installed libraries instead of building in-tree" OFF)

#----- nanoarrow -----
FetchContent_Declare(
    nanoarrow
    GIT_REPOSITORY https://github.com/apache/arrow-nanoarrow.git
    GIT_TAG        apache-arrow-nanoarrow-0.6.0
    GIT_SHALLOW    TRUE
)
set(NANOARROW_BUILD_TESTS OFF CACHE INTERNAL "")
set(NANOARROW_BUILD_INTEGRATION_TESTS OFF CACHE INTERNAL "")
set(NANOARROW_BUILD_BENCHMARKS OFF CACHE INTERNAL "")
set(NANOARROW_IPC ON CACHE INTERNAL "")
set(NANOARROW_FLATCC ON CACHE INTERNAL "")

#----- tcl (stub library only) -----
# TCL doesn't have native CMake support, so we fetch the source and build
# just the stub library ourselves. This is sufficient for extensions.
FetchContent_Declare(
    tcl
    GIT_REPOSITORY https://github.com/tcltk/tcl.git
    GIT_TAG        core-9-0-1
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL  # Don't try to build TCL's (non-existent) CMake project
)
# Use MakeAvailable but TCL has no CMakeLists.txt, so it just populates
FetchContent_MakeAvailable(tcl)

# Build tclstub as a static library from TCL sources
# The stub library is just tclStubLib.c which provides the stub table
add_library(tclstub STATIC
    ${tcl_SOURCE_DIR}/generic/tclStubLib.c
    ${tcl_SOURCE_DIR}/generic/tclTomMathStubLib.c
    ${tcl_SOURCE_DIR}/generic/tclOOStubLib.c
)

target_include_directories(tclstub PUBLIC
    ${tcl_SOURCE_DIR}/generic
    ${tcl_SOURCE_DIR}/libtommath  # for tommath.h
)

# Platform-specific includes
if(WIN32)
    target_include_directories(tclstub PUBLIC ${tcl_SOURCE_DIR}/win)
    target_compile_definitions(tclstub PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_include_directories(tclstub PUBLIC ${tcl_SOURCE_DIR}/unix)
endif()

set_target_properties(tclstub PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "tclstub9.0"
)

#----- box2d -----
# For tclbox2d package
FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE INTERNAL "")
set(BOX2D_BUILD_TESTBED OFF CACHE INTERNAL "")
set(BOX2D_BUILD_DOCS OFF CACHE INTERNAL "")
set(BOX2D_SAMPLES OFF CACHE INTERNAL "")
set(BOX2D_VALIDATE OFF CACHE INTERNAL "")
# Disable -Werror for box2d - GCC 14 is stricter and flags false positives
set(BOX2D_WERROR OFF CACHE INTERNAL "")

#----- jansson -----
FetchContent_Declare(
    jansson
    GIT_REPOSITORY https://github.com/akheron/jansson.git
    GIT_TAG        v2.14
    GIT_SHALLOW    TRUE
)
set(JANSSON_BUILD_DOCS OFF CACHE INTERNAL "")
set(JANSSON_EXAMPLES OFF CACHE INTERNAL "")
set(JANSSON_WITHOUT_TESTS ON CACHE INTERNAL "")

#----- lz4 -----
FetchContent_Declare(
    lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG        v1.10.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  build/cmake
)
set(LZ4_BUILD_CLI OFF CACHE INTERNAL "")
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE INTERNAL "")

#----- zlib -----
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    GIT_SHALLOW    TRUE
)

#----- msgpack-c -----
FetchContent_Declare(
    msgpackc
    GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
    GIT_TAG        c-6.0.0
    GIT_SHALLOW    TRUE
)
set(MSGPACK_BUILD_TESTS OFF CACHE INTERNAL "")
set(MSGPACK_BUILD_EXAMPLES OFF CACHE INTERNAL "")

#----- libpng (for libharu) -----
FetchContent_Declare(
    libpng
    GIT_REPOSITORY https://github.com/glennrp/libpng.git
    GIT_TAG        v1.6.43
    GIT_SHALLOW    TRUE
)
set(PNG_SHARED OFF CACHE INTERNAL "")
set(PNG_STATIC ON CACHE INTERNAL "")
set(PNG_TESTS OFF CACHE INTERNAL "")
set(PNG_TOOLS OFF CACHE INTERNAL "")
set(PNG_BUILD_ZLIB ON CACHE INTERNAL "")
set(SKIP_INSTALL_ALL ON CACHE INTERNAL "")

# Make zlib available first (libpng needs it)
FetchContent_MakeAvailable(zlib)

# Ensure zlib targets have -fPIC
if(TARGET zlibstatic)
    set_property(TARGET zlibstatic PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

# Create ZLIB::ZLIB alias that libpng expects
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

# Point libpng to our in-tree zlib
set(ZLIB_LIBRARY zlibstatic CACHE INTERNAL "")
set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} CACHE INTERNAL "")
set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} CACHE INTERNAL "")
set(ZLIB_FOUND TRUE CACHE INTERNAL "")

# Now make libpng available
FetchContent_MakeAvailable(libpng)

# Ensure libpng is built with PIC
if(TARGET png_static)
    set_property(TARGET png_static PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

# Make remaining dependencies available
FetchContent_MakeAvailable(nanoarrow jansson lz4 box2d msgpackc)

# Global include for lz4
include_directories(${lz4_SOURCE_DIR}/lib)

add_subdirectory(libdg)

# Disable -Werror for box2d - GCC 14 cross-compiler is stricter and flags false positives
# Use the CMake 3.24+ property, plus add -Wno-error flag
if(TARGET box2d)
    set_target_properties(box2d PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    # Also directly add -Wno-error and disable the specific warning
    target_compile_options(box2d PRIVATE 
        -Wno-error 
        -Wno-error=maybe-uninitialized
        -Wno-maybe-uninitialized
    )
endif()

# Cache source directories so subdirectories (pkgs/*) can access them
set(tcl_SOURCE_DIR ${tcl_SOURCE_DIR} CACHE PATH "TCL source directory")
set(box2d_SOURCE_DIR ${box2d_SOURCE_DIR} CACHE PATH "Box2D source directory")
set(jansson_SOURCE_DIR ${jansson_SOURCE_DIR} CACHE PATH "Jansson source directory")
set(jansson_BINARY_DIR ${jansson_BINARY_DIR} CACHE PATH "Jansson binary directory")
set(lz4_SOURCE_DIR ${lz4_SOURCE_DIR} CACHE PATH "LZ4 source directory")
set(zlib_SOURCE_DIR ${zlib_SOURCE_DIR} CACHE PATH "zlib source directory")
set(zlib_BINARY_DIR ${zlib_BINARY_DIR} CACHE PATH "zlib binary directory")
set(nanoarrow_SOURCE_DIR ${nanoarrow_SOURCE_DIR} CACHE PATH "nanoarrow source directory")
set(libpng_SOURCE_DIR ${libpng_SOURCE_DIR} CACHE PATH "libpng source directory")
set(libpng_BINARY_DIR ${libpng_BINARY_DIR} CACHE PATH "libpng binary directory")

# Ensure flatcc is built with -fPIC
if(TARGET flatccrt)
    set_property(TARGET flatccrt PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
if(TARGET flatcc)
    set_property(TARGET flatcc PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

###############################
# libharu (hpdf) - optional
###############################
option(WITH_HPDF "Build with libharu PDF support" ON)
if(WITH_HPDF)
    FetchContent_Declare(
        libharu
        GIT_REPOSITORY https://github.com/libharu/libharu.git
        GIT_TAG        v2.4.4
        GIT_SHALLOW    TRUE
    )
    set(LIBHPDF_STATIC ON CACHE INTERNAL "")
    set(LIBHPDF_SHARED OFF CACHE INTERNAL "")
    set(LIBHPDF_EXAMPLES OFF CACHE INTERNAL "")
    
    # Point libharu to our in-tree zlib and png
    set(ZLIB_LIBRARY zlibstatic CACHE INTERNAL "")
    set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR} CACHE INTERNAL "")
    set(PNG_LIBRARY png_static CACHE INTERNAL "")
    set(PNG_INCLUDE_DIR ${libpng_SOURCE_DIR} ${libpng_BINARY_DIR} CACHE INTERNAL "")
    
    FetchContent_MakeAvailable(libharu)
    
    # libharu creates target "hpdfs" for static library (not "hpdf_static")
    # Ensure it's built with PIC for linking into shared library
    if(TARGET hpdfs)
        set_property(TARGET hpdfs PROPERTY POSITION_INDEPENDENT_CODE ON)
        set(HPDF_TARGET hpdfs)
    elseif(TARGET hpdf_static)
        set_property(TARGET hpdf_static PROPERTY POSITION_INDEPENDENT_CODE ON)
        set(HPDF_TARGET hpdf_static)
    elseif(TARGET hpdf)
        set_property(TARGET hpdf PROPERTY POSITION_INDEPENDENT_CODE ON)
        set(HPDF_TARGET hpdf)
    else()
        message(WARNING "Could not find libharu target")
        set(WITH_HPDF OFF)
    endif()
endif()

###############################
# Platform Configuration
###############################

set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)

if(WIN32)
    add_definitions(-DUSE_TCL_STUBS)
else()
    add_definitions(-fPIC -DUSE_TCL_STUBS)
endif()

if(WIN32)
    include_directories(src src/lablib)
elseif(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX)
    include_directories(src src/lablib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    add_definitions(-DLINUX)
    include_directories(src src/lablib ../local/include)
    link_directories(../local/aarch64/lib)
else()
    add_definitions(-DLINUX)
    include_directories(src src/lablib ${APP_DIR})
    set(LIBDL dl)
endif()

###############################
# dlsh library
###############################
add_library(dlsh SHARED 
    src/dfana.c 
    src/tcl_dlg.c 
    src/dfevt.c 
    src/dlsh_pkg.c 
    src/tcl_df.c 
    src/tcl_dm.c 
    src/dlarith.c 
    src/dmana.c 
    src/tcl_dl.c 
    src/dgjson.c 
    src/dgmsgpack.c
    src/dgarrow.c
    src/lablib/gbufutl.c 
    src/lablib/gbuf.c 
    src/lablib/cg_base.c 
    src/lablib/axes.c 
    src/lablib/cgraph.c 
    src/lablib/timer.c 
    src/lablib/utilc_unix.c 
    src/lablib/randvars.c 
    src/lablib/prmutil.c 
    src/lablib/dfutils.c 
    src/lablib/df.c 
    src/lablib/dynio.c 
    src/lablib/rawapi.c 
    src/lablib/lodepng.c 
    src/lablib/lz4utils.c 
    src/lablib/dslog.c
    src/lablib/b64.c
)

set_target_properties(dlsh PROPERTIES 
    PUBLIC_HEADER "src/dfana.h;src/tcl_dl.h;src/lablib/cgraph.h;src/lablib/gbuf.h;src/lablib/utilc.h;src/dgmsgpack.h"
)

###############################
# Include directories for in-tree deps
###############################
target_include_directories(dlsh PRIVATE
    ${tcl_SOURCE_DIR}/generic
    ${tcl_SOURCE_DIR}/libtommath
    ${nanoarrow_SOURCE_DIR}/src/nanoarrow
    ${nanoarrow_SOURCE_DIR}/src/nanoarrow_ipc
    ${msgpackc_SOURCE_DIR}/include
    ${msgpackc_BINARY_DIR}/include
    ${msgpackc_BINARY_DIR}/include/msgpack  # for sysdep.h
    ${jansson_SOURCE_DIR}/src
    ${jansson_BINARY_DIR}/include  # for jansson_config.h
    ${lz4_SOURCE_DIR}/lib
    ${zlib_SOURCE_DIR}
    ${zlib_BINARY_DIR}  # for zconf.h
)

# Platform-specific TCL includes
if(WIN32)
    target_include_directories(dlsh PRIVATE ${tcl_SOURCE_DIR}/win)
else()
    target_include_directories(dlsh PRIVATE ${tcl_SOURCE_DIR}/unix)
endif()

if(WITH_HPDF)
    target_include_directories(dlsh PRIVATE
        ${libharu_SOURCE_DIR}/include
        ${libharu_BINARY_DIR}/include  # for hpdf_config.h
    )
endif()

###############################
# TCL stub library - use in-tree build
###############################
set(TCLLIB tclstub)

###############################
# Link libraries
###############################
target_link_libraries(dlsh PRIVATE
    # In-tree built dependencies (using target names)
    jansson
    lz4_static
    zlibstatic
    nanoarrow_ipc
    nanoarrow
    msgpack-c
    ${TCLLIB}
)

if(WITH_HPDF)
    target_link_libraries(dlsh PRIVATE ${HPDF_TARGET} png_static zlibstatic)
endif()

# Platform-specific link libraries
if(NOT WIN32 AND NOT APPLE)
    target_link_libraries(dlsh PRIVATE ${LIBDL})
endif()

###############################
# Windows-specific linker flags
###############################
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DEF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/dlsh.def)
        set_target_properties(dlsh PROPERTIES
            OUTPUT_NAME "libdlsh"
            LINK_FLAGS "/INCREMENTAL /LTCG /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /DEF:${DEF_FILE}"
        )
    endif()
elseif(APPLE)
    set_target_properties(dlsh PROPERTIES
        LINK_FLAGS "-dynamiclib"
    )
endif()

###############################
# Subdirectories and Installation
###############################
add_subdirectory(pkgs)

set(CPACK_PACKAGE_NAME dlsh)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Dynamic Lists and other TCL utilities.")
set(CPACK_PACKAGE_CONTACT SheinbergLab)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")

install(TARGETS dlsh 
    LIBRARY DESTINATION lib COMPONENT dlsh
    PUBLIC_HEADER DESTINATION include COMPONENT dlsh
)

include(InstallRequiredSystemLibraries)

if(WIN32)
    # TODO
elseif(APPLE)
    if(DEFINED CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM)
        set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--strict --timestamp --options=runtime")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")
        set(CMAKE_SKIP_RPATH TRUE)

        list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/macos)
        set(CPACK_PRODUCTBUILD_IDENTIFIER "org.sheinberglab")
        set(CPACK_RESOURCE_FILE_WELCOME ${CMAKE_SOURCE_DIR}/macos/welcome.txt)
        set(CPACK_RESOURCE_FILE_README ${CMAKE_SOURCE_DIR}/macos/readme.txt)
        set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/macos/license.txt)
    endif()
else()
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()

include(CPack)
cpack_add_component(dlsh)
