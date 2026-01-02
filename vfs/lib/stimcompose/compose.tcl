package provide stimCompose 1.45

namespace eval ::stimCompose {
    package require impro 
    set total_elements 64
    set total_textures 124
    set n_elements 4
    set coverage .25
    set max_displacement 96
    set noise_displacement 32

    proc load_alphas { args } {
	set g [dg_create]
	dl_set $g:vals [dl_llist]
	dl_set $g:widths [dl_ilist]
	dl_set $g:heights [dl_ilist]
	dl_set $g:tx [dl_ilist]
	dl_set $g:ty [dl_ilist]
	
	if { [file exists c:/stimuli/composition] } {
	    set objdir c:/stimuli/composition/elements
	} else {
	    set objdir l:/stimuli/composition/elements
	}
	foreach i $args {
	    set f [format "%s/element_%04d.tga" $objdir $i]
	    set info [img_info $f]
	    dl_local pixels [img_read $f]
	    dl_append $g:vals [dl_int [dl_select $pixels "0 0 0 1"]]
	    set w [lindex $info 0]
	    set h [lindex $info 1]
	    dl_append $g:widths $w
	    dl_append $g:heights $h
	    dl_append $g:tx [expr (256-$w)/2]
	    dl_append $g:ty [expr (256-$h)/2]
	}
	return $g
    }
    
    proc compose { g } {
	set img [img_create]
	img_compose $img  $g:tx $g:ty $g:vals $g:widths $g:heights 
	return $img
    }
    
    proc add_texture { textureid alphaimg } {
	if { [file exists c:/stimuli/composition] } {
	    set objdir c:/stimuli/composition/textures
	} else {
	    set objdir l:/stimuli/composition/textures
	}
	set f [format "%s/texture_%04d.tga" $objdir $textureid]
	set img [img_loadTarga $f]
	set result [img_addAlpha $img $alphaimg]
	img_delete $img
	return $result
    }
    
    proc make_object { elements tx ty texture { scale 1.0 } { premult 1 } } {
	set g [eval load_alphas $elements]
	dl_set $g:tx [dl_add $g:tx $tx]
	dl_set $g:ty [dl_add $g:ty $ty]
	set alpha [compose $g]
	dg_delete $g

	# rescale alpha channel and center
	set scaled_alpha [img_rescale $alpha -$scale]
	img_delete $alpha

	# now add alpha to full texture image
	set resultimage [add_texture $texture $scaled_alpha]

	if { $premult } { img_premultiplyAlpha $resultimage 128 128 128 }
	dl_local result [img_img2list $resultimage]
	img_delete $scaled_alpha $resultimage
	dl_return $result
    }

    proc make_silhouette_from_dg { g id } {
	make_silhouette \
	    [dl_tcllist $g:elements:$id] \
	    [dl_tcllist $g:xoffsets:$id] \
	    [dl_tcllist $g:yoffsets:$id] \
	    [dl_tcllist $g:textures:$id] \
	    [dl_tcllist $g:scales:$id]
    }

    proc make_silhouette { elements tx ty texture scale } {
	set g [eval load_alphas $elements]
	dl_set $g:tx [dl_add $g:tx $tx]
	dl_set $g:ty [dl_add $g:ty $ty]
	set alpha [compose $g]
	set scaled [img_rescale $alpha -$scale]
	img_delete $alpha
	set alpha $scaled
	dg_delete $g
	return $alpha
    }

    proc get_scale_factor { g id target } {
	set alpha [make_silhouette_from_dg $g $id]
	set area [img_alphaArea $alpha]
	set scale [expr sqrt($target/$area)]
	img_delete $alpha
	return $scale
    }

    proc set_scale_factors { g { target .2 } } {
	dl_local scales [dl_llist]
	dl_set $g:scales [dl_pack [dl_ones [dl_length $g:scales].]]
	for { set i 0 } { $i < [dl_length $g:scales] } { incr i } { 
	    set scale [get_scale_factor $g $i $target]
	    dl_append $scales [dl_flist $scale]
	}
	dl_set $g:scales $scales
	return $g
    }


    proc random_object { { premult 0 } } {
	set nelements $::stimCompose::total_elements
	set ntextures $::stimCompose::total_textures
	set n $::stimCompose::n_elements
	set maxdisp $::stimCompose::max_displacement
	
	set els [dl_tcllist \
		     [dl_randchoose $nelements $n]]
	set xoffs [dl_tcllist \
		       [dl_sub [dl_irand $n $maxdisp] [expr $maxdisp/2]]]
	set yoffs [dl_tcllist \
		       [dl_sub [dl_irand $n $maxdisp] [expr $maxdisp/2]]]
	set texture [dl_tcllist \
			 [dl_randchoose $ntextures 1]]
	set scale 1.0
	
	dl_return [make_object $els $xoffs $yoffs $texture $scale $premult]
    }

    proc random_family { members } {
	set nelements $::stimCompose::total_elements
	set ntextures $::stimCompose::total_textures
	set n $::stimCompose::n_elements
	set maxdisp $::stimCompose::max_displacement
	set noisedisp $::stimCompose::noise_displacement
	
	dl_local els [dl_randchoose $nelements $n]
	dl_local xoffs [dl_sub [dl_irand $n $maxdisp] [expr $maxdisp/2]]
	dl_local yoffs [dl_sub [dl_irand $n $maxdisp] [expr $maxdisp/2]]
	dl_local texture [dl_randchoose $ntextures 1]
	dl_local scale [dl_flist 1.]
	
	set g [dg_create]
	dl_set $g:elements [dl_llist $els]
	dl_set $g:xoffsets [dl_llist $xoffs]
	dl_set $g:yoffsets [dl_llist $yoffs]
	dl_set $g:textures [dl_llist $texture]
	dl_set $g:scales   [dl_llist $scale]
	
	for { set i 1 } { $i < $members } { incr i } {
	    dl_append $g:elements $els
	    dl_append $g:textures $texture
	    dl_local randoff [dl_sub [dl_irand $n $noisedisp] \
				  [expr $noisedisp/2]]
	    dl_append $g:xoffsets [dl_add $xoffs $randoff]
	    dl_local randoff [dl_sub [dl_irand $n $noisedisp] \
				  [expr $noisedisp/2]]
	    dl_append $g:yoffsets [dl_add $yoffs $randoff]
	    dl_append $g:scales 1.0
	}
	return $g
    }

    proc morph_objects { g n } {
	set out [dg_choose $g 0]
	dl_local x0 $g:xoffsets:0
	dl_local x1 $g:xoffsets:1
	dl_local y0 $g:yoffsets:0
	dl_local y1 $g:yoffsets:1

	dl_local xstep [dl_div [dl_sub $x1 $x0] [expr $n+1.]]
	dl_local ystep [dl_div [dl_sub $y1 $y0] [expr $n+1.]]

	for { set i 1 } { $i < [expr $n+2] } { incr i } {
	    dl_append $out:elements $g:elements:0
	    dl_append $out:xoffsets \
		[dl_int [dl_add $g:xoffsets:0 [dl_mult $i $xstep]]]
	    dl_append $out:yoffsets \
		[dl_int [dl_add $g:yoffsets:0 [dl_mult $i $ystep]]]
	    dl_append $out:textures $g:textures:0
	    dl_append $out:scales 1.0
	}
	return $out
    }

    proc make_related_family { in } {
	set g [dg_copy $in]
	set n $::stimCompose::n_elements
	set maxdisp $::stimCompose::max_displacement
	set noisedisp $::stimCompose::noise_displacement
	
	dl_reset $g:xoffsets
	dl_reset $g:yoffsets
	
	dl_local xoffs [dl_sub [dl_irand $n $maxdisp] [expr $maxdisp/2]]
	dl_local yoffs [dl_sub [dl_irand $n $maxdisp] [expr $maxdisp/2]]
	
	for { set i 0 } { $i < [dl_length $g:elements] } { incr i } {
	    dl_local randoff [dl_sub [dl_irand $n $noisedisp] \
				  [expr $noisedisp/2]]
	    dl_append $g:xoffsets [dl_add $xoffs $randoff]
	    dl_local randoff [dl_sub [dl_irand $n $noisedisp] \
				  [expr $noisedisp/2]]
	    dl_append $g:yoffsets [dl_add $yoffs $randoff]
	}
	return $g
    }
    
    
    proc show_object { pixels } {
	setwindow -.65 -.5 .65 .5
	dlg_image 0 0 [dl_llist "256 256" $pixels] .95 -center
    }
    
    proc show_family_member { g id } {
	show_object [make_object \
			 [dl_tcllist $g:elements:$id] \
			 [dl_tcllist $g:xoffsets:$id] \
			 [dl_tcllist $g:yoffsets:$id] \
			 [dl_tcllist $g:textures:$id] \
			 [dl_tcllist $g:scales:$id] \
			 1]
    }

    proc save_family_member_as_tga { g id filename } {
	dl_local pixels [make_object \
			     [dl_tcllist $g:elements:$id] \
			     [dl_tcllist $g:xoffsets:$id] \
			     [dl_tcllist $g:yoffsets:$id] \
			     [dl_tcllist $g:textures:$id] \
			     [dl_tcllist $g:scales:$id] \
			     1]
	img_writeTarga $pixels 256 256 $filename
    }
    
    proc show_family { g { onerow 0 } } {
	set nobjects [dl_length $g:elements]
	if { $onerow } { 
	    dlp_setpanels 1 $nobjects
	} else {
	    if { $nobjects <= 4 } {
		dlp_setpanels 2 2
	    } elseif { $nobjects <= 8 } {
		dlp_setpanels 2 4
	    } else {
		set nrows [expr ($nobjects/8)+1]
		dlp_setpanels $nrows [expr ($nobjects+$nrows-1)/$nrows]
	    }
	}
	for { set i 0 } { $i < $nobjects } { incr i } {
	    dlp_pushpanel $i
	    show_family_member $g $i
	    dlp_poppanel
	}
	
    }
    
    proc show_multiple_families { args } {
	dlp_pop all
	set n [llength $args]
	for { set i 0 } { $i < $n } { incr i } {
	    setfviewport 0 [expr $i*[expr 1./$n]] 1 [expr ($i+1.)/$n]
	    show_family [lindex $args $i] 1
	}
    }

    proc family_test { { n 8 } } {
	set g [random_family $n]
	set g1 [make_related_family $g]
	set g2 [make_related_family $g]
	set g3 [make_related_family $g]
	set g4 [make_related_family $g]
	show_multiple_families $g $g1 $g2 $g3 $g4
	dg_delete $g $g1 $g2 $g3 $g4
    }
}
