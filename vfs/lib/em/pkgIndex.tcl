# Tcl package index file for em (eye movement analysis) package
#
# The em package is split across a compiled C extension (detection
# primitives, registered under em::c::*) and a Tcl facade (em.tcl).
# This index loads the C extension first, then sources the Tcl half.
#
package ifneeded em 1.0 [list apply {dir {
    load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) \
              libem[info sharedlibextension]] Em
    source [file join $dir em.tcl]
}} $dir]
