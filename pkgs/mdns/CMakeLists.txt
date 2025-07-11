cmake_minimum_required(VERSION 3.14)

project(tcl_mdns)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DUSE_TCL_STUBS)

if(WIN32)
    include_directories(c:/usr/local/include)
    link_directories(c:/usr/local/lib)
    set(TCLLIB c:/usr/local/lib/tclstub.lib)
elseif(APPLE)
    add_definitions(-fPIC -DMACOS -Dunix -DLINUX)
    include_directories(/usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk)
    set(TCLLIB tclstub)
    set(CMAKE_SHARED_MODULE_SUFFIX .dylib)
    link_directories(/usr/local/lib /opt/homebrew/lib)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    include_directories(../local/include)
    link_directories(../local/aarch64/lib)
    set(TCLLIB tclstub)
else()
    add_definitions(-DLINUX -fPIC)
    include_directories(${APP_DIR} /usr/local/include)
    link_directories(/usr/local/lib)
    set(TCLLIB tclstub)
endif()

include_directories(${TCL_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

###############################
# tcl_mdns
###############################
add_library(tcl_mdns MODULE tcl_mdns.c mdns.c mdns_query.c)

if(WIN32)
    set(CMAKE_SHARED_MODULE_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /def:${DEF_FILE}")
elseif(APPLE)
    set(BUNDLE_LOAD "-dynamiclib")
endif()

set_target_properties(tcl_mdns PROPERTIES PREFIX "")

target_link_libraries(tcl_mdns ${TCLLIB})

install(TARGETS tcl_mdns LIBRARY DESTINATION lib/mdns/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR} DESTINATION lib COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
