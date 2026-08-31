#
# Tcl package index file
#

if { $::tcl_platform(os) == "Windows NT" } {
    package ifneeded tdom 0.9.6 \
	[list apply {{lib args} {
    if {![file exists $lib]} {
        return -code error "tdom: compiled library not present in this\
 dlsh.zip -- no $::tcl_platform(os)/$::tcl_platform(machine) build of\
 [file tail $lib] was packaged"
    }
    uplevel #0 [list load $lib {*}$args]
}} [file join $dir $::tcl_platform(os) $::tcl_platform(machine) tcl9tdom096[info sharedlibextension]]]
} else {
    package ifneeded tdom 0.9.6 \
	[list apply {{lib args} {
    if {![file exists $lib]} {
        return -code error "tdom: compiled library not present in this\
 dlsh.zip -- no $::tcl_platform(os)/$::tcl_platform(machine) build of\
 [file tail $lib] was packaged"
    }
    uplevel #0 [list load $lib {*}$args]
}} [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtcl9tdom0.9.6[info sharedlibextension]] Tdom]
}

