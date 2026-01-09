cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
project(tcljson)
set(CMAKE_BUILD_TYPE Release)

add_definitions(-DUSE_TCL_STUBS)

if(WIN32)
    include_directories(c:/usr/local/include ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/external)
    link_directories(c:/usr/local/lib)
else()
    add_definitions(-fPIC)
    include_directories(/usr/local/include ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/external)
    link_directories(/usr/local/lib /opt/homebrew/lib)
endif()


###############################
# tcljson
###############################
set(SOURCES
    tcljson.cpp
)

add_library(tcljson SHARED ${SOURCES})

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TCLLIB tclstub.lib)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /LTCG /NODEFAULTLIB:libcmt")
    endif()
elseif(APPLE)
    set(TCLLIB tclstub)
    set(BUNDLE_LOAD "-dynamiclib -undefined dynamic_lookup")
else()
    set(TCLLIB tclstub)
endif()

if(APPLE)
    target_link_libraries(tcljson ${TCLLIB} ${BUNDLE_LOAD})
else()
    target_link_libraries(tcljson ${TCLLIB})
endif()

# Set output to just "tcljson" without "lib" prefix
set_target_properties(tcljson PROPERTIES PREFIX "")

install(TARGETS tcljson LIBRARY DESTINATION lib/tcljson COMPONENT tcljson)
