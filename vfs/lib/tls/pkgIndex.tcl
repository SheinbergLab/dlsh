#
# Tcl package index file for tls (TclTLS) 2.0.1
#
# Built by pkgs/tls with OpenSSL linked in statically; see docs/tls.md.
# The version here must match TCLTLS_VERSION in pkgs/tls/CMakeLists.txt
# (the build checks). tls.tcl is compiled into the library, not shipped.
#
package ifneeded tls 2.0.1 \
    [list apply {{lib} {
    if {![file exists $lib]} {
        return -code error "tls: compiled library not present in this\
 dlsh.zip -- no $::tcl_platform(os)/$::tcl_platform(machine) build of\
 [file tail $lib] was packaged"
    }
    uplevel #0 [list load $lib Tls]
}} [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtcl9tls2.0.1[info sharedlibextension]]]
