#
# simple example to show how to build a box2d world
#
set dlshlib [file join /usr/local/dlsh dlsh.zip]
set base [file join [zipfs root] dlsh]
if { ![file exists $base] && [file exists $dlshlib] } {
    zipfs mount $dlshlib $base
}
set ::auto_path [linsert $::auto_path [set auto_path 0] $base/lib]

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
    set hrange_h $::display_hrange_h 
    return [expr $x*$w/(2*$hrange_h)]
}

proc deg_to_display { x y } {
    set w $::display_width
    set h $::display_height
    set hrange_h $::display_hrange_h
    set aspect [expr {1.0*$h/$w}]
    set hrange_v [expr $hrange_h*$aspect]
    set hw [expr $w/2]
    set hh [expr $h/2]
    dl_local x0 [dl_add [dl_mult [dl_div $x $hrange_h] $hw] $hw]
    dl_local y0 [dl_sub $hh [dl_mult [dl_div $y $hrange_v] $hh]]
    return [list [dl_tcllist $x0] [dl_tcllist $y0]]
}

proc create_ball_dg {} {
    global params
    set b2_staticBody 0

    set g [dg_create]

    dl_set $g:name [dl_slist ball]
    dl_set $g:shape [dl_slist Circle]
    dl_set $g:type $b2_staticBody
    dl_set $g:tx [dl_flist 0]
    dl_set $g:ty [dl_flist $params(ball_start)]
    dl_set $g:sx [dl_flist 0.5]
    dl_set $g:sy [dl_flist 0.5]
    dl_set $g:angle [dl_flist 0.0]
    dl_set $g:restitution [dl_flist 0.2]
    
    return $g
}

proc create_catcher_dg { tx name } {
    global params
    set b2_staticBody 0

    set catcher_y $params(catcher_y)
    set y [expr $catcher_y-(0.5+0.5/2)]

    set g [dg_create]

    dl_set $g:name [dl_slist ${name}_b ${name}_r ${name}_l]
    dl_set $g:shape [dl_repeat [dl_slist Box] 3]
    dl_set $g:type [dl_repeat $b2_staticBody 3]
    dl_set $g:tx [dl_flist $tx [expr {$tx+2.5}] [expr {$tx-2.5}]]
    dl_set $g:ty [dl_flist $y $catcher_y $catcher_y]
    dl_set $g:sx [dl_flist 5 0.5 0.5]
    dl_set $g:sy [dl_flist 0.5 2 2]
    dl_set $g:angle [dl_zeros 3.]
    dl_set $g:restitution [dl_zeros 3.]
    
    return $g
}

proc create_plank_dg {}  {
    global params
    set b2_staticBody 0

    set n $params(nplanks)
    set xrange $params(xrange)
    set xrange_2 [expr {$xrange/2}]
    set yrange $params(yrange)
    set yrange_2 [expr {$yrange/2}]

    set g [dg_create]

    dl_set $g:name [dl_paste [dl_repeat [dl_slist plank] $n] [dl_fromto 0 $n]]
    dl_set $g:shape [dl_repeat [dl_slist Box] $n]
    dl_set $g:type [dl_repeat $b2_staticBody $n]
    dl_set $g:tx [dl_sub [dl_mult $xrange [dl_urand $n]] $xrange_2]
    dl_set $g:ty [dl_sub [dl_mult $yrange [dl_urand $n]] $yrange_2]
    dl_set $g:sx [dl_repeat 3. $n]
    dl_set $g:sy [dl_repeat .5 $n]
    dl_set $g:angle [dl_mult 2 $::pi [dl_urand $n]]
    dl_set $g:restitution [dl_zeros $n.]
    
    return $g
}

proc make_world {} {
    set planks [create_plank_dg]
    set left_catcher [create_catcher_dg -3 catchl]
    set right_catcher [create_catcher_dg 3 catchr]
    set ball [create_ball_dg]

    foreach p "$ball $left_catcher $right_catcher" {
	dg_append $planks $p
	dg_delete $p
    }
    
    return $planks
}

proc build_world { g } {
    
    # create the world
    set world [box2d::createWorld]

    set ::planks     {}
    set ::plank_ids  {}
    set ::plank_dims {}

    set dg [make_world]
    set n [dl_length $dg:name]
    
    # load in objects
    for { set i 0 } { $i < $n } { incr i } {
	foreach v "name shape type tx ty sx sy angle restitution" {
	    set $v [dl_get $dg:$v $i]
	}

	if { $shape == "Box" } {
	    set body [box2d::createBox $world $name $type $tx $ty $sx $sy $angle]
	    set id [show_box $name $tx $ty $sx $sy $angle]
	} elseif { $shape == "Circle" } {
	    set body [box2d::createCircle $world $name $type $tx $ty $sx]
	    set id [show_ball $tx $ty $sx]
	}
	box2d::setRestitution $world $body $restitution

	# ball handles are returned
	if { $name == "ball" } {
	    set ball $body
	    set ball_id $id
	}

	if { [string match plank* $name] } {
	    lappend ::planks $body
	    lappend ::plank_ids $id
	    lappend ::plank_dims [list $sx $sy]
	}
    }
  
    # create a dynamic circle
    return "$world $ball $ball_id"
}

proc test_simulation { { simtime 6 } } {
    global params world ball ball_id
    box2d::setBodyType $world $ball 2    
    set step 0.0167
    set nsteps [expr {int(ceil(6.0/$step))}]
    set contacts {}
    for { set t 0 } { $t < $simtime } { set t [expr $t+$step] } {
	box2d::step $world $step
        if { [box2d::getContactBeginEventCount $world] } { 
	    lappend contacts [box2d::getContactBeginEvents $world]
        }
    }
    lassign [box2d::getBodyInfo $world $ball] tx ty a
    box2d::setTransform $world $ball 0 $params(ball_start)
    box2d::setBodyType $world $ball 0
    return  "$tx $ty [list $contacts]"
}

proc run_simulation {} {
    global world ball ball_id
    while !$::done { 
    	after 20
    	box2d::step $world .0167

	foreach p $::planks id $::plank_ids dims $::plank_dims {
	    lassign [box2d::getBodyInfo $world $p] tx ty a
	    update_box $id $tx $ty {*}$dims $a
	}
	
    	lassign [box2d::getBodyInfo $world $ball] tx ty a
    	update_ball $ball_id .5 $tx $ty $a
    	update
    }
}

proc isPlank { pair } { return [string match plank* [lindex $pair 0]] }

proc uniqueList {list} {
  set new {}
  foreach item $list {
    if {$item ni $new} {
      lappend new $item
    }
  }
  return $new
}

proc accept_board { x y contacts } {
    global params
    set catcher_y $params(catcher_y)

    set upper [expr $catcher_y+0.01]
    set lower [expr $catcher_y-0.01]

    if { [expr {$y < $upper && $y > $lower}] } {
	set result [expr {$x>0}]
    } else {
	return "-1 0"
    }

    set planks [lmap c $contacts \
		    { expr { [isPlank $c] ? [lindex [lindex $c 0] 0] : [continue] } }]
    set planks [uniqueList $planks]
    set nhit [llength $planks]
    if { $nhit < $params(minplanks) } { return -1 }
	
    return "$result $nhit"
}

proc run { n accept_proc } {
    global done world display 

    set done 0
    while { !$done } {
	$display delete all
	
	box2d::destroy all

	set new_world [make_world]
	
	lassign [build_world $new_world] ::world ::ball ::ball_id
	lassign [test_simulation] x y contacts
	lassign [$accept_proc $x $y $contacts] result nhit
	dg_delete $new_world
	
	if { $result != -1 } {
	    set done 1
	    puts "$result ($nhit)"
	}
    }

    update
    
    set done 0
    run_simulation
}

set params(xrange) 16
set params(yrange) 12
set params(catcher_y) -7.5
set params(ball_start) 8.0
set params(minplanks) 1
set params(nplanks) 10

set display_width 1024
set display_height 600
set display_hrange_h 17.7
set display [canvas .c -width $display_width -height $display_height -background black]
pack $display
wm protocol . WM_DELETE_WINDOW { set ::done 1; exit }

bind . <Up> { set ::done 1; run 10 accept_board }
bind . <Down> { box2d::setBodyType $::world $::ball 2 }
bind . <Escape> { set ::done 1; exit }
run 10 accept_board



