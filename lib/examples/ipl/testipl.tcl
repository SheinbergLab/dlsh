#
#  Tests for the IPL (Intel's Image Processing Library) tcl package
#

load_Ipl

proc testscale {} {
    set source [ipl readRaw monk4.raw]
    set zoom [ipl scale $source - 2 2]
    set shrink [ipl scale $source - .5 .5]
    set shrink_blur [ipl scale $source - .5 .5 LINEAR 3]
    
    # Now show
    setwindow 0 0 10 10
    ipl placeCentered $source 5 8 2
    ipl placeCentered $zoom 2 4 2
    ipl placeCentered $shrink 5 4 2
    ipl placeCentered $shrink_blur 8 4 2

    # Clean up
    ipl destroyAll
}

proc testcontrast {} {
    set source1 [ipl readRaw monk4.raw]
    set source2 [ipl readRaw waldo_256.raw]
    set lowcontrast1 [ipl scalePixels $source1 - .2]
    set lowcontrast2 [ipl scalePixels $source2 - .2 1 .3 1]
    
    # Now show
    setwindow 0 0 8 8
    ipl placeCentered $source1 1 4 2
    ipl placeCentered $source2 3 4 2
    ipl placeCentered $lowcontrast1 5 4 2
    ipl placeCentered $lowcontrast2 7 4 2

    # Clean up
    ipl destroyAll
}

proc testroi {} {
    set source [ipl readRaw monk4.raw]
    ipl setROI $source 1 20 20 128 128
    set shrink_blur1 [ipl scale $source - .5 .5 LINEAR 3]
    ipl deleteROI $source
    set shrink_blur2 [ipl scale $source - .5 .5 LINEAR 3]

    # Now show
    setwindow 0 0 10 10
    ipl placeCentered $source 2 5 2
    ipl placeCentered $shrink_blur1 5 5 2
    ipl placeCentered $shrink_blur2 8 5 2

    # Clean up
    ipl destroyAll
}


proc testcopy {} {
    set source [ipl readRaw waldo_256.raw]
    set copy1 [ipl copy $source -]
    ipl setROI $source 0 40 40 164 164
    set copy2 [ipl copy $source -]
    ipl setROI $source 2 64 64 64 64
    set copy3 [ipl copy $source -]
    
    # Now show
    setwindow 0 0 10 10
    ipl placeCentered $source 2 4 1.8
    ipl placeCentered $copy1 4 4 1.8
    ipl placeCentered $copy2 6 4 1.8
    ipl placeCentered $copy3 8 4 1.8

    # Clean up
    ipl destroyAll
}

proc testmirror {} {
    set source [ipl readRaw waldo_256.raw]
    set copy1 [ipl copy $source -]
    set copy2 [ipl copy $source -]
    set copy3 [ipl copy $source -]
    ipl mirror $copy1 $copy1 0
    ipl mirror $copy2 $copy2 1
    ipl mirror $copy3 $copy3 -1

    # Now show
    setwindow 0 0 10 10
    ipl placeCentered $source 2 4 1.8
    ipl placeCentered $copy1 4 4 1.8
    ipl placeCentered $copy2 6 4 1.8
    ipl placeCentered $copy3 8 4 1.8

    # Clean up
    ipl destroyAll
}


proc testcomposite {} {
    # Read source and background images
    set target [ipl readRaw waldo_256.raw]
    set background \
	    [ipl readRaw g:/stimuli/scene/physiology/backgrounds/g0/28087.raw]
    ipl setROI $background 0 275 400 256 256
    
    # Pull out the alpha channel
    ipl setROI $target 4 0 0 256 256
    set alpha [ipl copy $target -]
    ipl deleteROI $target

    ipl subtractS $alpha $alpha 128

    # Composite into the background
    ipl alphaComposite $target $background $alpha - $background PLUS

    # Remove the ROI and scale down
    ipl deleteROI $background
    set scaled [ipl scale $background - .5 .5]

    # Now show
    setwindow 0 0 10 10
    ipl placeCentered $scaled 5 5 7
 
    # Clean up
    ipl destroyAll
}


proc testreplace {} {
    # Read source and background images
    set background \
	    [ipl readRaw g:/stimuli/scene/physiology/backgrounds/g0/28087.raw]
    ipl setROI $background 0 275 400 256 256
    set subimg [ipl copy $background -]

    # Now show
    setwindow 0 0 10 10
    set id [ipl placeCentered $subimg 5 5 2]

    for { set i 0 } {$i < 30 } { incr i } {
	ipl setROI $background 0 [expr 275+$i*3] 400 256 256
	ipl copy $background $subimg
	ipl replace $id $subimg
	update idletasks
#	after 20
    }
 
    # Clean up
    ipl destroyAll
}

proc testtarga {} {
    # Read source and background images
    set target [ipl readTarga teddybear00.tga]
    set background [ipl readRaw 28087.raw]
    ipl setROI $background 0 740 160 256 256
    
    # Pull out the alpha channel
    ipl setROI $target 4 0 0 256 256
    set alpha [ipl copy $target -]
    ipl deleteROI $target

    ipl subtractS $alpha $alpha 128

    # Composite into the background
    ipl alphaComposite $target $background $alpha - $background PLUS

    # Remove the ROI and scale down
    ipl deleteROI $background
    set scaled [ipl scale $background - .5 .5]

    # Now show
    setwindow 0 0 10 10
    ipl placeCentered $scaled 5 5 7
 
    # Clean up
    ipl destroyAll
}
