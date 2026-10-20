# stage_test_zip.cmake -- stage a test dlsh.zip from THIS build, for ctest.
#
# The dlsh interpreter does not link libdlsh: it mounts a dlsh.zip and does
# `package require dlsh` ($DLSH_ZIP, else $DLSH_LIBRARY/dlsh.zip, else the
# installed /usr/local/dlsh/dlsh.zip). Without a staged zip, a local ctest
# therefore tests whatever dlsh happens to be INSTALLED, not this build. This
# writes the zip CI writes (build-dlsh-interp.yml): the checked-in vfs/lib
# tree plus the freshly built libdlsh, and also any package library this
# build produced, each in the lib/<pkg>/<os>/<arch>/ slot its pkgIndex.tcl
# loads from.
#
#   cmake -DOUT=<zip> -DSTAGE=<dir> -DVFS=<vfs/lib> -DPLAT=<os>/<arch>
#         "-DLIBS=dlsh=<libdlsh>|impro=<libimpro>|..." -P stage_test_zip.cmake

foreach(v OUT STAGE VFS PLAT LIBS)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "stage_test_zip: -D${v}=... is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${STAGE}/lib")
file(COPY "${VFS}/" DESTINATION "${STAGE}/lib")

# libdlsh is required; a package library that was not built (CI builds only
# the dlsh and dlsh_interp targets) is left out, and tests that need it skip
string(REPLACE "|" ";" libs "${LIBS}")
set(staged "")
foreach(pair IN LISTS libs)
  string(FIND "${pair}" "=" eq)
  string(SUBSTRING "${pair}" 0 ${eq} pkg)
  math(EXPR from "${eq} + 1")
  string(SUBSTRING "${pair}" ${from} -1 path)
  if(NOT EXISTS "${path}")
    if(pkg STREQUAL "dlsh")
      message(FATAL_ERROR "stage_test_zip: libdlsh not built: ${path}")
    endif()
    continue()
  endif()
  file(MAKE_DIRECTORY "${STAGE}/lib/${pkg}/${PLAT}")
  file(COPY "${path}" DESTINATION "${STAGE}/lib/${pkg}/${PLAT}")
  list(APPEND staged ${pkg})
endforeach()

file(REMOVE "${OUT}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUT}" --format=zip lib
                WORKING_DIRECTORY "${STAGE}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "stage_test_zip: writing ${OUT} failed (${rc})")
endif()
message(STATUS "stage_test_zip: ${OUT} (${staged})")
