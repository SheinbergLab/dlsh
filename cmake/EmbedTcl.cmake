# EmbedTcl.cmake -- turn a .tcl source file into a C header holding it as a
# null-terminated byte array, so it can be Tcl_Eval'd at load time.
#
# Invoke via:  cmake -DINPUT=foo.tcl -DOUTPUT=foo.h -DVAR=name -P EmbedTcl.cmake
#
# Byte-array (not C string) encoding is used so the Tcl source needs no
# escaping of quotes, backslashes, braces, or newlines, and works in C (which
# has no raw string literals).

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED VAR)
    message(FATAL_ERROR "EmbedTcl.cmake requires -DINPUT, -DOUTPUT, -DVAR")
endif()

file(READ "${INPUT}" _hex HEX)
# one "0xNN," per byte
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")

file(WRITE "${OUTPUT}"
"/* Auto-generated from ${INPUT} by cmake/EmbedTcl.cmake -- do not edit. */
static const char ${VAR}[] = { ${_bytes} 0x00 };
")
