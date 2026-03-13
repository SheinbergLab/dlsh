#
# Tcl package index file for Tktable
# NOTE: Version must match TKTABLE_VERSION in .github/workflows/release.yml
#

package ifneeded Tktable 2.12.1 [list apply {{dir} {
    load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) \
	libtcl9Tktable2.12.1[info sharedlibextension]] Tktable

    set initScript [file join $dir tkTable.tcl]
    if {[file exists $initScript]} {
	source -encoding utf-8 $initScript
    }
}} $dir]
