cmake_minimum_required(VERSION 3.15)

project(trajectory_analysis)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-DUSE_TCL_STUBS)

if(WIN32)
	include_directories(c:/usr/local/include ../../src ../../src/lablib)
	link_directories(c:/usr/local/lib)
else(APPLE)
	add_definitions(-fPIC)
	include_directories(../../src ../../src/lablib)
	link_directories(../../build /usr/local/lib)
endif()

include_directories(${TCL_INCLUDE_DIR} .)

###############################
# trajectory_analysis
###############################
add_library(trajectory_analysis SHARED trajectory_analysis.c)

if(WIN32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(DLSH libdlsh.lib)
	set(TCLLIB tclstub.lib)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL /LTCG /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /OUT:libimpro.dll")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
endif()
elseif(APPLE)
	set(TCLLIB tclstub)
	set(BUNDLE_LOAD "-dynamiclib -undefined dynamic_lookup")	
	set(DLSH dlsh)
else()
        set(DLSH dlsh)
	set(TCLLIB tclstub)
	set(ZLIB z)
endif()

if(APPLE)
 target_link_libraries(trajectory_analysis ${TCLLIB} ${BUNDLE_LOAD} ${DLSH})
else()
 target_link_libraries(trajectory_analysis ${TCLLIB} ${DLSH})
endif()


