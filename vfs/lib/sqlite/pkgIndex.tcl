# -*- tcl -*-
# Tcl package index file, version 1.1
#
if { $::tcl_platform(os) == "Windows NT" } {
    package ifneeded sqlite3 3.49.1 \
	[list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) tcl9sqlite3491[info sharedlibextension]]]
} else {
    package ifneeded sqlite3 3.49.1 \
	[list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtcl9sqlite3.49.1[info sharedlibextension]]]
}
