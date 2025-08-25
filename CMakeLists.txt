cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED PROJECT_VERSION)
  set(PROJECT_VERSION 0.0.0)
endif()
project(dlsh VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

add_subdirectory( libdg )

# Find required Arrow package
find_package(Arrow REQUIRED CONFIG)

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
    include_directories(src src/lablib /usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk)
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
    include_directories(src src/lablib ${APP_DIR} /usr/local/include)
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
    src/lablib/gbufutl.c 
    src/lablib/gbuf.c 
    src/lablib/cg_ps.c 
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
    src/dgarrow.cpp
)

set_target_properties(dlsh PROPERTIES PUBLIC_HEADER "src/dfana.h;src/tcl_dl.h;src/lablib/cgraph.h;src/lablib/gbuf.h;src/lablib/utilc.h;")

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

target_link_libraries(dlsh PRIVATE ${LIBJANSSON} ${TCLLIB} ${PDFLIB} ${LZ4LIB} ${ZLIB} Arrow::arrow_shared)

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
