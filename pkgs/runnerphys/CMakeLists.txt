cmake_minimum_required(VERSION 3.15)
project(runnerphys C)

set(CMAKE_BUILD_TYPE Release)
add_definitions(-DUSE_TCL_STUBS)

###############################
# runnerphys
###############################
add_library(runnerphys SHARED runnerphys.c)

# In-tree tclstub from the parent dlsh build (PUBLIC Tcl includes)
target_link_libraries(runnerphys PRIVATE tclstub)

# EXACTNESS: runnerphys must give bit-identical doubles to runner-1.0.tm's
# Tcl on every platform. No fused multiply-add (gcc fuses by default on
# aarch64, Apple clang within an expression) and no fast-math. Set here on
# the target so no global flag can override it.
if(MSVC)
    target_compile_options(runnerphys PRIVATE /fp:precise)
else()
    target_compile_options(runnerphys PRIVATE -ffp-contract=off -fno-fast-math -fPIC)
endif()

if(WIN32)
    set_target_properties(runnerphys PROPERTIES OUTPUT_NAME "librunnerphys")
elseif(APPLE)
    set_target_properties(runnerphys PROPERTIES LINK_FLAGS "-dynamiclib")
endif()
