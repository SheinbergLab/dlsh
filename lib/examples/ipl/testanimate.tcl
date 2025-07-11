package require dlsh
dl_noOp
load ./tkimgraw
load_Ipl

proc setup {} {
    global oval
    if [winfo exists .c] { destroy .c }
    canvas .c -height 256 -width 256
    set img [image create photo -file \
	    g:/stimuli/scene/waldotargs/waldo_256.raw]
    .c create image 0 0 -image $img -anchor nw
    set oval [.c create oval 1 120 17 136]
    pack .c
}

proc move_oval { i } {
    global oval
    set i [expr $i%512]
    if { $i > 256 } { set i [expr 512-$i] }
    .c coords $oval $i 120 [expr $i+16] 136
}

proc animate {} {
    tkwait visibility .c
    for { set i 0 } { $i < 500 } { incr i } {
	move_oval $i
	update idletasks
    }
}

proc move_oval2 { i } {
    global oval
    set i [expr $i%1024]
    if { $i > 512 } { set i [expr 1024-$i] }
    .f.c coords $oval $i 200 [expr $i+16] 216
}

proc animate2 {} {
    for { set i 0 } { $i < 10000 } { incr i } {
	move_oval2 $i
	update idletasks
    }
}

proc stringtest {} {
    set f [open c:/stimuli/scene/physiology/rights/g0/239000.raw]
    fconfigure $f -translation binary
    set b [read $f 262164]

    if [winfo exists .c] { destroy .c }
    canvas .c
    set img [image create photo]
    rawToPhoto $img $b
    .c create image 0 0 -image $img -anchor nw
    pack .c
    
    close $f
}

proc ipltest {} {
    global oval
    set source \
	    [ipl readRaw c:/stimuli/scene/physiology/backgrounds/g0/28087.raw]
    set shrink_blur [ipl scale $source - .5 .5 LINEAR 3]
    ipl writeRawString $shrink_blur y

    if [winfo exists .c] { destroy .c }
    if [winfo exists .f] { destroy .f }
    frame .f -width 512 -height 384
    canvas .f.c -width 512 -height 384
    set img [image create photo -width 512 -height 384]
    rawToPhoto $img $y
    .f.c create image 0 0 -image $img -anchor nw
    set oval [.f.c create oval 1 120 17 136 -outline yellow -width 2]
    pack .f.c -fill both -expand true
    pack .f

    ipl destroyAll
    tkwait visibility .f
}

#setup
#animate

ipltest
animate2