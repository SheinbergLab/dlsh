#
# NAME
#    testimg.tcl
#   
# DESCRIPTION
#    Show how to embed images into plots within the coordinate system of
# the plot
#
package require dlsh
load_Cgwin
load_Impro

proc make_noise { { noise_size 16 } } {
    set n [expr $noise_size*$noise_size]
    dl_local noise_r [dl_char [dl_irand $n 256]]
    dl_local noise_g [dl_char [dl_irand $n 256]]
    dl_local noise_b [dl_char [dl_irand $n 256]]
    dl_local npix [dl_unpack [dl_combineLists $noise_r $noise_g $noise_b]]
    dl_return [dl_llist "$noise_size $noise_size" $npix]
}

proc show_noise { x y scale } {
    dl_local noiseimg [make_noise 16]
    dlg_image $x $y $noiseimg $scale -center
}

proc load_image { filename } {
    scan [img_info $filename] "%d %d %d %d" w h d header
    dl_local pix [img_read $filename]
    dl_return [dl_llist "$w $h" $pix]
}

proc makeplot {} {
    set p [dlp_newplot]
    dlp_setxrange $p -5 5
    dlp_set $p adjustxy 1;		# make aspect ratio 1

    # These create image and plot at the time the plot is drawn
    dlp_cmd $p "show_noise -1 -1 1.5"
    dlp_cmd $p "show_noise 1 1 1.5"
    
    # This loads the image data and puts into the plot (using %p shorthand)
    dlp_set $p img1 [load_image c:/stimuli/search/bag00.tga]
    dlp_set $p img2 [load_image c:/stimuli/search/atomizer00.tga]
    dlp_cmd $p "dlg_image 1 -1 %p:img1 1.5 -center"
    dlp_cmd $p "dlg_image -1 1 %p:img2 1.5 -center"
    return $p
}

clearwin
pack [cgwin .plot]
dlp_plot [makeplot]
