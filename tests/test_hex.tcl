#!/usr/bin/env dlsh
#
# test_hex.tcl
#   Correctness test for the hex package (vfs/lib/hex/hex.tcl): lattice
#   generation, cube/axial conversions, and the pixel helpers' precision --
#   computed in double, returned as float lists, exactly mirror-symmetric.
#
#   Usage:  dlsh test_hex.tcl        (exits non-zero on failure)
#
#   Loads the on-disk hex.tcl, not the copy baked into dlsh.zip, so it tests
#   the source tree.

# --- dlsh bootstrap ---
if {[catch {package require dlsh}]} {
    foreach path {/usr/local/dlsh/dlsh.zip /usr/local/lib/dlsh.zip} {
        if {[file exists $path]} {
            catch {zipfs mount $path /dlsh}
            set base [file join [zipfs root] dlsh]
            set ::auto_path [linsert $::auto_path 0 ${base}/lib]
            break
        }
    }
    package require dlsh
}
catch { namespace delete ::hex }
source [file join [file dirname [file normalize [info script]]] .. vfs lib hex hex.tcl]

set ::fail 0
proc check {label got want} {
    if {$got eq $want} { puts "OK   $label" } \
    else { puts "FAIL $label -> got {$got} want {$want}"; incr ::fail }
}
proc near {a b {tol 1e-6}} { expr {abs($a - $b) <= $tol} }

# --- lattice ----------------------------------------------------------------
foreach N {0 1 3 6} {
    set region [hex::cube_region [dl_ilist 0 0 0] $N]
    check "cube_region $N: 3N(N+1)+1 cells" [dl_length $region] [expr {3*$N*($N+1)+1}]
    # every cube coordinate sums to zero
    check "cube_region $N: x+y+z == 0" [dl_max [dl_abs [dl_sums $region]]] 0
}
foreach R {1 2 5} {
    set ring [hex::cube_ring [dl_ilist 0 0 0] $R]
    check "cube_ring $R: 6R cells" [dl_length $ring] [expr {6*$R}]
    set ds [lsort -unique [lmap c [dl_tcllist $ring] {
        expr {(abs([lindex $c 0]) + abs([lindex $c 1]) + abs([lindex $c 2]))/2}}]]
    check "cube_ring $R: all at distance R" $ds $R
}
check "cube_distance scalar" [hex::cube_distance [dl_ilist 0 0 0] [dl_ilist 2 -1 -1]] 2
check "neighbor 0 of origin" [dl_tcllist [hex::neighbor [dl_ilist 0 0 0] 0]] {{1 -1 0}}

# cube -> axial -> cube round trip
set region [hex::cube_region [dl_ilist 0 0 0] 4]
set back [hex::hex_to_cube [hex::cube_to_hex $region]]
check "cube_to_hex / hex_to_cube round trip" [dl_tcllist $back] [dl_tcllist $region]

# --- pixel helpers: float out, double inside, exact symmetry ----------------
set ax [hex::cube_to_hex [hex::cube_region [dl_ilist 0 0 0] 6]]
set size [expr {2.0/sqrt(3.0)}]              ;# neighbours 2.0 apart
foreach f {flat pointy} {
    set px [hex::${f}_to_pixel $ax $size]
    check "$f: float lists out" [dl_datatype $px:0:0] float
    set pts [dl_tcllist $px]
    check "$f: one point per cell" [llength $pts] 127

    # exact formula, rounded to float, per point
    set worst 0.0
    foreach qr [dl_tcllist $ax] p $pts {
        lassign $qr q r; lassign $p x y
        if { $f eq "flat" } {
            set ex [expr {$size*(sqrt(3.0)*$q + sqrt(3.0)/2.0*$r)}]
            set ey [expr {$size*1.5*$r}]
        } else {
            set ex [expr {-$size*1.5*$q}]
            set ey [expr {$size*(sqrt(3.0)/2.0*$q + sqrt(3.0)*$r)}]
        }
        set worst [expr {max($worst, abs($x-$ex), abs($y-$ey))}]
    }
    check "$f: within float rounding of the exact position" [near $worst 0 1e-6] 1

    # the regression this file exists for: float32 arithmetic gave 44 of 127
    # points no exact mirror partner at this spacing
    set miss 0
    foreach p $pts {
        lassign $p x y
        set hit 0
        foreach o $pts { lassign $o u v; if { $u == -$x && $v == $y } { set hit 1; break } }
        if { !$hit } { incr miss }
    }
    check "$f: every point has an exact mirror partner" $miss 0

    # six neighbours exactly 2.0 away from an interior point
    set c [lindex $pts [lsearch -exact [dl_tcllist $ax] {1 0}]]
    lassign $c cx cy
    set k 0
    foreach o $pts { lassign $o u v; if { [near [expr {hypot($u-$cx, $v-$cy)}] 2.0 1e-5] } { incr k } }
    check "$f: six neighbours at the spacing" $k 6
}
# the layouts the names actually produce (see Naming in hex.tcl)
set row [dl_tcllist [hex::flat_to_pixel [dl_llist [dl_ilist 1 0]] $size]]
check "flat_to_pixel: q steps along x (rows)" [list [near [lindex $row 0 0] 2.0] [near [lindex $row 0 1] 0.0]] {1 1}
set col [dl_tcllist [hex::pointy_to_pixel [dl_llist [dl_ilist 0 1]] $size]]
check "pointy_to_pixel: r steps along y (columns)" [list [near [lindex $col 0 0] 0.0] [near [lindex $col 0 1] 2.0]] {1 1}

# --- corner -----------------------------------------------------------------
set cr [hex::corner "0 0" 1.0 [dl_fromto 0 6]]
check "corner: float lists, six corners" [list [dl_datatype $cr:0] [dl_length $cr:0]] {float 6}
set worst 0.0
foreach x [dl_tcllist $cr:0] y [dl_tcllist $cr:1] i {0 1 2 3 4 5} {
    set a [expr {acos(-1.0)/180.0*(60*$i + 30)}]
    set worst [expr {max($worst, abs($x-cos($a)), abs($y-sin($a)))}]
}
check "corner: on the unit circle at 30 + 60i degrees" [near $worst 0 1e-6] 1
# corner used to read $::pi; it must work where nothing has defined it
set saved $::pi; unset ::pi
check "corner: works without ::pi" [catch {hex::corner "0 0" 1.0 [dl_fromto 0 6]}] 0
set ::pi $saved

# --- returned lists live in the caller (set / return idiom) -----------------
proc make_grid {} { return [hex::flat_to_pixel [hex::cube_to_hex [hex::cube_region [dl_ilist 0 0 0] 2]] 1.0] }
set g [make_grid]
check "a returned grid outlives its proc" [dl_length $g] 19
set gs {}
foreach n {1 2 3} { lappend gs [make_grid] }
check "grids held in a Tcl list stay alive" [lmap x $gs {dl_length $x}] {19 19 19}

puts [expr {$::fail ? "$::fail FAILURE(S)" : "ALL PASS"}]
exit [expr {$::fail ? 1 : 0}]
