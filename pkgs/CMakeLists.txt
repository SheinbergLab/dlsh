cmake_minimum_required(VERSION 3.15)

###############################
# Platform/Architecture Detection for VFS paths
###############################

# Detect OS name as Tcl sees it
if(WIN32)
    set(TCL_OS "Windows NT")
elseif(APPLE)
    set(TCL_OS "Darwin")
else()
    set(TCL_OS "Linux")
endif()

# Detect machine architecture as Tcl sees it
if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM")
        set(TCL_MACHINE "arm64")
    else()
        set(TCL_MACHINE "amd64")
    endif()
elseif(APPLE)
    set(TCL_MACHINE "arm64")
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        set(TCL_MACHINE "aarch64")
    else()
        set(TCL_MACHINE "x86_64")
    endif()
endif()

message(STATUS "VFS target: ${TCL_OS}/${TCL_MACHINE}")

###############################
# Helper function for installing packages to VFS
###############################

# install_tcl_package(NAME <n> 
#                     TARGET <cmake_target>
#                     [VERSION <version>]
#                     [TCL_FILES <file1> <file2> ...]
#                     [DIRECTORIES <dir1> <dir2> ...])
#
function(install_tcl_package)
    cmake_parse_arguments(PKG "" "NAME;TARGET;VERSION" "TCL_FILES;DIRECTORIES" ${ARGN})
    
    if(NOT PKG_VERSION)
        set(PKG_VERSION "1.0")
    endif()
    
    set(PKG_DIR "lib/${PKG_NAME}")
    set(PKG_LIB_DIR "${PKG_DIR}/${TCL_OS}/${TCL_MACHINE}")
    
    # Install the shared library file directly (using generator expression)
    # This ensures the vfs component is used regardless of target's default component
    install(FILES $<TARGET_FILE:${PKG_TARGET}>
        DESTINATION ${PKG_LIB_DIR}
        COMPONENT vfs
    )
    
    # Install Tcl files to package root
    if(PKG_TCL_FILES)
        install(FILES ${PKG_TCL_FILES}
            DESTINATION ${PKG_DIR}
            COMPONENT vfs
        )
    endif()
    
    # Install directories
    if(PKG_DIRECTORIES)
        foreach(dir ${PKG_DIRECTORIES})
            install(DIRECTORY ${dir}
                DESTINATION ${PKG_DIR}
                COMPONENT vfs
            )
        endforeach()
    endif()
endfunction()

###############################
# macOS Code Signing Setup
###############################

if(APPLE AND DEFINED CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM)
    set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--strict --timestamp --options=runtime")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")
    set(CMAKE_SKIP_RPATH TRUE)
    
    set(CODESIGN_COMMAND /usr/bin/codesign --force --verify 
        ${CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS} 
        --sign "${CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}")
endif()

###############################
# Copy vfs/lib contents FIRST (base layer)
###############################
#
# The vfs/lib directory contains:
#   - Third-party packages with binaries (TkTable, tdom, thread, sqlite, yajltcl, etc.)
#   - Pure Tcl packages (tcllib2.0, tklib0.8, bwidget, tkcon, etc.)
#   - Package directory structure with .gitkeep placeholders
#
# This is installed FIRST so that built libraries can be installed on top.
#
if(EXISTS ${CMAKE_SOURCE_DIR}/vfs/lib)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/vfs/lib/
        DESTINATION lib
        COMPONENT vfs
        PATTERN ".gitkeep" EXCLUDE
        PATTERN ".git" EXCLUDE
    )
endif()

###############################
# Pure Tcl Packages (from lib/local/)
###############################

set(TCL_ONLY_PACKAGES
    corrgram
    graphing
    mtspec
    phase
    rasts
    robust
    roc
    selectivity
    spec
    spikes
    stimcompose
    xCorr
)

foreach(pkg ${TCL_ONLY_PACKAGES})
    if(EXISTS ${CMAKE_SOURCE_DIR}/lib/local/${pkg})
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/${pkg}
            DESTINATION lib
            COMPONENT vfs
        )
    endif()
endforeach()

# Packages from pkgs/ that are pure Tcl
install(DIRECTORY ${CMAKE_SOURCE_DIR}/pkgs/hex DESTINATION lib COMPONENT vfs)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/pkgs/qpcs DESTINATION lib COMPONENT vfs)

###############################
# Our C/C++ packages (from pkgs/)
###############################

# Add subdirectories - each should define its target
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/trajectory_analysis)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/curve)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/tclbox2d)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/dbscan)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/dslog)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/dtw)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/impro)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/points)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/postgres)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/spritesheet)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/tcljson)

# Install each package - the binary goes on top of vfs/lib structure
install_tcl_package(NAME trajectory_analysis TARGET trajectory_analysis)
install_tcl_package(NAME curve TARGET curve)
install_tcl_package(NAME box2d TARGET tclbox2d)
install_tcl_package(NAME dbscan TARGET dbscan)
install_tcl_package(NAME dslog TARGET dslog)
install_tcl_package(NAME dtw TARGET dtw)
install_tcl_package(NAME impro TARGET impro)
install_tcl_package(NAME points TARGET points)
install_tcl_package(NAME postgres TARGET postgres)
install_tcl_package(NAME spritesheet TARGET spritesheet)
install_tcl_package(NAME tcljson TARGET tcljson)

###############################
# dlsh itself
###############################

install(TARGETS dlsh 
    LIBRARY DESTINATION lib/dlsh/${TCL_OS}/${TCL_MACHINE}
    COMPONENT vfs
)

# Tcl files for dlsh
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/ 
    DESTINATION lib/dlsh 
    COMPONENT vfs
    FILES_MATCHING 
    PATTERN "*.tcl"
    PATTERN "local" EXCLUDE
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/bin 
    DESTINATION lib/dlsh 
    COMPONENT vfs
    FILES_MATCHING PATTERN "*.tcl"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/examples 
    DESTINATION lib/dlsh 
    COMPONENT vfs
)

install(FILES 
    ${CMAKE_SOURCE_DIR}/lib/dlshrc 
    ${CMAKE_SOURCE_DIR}/lib/tclIndex
    DESTINATION lib/dlsh 
    COMPONENT vfs
)

###############################
# CPack configuration
###############################

include(CPack)
cpack_add_component(vfs DISPLAY_NAME "VFS Library Tree")
