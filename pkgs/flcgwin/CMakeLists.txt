cmake_minimum_required(VERSION 3.15)

project(flcgwin)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DUSE_TCL_STUBS)

if(WIN32)
    add_compile_options("/DFL_DLL")
    include_directories(c:/usr/local/include ../../src ../../src/lablib)
    link_directories(c:/usr/local/lib/$ENV{VSCMD_ARG_TGT_ARCH})
else(APPLE)
    add_definitions(-fPIC)
    include_directories(../../src ../../src/lablib /usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk)
    link_directories(../../build /usr/local/lib /opt/homebrew/lib)
endif()

include_directories(${TCL_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

###############################
# flcgwin
###############################
add_library(flcgwin SHARED flcgwin.cpp cg_tab_manager.cpp)

if(WIN32)
    set(TCLLIB tclstub.lib)
    set(DLSH libdlsh.lib)
    set(FLTKLIB fltk_dll.lib)
    set(DEF_FILE ../flcgwin.def)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:MSCVRTD gdiplus.lib  comctl32.lib  ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib /OUT:libflcgwin.dll /DEF:${DEF_FILE}")
elseif(APPLE)
    set(TCLLIB tclstub)
    set(BUNDLE_LOAD "-dynamiclib -undefined dynamic_lookup")
    set(DLSH dlsh)
else()
    set(DLSH dlsh)
    set(TCLLIB tclstub)
    set(ZLIB z)
endif()

if(APPLE)
    target_link_libraries(flcgwin ${TCLLIB} ${BUNDLE_LOAD} ${DLSH})
elseif(WIN32)
    target_link_libraries(flcgwin ${TCLLIB} ${DLSH} ${FLTKLIB})
else()
    target_link_libraries(flcgwin ${TCLLIB} ${DLSH})
endif()

install(TARGETS flcgwin LIBRARY DESTINATION lib/flcgwin/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR} DESTINATION lib COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
