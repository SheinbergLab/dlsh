# binding_diff.tcl -- compare what `set` and `dl_local` actually do.
#
# dl_set absorbing a list into a group behaved differently depending on how
# the source was bound: an inline result was moved, a dl_local list was
# copied, and a `set` list was moved out from under the variable. Nothing
# crashed -- the variable just started naming a list that had been renamed
# into a group. That is the shape of bug worth hunting for siblings of: a
# silent difference that only shows under one binding style.
#
# So run the same scenario under each style and print what happened side by
# side. A row where the styles disagree is not automatically a bug -- they
# are genuinely different tools, and dl_clean is supposed to tell them apart
# -- but every disagreement should be one someone chose.
#
#   dlsh binding_diff.tcl
#
# Exits non-zero if a scenario errors outright, which is always a bug.

package require dlsh
fconfigure stdout -buffering none

# Bind NAME to VALUE in the caller's frame, the way the named style would.
proc bind {style name value} {
    if {$style eq "set"} {
        uplevel 1 [list set $name $value]
    } else {
        uplevel 1 [list dl_local $name $value]
    }
}

# Is the list this handle names still usable?
proc live {h} {
    if {[catch {dl_length $h}]} { return "gone" }
    return "live"
}

set ::scenarios {}
proc scenario {name body} { lappend ::scenarios [list $name $body] }

# --- lifetime ------------------------------------------------------------
scenario "survives dl_clean" {
    bind $style x [dl_ilist 1 2 3]
    dl_clean
    return [live $x]
}
scenario "survives unset of the variable" {
    bind $style x [dl_ilist 1 2 3]
    set h $x
    unset x
    return [live $h]
}
scenario "dl_delete frees it immediately" {
    bind $style x [dl_ilist 1 2 3]
    dl_delete $x
    return [live $x]
}

# --- aliasing ------------------------------------------------------------
scenario "alias sees mutation" {
    bind $style x [dl_ilist 1 2 3]
    set y $x
    dl_append $y 4
    return [dl_tcllist $x]
}
scenario "dl_delete via alias kills the original" {
    bind $style x [dl_ilist 1 2 3]
    set y $x
    dl_delete $y
    return [live $x]
}

# --- crossing a frame ----------------------------------------------------
scenario "returned from a proc" {
    proc _mk {style} { bind $style inner [dl_ilist 1 2 3]; return $inner }
    set h [_mk $style]
    return [live $h]
}
scenario "survives the frame that made it" {
    proc _mk2 {style} { bind $style inner [dl_ilist 1 2 3]; return [list $inner] }
    set h [lindex [_mk2 $style] 0]
    return [live $h]
}
scenario "mutation inside a proc is visible outside" {
    bind $style x [dl_ilist 1 2 3]
    proc _mut {h} { dl_append $h 99 }
    _mut $x
    return [dl_tcllist $x]
}

# --- absorbing into a group ---------------------------------------------
scenario "dl_set g:col leaves the source usable" {
    bind $style x [dl_ilist 1 2 3]
    set g [dg_create]
    dl_set $g:col $x
    set r [live $x]
    dg_delete $g
    return $r
}
scenario "dg_addExistingList leaves the source usable" {
    bind $style x [dl_ilist 1 2 3]
    set g [dg_create]
    dg_addExistingList $g $x col
    set r [live $x]
    dg_delete $g
    return $r
}
scenario "column survives deleting the source" {
    bind $style x [dl_ilist 1 2 3]
    set g [dg_create]
    dl_set $g:col $x
    catch {dl_delete $x}
    set r [catch {dl_tcllist $g:col} out]
    dg_delete $g
    return [expr {$r ? "column lost" : "column ok: $out"}]
}
scenario "source survives deleting the group" {
    bind $style x [dl_ilist 1 2 3]
    set g [dg_create]
    dl_set $g:col $x
    dg_delete $g
    return [live $x]
}

# --- shimmering ----------------------------------------------------------
scenario "usable after llength on the handle" {
    bind $style x [dl_ilist 1 2 3]
    catch {llength $x}
    return [live $x]
}
scenario "usable after string ops on the handle" {
    bind $style x [dl_ilist 1 2 3]
    catch {string length $x}
    catch {expr {[string first % $x] >= 0}}
    return [live $x]
}

# --- other operations that re-parent or rename ---------------------------
# Same class as the absorb bug: the command changes what a list is called or
# who owns it, and the handle a variable is holding may or may not follow.
scenario "usable after dl_rename" {
    bind $style x [dl_ilist 1 2 3]
    catch {dl_rename $x renamed_one}
    return [live $x]
}
scenario "dl_reset leaves the handle valid" {
    bind $style x [dl_ilist 1 2 3]
    catch {dl_reset $x}
    return [live $x]
}
scenario "sublist assignment into a group" {
    bind $style x [dl_ilist 1 2 3]
    set g [dg_create]
    dl_set $g:col [dl_llist [dl_ilist 9 9]]
    catch {dl_set $g:col:0 $x}
    set r [live $x]
    dg_delete $g
    return $r
}
scenario "list is still usable after dg_copy of its group" {
    bind $style x [dl_ilist 1 2 3]
    set g [dg_create]
    dl_set $g:col $x
    catch {dg_copy $g gcopy}
    set r [live $x]
    catch {dg_delete $g}; catch {dg_delete gcopy}
    return $r
}
scenario "two groups can hold it in turn" {
    bind $style x [dl_ilist 1 2 3]
    set g1 [dg_create]; set g2 [dg_create]
    dl_set $g1:a $x
    catch {dl_set $g2:b $x}
    set r "[live $x] / [catch {dl_tcllist $g2:b}]"
    catch {dg_delete $g1}; catch {dg_delete $g2}
    return $r
}

# --- running -------------------------------------------------------------
proc run_one {style body} {
    # each scenario gets a clean slate
    catch {dl_clean}
    catch {foreach g [dg_dir] { dg_delete $g }}
    set rc [catch {apply [list {style} $body] $style} out]
    if {$rc} { return [list ERROR [string range [lindex [split $out \n] 0] 0 44]] }
    return [list OK $out]
}

set errors 0
set differ 0
puts ""
puts [format "  %-46s %-22s %-22s" "scenario" "set" "dl_local"]
puts [format "  %-46s %-22s %-22s" [string repeat - 46] [string repeat - 22] [string repeat - 22]]
foreach s $::scenarios {
    lassign $s name body
    lassign [run_one set      $body] rc1 r1
    lassign [run_one dl_local $body] rc2 r2
    if {$rc1 eq "ERROR" || $rc2 eq "ERROR"} { incr errors }
    set mark ""
    if {$r1 ne $r2} { set mark " <-- differs"; incr differ }
    puts [format "  %-46s %-22s %-22s%s" $name $r1 $r2 $mark]
}
puts ""
puts "  scenarios: [llength $::scenarios]   differing: $differ   errored: $errors"
puts ""
if {$errors} { exit 1 }
exit 0
