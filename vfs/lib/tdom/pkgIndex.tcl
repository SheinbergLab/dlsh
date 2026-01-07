#
# Tcl package index file
#

if { $::tcl_platform(os) == "Windows NT" } {
    package ifneeded tdom 0.9.5 \
	[list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) tcl9tdom095[info sharedlibextension]]]
} else {
    package ifneeded tdom 0.9.5 \
	[list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtcl9tdom0.9.5[info sharedlibextension]] Tdom]
}

