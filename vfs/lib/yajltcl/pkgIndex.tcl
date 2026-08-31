#
# Tcl package index file for yajltcl 1.8.1
#
package ifneeded yajltcl 1.8.1 \
    [list apply {{lib args} {
    if {![file exists $lib]} {
        return -code error "yajltcl: compiled library not present in this\
 dlsh.zip -- no $::tcl_platform(os)/$::tcl_platform(machine) build of\
 [file tail $lib] was packaged"
    }
    uplevel #0 [list load $lib {*}$args]
}} [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtcl9yajltcl1.8.1[info sharedlibextension]]]\n[list source [file join $dir yajl.tcl]]
