cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BUILD_TYPE Release)

if(APPLE)
  add_definitions(-fPIC -Dunix -DMACOS)
elseif(WIN32)

else()
  set (CMAKE_INSTALL_PREFIX /usr/local)
  add_definitions(-Dunix -DLINUX -fPIC)
endif()

project( dg VERSION 1.00 DESCRIPTION "dg io library")

if(TARGET jansson)
    target_include_directories(dg PRIVATE 
        ${jansson_SOURCE_DIR}/src
        ${jansson_BINARY_DIR}/include
    )
else()
    include_directories(/usr/local/include)
    if(APPLE)
        include_directories(/opt/homebrew/include/)
    endif()
endif()

include_directories(../src ../src/lablib)

if(APPLE)
  include_directories(/opt/homebrew/include/)
endif()

add_library(dg
  ../src/lablib/flipfuncs.c
  ../src/lablib/dfutils.c
  ../src/lablib/df.c
  ../src/lablib/dynio.c
  ../src/lablib/lz4utils.c
  ../src/lablib/randvars.c
  ../src/dgjson.c
  ../src/dfana.c
  ../src/dlarith.c
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory( test )
endif()

set(CPACK_PACKAGE_NAME dg)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Utils for reading dynamic groups.")
set(CPACK_PACKAGE_CONTACT SheinbergLab)

set_target_properties(dg PROPERTIES PUBLIC_HEADER "../src/lablib/dynio.h;../src/lablib/df.h")
if(WIN32)
  # TODO
elseif(APPLE)
  install(TARGETS dg LIBRARY COMPONENT dg)
  install(TARGETS dg ARCHIVE COMPONENT dg)
  install(TARGETS dg PUBLIC_HEADER COMPONENT dg)
else()
  install(TARGETS dg LIBRARY COMPONENT dg)
  install(TARGETS dg ARCHIVE COMPONENT dg)
  install(TARGETS dg PUBLIC_HEADER COMPONENT dg)
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()

include(CPack)
cpack_add_component(dg)
