#
# simple example to show how to build a box2d world
#
package require box2d
package require dlsh

proc get_box_coords { tx ty w h { a 0 } } {
    dl_local x [dl_mult $w [dl_flist -.5 .5 .5 -.5 -.5 ]]
    dl_local y [dl_mult $h [dl_flist -.5  -.5 .5 .5 -.5 ]]

    set cos_theta [expr cos($a)]
    set sin_theta [expr sin($a)]

    dl_local rotated_x [dl_sub [dl_mult $x $cos_theta] [dl_mult $y $sin_theta]]
    dl_local rotated_y [dl_add [dl_mult $y $cos_theta] [dl_mult $x $sin_theta]]

    dl_local x [dl_add $tx $rotated_x]
    dl_local y [dl_add $ty $rotated_y]

    lassign [deg_to_display $x $y] xlist ylist
    set coords [list]
    foreach a $xlist b $ylist {	lappend coords $a $b }
    return $coords
}

proc get_line_coords { tx ty r { a 0 } } {
    dl_local x [dl_flist 0 $r]
    dl_local y [dl_flist 0 0]

    set cos_theta [expr cos($a)]
    set sin_theta [expr sin($a)]

    dl_local rotated_x [dl_sub [dl_mult $x $cos_theta] [dl_mult $y $sin_theta]]
    dl_local rotated_y [dl_add [dl_mult $y $cos_theta] [dl_mult $x $sin_theta]]

    dl_local x [dl_add $tx $rotated_x]
    dl_local y [dl_add $ty $rotated_y]

    lassign [deg_to_display $x $y] xlist ylist
    set coords [list]
    foreach a $xlist b $ylist {	lappend coords $a $b }
    return $coords
}

proc show_box { name tx ty w h { a 0 } } {
    set coords [get_box_coords $tx $ty $w $h $a]
    return [$::display create polygon $coords -outline white -tag $name]
}

proc update_box { id tx ty w h { a 0 } } {
    set coords [get_box_coords $tx $ty $w $h $a]
    $::display coords $id $coords
}

proc update_ball { ball r x y a } {
    set radius [deg_to_pixels $r]
    lassign [deg_to_display $x $y] x0 y0
    $::display coords $ball \
	[expr $x0-$radius] [expr $y0-$radius] \
	[expr $x0+$radius] [expr $y0+$radius] 
    $::display coords $::ball_line [get_line_coords $x $y $r $a]
}

proc show_ball { tx ty r } {
    set radius [deg_to_pixels $r]
    set ::ball_id [$::display create oval 0 0 0 0 -outline white]
    set ::ball_line [$::display create line 0 0 $radius 0 -fill white]
    update_ball $::ball_id $r $tx $ty 0
    return $::ball_id
}

proc deg_to_pixels { x } {
    set w $::display_width
    set hrange_h 10.0
    return [expr $x*$w/(2*$hrange_h)]
}

proc deg_to_display { x y } {
    set w $::display_width
    set h $::display_height
    set aspect [expr {1.0*$h/$w}]
    set hrange_h 10.0
    set hrange_v [expr $hrange_h*$aspect]
    set hw [expr $w/2]
    set hh [expr $h/2]
    dl_local x0 [dl_add [dl_mult [dl_div $x $hrange_h] $hw] $hw]
    dl_local y0 [dl_sub $hh [dl_mult [dl_div $y $hrange_v] $hh]]
    return [list [dl_tcllist $x0] [dl_tcllist $y0]]
}

proc make_world { { n 1 } } {
    # create the world
    set world [box2d::createWorld]

    set ::planks     {}
    set ::plank_ids  {}
    set ::plank_dims {}
    # create planks
    for { set i 0 } { $i < $n } { incr i } {
	set x [expr rand()*10-5]
	set y [expr rand()*10-5]
	set a [expr rand()*2*$::pi]

	set w 3
	set h .5
	set plank [box2d::createBox $world 2 $x $y $w $h $a]
	set id [show_box plank${i} $x $y $w $h $a]

	lappend ::planks $plank
	lappend ::plank_ids $id
	lappend ::plank_dims [list $w $h]
	
	set pivot [box2d::createCircle $world 0 $x $y .2]
	box2d::createRevoluteJoint $world $plank $pivot
    }
    
    # create a dynamic circle
    set ball [box2d::createCircle $world 2 0 7 .3]
    set ball_id [show_ball 0 7 .3]
    return "$world $ball $ball_id"
}

proc run_simulation {} {
    global world ball ball_id
    while !$::done { 
    	after 20
    	box2d::step $world .020

	foreach p $::planks id $::plank_ids dims $::plank_dims {
	    lassign [box2d::getBodyInfo $world $p] tx ty a
	    update_box $id $tx $ty {*}$dims $a
	}
	
    	lassign [box2d::getBodyInfo $world $ball] tx ty a
    	update_ball $ball_id .3 $tx $ty $a
    	update
        if { [box2d::getContactBeginEventCount $world] } { 
            puts [box2d::getContactBeginEvents $world] 
        }
    }
}

proc run { { n 10 } } {
    global done world display

    $display delete all
    
    box2d::destroy all
    set done 0
    lassign [make_world $n] ::world ::ball ::ball_id
    
    update

    run_simulation
}

set display_width 600
set display_height 600
set display [canvas .c -width $display_width -height $display_height -background black]
pack $display
wm protocol . WM_DELETE_WINDOW { set ::done 1; exit }


bind $display <Double-1> { set ::done 1; run }
bind . <space> { box2d::setBodyType $::world $::box 2 }
bind . <Escape> { set ::done 1; exit }
run



