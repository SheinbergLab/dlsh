cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(dgarrow)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DUSE_TCL_STUBS)

if(WIN32)
    include_directories(c:/usr/local/include ../../src ../../src/lablib)
    link_directories(c:/usr/local/lib)
else()
    add_definitions(-fPIC)
    include_directories(../../src ../../src/lablib)
    link_directories(../../build /usr/local/lib /opt/homebrew/lib)
endif()

include_directories(${TCL_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

# Find Arrow
find_package(Arrow REQUIRED)

###############################
# dgarrow
###############################
add_library(dgarrow SHARED dgarrow.cpp)

# Add Arrow include directories
target_include_directories(dgarrow PRIVATE ${arrow_SOURCE_DIR}/cpp/src)

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DLSH libdlsh.lib)
        set(TCLLIB tclstub.lib)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /LTCG /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT")
    endif()
elseif(APPLE)
    set(TCLLIB tclstub)
    set(BUNDLE_LOAD "-dynamiclib -undefined dynamic_lookup")
    set(DLSH dlsh)
else()
    set(DLSH dlsh)
    set(TCLLIB tclstub)
endif()

if(APPLE)
    target_link_libraries(dgarrow ${TCLLIB} ${BUNDLE_LOAD} ${DLSH} Arrow::arrow_static)
else()
    target_link_libraries(dgarrow ${TCLLIB} ${DLSH} Arrow::arrow_static)
endif()

install(TARGETS dgarrow LIBRARY DESTINATION lib/dgarrow/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR} DESTINATION lib COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
