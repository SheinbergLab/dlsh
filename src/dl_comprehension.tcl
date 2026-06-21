# dl_comprehension.tcl
#   Vectorized-comprehension helpers for dlsh. This file is EMBEDDED into
#   libdlsh at build time (see cmake/EmbedTcl.cmake) and Tcl_Eval'd from
#   Dl_Init, so these procs are always available the instant the package
#   loads -- no dependence on the VFS lib path / auto_path.
#
#   They are the escape hatch for per-element logic that is NOT expressible as
#   vectorized dl_* ops. When your transform/filter IS vectorizable, prefer
#   that directly -- it is far faster than a Tcl-body map:
#
#       map     [x*x for x in xs]            -> dl_mult $xs $xs
#       filter  [x for x in xs if x>5]       -> dl_select $xs [dl_gt $xs 5]
#       ternary [x if x>4 else -1 ...]       -> dl_where [dl_gt $xs 4] $xs -1
#       reduce  sum/mean/max per row         -> dl_sums / dl_means / dl_maxs
#
#   All builders return the new list via dl_return, so the result lives in the
#   CALLER's frame (until that proc returns) -- the standard dlsh contract. To
#   pass a built list further UP through a proc of your own, re-protect it:
#       proc mk {} { return [dl_return [dl_map x $xs {expr {$x*2}}]] }
#
#   Built on: dl_foreach, dl_select, dl_create, dl_ilist, dl_return.

namespace eval ::dl {}

# Infer a dl datatype name (int|float|string) from collected Tcl scalars.
proc ::dl::_infer_type {vals} {
    if {[llength $vals] == 0} { return float }
    set allint 1
    foreach v $vals {
        if {![string is double -strict $v]} { return string }
        if {![string is integer -strict $v]} { set allint 0 }
    }
    return [expr {$allint ? "int" : "float"}]
}

# dl_map var listname body ?type?
#   Apply body once per element, collecting the (scalar) results into a new
#   dynlist. var is bound to each element value; for a list-of-lists it is
#   bound to each sublist as a dynlist (so the body can use dl_* ops on it).
#   type: auto (default, inferred int/float/string) | int | float | string.
#
#       dl_map x $xs {expr {$x*$x+1}}        ;# per-element transform
#       dl_map r $rows {dl_sum $r}           ;# per-row reduce -> flat list
proc dl_map {var listname body {type auto}} {
    upvar 1 $var v
    set out {}
    dl_foreach v $listname { lappend out [uplevel 1 $body] }
    if {$type eq "auto"} { set type [::dl::_infer_type $out] }
    return [dl_return [dl_create $type {*}$out]]
}

# dl_filter var listname pred
#   Keep the elements (or sublists) for which pred -- a command returning a
#   boolean -- is true. Returns a new list of the kept elements.
#
#       dl_filter x $xs {expr {$x > 5 && $x % 3 == 0}}
proc dl_filter {var listname pred} {
    upvar 1 $var v
    set mask {}
    dl_foreach v $listname { lappend mask [expr {[uplevel 1 $pred] ? 1 : 0}] }
    return [dl_return [dl_select $listname [dl_ilist {*}$mask]]]
}

# dl_reduce accVar elemVar listname body init
#   Left fold. acc starts at init; each element updates acc <- body. Returns
#   the final acc (a Tcl scalar, not a dynlist).
#
#       dl_reduce a x $xs {expr {$a + $x}} 0        ;# sum
#       dl_reduce a x $xs {expr {max($a,$x)}} -1e30 ;# max
proc dl_reduce {accVar elemVar listname body init} {
    upvar 1 $accVar acc $elemVar v
    set acc $init
    dl_foreach v $listname { set acc [uplevel 1 $body] }
    return $acc
}

# dl_comp var listname ?-where pred? ?-map body? ?-type t?
#   Comprehension: [body for var in listname if pred]. Both -where (a boolean
#   command) and -map (a value command) are optional; -map defaults to the
#   element itself. Collects scalar results into a new dynlist.
#
#       dl_comp x $xs -where {expr {$x > 5}} -map {expr {$x * $x}}
proc dl_comp {var listname args} {
    upvar 1 $var v
    set pred {}; set mapb {}; set type auto
    foreach {opt val} $args {
        switch -- $opt {
            -where  { set pred $val }
            -map    { set mapb $val }
            -type   { set type $val }
            default { error "dl_comp: unknown option \"$opt\" (want -where|-map|-type)" }
        }
    }
    set out {}
    dl_foreach v $listname {
        if {$pred ne "" && ![uplevel 1 $pred]} continue
        if {$mapb eq ""} { lappend out $v } else { lappend out [uplevel 1 $mapb] }
    }
    if {$type eq "auto"} { set type [::dl::_infer_type $out] }
    return [dl_return [dl_create $type {*}$out]]
}
