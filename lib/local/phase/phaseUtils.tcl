package provide phase 0.9

namespace eval ::pu {
    #set of utility functions to use for the phase scrambling procedures

    proc init { } {
	#initializes an episilon and loads the package necessary for img_read and the dl_fft2D and dl_ifft2D
	load_Impro
	load_Stats
	set ::eps 0.00000000001
    }

    proc get_img_alpha {imgname path ext} {
	#we don't phase scramble the alpha channel, so this is to get back this channel separate
	#from all the others.  We do assume that there are four channels.
	set fn [file join ${path}/ ${imgname}.$ext]
	dl_local ia [img_read $fn]
	dl_local iaf [dl_div [dl_float $ia] 255.0]
	dl_local iafmat [dl_transpose [dl_reshape $iaf [expr [dl_length $iaf]/4.0] 4]]
	dl_return $iafmat:3
    }

  proc get_amp_phase_gray {imgname path ext} {
	#given the name of the image path and extension, e.g. doll00 l:/stimuli/search tga
	#it will read in and then return the modulus and phase of the two dimensional fourier
	#transform of the image.
	set fn [file join ${path}/ ${imgname}.$ext]
	dl_local ib [img_read $fn];#function in load_Impro -gives us one big long list of values
	dl_local ibf [dl_div [dl_float $ib] 255.0];#want to turn them into floats that range between zero and one.
	dl_local ibfmat [dl_transpose [dl_reshape $ibf [expr [dl_length $ibf]/4.0] 4]];
	dl_local ibfmat_gray [dl_add [dl_mult [dl_get $ibfmat 0] 0.3] [dl_mult [dl_get $ibfmat 1] 0.59] [dl_mult [dl_get $ibfmat 2] 0.11]]
	dl_local temp [dl_fft2D [dl_interleave $ibfmat_gray 0.0]]
	dl_local mod [::pu::computeabs $temp]
	dl_local ang [::pu::computephase $temp]
	dl_return [dl_llist $mod $ang]
    }

    proc computeabs {inlist} {
	dl_local r [dl_select $inlist "1 0"];#this uses a trick to get every other value starting with the first.
	#these are the real values from the results of the fft2D
	dl_local c [dl_select $inlist "0 1"];#same thing to get the imag component
	dl_return [dl_sqrt [dl_add [dl_pow $r 2] [dl_pow $c 2]]];#now square, add and then square root for modulus aka magnitude
    }
    
    proc computephase {inlist} {
	#this is a little tricky because you have to take into account which quadrant of the 
	#complex plane you are in. We want all between -pi and pi
	dl_local r [dl_select $inlist "1 0"]
	dl_local c [dl_select $inlist "0 1"]
	#this will correctly take care of all first quadrant values (both positive)
	#and all fourth quadrant values (real pos and complex negative)
	dl_local angle [dl_atan [dl_div $c [dl_add $r $::eps]]];#don't want to divide by zero
	#now correct those quadrants where both are negative
	#Where are both negative
	dl_local bothneg [dl_mult [dl_lt $r 0] [dl_lt $c 0]]
	dl_local xnegypos [dl_mult [dl_lt $r 0] [dl_gt $c 0]]	  
	dl_local angle [dl_sub $angle [dl_mult $bothneg $::pi]]
	#now correct for those values in second quadrant, real neg, complex pos
	dl_local angle [dl_add $angle [dl_mult $xnegypos  $::pi]]
	dl_return $angle
    }

    proc mixPhases {orig rand proporig {doright 1} } {
	#logic here is to remember that, for a unit circle on the complex plane the sine and cosines of the angle
	#give us the y and x values.  So we use the phase angles for each frequency to compute the x's and y's
	#and then weight them.  We add eps to avoid a divide by zero problem
	#once we have the weighted x and y vectors, uniformly distributed across the -pi to pi range we can
	#use the arc tangent to get our new angle. You can see this a little more clearly if you draw
	#things out on a circle with pencil and paper.
	if $doright {
	    dl_local xs [dl_add [dl_mult [dl_cos $orig] $proporig] [dl_mult [dl_cos $rand] [expr 1 - $proporig]]]
	    dl_local ys [dl_add [dl_mult [dl_sin $orig] $proporig] [dl_mult [dl_sin $rand] [expr 1 - $proporig]]]
	    dl_return [::pu::computephase [dl_interleave $xs $ys]]
	} else {
	#this is a strict linear interpolation scheme which shows the over representation
	#of the zero values around 0.5 mix
	    dl_local rand [dl_add $rand [expr 2 * $::pi]]
	    dl_local temp [dl_add [dl_mult $orig $proporig] [dl_mult $rand [expr 1 - $proporig]]]
	    dl_return [dl_add $temp [dl_mult [dl_gte $temp $::pi] [expr -1 * 2 * $::pi]]]
	}
    }

    proc makeRandVec { len } {
	dl_local tempmat [dl_mult [dl_sub [dl_urand $len] 0.5] [expr 2 * $::pi]]
	#We want a conjugate symmetric matrix, 
	set sl [dl_tcllist [dl_int [dl_sqrt [dl_length $tempmat]]]]
	dl_local rowid [dl_div [dl_fromto 0 $len] $sl]
	dl_local colid [dl_sub [dl_fromto 0 $len] [dl_mult [dl_div [dl_fromto 0 $len] $sl] $sl]]
	for {set i 0} {$i < $len} {incr i} {
	    set nr [expr $sl - [dl_get $rowid $i]]
	    set nc [expr $sl - [dl_get $colid $i]]
	    if {$nr == $sl} {set nr 0}
	    if {$nc == $sl} {set nc 0}
	    dl_put $tempmat [::pu::convert2listid $nr $nc $sl] [expr -1 * [dl_get $tempmat $i]]
	}
	dl_return $tempmat
    }

     proc convert2matrixid {pos sqlen} {
	set row [expr int($pos / $sqlen)]
	set col [expr $pos - ($row * $sqlen)]
	return "$row $col"
    }
	
    proc convert2listid {row col sqlen} {
	return [expr $row * $sqlen + $col]
    }
    
    proc recreateImage_gray {id} {
	#This function was the real pain.  First we get from the stimspec the modulus and mixed phase vectors
	#Then we have to turn these back into the x + i y form that the ifft wants.  Recall that
	#e^i*theta = cos(theta) + i sin(theta).  dl_ifft2D expects the real and imag components alternated
	#hence the interleave.  dl_ifft2D is unnormalized so we divide by the number of components (actually =
	#to the square of each dimension. We don't want to divide the alpha channel though because it didn't
	#undergo an fft.  Lastly, we put them altogether again and transpose, ... to get them back into a form
	#that imgcreate recognizes. Couldn't have done this without Jed's groundbreaking work in noise protocol.
	set ap [dl_tcllist stimdg:amt_phase:$id]
	set ssluidx [dl_tcllist stimdg:ssluidx:$id]
	dl_local fm [dl_get stimspec:modphase:$ssluidx 0]
	dl_local tempphase [dl_get stimspec:modphase:$ssluidx 1]
	dl_local randphasevec [dl_get stimdg:lu_rand_phase_vec:0 0];#using same randomvec for each image and
	dl_return [::pu::getBackImage $fm $tempphase $randphasevec $ap]
    }


    proc recreateImageDynamic {id trajnum } {
	#basically the same as above but designed to work with slightly different stimdg configuration
	#of classify/phase where we have multiple images per trial to show with different phase vectors
	#id refers to the trial of the stimdg and trajnum is which of the random vectors for that trial
	#we are working with 


	set ap [dl_tcllist [dl_get stimdg:phasetrajectory:$id $trajnum]]
	set ssluidx [dl_tcllist stimdg:ssluidx:$id]
	dl_local fm [dl_get stimspec:modphase:$ssluidx 0]
	dl_local tempphase [dl_get stimspec:modphase:$ssluidx 1]
	dl_local randphasevec stimdg:lu_rand_phase_vec:$id;#using same randomvec for each image and
	dl_return [::pu::getBackImage $fm $tempphase $randphasevec $ap]

    }
    
    proc phaseDemo {imgin {ap 0.5} {equalize 1} {histmode 0} {show 1} {doright 1} {save_name "none"}}  {
	# %HELP%
	# ::pu::phaseDemo imgin amt_phase no_border 
	#       Test function to show effects of phase randomization.
	#       imgin is the full path name to the image you want to work on;
	#       amt_phase is the proportion of original phase vector (rest is random) [0.5]
	#       show is whether to display original and scrambled images side by side in analysis window
	#       To save a copy of the scrambled image as a tga enter a full file name
	#       example: ::pu::phaseDemo c:/stimuli/distEnc/fish00.tga .3
	# %END%
	
	#parse your input name; do it this way because this is how
	#it usually comes out of stimdg in an experiment
	set path [file dirname $imgin]
	set filename [file tail $imgin]
	set name [file rootname $filename]
	set ext [string range [file extension $filename] 1 end]
	
	#you need these for it all to work, Impro for img_read, and Stats for the dl_fft2D and dl_ifft2D
	::pu::init
	#break image in to amplitude and phase components, and alpha channel
	dl_local amp_phase [::pu::get_amp_phase_gray $name $path $ext]
	dl_local fm $amp_phase:0
	dl_local tempphase $amp_phase:1
	dl_local randphasevec [::pu::makeRandVec [dl_length $tempphase]]
	dl_local ch0 [::pu::getBackImage $fm $tempphase $randphasevec $ap $equalize $histmode $doright]
	if {$show} {
	    clearwin
	    setwindow 0 0 10 10 
	    set p [dlp_create pDplot]
	    dl_local orig [img_readTarga $imgin]
	    set width [dl_tcllist [dl_int [dl_sqrt [dl_div [dl_length $orig] 4]]]]
	    dlp_set $p picdat [dl_char [dl_mult $ch0 255]]
	    dlg_image 3 1 [dl_llist "$width $width" $p:picdat] 6
	    dlp_set $p picdat $orig
	    dlg_image 1 8  [dl_llist "$width $width" $p:picdat] 1
	}
	
	#do you want to save?
	if {![string match $save_name "none"]} {
	    eval img_writeTarga [dl_char [dl_mult $ch0 255]] [expr int(sqrt([dl_length $ch0]))]  [expr int(sqrt([dl_length $ch0]))]   $save_name
	}
		dl_delete $ch0


    }

    proc getBackImage {magvec angvec randvec ap {equalize 0} {histmode 0} {doright 1}} {
	dl_local fp [::pu::mixPhases $angvec $randvec $ap $doright]
	#set dc phase to original 
	set sl [expr int(sqrt([dl_length $angvec]))]
	set half_sl [expr $sl / 2]
	dl_put $fp 0 0.
	dl_put $fp $half_sl 0.
	dl_put $fp [expr $sl * $half_sl] 0.
	dl_put $fp [expr $sl * $half_sl + $half_sl] 0.
	dl_local fc_r [dl_mult $magvec [dl_cos $fp]]
	dl_local fc_i [dl_mult $magvec [dl_sin $fp]]
	dl_local ch0 [dl_div [dl_select [dl_ifft2D [dl_interleave $fc_r $fc_i]] "1 0"] [dl_length $fc_r]]
	if $histmode {
	    #this is just something for phaseDemo so you can see the histogram of phase angles
	    newTab
	    dlp_plot [dlp_hist $fp 60]
	    newTab
	}
	#equalize the output
	if $equalize {
	    #this takes a long time with big images
	    dl_local r_ch0 [dl_recode $ch0]
	    dl_local lumcnts [dl_sortedFunc $r_ch0 $r_ch0 dl_lengths]
	    dl_local newvals [dl_div [dl_float [dl_cumsum $lumcnts:1]] [dl_length $ch0]]
	    dl_local eqch0 [dl_choose $newvals $r_ch0]
	    dl_return $eqch0
	} else {
	    dl_return $ch0
	}
    }
}