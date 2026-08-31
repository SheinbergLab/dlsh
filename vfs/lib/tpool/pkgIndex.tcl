# Tcl package index file
#
# tpool - loads the C extension and sources the thread_pool Tcl wrapper
#
# "package require tpool"       -- just the C tpool_map command
# "package require thread_pool" -- the Tcl wrapper (auto-requires tpool)

package ifneeded tpool 1.0 [list apply {{lib args} {
    if {![file exists $lib]} {
        return -code error "tpool: compiled library not present in this\
 dlsh.zip -- no $::tcl_platform(os)/$::tcl_platform(machine) build of\
 [file tail $lib] was packaged"
    }
    uplevel #0 [list load $lib {*}$args]
}} [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtpool[info sharedlibextension]]]

package ifneeded thread_pool 1.0 "package require tpool; [list source [file join $dir thread_pool.tcl]]"
