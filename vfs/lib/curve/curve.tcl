#
# NAME
#   curve.tcl
#
# DESCRIPTION
#   Tcl support code for polygon curve/clipping/union functions
#
# AUTHOR
#  DLS, JUL-2011
#

package provide curve 1.1

namespace eval curve {

    set polyinterp_steps 20

    #
    # given a dg with points already, add a new, random one, in order
    #
    proc add_points { poly n { mindist 0.05 } } {
	set maxtries 1000
	set mind2 [expr $mindist*$mindist]
	for { set i 0 } { $i < $n } { incr i } {
	    set done 0
	    set tries 0
	    while { !$done && $tries < $maxtries } {
		dl_local randoms [dl_sub [dl_mult 0.8 [dl_urand 2]] 0.4]
		set x [dl_get $randoms 0]
		set y [dl_get $randoms 1]
		dl_local xd [dl_sub $poly:x $x]
		dl_local yd [dl_sub $poly:y $y]
		dl_local d2 [dl_add [dl_mult $xd $xd] [dl_mult $yd $yd]]
		if { [dl_min $d2] >= $mind2 } { set done 1 }
		incr tries
	    }
	    if { $tries == $maxtries } { error "unable to place points" }

	    set index [curve::closestPoint $poly:x $poly:y $x $y]
	    dl_insert $poly:x [expr $index+1] $x
	    dl_insert $poly:y [expr $index+1] $y
	}
	return $poly
    }

    #
    # create a random polygon
    #
    proc create_poly { n } {
	set done 0
	set g [dg_create]
	while { !$done } {
	    dl_set $g:x [dl_sub [dl_mult 0.6 [dl_urand 3]] .3]
	    dl_set $g:y [dl_sub [dl_mult 0.6 [dl_urand 3]] .3]
	    add_points $g $n
	    if { ![curve::polygonSelfIntersects $g:x $g:y] } {
		set done 1
	    }
	}
	return $g
    }


    #
    # interpolate (using curve::cubic) and draw
    #
    proc show_poly { poly } {
	set filled 1
	setwindow -.5 -.5 .5 .5
	dl_local interped [curve::cubic $poly:0 $poly:1 $curve::polyinterp_steps]
	dlg_lines $interped:0 $interped:1 -filled $filled
	return $poly
    }

    #
    # given two polygons create union (curve::polygonUnion)
    #
    proc poly_union { p1 p2 } {
	dl_local interped1 [curve::cubic $p1:x $p1:y $curve::polyinterp_steps]
	dl_local interped2 [curve::cubic $p2:x $p2:y $curve::polyinterp_steps]
	dl_local poly1 [dl_llist [dl_llist $interped1] 0]
	dl_local poly2 [dl_llist [dl_llist $interped2] 0]
	dl_local union [curve::polygonUnion $poly1 $poly2]
	dl_return $union
    }

    proc poly_union_interpolated { p1 p2 } {
	dl_local poly1 [dl_llist [dl_llist [dl_llist $p1:0 $p1:1]] 0]
	dl_local poly2 [dl_llist [dl_llist [dl_llist $p2:0 $p2:1]] 0]
	dl_local union [curve::polygonUnion $poly1 $poly2]
	dl_return $union
    }

    proc poly_create { npolys nverts } {
	set done 0
	while { !$done } {
	    set polygons(0) [create_poly $nverts]
	    set polygons(1) [create_poly $nverts]
	    dl_local union [poly_union $polygons(0) $polygons(1)]
	    if { [dl_length $union:0] == 1 } {
		set done 1
	    }
	}

	for { set i 2 } { $i < $npolys } { incr i } {
	    set done 0
	    while { !$done } {
		set polygons($i) [create_poly $nverts]
		dl_local interped [curve::cubic $polygons($i):x $polygons($i):y $curve::polyinterp_steps]
		dl_local union_xy [dl_llist $union:0:0:0 $union:0:0:1]
		dl_local newunion [poly_union_interpolated $union_xy $interped]
		if { [dl_length $newunion:0] == 1 } {
		    dg_delete $polygons($i)
		    dl_local union $newunion
		    set done 1
		} else {
		    dg_delete $polygons($i)
		}
	    }
	}
	dl_local union [dl_llist $union:0:0:0 $union:0:0:1]
	dl_return $union
    }
}
