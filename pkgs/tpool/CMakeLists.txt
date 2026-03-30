cmake_minimum_required(VERSION 3.14)

project(tpool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# Built as a stubs-based loadable extension, same as the Tcl Thread package.
# Worker threads create new interps and call Tcl_InitStubs on each one.
# The zipfs VFS mount is process-global so child interps see it automatically.
if(WIN32)
    add_definitions(-DUSE_TCL_STUBS)
else()
    add_definitions(-fPIC -DUSE_TCL_STUBS)
endif()

if(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX)
endif()

add_library(tpool SHARED tpool.cpp)

target_include_directories(tpool PRIVATE
    ${tcl_SOURCE_DIR}/generic
    ${tcl_SOURCE_DIR}/libtommath
)
if(WIN32)
    target_include_directories(tpool PRIVATE ${tcl_SOURCE_DIR}/win)
else()
    target_include_directories(tpool PRIVATE ${tcl_SOURCE_DIR}/unix)
endif()

target_link_libraries(tpool tclstub)

install(TARGETS tpool
    LIBRARY DESTINATION lib/tpool/${TCL_OS}/${TCL_MACHINE}
    COMPONENT vfs
)
