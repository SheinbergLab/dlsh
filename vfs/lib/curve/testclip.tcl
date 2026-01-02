#
# Sample code for testing clipper clipping library
#
lappend auto_path /usr/local/lib/tcltk

package require dlsh
package require curve
package require BWidget
package require impro

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
    setwindow -.5 -.5 .5 .5
    dl_local interped [curve::cubic $poly:x $poly:y 20]
    dlg_lines $interped:0 $interped:1 -filled $::filled

    return $poly
}

#
# create union from polygons (curve::clipper)
#
proc poly_union { args } {
    set s 10000.
    dl_local ps [dl_llist]
    foreach poly $args {
	dl_local interped [curve::cubic $poly:x $poly:y 20]
	dl_append $ps $interped
    }
    dl_local polys [dl_int [dl_mult $ps $s]]
    dl_local union [dl_div [curve::clipper $polys] $s]
    dl_return $union
}

proc show_polygons {} {
    clearwin
    pushpviewport 0 0 .33 1
    setwindow -.5 -.5 .5 .5

    $::canv delete polys
    for { set i 0 } { $i < $::npolys } { incr i } {
	dlg_lines lastpolygons:p$i:0 lastpolygons:p$i:1 -filled $::filled

	dl_local scaled [dl_add [dl_mult lastpolygons:p$i 256] 128]
	dl_local c [dl_collapse [dl_transpose [dl_int $scaled]]]
	set coords [dl_tcllist $c]
	$::canv create line {*}$coords -fill white -tags polys
    }
    poppviewport

    pushpviewport .33 0 .66 1
    setwindow -.5 -.5 .5 .5

    $::canv delete union
    set xoffset 256
    if { !$::symmetric } {
	dlg_lines lastpolygons:union:0 lastpolygons:union:1 -filled $::filled

	dl_local scaled [dl_add [dl_mult lastpolygons:union 256] [dl_add 128 [dl_flist $xoffset 0]]]
	dl_local c [dl_collapse [dl_transpose [dl_int $scaled]]]
	set coords [dl_tcllist $c]
	$::canv create polygon {*}$coords -fill white -tags union

    } else {
	dl_local reflected [make_symmetrical lastpolygons:union]

	dl_local scaled [dl_add [dl_mult $reflected 256] [dl_add 100 [dl_flist $xoffset 0]]]
	dl_local c [dl_collapse [dl_transpose [dl_int $scaled]]]
	set coords [dl_tcllist $c]
	$::canv create polygon {*}$coords -fill white -tags union
	
	dlg_lines $reflected:0 $reflected:1 -filled $::filled
    }
    poppviewport

    # show as rendered polygon
    pushpviewport .67 0 1 1
    setwindow -.5 -.5 .5 .5

    if { !$::symmetric } {
	set polyimg [create_img_from_poly lastpolygons:union:0 lastpolygons:union:1]
    } else {
	dl_local reflected [make_symmetrical lastpolygons:union]
	set polyimg [create_img_from_poly $reflected:0 $reflected:1]
    }

    set flipped [img_mirrory $polyimg]
    set imagedata [img_toPNG $flipped]
    
    $::canv delete polyimg
    image create photo polyimg -data $imagedata
    $::canv create image 600 128 -tags polyimg -image polyimg
    
    dl_local pix [dl_llist "256 256" [img_img2list $polyimg]]
    dlg_image 0 0 $pix 1.0 1.0 -center
    poppviewport

    img_delete $flipped $polyimg
}


# a valid symmetrical object has a single path returned from the initial clip
proc symmetrical_valid { v } {
    set s 1000.0
    dl_local clip [dl_int [dl_mult [dl_llist [dl_flist -.5 0 0 -.5] [dl_flist -.5 -.5 .5 .5]] $s]]
    dl_local poly [dl_int [dl_mult $v $s]]
    dl_local left_half [curve::clipper [dl_llist $poly] [dl_llist $clip]]
    if { [dl_length $left_half] == 1 } { return 1 } { return 0 } 
}

# simple routine that creates a symmetric object by cutting in half
# and then making union between cut half and mirror opposite
proc make_symmetrical { v } {
    set s 1000.0
    dl_local clip [dl_int [dl_mult [dl_llist [dl_flist -.5 0 0 -.5] [dl_flist -.5 -.5 .5 .5]] $s]]
    dl_local poly [dl_int [dl_mult $v $s]]
    dl_local left_half [curve::clipper [dl_llist $poly] [dl_llist $clip]]
    dl_local right_half [dl_mult $left_half [dl_llist [dl_ilist -1 1]]]
    dl_local union [dl_div [curve::clipper [dl_llist $left_half:0 $right_half:0]] $s]
    dl_return [dl_llist $union:0:0 $union:0:1]
}

#
# test with a set of random shapes (4x3 in this test)
#
proc clip_test {} {
    if { [dg_exists lastpolygons] } { dg_delete lastpolygons }
    dg_create lastpolygons

    set done 0
    set maxtries 100
    set tries 0

    while { !$done } {
	set ps ""
	incr tries
	for { set i 0 } { $i < $::npolys } { incr i } {
	    lappend ps [create_poly $::nverts]
	}
	
	dl_local union [eval poly_union $ps]
	if { [dl_length $union] == 1 &&
	     [symmetrical_valid [dl_llist $union:0:0 $union:0:1]] } {
	    set done 1
	} else {
	    foreach p $ps { dg_delete $p }
	}
	if { $tries == $maxtries } { error "unable to create single union" }
    }
    
    set i -1
    foreach p $ps {
	dl_set lastpolygons:p[incr i] [curve::cubic $p:x $p:y 20]
	dg_delete $p
    }
    dl_set lastpolygons:union [dl_llist $union:0:0 $union:0:1]    
    
    show_polygons
}

proc save_poly { filename } {
    set w 512
    set h 512
    set img [img_create -width $w -height $h -depth 4]
    dl_local x [dl_add [dl_mult lastpolygons:union:0 $w] [expr ($w/2.)-1]]
    dl_local y [dl_add [dl_mult -1.0 lastpolygons:union:1 $h] [expr $h/2.-1.]]
    set poly [img_drawPolygon $img $x $y 255 255 255 255]
    img_writePNG $poly $filename
    img_delete $img
    img_delete $poly
}

proc create_img_from_poly { shape_poly_x shape_poly_y } {
    set r 255
    set g 255
    set b 255
    
    set width 256; set width_2 [expr $width/2]
    set height 256; set height_2 [expr $height/2]
    set depth 4
    
    set x $shape_poly_x
    set y $shape_poly_y
    
    dl_local xscaled [dl_add [dl_mult $x $width] $width_2]
    dl_local yscaled [dl_add [dl_mult [dl_negate $y] $height] $height_2]
    set img [img_create -width $width -height $height -depth $depth]
    set poly [img_drawPolygon $img $xscaled $yscaled $r $g $b 255]
    img_delete $img
    return $poly
}



# widget

set ::clipfunc Union
set ::filled 0
set ::nverts 2
set ::npolys 3
set ::symmetric 0
set ::outputfile blob.png

proc setup {} {
    set clipfuncs "Union Intersection Difference XOR"
    frame .f1

    set ::canv .f1.cgwin
    pack [canvas $::canv -background black -width 720 -height 240] \
	-expand true -fill both

    frame .f2
    frame .f2.cf
    pack [label .f2.cf.label -text "Clip Functions: "] -side left
    pack [ComboBox .f2.cf.cb -values $clipfuncs \
	      -textvariable clipfunc \
	      -modifycmd { clip_test }]
    pack .f2.cf -anchor w
    
    frame .f2.fill
    pack [label .f2.fill.label -text "Fill: "] -side left
    pack [checkbutton .f2.fill.filled \
	      -variable filled -command show_polygons] -side left
    pack .f2.fill -anchor w

    frame .f2.sym
    pack [label .f2.sym.label -text "Symmetrical: "] -side left
    pack [checkbutton .f2.sym.symmetric \
	      -variable symmetric -command show_polygons] -side left
    pack .f2.sym -anchor w

    frame .f2.nv
    pack [label .f2.nv.npolyslabel -text "NVerts: "] -side left
    pack [spinbox .f2.nv.nverts -command clip_test \
	      -textvariable nverts \
	      -from 0 \
	      -to 15 -width 25] -side right
    pack .f2.nv -anchor w

    frame .f2.np
    pack [label .f2.np.npolyslabel -text "NPolys: "] -side left
    pack [spinbox .f2.np.npolys -text "NPolys:" -command clip_test \
	      -textvariable npolys \
	      -from 1 \
	      -to 4 -width 25] -side right
    pack .f2.np -anchor w

    frame .f2.save
    pack [label .f2.save.label -text "Output filename: "] -side left
    pack [entry .f2.save.save -textvariable outputfile] -side right
    pack .f2.save -anchor w
    

    pack [button .f2.button -text "Save" \
	      -command { save_poly $::outputfile }] -side top

    pack .f1 .f2 -expand true -fill both -side left


    wm title . "Polygon Clipping Demo"
    bind .f1.cgwin <1> { clip_test }
    bind . <Control-h> { console show }
}

setup
clip_test
