#
# simple example to show how to build a box2d world
#  taken from https://box2d.org/documentation/hello.html
#
package require box2d
package require dlsh

proc get_box_coords { tx ty w h { a 0 } } {
    dl_local x [dl_mult $w [dl_flist -.5 .5 .5 -.5 -.5 ]]
    dl_local y [dl_mult $h [dl_flist -.5  -.5 .5 .5 -.5 ]]

    set cos_theta [expr cos(-1*$a*($::pi/180.))]
    set sin_theta [expr sin(-1*$a*($::pi/180.))]

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

proc make_world {} {
    # create the world
    set world [box2d::createWorld]
    
    # create the ground plane
    box2d::createBox $world 0 0 -7 18 1
    show_box ground 0 -7 18 1
    
    # create a dynamic box
    set box [box2d::createBox $world 2 0 4 1 1]
    set box_id [show_box box 0 4 1 1]
    return "$world $box $box_id"
}

proc run_simulation {} {
    global world box box_id
    while !$::done { 
	after 20
	box2d::step $world .020
	lassign [box2d::getBodyInfo $world $box] tx ty a
	update_box $box_id $tx $ty 1 1 $a
	update
    }
}

proc run {} {
    global done world display

    $display delete all
    
    box2d::destroy all
    set done 0
    lassign [make_world] ::world ::box ::box_id
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



