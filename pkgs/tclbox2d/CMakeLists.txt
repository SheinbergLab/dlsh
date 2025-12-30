cmake_minimum_required(VERSION 3.15)
project(tclbox2d)

set(CMAKE_BUILD_TYPE Release)
add_definitions(-DUSE_TCL_STUBS)

# Platform definitions
if(WIN32)
    # Windows-specific
elseif(APPLE)
    add_definitions(-fPIC -DMACOS -Dunix -DLINUX)
else()
    add_definitions(-DLINUX -fPIC)
endif()

###############################
# tclbox2d
###############################
add_library(tclbox2d SHARED tclbox2d.c)

# Use in-tree dependencies from parent build
# tclstub and box2d targets should propagate their include directories
target_link_libraries(tclbox2d PRIVATE tclstub box2d)

# TCL includes from in-tree build (tclstub has PUBLIC includes, but add explicitly just in case)
if(DEFINED tcl_SOURCE_DIR)
    target_include_directories(tclbox2d PRIVATE 
        ${tcl_SOURCE_DIR}/generic
        ${tcl_SOURCE_DIR}/libtommath
    )
    if(WIN32)
        target_include_directories(tclbox2d PRIVATE ${tcl_SOURCE_DIR}/win)
    else()
        target_include_directories(tclbox2d PRIVATE ${tcl_SOURCE_DIR}/unix)
    endif()
endif()

# box2d v3.x includes
# First try cached SOURCE_DIR from parent, then try target property
if(box2d_SOURCE_DIR)
    target_include_directories(tclbox2d PRIVATE ${box2d_SOURCE_DIR}/include)
elseif(TARGET box2d)
    get_target_property(BOX2D_INCLUDES box2d INTERFACE_INCLUDE_DIRECTORIES)
    if(BOX2D_INCLUDES)
        target_include_directories(tclbox2d PRIVATE ${BOX2D_INCLUDES})
    endif()
else()
    message(FATAL_ERROR "box2d not found - build from parent dlsh project")
endif()

# Windows-specific linker settings
if(WIN32)
    set(DEF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/tclbox2d.def)
    set_target_properties(tclbox2d PROPERTIES
        OUTPUT_NAME "libtclbox2d"
        LINK_FLAGS "/INCREMENTAL /LTCG /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /DEF:${DEF_FILE}"
    )
elseif(APPLE)
    set_target_properties(tclbox2d PROPERTIES
        LINK_FLAGS "-dynamiclib"
    )
endif()

# Install
install(TARGETS tclbox2d 
    LIBRARY DESTINATION lib/box2d/${TCL_OS}/${TCL_MACHINE} 
    COMPONENT dlsh-zip
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/ 
    DESTINATION lib/box2d 
    COMPONENT dlsh-zip 
    FILES_MATCHING PATTERN *.tcl
)
