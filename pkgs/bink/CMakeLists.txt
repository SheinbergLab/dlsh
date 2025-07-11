cmake_minimum_required(VERSION 3.15)

project(tcl_bink)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-fPIC -DUSE_INTERP_RESULT -DUSE_TCL_STUBS)

if(WIN32)
    include_directories(c:/usr/local/include)
    link_directories(c:/usr/local/lib)
elseif(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX)
    include_directories(/usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk)
    link_directories(/usr/local/lib /opt/homebrew/lib)
    set(BUNDLE_LOAD "-dynamiclib")
    set(TCLLIB tclstub)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    include_directories(../local/include)
    link_directories(../local/aarch64/lib)
    set(TCLLIB tclstub)
else()
    add_definitions(-DLINUX)
    include_directories(${APP_DIR} /usr/local/include)
    link_directories(/usr/local/lib)
    set(TCL_INCLUDE_DIR /usr/include/tcl)
    set(TCLLIB tclstub)
endif()

include_directories(${TCL_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

# We have non-public bink dependencies in our own dlsh-dependencies repo.
if(DEFINED DLSH_DEPENDENCIES)
    include_directories(${DLSH_DEPENDENCIES}/bink/include)
    if(WIN32)
        set(BINK_LIB ${DLSH_DEPENDENCIES}/bink/lib/windows/bink2w64.lib)
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
            set(BINK_LIB ${DLSH_DEPENDENCIES}/bink/lib/macos/libBink2MacArm64.dylib)
        else()
            set(BINK_LIB ${DLSH_DEPENDENCIES}/bink/lib/macos/libBink2Macx64.dylib)
        endif()
    else()
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
            set(BINK_LIB ${DLSH_DEPENDENCIES}/bink/lib/linux/bink2lnxarm64.a)
        else()
            set(BINK_LIB ${DLSH_DEPENDENCIES}/bink/lib/linux/bink2lnx64.a)
        endif()
    endif()
endif()

add_library(tcl_bink SHARED tcl_bink.c lodepng.c)

target_link_libraries(tcl_bink ${TCLLIB} ${BINK_LIB})

if (APPLE)
    add_custom_command(
        TARGET tcl_bink POST_BUILD
        COMMAND ${CMAKE_INSTALL_NAME_TOOL} -change "@executable_path/libBink2MacArm64.dylib" "@rpath/libBink2MacArm64.dylib" $<TARGET_FILE:tcl_bink>
    )
endif()

install(TARGETS tcl_bink LIBRARY DESTINATION lib/bink/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR} DESTINATION lib COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
