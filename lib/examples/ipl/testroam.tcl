#
#  Tests for the IPL (Intel's Image Processing Library) tcl package
#
package require dlsh
dl_noOp
load_Ipl
package require Tkimgraw
load_Cgwin

proc setup {} {
    global the_img background
    frame .f -width 512 -height 512
    canvas .f.c -background black -width 256 -height 256
    set the_img [image create photo]
    .f.c create image 128 128 -image $the_img -anchor center
    cgwin .f.cg -width 256 -height 256 -background black
    pack .f.c .f.cg -side left

    frame .buttons
    button .buttons.roam1 -text "Canvas Roam" -command "roam" -width 10
    button .buttons.roam2 -text "Cgraph Roam" -command "roam2" -width 10
    pack .buttons.roam1 .buttons.roam2 -side left

    pack .f .buttons

    # Read source and background images
    set background \
	[ipl readRaw l:/stimuli/scene/physiology/backgrounds/g0/28087.raw]

    bind . <Control-q> { exit } 
    bind . <Control-r> { roam } 
    bind . <Control-h> { console show } 
}

proc roam {} {
    global background the_img
    ipl setROI $background 0 275 400 128 128
    set subimg [ipl copy $background -]
    for { set i 0 } {$i < 30 } { incr i } {
	if { $i } {
	    ipl setROI $background 0 [expr 275+$i*3] 400 128 128
	    ipl copy $background $subimg
	}
	dl_local c [ipl toDynlist $subimg]
	dl_char $c imgdata 
	rawToPhoto $the_img $imgdata
	update idletasks
    }
    ipl destroy $subimg
}



proc roam2 {} {
    global background
    ipl setROI $background 0 275 400 128 128
    set subimg [ipl copy $background -]

    # Now show
    setwindow 0 0 10 10
    set id [ipl placeCentered $subimg 5 5 5]

    for { set i 0 } {$i < 30 } { incr i } {
	ipl setROI $background 0 [expr 275+$i*3] 400 128 128
	ipl copy $background $subimg
	ipl replace $id $subimg
	update idletasks
    }
    ipl destroy $subimg
}


setup

