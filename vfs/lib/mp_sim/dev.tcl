# dev.tcl -- dlshell startup snippet for working on mp_sim in-tree.
#
# Usage from a dlshell session (one-time per session):
#   source /Users/sheinb/src/dlsh/pkgs/mp_sim/dev.tcl
#
# That puts this checkout's pkgs dir at the front of auto_path so
# `package require mp_sim` picks up the current source rather than any
# installed/zipped copy. Re-source any time you've edited mp_sim.tcl
# and want a fresh load -- it forgets the package and re-requires it,
# so namespace state from the prior load is wiped.

set pkgs_dir /Users/sheinb/src/dlsh/pkgs

# Front-of-path so it shadows any installed mp_sim.
if {[lsearch -exact $auto_path $pkgs_dir] >= 0} {
    set auto_path [lsearch -inline -all -not -exact $auto_path $pkgs_dir]
}
set auto_path [linsert $auto_path 0 $pkgs_dir]

# Force a fresh load: forget the package + delete the namespace, so
# `package require` re-evaluates mp_sim.tcl from scratch.
catch {package forget mp_sim}
catch {namespace delete ::mp_sim}

package require mp_sim

# Some dlsh distributions (the zipfs build used by dlshell) don't auto-
# source dlshrc, so $colors / $pi may not be set. Source it idempotently
# if it's missing so the play scripts can use named colors.
if {![info exists ::colors]} {
    set rc_candidates [list \
        [file join $::env(DLSH_LIBRARY) dlshrc] \
        [file join [zipfs root] dlsh lib dlsh dlshrc] \
        /Users/sheinb/src/dlsh/vfs/lib/dlsh/dlshrc]
    foreach rc $rc_candidates {
        if {[file readable $rc]} { source $rc; break }
    }
}

puts "mp_sim [package present mp_sim] loaded from [package ifneeded mp_sim [package present mp_sim]]"
