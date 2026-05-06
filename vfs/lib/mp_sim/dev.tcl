# dev.tcl -- dlshell startup snippet for working on mp_sim in-tree.
#
# Usage from a dlshell session (one-time per session):
#   source /Users/sheinb/src/dlsh/vfs/lib/mp_sim/dev.tcl
#
# Puts vfs/lib at the front of auto_path so `package require mp_sim`
# picks up on-disk edits rather than any version mounted from a
# (potentially older) deployed dlsh.zip. Re-source any time you've
# edited mp_sim.tcl -- forgets the package and re-requires it so a
# fresh evaluation runs from scratch.
#
# vfs/lib is the source-of-truth for pure-Tcl packages: the dlsh.zip
# build sweeps it into the deployed zip, so this is also where the
# release version lives. No separate pkgs/ folder needed for
# pure-Tcl -- pkgs/ is reserved for packages with C extensions.

set lib_dir /Users/sheinb/src/dlsh/vfs/lib

# Front-of-path so on-disk shadows the zip-mounted copy.
if {[lsearch -exact $auto_path $lib_dir] >= 0} {
    set auto_path [lsearch -inline -all -not -exact $auto_path $lib_dir]
}
set auto_path [linsert $auto_path 0 $lib_dir]

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
