#
# Implementation of hex support functions
# Following guidelines of http://www.redblobgames.com/grids/hexagons/
#

package provide hex 0.8
package require dlsh

namespace eval hex {
    proc corner { center size i } {
	dl_local angle_deg [dl_add [dl_mult $i 60.] 30]
	dl_local angle_rad [dl_mult [expr $::pi/180.] $angle_deg]
	dl_local xoffs [dl_mult $size [dl_cos $angle_rad]]
	dl_local yoffs [dl_mult $size [dl_sin $angle_rad]]
	dl_return [dl_add $center [dl_llist $xoffs $yoffs]]
    }

    # Cube: q (x), r (z) / Hex: x (q), y (-x-z), z (r)
    proc cube_to_hex { h } {
	dl_return [dl_choose $h [dl_llist "0 2"]]
    }

    proc hex_to_cube { h } {
	dl_local x [dl_choose $h [dl_llist 0]]
	dl_local z [dl_choose $h [dl_llist 1]]
	dl_local y [dl_sub [dl_mult -1 $x] $z]
	dl_return [dl_transpose [dl_llist $x $y $z]]
    }


    proc add { a b } {
	dl_return [dl_add $a $b]
    }

    proc sub { a b } {
	dl_return [dl_sub $a $b]
    }

    proc scale { a k } {
	dl_return [dl_mult $a $k]
    }

    proc directions {} {
	dl_local d [dl_llist [dl_ilist 1 -1 0] [dl_ilist 1 0 -1] \
			[dl_ilist 0 1 -1] [dl_ilist -1 1 0] \
			[dl_ilist -1 0 1] [dl_ilist 0 -1 1]]
	dl_return $d
    }

    proc neighbor { h d } {
	dl_local dirs [hex::directions]
	dl_return [hex::add $h [dl_choose $dirs $d]]
    }

    proc diagonals {} {
	dl_local d [dl_llist [dl_ilist 2 -1 -1] [dl_ilist 1 1 -2] \
			[dl_ilist -1 2 -1] [dl_ilist -2 1 1] \
			[dl_ilist -1 -1 2] [dl_ilist 1 -2 1]]
	dl_return $d
    }

    proc diagonal_neighbor { h d } {
	dl_local dirs [hex::diagonals]
	dl_return [hex::add $h [dl_choose $dirs $d]]
    }

    proc cube_distance { a b } {
	if { [dl_datatype $a] == "list" || [dl_datatype $b] == "list" } {
	    dl_return [dl_div [dl_sums [dl_abs [dl_sub $a $b]]] 2]
	} else {
	    return [dl_tcllist [dl_div [dl_sum [dl_abs [dl_sub $a $b]]] 2]]
	}
    }

    proc cube_region { center N } {
	dl_local region [dl_llist]
	for { set dx -$N } { $dx <= $N } { incr dx } {
	    for { set dy [expr max(-$N,-$dx-$N)] } \
		{ $dy <= [expr min($N, -$dx+$N)] } { incr dy } {
		    set dz [expr -1*$dx-$dy]
		    dl_append $region [hex::add $center [dl_ilist $dx $dy $dz]]
		}
	}
	dl_return $region
    }

    proc cube_ring { center radius } {
	dl_local dirs [hex::directions]
	dl_local c [hex::add $center [hex::scale [dl_choose $dirs 4] $radius]]
	dl_local ring [dl_llist]
	foreach i {0 1 2 3 4 5} {
	    foreach j [dl_tcllist [dl_fromto 0 $radius]] {
		dl_append $ring $c
		dl_local c [hex::neighbor $c $i]
	    }
	}
	dl_return [dl_unpack $ring]
    }
    
    proc pointy_to_pixel { h { size 1.0 } } {
	set sqrt3 [expr sqrt(3)]
	dl_local q [dl_choose $h [dl_llist 0]]
	dl_local r [dl_choose $h [dl_llist 1]]
	dl_local x [dl_mult -1.0 $size 1.5 $q]
	dl_local y [dl_mult $size \
			[dl_add \
			     [dl_mult [expr {$sqrt3/2}] $q] \
			     [dl_mult $sqrt3 $r]]]
	dl_return [dl_transpose [dl_llist $x $y]]
    }
    
    proc flat_to_pixel { h { size 1.0 } } {
	set sqrt3 [expr sqrt(3)]
	dl_local q [dl_choose $h [dl_llist 0]]
	dl_local r [dl_choose $h [dl_llist 1]]
	dl_local x [dl_mult $size \
			[dl_add \
			     [dl_mult $sqrt3 $q] \
			     [dl_mult [expr {$sqrt3/2.}] $r]]]
	dl_local y [dl_mult $size 1.5 $r]
	dl_return [dl_transpose [dl_llist $x $y]]
    }
    
    proc test { { type pointy } } {
	package require dlsh
	package require hex
	
	clearwin
	setwindow -10 -10 10 10
	set s 1
	dl_local cols [dlg_rgbcolors [dl_int [dl_mult 255 [dl_urand 127]]] [dl_int [dl_mult 255 [dl_urand 127]]] [dl_int [dl_mult 255 [dl_urand 127]]]]
	dl_local hex_grid [::hex::cube_region [dl_ilist 0 0 0] 6]
	dl_local centers [::hex::${type}_to_pixel $hex_grid $s]
	dl_local hex [dl_mult 0.8 [::hex::corner "0 0" 1.0 [dl_fromto 0 7]]]
	dl_local hgrid [dl_add $centers [dl_llist $hex]]
	dl_local hgrid [dl_transpose $hgrid]
	dlg_lines $hgrid:0 $hgrid:1 -fillcolors $cols -linecolors $cols
	flushwin
    }
}
