cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
project(spritesheet)
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
# spritesheet
###############################
set(SOURCES
    src/spritesheet_extract.cpp
    src/tcl_spritesheet.cpp
    external/tinyxml2.cpp
)

add_library(spritesheet SHARED ${SOURCES})

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
    target_link_libraries(spritesheet ${TCLLIB} ${BUNDLE_LOAD})
else()
    target_link_libraries(spritesheet ${TCLLIB})
endif()

# Set output to just "spritesheet" without "lib" prefix
set_target_properties(spritesheet PROPERTIES PREFIX "")

install(TARGETS spritesheet LIBRARY DESTINATION lib/spritesheet COMPONENT spritesheet)