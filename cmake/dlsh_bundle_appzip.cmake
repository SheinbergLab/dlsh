# dlsh_bundle_appzip.cmake -- POST_BUILD helper for the self-contained dlsh.
#
# Packages the Tcl 9 + Tk script libraries (from known-good sources) so the
# interpreter finds them at startup, in one of two modes:
#
#   MODE=append  (single-file, portable): build a zip with tcl_library/ [+
#       tk_library/] at the root and concatenate it onto the executable.
#       TclZipfs_AppHook() mounts it at //zipfs:/app. NOTE: trailing data past
#       __LINKEDIT breaks macOS code signing -- fine for Linux / dev / CI
#       artifacts, not for notarized distribution.
#
#   MODE=sidecar (signable): build dlsh.zip with lib/tcl9.0/ [+ lib/tk9.0/]
#       next to the executable. The binary has no trailing data, so it signs
#       cleanly; Dlsh_BootstrapRuntime() mounts the sidecar and points
#       TCL_LIBRARY/TK_LIBRARY into it.
#
# We build the archive from sources rather than Tk's own make-generated zip,
# which (when Tk is configured against a from-source Tcl) wrongly packages
# tcl_library content instead of Tk's.
#
# Args (-D):  MODE, EXE, TCL_ZIP, [TK_LIBDIR], STAGE, [RESIGN], [CODESIGN_ID]

if(NOT MODE OR NOT EXE OR NOT TCL_ZIP OR NOT STAGE)
  message(FATAL_ERROR "dlsh_bundle_appzip: MODE, EXE, TCL_ZIP and STAGE required")
endif()

# Use the real `zip` tool, not `cmake -E tar --format=zip`: libarchive's zip
# (esp. on Linux) can emit data-descriptor/zip64 records that Tcl's zipfs reader
# rejects at read time ("couldn't read file ... : Success"). `zip` is already a
# build dependency (Tk's own zipfs step needs it) and produces canonical,
# zipfs-readable archives on both macOS and Linux. -X drops extra attributes.
find_program(ZIP_TOOL zip)
if(NOT ZIP_TOOL)
  message(FATAL_ERROR "dlsh_bundle_appzip: `zip` not found on PATH (required)")
endif()
function(_make_zip outfile)   # remaining ARGN = members, relative to STAGE
  file(REMOVE "${outfile}")
  execute_process(COMMAND "${ZIP_TOOL}" -qr -X "${outfile}" ${ARGN}
                  WORKING_DIRECTORY "${STAGE}" RESULT_VARIABLE _zrc)
  if(_zrc)
    message(FATAL_ERROR "dlsh_bundle_appzip: zip failed creating ${outfile}")
  endif()
endfunction()

file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${STAGE}")

# Extract Tcl's library (top dir tcl_library/) into the staging area.
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${TCL_ZIP}"
                WORKING_DIRECTORY "${STAGE}" RESULT_VARIABLE _rc)
if(_rc)
  message(FATAL_ERROR "dlsh_bundle_appzip: failed to extract ${TCL_ZIP}")
endif()

if(MODE STREQUAL "append")
  # Root layout: tcl_library/ [+ tk_library/]; archive is cat'd onto the exe.
  set(_members tcl_library)
  if(TK_LIBDIR)
    file(COPY "${TK_LIBDIR}/" DESTINATION "${STAGE}/tk_library")
    list(APPEND _members tk_library)
  endif()
  set(_zip "${STAGE}/dlsh-app.zip")
  _make_zip("${_zip}" ${_members})
  # zipfs locates the archive by scanning from EOF, so a plain concatenation
  # onto the Mach-O/ELF is exactly right. (cmake has no binary append.)
  execute_process(COMMAND sh -c "cat '${_zip}' >> '${EXE}'" RESULT_VARIABLE _rc)
  if(_rc)
    message(FATAL_ERROR "dlsh_bundle_appzip: failed to append zip to ${EXE}")
  endif()
  message(STATUS "dlsh: appended //zipfs:/app archive (${_members}) to ${EXE}")

elseif(MODE STREQUAL "sidecar")
  # dlsh.zip layout: lib/tcl9.0/ [+ lib/tk9.0/], placed next to the executable.
  file(MAKE_DIRECTORY "${STAGE}/lib")
  file(RENAME "${STAGE}/tcl_library" "${STAGE}/lib/tcl9.0")
  if(TK_LIBDIR)
    file(COPY "${TK_LIBDIR}/" DESTINATION "${STAGE}/lib/tk9.0")
  endif()
  get_filename_component(_exedir "${EXE}" DIRECTORY)
  set(_zip "${_exedir}/dlsh.zip")
  _make_zip("${_zip}" lib)
  message(STATUS "dlsh: wrote sidecar runtime ${_zip} (lib/tcl9.0 [+ lib/tk9.0])")

else()
  message(FATAL_ERROR "dlsh_bundle_appzip: unknown MODE '${MODE}'")
endif()

# Code-sign. Appended binaries cannot be signed (trailing data past __LINKEDIT
# -> "failed strict validation"); the sidecar binary signs cleanly. So make
# signing best-effort in append mode, required in sidecar mode.
if(RESIGN)
  if(NOT CODESIGN_ID)
    set(CODESIGN_ID "-")     # ad-hoc
  endif()
  execute_process(COMMAND codesign --force -s "${CODESIGN_ID}" "${EXE}"
                  RESULT_VARIABLE _rc ERROR_VARIABLE _err)
  if(_rc)
    if(MODE STREQUAL "append")
      message(WARNING "dlsh: codesign skipped for appended binary (${_err}); "
                      "runs locally but is unsigned -- use sidecar mode to sign")
    else()
      message(FATAL_ERROR "dlsh: codesign failed (${_err})")
    endif()
  else()
    message(STATUS "dlsh: code-signed ${EXE} (${CODESIGN_ID})")
  endif()
endif()
