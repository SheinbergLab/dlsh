cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
project(collision)
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
# collision
###############################
set(SOURCES
    src/collision_extract.cpp
    src/tcl_collision.cpp
    external/tinyxml2.cpp
)

add_library(collision SHARED ${SOURCES})

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
    target_link_libraries(collision ${TCLLIB} ${BUNDLE_LOAD})
else()
    target_link_libraries(collision ${TCLLIB})
endif()

# Set output to just "collision" without "lib" prefix
set_target_properties(collision PROPERTIES PREFIX "")

install(TARGETS collision LIBRARY DESTINATION lib/collision COMPONENT collision)