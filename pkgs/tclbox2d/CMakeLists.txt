cmake_minimum_required(VERSION 3.15)

project(tclbox2d)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DUSE_TCL_STUBS)

if(WIN32)
	include_directories(c:/usr/local/include)
	link_directories(c:/usr/local/lib/$ENV{VSCMD_ARG_TGT_ARCH})
elseif(APPLE)
	add_definitions(-fPIC -DMACOS -Dunix -DLINUX)
	include_directories(/usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk)
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
# tclbox2d
###############################
add_library(tclbox2d SHARED tclbox2d.c)

if(WIN32)
	set(DEF_FILE ../tclbox2d.def)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /LTCG /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /OUT:libtclbox2d.dll /DEF:${DEF_FILE} /IMPLIB:libtclbox2d.lib")
	set(DLSH libdlsh.lib)
	set(TCLLIB tclstub.lib)
elseif(APPLE)
	set(TCLLIB tclstub)
	set(BUNDLE_LOAD "-dynamiclib")
else()
	set(TCLLIB tclstub)
	set(ZLIB z)
endif()

target_link_libraries(tclbox2d ${TCLLIB} box2d)

install(TARGETS tclbox2d LIBRARY DESTINATION lib/box2d/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/ DESTINATION lib/box2d COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl)
