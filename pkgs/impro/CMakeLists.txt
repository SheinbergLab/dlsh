cmake_minimum_required(VERSION 3.15)

project(impro)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DUSE_TCL_STUBS)

if(WIN32)
    include_directories(c:/usr/local/include ../../src ../../src/lablib)
    link_directories(c:/usr/local/lib)
elseif(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX -fPIC)
    include_directories(/usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk ../../src ../../src/lablib)
    link_directories(../../build /usr/local/lib /opt/homebrew/lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    include_directories(../local/include)
    link_directories(../local/aarch64/lib)
    set(TCLLIB tclstub)
else()
    add_definitions(-DLINUX -fPIC)
    include_directories(/usr/local/include ../../src ../../src/lablib)
    link_directories(/usr/local/lib)
    set(LIBDL dl)
    set(LIBDLSH dlsh)
    set(TCLLIB tclstub)
endif()

include_directories(${TCL_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

###############################
# impro
###############################
add_library(impro SHARED cimg_funcs.cpp imgdraw.c lodepng.c targa.c tcl_impro.c clahe.c)

if(WIN32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DLSH libdlsh.lib)
    set(TCLLIB tclstub.lib)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /LTCG /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /OUT:libimpro.dll")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
endif()
elseif(APPLE)
    set(TCLLIB tclstub)
    set(ZLIB z)
    set(BUNDLE_LOAD "-dynamiclib")
    set(DLSH dlsh)
else()
    set(DLSH dlsh)
    set(TCLLIB tclstub)
    set(ZLIB z)
endif()

target_link_libraries(impro ${DLSH} ${TCLLIB} ${LZ4LIB} ${JANSSON}  ${ZLIB})

install(TARGETS impro LIBRARY DESTINATION lib/impro/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR} DESTINATION lib COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
