cmake_minimum_required(VERSION 3.15)

project(postgres)

set(CMAKE_BUILD_TYPE Release)

add_definitions(-fPIC -DUSE_TCL_STUBS)

set(TCLLIB tclstub)
find_library(LIBJANSSON NAMES "libjansson.a")

if(WIN32)
    include_directories(c:/usr/local/include)
    link_directories(c:/usr/local/lib)
elseif(APPLE)
    add_definitions(-DMACOS -Dunix -DLINUX)
    include_directories(/usr/local/include /opt/homebrew/include/ /opt/homebrew/include/tcl-tk /opt/homebrew/include/postgresql@17)
    link_directories(/usr/local/lib /opt/homebrew/lib /opt/homebrew/lib/postgresql@17)
else()
    add_definitions(-DLINUX)
    include_directories(${APP_DIR} /usr/local/include /usr/include/postgresql)
    link_directories(/usr/local/lib /usr/lib/postgresql)
endif()

include_directories(${TCL_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})

###############################
# postgres
###############################
add_library(postgres SHARED src/postgres.c)

if(WIN32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TCLLIB tclstub.lib)
    set(CMAKE_EXE_MODULE_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt /NODEFAULTLIB:MSCVRT /def:${DEF_FILE}")
    set(APP_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../build_win64/tacothread.lib")
endif()
elseif(APPLE)
    set(LIBPQ pq)
    set(TCLLIB tclstub)
    set(BUNDLE_LOAD "-dynamiclib")
else()
    set(LIBPQ pq)
    set(TCLLIB tclstub)
endif()

target_link_libraries(postgres ${TCLLIB} ${LIBJANSSON} ${LIBPQ})

install(TARGETS postgres LIBRARY DESTINATION lib/postgres/${TCL_OS}/${TCL_MACHINE} COMPONENT dlsh-zip)
install(DIRECTORY ${PROJECT_SOURCE_DIR} DESTINATION lib COMPONENT dlsh-zip FILES_MATCHING PATTERN *.tcl PATTERN "**/src" EXCLUDE)
