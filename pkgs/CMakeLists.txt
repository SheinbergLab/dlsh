cmake_minimum_required(VERSION 3.15)

#
# Add our own tcl-only packages to dlsh.zip.
#

install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/corrgram TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/graphing TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/pkgs/hex TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/mtspec TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/phase TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/rasts TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/pkgs/qpcs TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/robust TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/roc TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/selectivity TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/spec TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/spikes TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/stimcompose TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/xCorr TYPE LIB COMPONENT dlsh-zip)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/local/spritesheet TYPE LIB COMPONENT dlsh-zip)

#
# Add our non-public dependencies to dlsh.zip
#

if(DEFINED DLSH_DEPENDENCIES)
    install(DIRECTORY ${DLSH_DEPENDENCIES}/mclistbox-1.03 TYPE LIB COMPONENT dlsh-zip)
endif()

#
# Add external tcl-only packages to dlsh.zip.
#

# tcllib and tklib use a standard configure/make pattern.
install(
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/tcllib COMMAND ./configure --prefix=${CMAKE_INSTALL_PREFIX})"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/tcllib COMMAND make install-libraries)"
    COMPONENT dlsh-zip
)
install(
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/tklib COMMAND ./configure --prefix=${CMAKE_INSTALL_PREFIX})"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/tklib COMMAND make install-libraries)"
    COMPONENT dlsh-zip
)

# TkCon is pure tcl, but the official pkgIndex.tcl comes from a template as pkgIndex.tcl.in.
# We can run configure/make to produce the pkgIndex.tcl we need.
# Annoyingly, configure fails without tk-dev installed.
# And we still have to manually copy the files we need!
install(
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/TkCon COMMAND ./configure && make)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/TkCon COMMAND cp ${CMAKE_SOURCE_DIR}/deps/TkCon/pkgIndex.tcl .)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/TkCon COMMAND cp ${CMAKE_SOURCE_DIR}/deps/TkCon/tkcon.tcl .)"
    COMPONENT dlsh-zip
)

# bwidget is just tcl files that we copy.
install(
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/bwidget COMMAND cp ${CMAKE_SOURCE_DIR}/deps/bwidget/*.tcl .)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/bwidget COMMAND cp ${CMAKE_SOURCE_DIR}/deps/bwidget/*.txt .)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/bwidget COMMAND cp -r ${CMAKE_SOURCE_DIR}/deps/bwidget/demo .)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/bwidget COMMAND cp -r ${CMAKE_SOURCE_DIR}/deps/bwidget/images .)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/bwidget COMMAND cp -r ${CMAKE_SOURCE_DIR}/deps/bwidget/lang .)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/bwidget COMMAND cp -r ${CMAKE_SOURCE_DIR}/deps/bwidget/tests .)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lib/bwidget COMMAND cp -r ${CMAKE_SOURCE_DIR}/deps/bwidget/BWman .)"
    COMPONENT dlsh-zip
)

#
# Add external tcl-plus-c packages to dlsh.zip.
#

install(
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/sqlite COMMAND ./configure --enable-all)"
    CODE "set(ENV{TCLLIBPATH} ${CMAKE_INSTALL_PREFIX}/lib)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/sqlite COMMAND make tclextension-install)"
    COMPONENT dlsh-zip
)
if(APPLE)
    # Use the homebrew installation of tcl 9.
    set(WITH_TCL "--with-tcl=/opt/homebrew/lib")
endif()
install(
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/yajl-tcl COMMAND autoconf)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/yajl-tcl COMMAND ./configure --exec_prefix=${CMAKE_INSTALL_PREFIX} ${WITH_TCL})"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/yajl-tcl COMMAND make)"
    CODE "execute_process(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/deps/yajl-tcl COMMAND make install)"
    COMPONENT dlsh-zip
)

#
# Add our own tcl-plus-c packages to dlsh.zip
#

# Our packages use pkgIndex.tcl that expect shared libs in platform- and machine-specific subdirs.
# Define these once here to use in package builds included below.
if(WIN32)
    set(TCL_OS "Windows NT")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM")
        set(TCL_MACHINE "arm64")
    else()
        set(TCL_MACHINE "amd")
    endif()
elseif(APPLE)
    set(TCL_OS "Darwin")
    set(TCL_MACHINE "arm64")
else()
    set(TCL_OS "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        set(TCL_MACHINE "aarch64")
    else()
        set(TCL_MACHINE "x86_64")
    endif()
endif()

if(APPLE)
    if(DEFINED "CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM")
        # Configure for apple code signing, notarization, and gatekeeper/entitlements.
        set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--strict --timestamp --options=runtime")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Developer ID Application")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")
        set(CMAKE_SKIP_RPATH TRUE)

        # Sign external tcl-plus-c package binaries.
        set(CODESIGN_COMMAND /usr/bin/codesign --force --verify ${CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS} --sign)
        install(
            CODE "file(GLOB SQLITE_DYLIB ${CMAKE_INSTALL_PREFIX}/lib/sqlite*/*.dylib)"
            CODE "execute_process(COMMAND ${CODESIGN_COMMAND} \"${CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}\" \$\{SQLITE_DYLIB\})"
            CODE "file(GLOB YAJLTCL_DYLIB ${CMAKE_INSTALL_PREFIX}/lib/yajltcl*/*.dylib)"
            CODE "execute_process(COMMAND ${CODESIGN_COMMAND} \"${CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY}\" \$\{YAJLTCL_DYLIB\})"
            COMPONENT dlsh-zip
        )
    endif()
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/trajectory_analysis)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/curve)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/tclbox2d)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/dbscan)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/dslog)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/dtw)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/impro)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/mdns)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/points)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/postgres)
add_subdirectory(${CMAKE_SOURCE_DIR}/pkgs/spritesheet)

#
# Add dlsh itself to dlsh.zip
#
install(TARGETS dlsh LIBRARY DESTINATION lib/dlsh/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip PUBLIC_HEADER DESTINATION include)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/ DESTINATION lib/dlsh COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl PATTERN "**/local" EXCLUDE)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/bin DESTINATION lib/dlsh COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/examples DESTINATION lib/dlsh COMPONENT dlsh-zip)
install(FILES ${CMAKE_SOURCE_DIR}/lib/dlshrc DESTINATION lib/dlsh COMPONENT dlsh-zip)
install(FILES ${CMAKE_SOURCE_DIR}/lib/tclIndex DESTINATION lib/dlsh COMPONENT dlsh-zip)

include(CPack)
cpack_add_component(dlsh-zip)
