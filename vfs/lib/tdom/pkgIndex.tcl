#
# Tcl package index file
#

if { $::tcl_platform(os) == "Windows NT" } {
    package ifneeded tdom 0.9.6 \
	[list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) tcl9tdom096[info sharedlibextension]]]
} else {
    package ifneeded tdom 0.9.6 \
	[list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtcl9tdom0.9.6[info sharedlibextension]] Tdom]
}

