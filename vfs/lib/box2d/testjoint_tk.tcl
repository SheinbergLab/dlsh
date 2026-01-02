if { [lsearch $auto_path /usr/local/lib] == -1 } {
    lappend auto_path /usr/local/lib
}

package require dlsh
package require box2d

proc make_stims { { nboxes 2 } { ncircles 2 } } {
    global circle
    set b2_staticBody 0
    set b2_kinematicBody 1
    set b2_dynamicBody 2
    
    set bworld [box2d::create]

    # Create a "catcher" by attaching 5 boxes to a static body
    set catcher1 [create_catcher $bworld -4 -3 0]
    set catcher2 [create_catcher $bworld 4 -3 0]

    for { set i 0 } { $i < $nboxes } { incr i } {
	set tx [expr rand()*12-6]
	set ty [expr rand()*9-1]
	set scale [expr 1.5*rand()+1]
	
	set angle [expr rand()*180]

	set box [create_body $bworld $b2_staticBody $tx $ty 0]
	add_box $bworld $box 0 0 $scale .5 $angle


	set pivot [create_body $bworld $b2_staticBody $tx [expr $ty+.3] 0]
	add_box $bworld $pivot 0 0 $scale .5 $angle

	
	#box2d::createRevoluteJoint $bworld $box $pivot 0.5 0.5 0.5 0.5
    }

    set tx [expr rand()*10-5]
    set ty 9
    set radius .5
    set circle [create_body $bworld $b2_staticBody $tx $ty 0]

    # set global so we can change to dynamic 
    set ::ball_body $circle

    add_circle $bworld $circle 0 0 $radius
    
    return $bworld
}

proc create_catcher { bworld tx ty { angle 0 } } {
    set b2_staticBody 0
    set catcher [create_body $bworld $b2_staticBody $tx $ty $angle]

    set sides {}
    lappend sides [add_box $bworld $catcher    0 -4.25 5 .5 0]
    lappend sides [add_box $bworld $catcher  2.5 -3.5 .5 2 0]
    lappend sides [add_box $bworld $catcher -2.5 -3.5 .5 2 0]
    return $catcher
}

proc create_body { bworld type tx ty { angle 0 } } {
    set body [box2d::createBody $bworld $type $tx $ty $angle]
    return $body
}

proc add_box { bworld body tx ty sx sy { angle 0 } } {
    box2d::createBoxFixture $bworld $body $sx $sy $tx $ty $angle
    lassign [box2d::getBodyInfo $bworld $body] bx by
    set x [expr $bx+$tx]
    set y [expr $by+$ty]
    show_box $body $x $y 0 $sx $sy 1 [expr -1*$angle] 0 0 1
}


proc add_circle { bworld body tx ty radius } {
    box2d::createCircleFixture $bworld $body $tx $ty $radius
    lassign [box2d::getBodyInfo $bworld $body] bx by
    show_sphere [expr $bx+$tx] [expr $by+$ty] 0 $radius $radius $radius $::colors(white)
}

proc run_simulation { world } {
    global circle
    while !$::done { 
	after 20
	box2d::update $world .02
	lassign [box2d::getBodyInfo $world $circle] tx ty
	update_ball $::ball $::ball_radius $tx $ty
	update
    }
}


proc show_box { name tx ty tz sx sy sz spin rx ry rz } {
    dl_local x [dl_mult $sx [dl_flist -.5 .5 .5 -.5 -.5 ]]
    dl_local y [dl_mult $sy [dl_flist -.5  -.5 .5 .5 -.5 ]]

    set cos_theta [expr cos(-1*$spin*($::pi/180.))]
    set sin_theta [expr sin(-1*$spin*($::pi/180.))]

    dl_local rotated_x [dl_sub [dl_mult $x $cos_theta] [dl_mult $y $sin_theta]]
    dl_local rotated_y [dl_add [dl_mult $y $cos_theta] [dl_mult $x $sin_theta]]


    dl_local x [dl_add $tx $rotated_x]
    dl_local y [dl_add $ty $rotated_y]

    lassign [deg_to_display $x $y] xlist ylist
    set coords [list]
    foreach a $xlist b $ylist {	lappend coords $a $b }
    
    $::display create polygon $coords -outline white
}

proc show_sphere { tx ty tz sx sy sz color } {
    set radius [deg_to_pixels $sx]
    set ::ball_radius $radius
    set ::ball [$::display create oval 0 0 0 0 -outline white]

    update_ball $::ball $::ball_radius $tx $ty
}

proc onContact { w a b } {
#    puts "{$a [box2d::getBodyInfo $w $a]} {$b [box2d::getBodyInfo $w $b]}"
}

proc onPreSolve { w a b x y v } {    
#    puts "$w $a $b $x $y $v"
    set radius 2
    set contact_point [$::display create oval 50 50 60 60 -outline white -fill cyan]
    update_ball $contact_point $radius $x $y
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

proc update_ball { ball radius x y } {
    set msize $radius
    lassign [deg_to_display $x $y] x0 y0
    $::display coords $ball [expr $x0-$msize] [expr $y0-$msize] \
	[expr $x0+$msize] [expr $y0+$msize] 
}

set display_width 600
set display_height 600
set display [canvas .c -width $display_width -height $display_height -background black]
pack $display
wm protocol . WM_DELETE_WINDOW { set ::done 1; exit }

proc run {} {
    global done world display

    $display delete all
    
    box2d::destroy all
    set done 0
    set world [make_stims 7 1]
    update

    #box2d::setBeginContactCallback $world onContact
    box2d::setPreSolveCallback $world onPreSolve
    run_simulation $world
}

bind $display <Double-1> { set ::done 1; run }
bind . <space> { box2d::setBodyType $::world $::ball_body 2 }
bind . <Escape> { set ::done 1; exit }
run
