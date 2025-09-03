cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED PROJECT_VERSION)
  set(PROJECT_VERSION 0.0.0)
endif()
project(dlsh VERSION ${PROJECT_VERSION})

add_subdirectory( libdg )

# Fetch nanoarrow as a dependency
include(FetchContent)

FetchContent_Declare(
    nanoarrow
    GIT_REPOSITORY https://github.com/apache/arrow-nanoarrow.git
    GIT_TAG        apache-arrow-nanoarrow-0.6.0  # Use a specific release tag
    GIT_SHALLOW    TRUE
)

# Set options for nanoarrow build
set(NANOARROW_BUILD_TESTS OFF CACHE INTERNAL "")
set(NANOARROW_BUILD_INTEGRATION_TESTS OFF CACHE INTERNAL "")
set(NANOARROW_BUILD_BENCHMARKS OFF CACHE INTERNAL "")
set(NANOARROW_IPC ON CACHE INTERNAL "")  # Enable IPC support
set(NANOARROW_FLATCC ON CACHE INTERNAL "")  # Enable flatcc for IPC
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")  # Build static libs

# Force position-independent code for all targets (needed for static libs in shared lib on Linux)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_MakeAvailable(nanoarrow)

# Ensure flatcc is also built with -fPIC
if(TARGET flatccrt)
    set_property(TARGET flatccrt PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()
if(TARGET flatcc)
    set_property(TARGET flatcc PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

# Use system MessagePack with better fallback handling
find_package(PkgConfig QUIET)
set(MSGPACK_FOUND FALSE)

if(PkgConfig_FOUND)
    # Try both possible pkg-config names
    pkg_check_modules(MSGPACK QUIET msgpack)
    if(NOT MSGPACK_FOUND)
        pkg_check_modules(MSGPACK QUIET msgpack-c)
    endif()
    if(MSGPACK_FOUND)
        message(STATUS "Found MessagePack via pkg-config: ${MSGPACK_LIBRARIES}")
    endif()
endif()

if(NOT MSGPACK_FOUND)
    # Fallback: try to find manually
    find_path(MSGPACK_INCLUDE_DIR msgpack.h 
        PATHS /usr/include /usr/local/include /opt/homebrew/include)
    find_library(MSGPACK_LIBRARY NAMES msgpackc msgpack-c msgpack
        PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu 
              /usr/local/lib /opt/homebrew/lib)
    
    if(MSGPACK_INCLUDE_DIR AND MSGPACK_LIBRARY)
        set(MSGPACK_FOUND TRUE)
        set(MSGPACK_INCLUDE_DIRS ${MSGPACK_INCLUDE_DIR})
        set(MSGPACK_LIBRARIES ${MSGPACK_LIBRARY})
        message(STATUS "Found MessagePack manually: ${MSGPACK_LIBRARY}")
    else()
        message(FATAL_ERROR "MessagePack not found. Please install libmsgpack-dev")
    endif()
endif()

set(CMAKE_BUILD_TYPE Release)
if (WIN32)
    add_definitions(-DUSE_TCL_STUBS)
else()
    add_definitions(-fPIC -DUSE_TCL_STUBS)
endif()

if(WIN32)
    include_directories(src src/lablib c:/tcl/include c:/usr/local/include c:/usr/local/include/hpdf)
    link_directories(c:/usr/local/lib/$ENV{VSCMD_ARG_TGT_ARCH})
elseif(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX)
    include_directories(src src/lablib /usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk ${MSGPACK_INCLUDE_DIRS})
    link_directories(/usr/local/lib /opt/homebrew/lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    add_definitions(-DLINUX)
    include_directories(../local/include)
    link_directories(../local/aarch64/lib)
    set(TCLLIB tclstub)
    set(PDFLIB hpdf)
    set(LZ4LIB lz4)
    set(ZLIB z)
else()
    add_definitions(-DLINUX)
    include_directories(src src/lablib ${APP_DIR} /usr/local/include ${MSGPACK_INCLUDE_DIRS})
    link_directories(/usr/local/lib)
    set(LIBDL dl)
endif()

include_directories(PRIVATE ${TCL_INCLUDE_DIR} src)

###############################
# dlsh
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
)

set_target_properties(dlsh PROPERTIES PUBLIC_HEADER "src/dfana.h;src/tcl_dl.h;src/lablib/cgraph.h;src/lablib/gbuf.h;src/lablib/utilc.h;src/dgmsgpack.h")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DEF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/dlsh.def)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /LTCG /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /OUT:libdlsh.dll /DEF:${DEF_FILE} /IMPLIB:libdlsh.lib")
        set(TCLLIB tclstub.lib)
        set(ZLIB zlibstatic.lib)
        set(LZ4LIB liblz4_static.lib)
        set(PDFLIB hpdf.lib)
        set(LIBJANSSON jansson.lib)
    endif()
elseif(APPLE)
    set(TCLLIB tclstub)
    set(ZLIB z)
    find_library(PDFLIB NAMES "libhpdf.a")
    find_library(LIBJANSSON NAMES "libjansson.a")
    find_library(LZ4LIB NAMES "liblz4.a")
    set(BUNDLE_LOAD "-dynamiclib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(LIBJANSSON jansson)
    set(LABLIB lab)
    set(TCLLIB tclstub)
    set(PDFLIB libhpdf.a)
    set(LZ4LIB lz4)
    set(ZLIB z)
else()
    find_library(LIBJANSSON NAMES "libjansson.a")
    set(LABLIB lab)
    set(TCLLIB tclstub)
    find_library(PDFLIB NAMES "libhpdf.a")
    set(LZ4LIB lz4)
    set(ZLIB z)
    set(PDFLIB hpdf)
endif()

# Link with MessagePack and nanoarrow (static)
target_link_libraries(dlsh PRIVATE 
    ${LIBJANSSON} 
    ${TCLLIB} 
    ${PDFLIB} 
    ${LZ4LIB} 
    ${ZLIB}
    ${MSGPACK_LIBRARIES}
    nanoarrow_ipc
    nanoarrow
)

# Include nanoarrow headers
target_include_directories(dlsh PRIVATE 
    ${nanoarrow_SOURCE_DIR}/src/nanoarrow
    ${nanoarrow_SOURCE_DIR}/src/nanoarrow_ipc
)

set(CPACK_PACKAGE_NAME dlsh)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Dynamic Lists and other TCL utilities.")
set(CPACK_PACKAGE_CONTACT SheinbergLab)

add_subdirectory( pkgs )

set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
install(TARGETS dlsh 
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
    COMPONENT dlsh
)

include(InstallRequiredSystemLibraries)

if(WIN32)
    # TODO
elseif(APPLE)
    if(DEFINED "CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM")
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
