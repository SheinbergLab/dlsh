#
# Implementation of hex support functions
# Following guidelines of http://www.redblobgames.com/grids/hexagons/
#
# Written in the current dynlist idiom (docs/dynlists.md): lists are bound
# with `set` and handed back with `return`, like ordinary Tcl values.
#
# Precision: the pixel helpers and corner compute in DOUBLE and return FLOAT
# lists. Hex coordinates are int lists, and int x float-literal arithmetic
# runs in float32, whose rounding lands differently for a point and its
# mirror image (7.9999998 vs 8.0) -- at spacing 2, 44 of the 127 points of a
# radius-6 region lost their exact mirror partner. Promoting the coordinates
# to double first makes every literal parse as double too ("literals follow
# the operand"); rounding once on return gives correctly rounded floats,
# which are mirror-symmetric, while callers keep the list type they always
# got (stim2's polyverts, for one, takes float lists).
#
# Naming: relative to redblobgames the two pixel helpers are swapped.
# flat_to_pixel lays hexes out in horizontal ROWS (neighbours size*sqrt(3)
# apart along x) -- redblob's POINTY-top layout; pointy_to_pixel lays them in
# vertical COLUMNS with x negated -- a flat-top layout. Left as is: existing
# callers (ess prf, match_to_sample/phd, stim2's mp_tiled) rely on them.
#

package provide hex 0.8
package require dlsh

namespace eval hex {
    # corners i (0..5 for a closed hexagon use 0..6) of a hex of radius
    # `size` centred on `center` (a two-element x y list)
    proc corner { center size i } {
        set angle_deg [dl_add [dl_mult [dl_double $i] 60.] 30]
        set angle_rad [dl_mult [expr {acos(-1.0)/180.}] $angle_deg]
        set xoffs [dl_mult $size [dl_cos $angle_rad]]
        set yoffs [dl_mult $size [dl_sin $angle_rad]]
        return [dl_float [dl_add [dl_double $center] [dl_llist $xoffs $yoffs]]]
    }

    # Cube: q (x), r (z) / Hex: x (q), y (-x-z), z (r)
    proc cube_to_hex { h } {
        return [dl_choose $h [dl_llist "0 2"]]
    }

    proc hex_to_cube { h } {
        set x [dl_choose $h [dl_llist 0]]
        set z [dl_choose $h [dl_llist 1]]
        set y [dl_sub [dl_mult -1 $x] $z]
        return [dl_transpose [dl_llist $x $y $z]]
    }

    proc add { a b } {
        return [dl_add $a $b]
    }

    proc sub { a b } {
        return [dl_sub $a $b]
    }

    proc scale { a k } {
        return [dl_mult $a $k]
    }

    proc directions {} {
        return [dl_llist [dl_ilist 1 -1 0] [dl_ilist 1 0 -1] \
                    [dl_ilist 0 1 -1] [dl_ilist -1 1 0] \
                    [dl_ilist -1 0 1] [dl_ilist 0 -1 1]]
    }

    proc neighbor { h d } {
        return [hex::add $h [dl_choose [hex::directions] $d]]
    }

    proc diagonals {} {
        return [dl_llist [dl_ilist 2 -1 -1] [dl_ilist 1 1 -2] \
                    [dl_ilist -1 2 -1] [dl_ilist -2 1 1] \
                    [dl_ilist -1 -1 2] [dl_ilist 1 -2 1]]
    }

    proc diagonal_neighbor { h d } {
        return [hex::add $h [dl_choose [hex::diagonals] $d]]
    }

    proc cube_distance { a b } {
        if { [dl_datatype $a] == "list" || [dl_datatype $b] == "list" } {
            return [dl_div [dl_sums [dl_abs [dl_sub $a $b]]] 2]
        }
        return [dl_tcllist [dl_div [dl_sum [dl_abs [dl_sub $a $b]]] 2]]
    }

    # every cube coordinate within N steps of center
    proc cube_region { center N } {
        set region [dl_llist]
        for { set dx -$N } { $dx <= $N } { incr dx } {
            for { set dy [expr {max(-$N, -$dx-$N)}] } \
                { $dy <= [expr {min($N, -$dx+$N)}] } { incr dy } {
                    set dz [expr {-$dx-$dy}]
                    dl_append $region [hex::add $center [dl_ilist $dx $dy $dz]]
                }
        }
        return $region
    }

    # the ring of cube coordinates exactly `radius` steps from center
    proc cube_ring { center radius } {
        set dirs [hex::directions]
        set c [hex::add $center [hex::scale [dl_choose $dirs 4] $radius]]
        set ring [dl_llist]
        foreach i {0 1 2 3 4 5} {
            foreach j [dl_tcllist [dl_fromto 0 $radius]] {
                dl_append $ring $c
                set c [hex::neighbor $c $i]
            }
        }
        return [dl_unpack $ring]
    }

    # vertical columns, x negated (a flat-top layout; see Naming above)
    proc pointy_to_pixel { h { size 1.0 } } {
        set sqrt3 [expr {sqrt(3.0)}]
        set q [dl_double [dl_choose $h [dl_llist 0]]]
        set r [dl_double [dl_choose $h [dl_llist 1]]]
        set x [dl_mult -1.0 $size 1.5 $q]
        set y [dl_mult $size [dl_add [dl_mult [expr {$sqrt3/2.0}] $q] \
                                  [dl_mult $sqrt3 $r]]]
        return [dl_float [dl_transpose [dl_llist $x $y]]]
    }

    # horizontal rows (a pointy-top layout; see Naming above)
    proc flat_to_pixel { h { size 1.0 } } {
        set sqrt3 [expr {sqrt(3.0)}]
        set q [dl_double [dl_choose $h [dl_llist 0]]]
        set r [dl_double [dl_choose $h [dl_llist 1]]]
        set x [dl_mult $size [dl_add [dl_mult $sqrt3 $q] \
                                  [dl_mult [expr {$sqrt3/2.0}] $r]]]
        set y [dl_mult $size 1.5 $r]
        return [dl_float [dl_transpose [dl_llist $x $y]]]
    }

    proc test { { type pointy } } {
        package require dlsh
        package require hex

        clearwin
        setwindow -10 -10 10 10
        set s 1
        set cols [dlg_rgbcolors [dl_int [dl_mult 255 [dl_urand 127]]] \
                      [dl_int [dl_mult 255 [dl_urand 127]]] \
                      [dl_int [dl_mult 255 [dl_urand 127]]]]
        set hex_grid [::hex::cube_region [dl_ilist 0 0 0] 6]
        set centers [::hex::${type}_to_pixel $hex_grid $s]
        set hex [dl_mult 0.8 [::hex::corner "0 0" 1.0 [dl_fromto 0 7]]]
        set hgrid [dl_transpose [dl_add $centers [dl_llist $hex]]]
        dlg_lines $hgrid:0 $hgrid:1 -fillcolors $cols -linecolors $cols
        flushwin
    }
}
