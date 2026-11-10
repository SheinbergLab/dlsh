# embed_init_script.cmake -- write a Tcl file as the BODY of a C char-array
# initializer, null-terminated, for TclTLS's
#     static const char tlsTclInitScript[] = {
#     #include "tls.tcl.h"
#     };
# Bytes rather than a string literal, so nothing in the script needs escaping
# (same approach as cmake/EmbedTcl.cmake, minus the declaration).
#
#   cmake -DINPUT=tls.tcl -DOUTPUT=tls.tcl.h -P embed_init_script.cmake

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "embed_init_script.cmake requires -DINPUT and -DOUTPUT")
endif()

file(READ "${INPUT}" _hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
file(WRITE "${OUTPUT}"
"/* Generated from ${INPUT} by pkgs/tls/embed_init_script.cmake -- do not edit. */
${_bytes} 0x00
")
