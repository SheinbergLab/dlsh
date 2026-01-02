if { [lsearch $auto_path /usr/local/lib] == -1 } {
    lappend auto_path /usr/local/lib
}

package require dlsh
package require box2d

proc make_stims { nplanks } {
    global circle
    set b2_staticBody 0
    set b2_kinematicBody 1
    set b2_dynamicBody 2
    
    set bworld [box2d::create]

    # Create a "catcher" by attaching boxes to a static body
    set catcher1 [create_catcher $bworld -4 -3 0]
    set catcher2 [create_catcher $bworld 4 -3 0]

    for { set i 0 } { $i < $nplanks } { incr i } {
	set tx [expr rand()*7-3.5]
	set ty [expr rand()*8-3]
	set width [expr 1.5*rand()+1]
	set angle [expr rand()*180]
	
	set pivot [create_body $bworld $b2_staticBody $tx $ty 0]
	add_circle $bworld $pivot pivot${i} 0 0 .1

	set plank [create_body $bworld $b2_dynamicBody $tx $ty 0]
	add_box $bworld $plank plank${i} 0 0 $width .5 $angle

	
	box2d::createRevoluteJoint $bworld $pivot $plank 0 0 0 0
    }

    set tx [expr rand()*10-5]
    set ty 9
    set radius .5
    set circle [create_body $bworld $b2_staticBody $tx $ty 0]

    # set global so we can change to dynamic 
    set ::ball_body $circle

    add_circle $bworld $circle ball 0 0 $radius

    set tx [expr rand()*10-5]
    set ty 8
    set angle 0
    

    # set global so we can change to dynamic 
#    set box [create_body $bworld $b2_staticBody $tx $ty $angle]
#    set ::box_body $box
#    add_box $bworld $box box 0 0 1 2 45 white
    
    return $bworld
}

proc create_catcher { bworld tx ty { angle 0 } } {
    set b2_staticBody 0
    set catcher [create_body $bworld $b2_staticBody $tx $ty $angle]

    set sides {}
    lappend sides [add_box $bworld $catcher bottom  0 -4.25 5 .5 0]
    lappend sides [add_box $bworld $catcher left 2.5 -3.5 .5 2 0]
    lappend sides [add_box $bworld $catcher right -2.5 -3.5 .5 2 0]
    return $catcher
}

proc create_body { bworld type tx ty { angle 0 } } {
    set body [box2d::createBody $bworld $type $tx $ty $angle]
    dict set ::bodyinfo $body [dict create]
    return $body
}

proc add_box { bworld body name x y width height { angle 0 } { color white } } {
    box2d::createBoxFixture $bworld $body $width $height $x $y $angle
    add_box_fixture $body $name $x $y $width $height [expr -1*$angle] $color
}


proc add_circle { bworld body name tx ty radius } {
    box2d::createCircleFixture $bworld $body $tx $ty $radius
    add_circle_fixture $body $name $tx $ty $radius white
}

proc run_simulation { world } {
    while !$::done { 
	box2d::update $world .02
	foreach body [box2d::getBodies $world] {
	    draw_body $world $body
	}
	update
	after 20
    }
}

proc add_box_fixture { body name tx ty sx sy spin color  } {
    dl_local x [dl_mult $sx [dl_flist -.5 .5 .5 -.5 -.5 ]]
    dl_local y [dl_mult $sy [dl_flist -.5  -.5 .5 .5 -.5 ]]

    set cos_theta [expr cos(-1*$spin*($::pi/180.))]
    set sin_theta [expr sin(-1*$spin*($::pi/180.))]

    dl_local rotated_x [dl_sub [dl_mult $x $cos_theta] [dl_mult $y $sin_theta]]
    dl_local rotated_y [dl_add [dl_mult $y $cos_theta] [dl_mult $x $sin_theta]]

    dl_local x [dl_add $tx $rotated_x]
    dl_local y [dl_add $ty $rotated_y]

    set xlist [dl_tcllist $x]
    set ylist [dl_tcllist $y]
    dict set ::bodyinfo $body $name [dict create type box xcoords $xlist ycoords $ylist]
    
    set coords [list]
    foreach a $xlist b $ylist {	lappend coords $a $b }
    $::display create polygon $coords -outline $color -fill {} -tag ${body}_${name} 
}

proc add_circle_fixture { body name tx ty radius color } {
    dict set ::bodyinfo $body $name [dict create type circle x $tx y $ty radius $radius]

    $::display create oval 0 0 0 0 -outline $color -fill {} -tag ${body}_${name}
}

proc draw_body { world body } {
    dict for { fixture finfo } [dict get $::bodyinfo $body] {
	set ftype [dict get $finfo type]
	draw_${ftype} ${world} ${body} ${fixture} $finfo
    }
}

proc draw_circle { world body fixture finfo } {
    lassign [box2d::getBodyInfo $world $body] bx by
    set r [dict get $finfo radius]
    set radius [deg_to_pixels $r]
    set x [expr {[dict get $finfo x]+$bx}]
    set y [expr {[dict get $finfo y]+$by}]
    lassign [deg_to_display $x $y] x0 y0
    $::display coords ${body}_${fixture} \
	[expr {$x0-$radius}] [expr {$y0-$radius}] \
	[expr {$x0+$radius}] [expr {$y0+$radius}] 
}

proc draw_box { world body fixture finfo } {
    lassign [box2d::getBodyInfo $world $body] bx by bangle
    
    dl_local x [dl_flist {*}[dict get $finfo xcoords]]
    dl_local y [dl_flist {*}[dict get $finfo ycoords]]

    set cos_theta [expr cos($bangle)]
    set sin_theta [expr sin($bangle)]

    dl_local rotated_x [dl_sub [dl_mult $x $cos_theta] [dl_mult $y $sin_theta]]
    dl_local rotated_y [dl_add [dl_mult $y $cos_theta] [dl_mult $x $sin_theta]]

    dl_local x [dl_add $bx $rotated_x]
    dl_local y [dl_add $by $rotated_y]

    lassign [deg_to_display $x $y] xlist ylist
    set coords [list]

    foreach a $xlist b $ylist {	lappend coords $a $b }
    
    $::display coords ${body}_${fixture} $coords
}

proc onContact { w a b } {
#    puts "{$a [box2d::getBodyInfo $w $a]} {$b [box2d::getBodyInfo $w $b]}"
}

proc onPreSolve { w a b x y v } {    
#    puts "$w $a $b $x $y $v"
    set radius 2
    lassign [deg_to_display $x $y] x0 y0
    $::display create oval \
	[expr $x0-$radius] [expr $y0-$radius] \
	[expr $x0+$radius] [expr $y0+$radius] \
	-outline white -fill cyan	
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

set display_width 600
set display_height 600
if { ![winfo exists .c] } {
    set display [canvas .c -width $display_width -height $display_height -background black]
}

pack $display
wm protocol . WM_DELETE_WINDOW { set ::done 1; exit }

proc run {} {
    global done world display bodyinfo

    $display delete all
    box2d::destroy all
    set bodyinfo [dict create]

    set done 0
    set world [make_stims 8]

    #box2d::setBeginContactCallback $world onContact
    box2d::setPreSolveCallback $world onPreSolve
    run_simulation $world
}

bind $display <Double-1> { set ::done 1; run }
bind . <space> {
    box2d::setBodyType $::world $::ball_body 2;
#    box2d::setBodyType $::world $::box_body 2
}
bind . <Key-q> { set ::done 1 }
bind . <Escape> { set ::done 1; exit }
bind . <Key-r> { source /Users/sheinb/tmp/testb2d.tcl }
run
