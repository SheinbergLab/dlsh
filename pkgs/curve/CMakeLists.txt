cmake_minimum_required(VERSION 3.15)

project(curve)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DUSE_TCL_STUBS)

if(WIN32)
    include_directories(c:/usr/local/include)
    link_directories(c:/usr/local/lib/$ENV{VSCMD_ARG_TGT_ARCH})
elseif(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX -fPIC)
    include_directories(/usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk)
    link_directories(/usr/local/lib /opt/homebrew/lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    include_directories(../local/include)
    link_directories(../local/aarch64/lib)
    set(TCLLIB tclstub)
else()
    add_definitions(-DLINUX -fPIC)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
    set(TCLLIB tclstub)
    set(LIBDL dl)
endif()

include_directories(${TCL_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

###############################
# curve
###############################
add_library(curve SHARED clipper.cpp clipper_dl.cpp clipper_svg.cpp curve.c gpc.c tcl_curve.c)

if(WIN32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DEF_FILE ../curve.def)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /LTCG /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /OUT:libcurve.dll /DEF:${DEF_FILE} /IMPLIB:libcurve.lib")
    set(DLSH libdlsh.lib)
    set(TCLLIB tclstub.lib)
endif()
elseif(APPLE)
    set(DLSH dlsh)
    set(TCLLIB tclstub)
    set(BUNDLE_LOAD "-dynamiclib")
else()
    set(DLSH dlsh)
    set(TCLLIB tclstub)
    set(ZLIB z)
endif()

target_link_libraries(curve ${DLSH} ${TCLLIB} ${PDFLIB} ${LZ4LIB} ${ZLIB})

install(TARGETS curve LIBRARY DESTINATION lib/curve/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR} DESTINATION lib COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
