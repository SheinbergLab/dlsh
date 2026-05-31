cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BUILD_TYPE Release)

if(APPLE)
	add_definitions(-Dunix)
elseif(WIN32)

else()
	add_definitions(-Dunix -DLINUX -fPIC)
endif()

project( dg VERSION 0.95 DESCRIPTION "dg I/O test code")

if(WIN32)
  set(LIBZ zlibstatic.lib)
  set(LIBLZ4 liblz4_static.lib)
  set(LIBJANSSON jansson.lib)
  link_directories( c:/usr/local/lib/$ENV{VSCMD_ARG_TGT_ARCH} )
else()
  find_library(LIBZ z)
  find_library(LIBLZ4 NAMES "liblz4.a")
  find_library(LIBXXHASH NAMES "libxxhash.a")
  find_library(LIBJANSSON NAMES "libjansson.a")
  link_directories( ..  )
endif()

# Use the in-tree headers (../../src/lablib) first so tests build against the
# current sources, not a possibly-stale installed copy under /usr/local/include.
include_directories( ../../src/lablib ../src )

add_executable( testdgread src/testdgread.c )
target_link_libraries( testdgread dg ${LIBZ} ${LIBLZ4} ${LIBXXHASH} )

add_executable( dgtojson src/dgtojson.c )
target_link_libraries( dgtojson dg ${LIBZ} ${LIBLZ4} ${LIBJANSSON} ${LIBXXHASH} )

# Round-trip regression test for the in-memory gzip reader
# (dguGzipFileToStruct): write a dg gzip-compressed, read it back in memory,
# assert identical (incl. a ragged nested list).  Built always; runs under
# `ctest` only, so it does not affect the normal `cmake --build` / release flow.
add_executable( dgz_roundtrip src/dgz_roundtrip.c )
target_link_libraries( dgz_roundtrip dg ${LIBZ} ${LIBLZ4} ${LIBXXHASH} )

add_test( NAME dgz_roundtrip COMMAND dgz_roundtrip )

