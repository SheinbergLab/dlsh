package require box2d

proc make_stims { { nboxes 2 } { ncircles 2 } } {
    global circle
    set b2_staticBody 0
    set b2_kinematicBody 1
    set b2_dynamicBody 2
    
    set bworld [box2d::create]

    # Create a "catcher" by attaching 5 boxes to a static body
    set catcher1 [create_catcher $bworld -4 0 0]
    set catcher2 [create_catcher $bworld 4 0 0]

    for { set i 0 } { $i < $nboxes } { incr i } {
	set tx [expr rand()*12-6]
	set ty [expr rand()*6+3]
	set scale [expr 1.5*rand()+1]
	set box [create_body $bworld $b2_staticBody $tx $ty 0]
	set angle [expr rand()*180]
	add_box $bworld $box 0 0 $scale .5 $angle
    }

    for { set i 0 } { $i < $ncircles } { incr i } {
	set tx [expr rand()*10-5]
	set ty [expr rand()*3+9]
	set radius .5
	set circle [create_body $bworld $b2_dynamicBody $tx $ty 0]
	add_circle $bworld $circle 0 0 $radius
    }
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
    show_box $body [expr $bx+$tx] [expr $by+$ty] 0 $sx $sy 1 [expr -1*$angle] 0 0 1
}


proc add_circle { bworld body tx ty radius } {
    box2d::createCircleFixture $bworld $body $tx $ty $radius
    lassign [box2d::getBodyInfo $bworld $body] bx by
    show_sphere [expr $bx+$tx] [expr $by+$ty] 0 $radius $radius $radius $::colors(white)
}

proc run_simulation { world } {
    global circle
    for { set i 0 } { $i < 200 } { incr i } {
	box2d::update $world .02
	lassign [box2d::getBodyInfo $world $circle] tx ty
	dlg_markers $tx $ty circle -size 1. -scaletype x -color $::colors(cyan)
	flushwin
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

    dlg_lines $x $y 
    #dlg_text $tx $ty $name -color $::colors(red)
}

proc show_sphere { tx ty tz sx sy sz color } {
    dlg_markers $tx $ty fcircle -size [expr $sx*2.] -scaletype x -color $color
}

proc onContact { w a b } {
    puts "{$a [box2d::getBodyInfo $w $a]} {$b [box2d::getBodyInfo $w $b]}"
}

proc onPreSolve { w a b x y v } {
    puts "$w $a $b $x $y $v"
}

box2d::destroy all
clearwin
setwindow -10 -5 10 14
set world [make_stims 7 1]
flushwin

#box2d::setBeginContactCallback $world onContact
box2d::setPreSolveCallback $world onPreSolve
run_simulation $world
