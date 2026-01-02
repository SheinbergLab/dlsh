package require dlsh
dl_noOp
load_Cgwin
cgwin .cg -width 200 -height 200
pack .cg

load_Impro

# Create a dynamic group to hold the movie and size info
dg_create movie

# Create frames of drifting grating and append each image onto movie:frames
proc load_gratings {} {
    set nframes 32    
    set h 100
    dl_set movie:frames [dl_llist]
    dl_local t [dl_div [dl_fromto 0 100] 5.]
    dl_set movie:dims "[dl_length $t] $h"
    dl_set movie:nframes $nframes
    for { set i 0 } { $i < $nframes } { incr i } {
	set phase [expr $i*((2.*$::pi)/$nframes)]
	dl_pushTemps
	dl_append movie:frames [dl_char [dl_replicate \
		[dl_add [dl_mult [dl_sin [dl_add $t $phase]] 127] 128] $h]]
	dl_popTemps
    }
}

proc load_images { imglist } {
    dl_set movie:frames [img_readraws $imglist]
    dl_set movie:dims "256 256"
    dl_set movie:nframes [dl_length movie:frames]
}

# Start the first frame by hand, and then replace with successive frames
proc show_movie {} {
    setwindow 0 0 10 10
    set w [dl_get movie:dims 0] 
    set h [dl_get movie:dims 1]
    set nframes [dl_first movie:nframes]
    set info [dlg_image 5 5 movie:frames:0 6 -width $w -height $h -center]
    set id [lindex $info 0]
    update
    for { set i 1 } { $i < 64 } { incr i } {
	dlg_replaceImage $id movie:frames:[expr $i%$nframes] \
		-width $w -height $h
	update
    }
}


# load_gratings
load_images [glob g:/stimuli/scene/monkeys/?????.raw]
show_movie
bind . <Button-1> show_movie
bind . <Control-c> { console show }
