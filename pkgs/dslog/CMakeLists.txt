cmake_minimum_required(VERSION 3.15)

project(dslog)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-fPIC -DUSE_TCL_STUBS)

set(DLSH dlsh)
set(TCLLIB tclstub)

include_directories(../../src/ ../../src/lablib /usr/local/include)

if(WIN32)
    link_directories(c:/usr/local/lib)
elseif(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX)
    include_directories(/opt/homebrew/include/ /opt/homebrew/include/tcl-tk)
    link_directories(/usr/local/lib /opt/homebrew/lib)
    set(BUNDLE_LOAD "-dynamiclib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    link_directories(../local/aarch64/lib)
else()
    add_definitions(-DLINUX)
    link_directories(/usr/local/lib)
endif()

include_directories(${TCL_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

###############################
# dslog
###############################
add_library(dslog SHARED tcl_dslog.c)

set(DLSH dlsh)

if(WIN32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DLSH dlsh64.lib)
    set(LAB lablib64.lib)
    set(GLFW glfw3_64)
    set(TCLLIB tclstub.lib)
    set(CMAKE_EXE_MODULE_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /def:${DEF_FILE}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(DLSH dlsh.lib)
    set(TCLLIB tclstub.lib)
    set(CMAKE_EXE_MODULE_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /def:${DEF_FILE}")
endif()
elseif(APPLE)
    set(TCLLIB tclstub)
    set(PDFLIB pdf)	
    set(LZ4LIB lz4)
    set(ZLIB z)
    set(BUNDLE_LOAD "-dynamiclib")
else()
    set(TCLLIB tclstub)
    set(PDFLIB pdf)	
    set(LZ4LIB lz4)
    set(ZLIB z)
endif()

target_link_libraries(dslog ${DLSH} ${TCLLIB} ${ZLIB})

install(TARGETS dslog LIBRARY DESTINATION lib/dslog/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR} DESTINATION lib COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
