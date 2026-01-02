package provide spec 0.9

namespace eval ::spec {

####Functions for Power Computation
  
    proc spectrogram { args } {
	# 	%HELP%
	# 	spectrogram X, WINDOW, NOVERLAP, NFFT, Fs
	# 	X is the data
	# 	WINDOW can be one of the following: an integer, or a dl_flist.
	#          If it is is an integer, than that is the length of each
	#          data segment, if a vector than the data segment is the length
	#          of the vector which will be used for windowing. If no value
	#          is supplied than it defaults to a Hanning window of length 256
	# 	NOVERLAP is the number of samples to overlap from one
	# 	   data segment to the next
	# 	NFFT is the length of the fft to perform
	# 	Fs is the sampling frequency if known
	# 	%END%
	


	###Checking Input Arguements###
	if {[llength $args] == 0} {
	    puts "Error. No inputs supplied.";return 0
	}
	
	###Checking Data Depth is of proper type###
	if {![dl_isMatrix [lindex $args 0]]} {
	    puts "Error: data must be a list of floats\nTo check if proper format try \"dl_isMatrix\"";update;return 0
	}
	
	if {[llength $args] == 1} {
	    dl_local dat $args
	    dl_local han [::helpspec::hanningWindow 256]
	    set ovlp 128
	    set nfft 256
	    set fs 1
	}
	if {[llength $args] == 2} {
	    dl_local dat [lindex $args 0]
	    if {[dl_length [lindex $args 1]] == 1 && [dl_depth [lindex $args 1]] == 0} {
		dl_local han [::helpspec::hanningWindow [lindex $args 1]]
	    } else {
		dl_local han [lindex $args 1] 
	    }
	    set ovlp [expr [dl_length $han] / 2]
	    set nfft [dl_length $han]
	    set fs 1
	}
	if {[llength $args] == 3} {
	    dl_local dat [lindex $args 0]
	    if {[dl_length [lindex $args 1]] == 1 && [dl_depth [lindex $args 1]] == 0} {
		dl_local han [::helpspec::hanningWindow [lindex $args 1]]
	    } else {
		dl_local han [lindex $args 1] 
	    }
	    set ovlp [lindex $args 2]
	    set nfft [dl_length $han]
	    set fs 1
	}
	if {[llength $args] == 4} {
	    dl_local dat [lindex $args 0]
	    if {[dl_length [lindex $args 1]] == 1 && [dl_depth [lindex $args 1]] == 0} {
		dl_local han [::helpspec::hanningWindow [lindex $args 1]]
	    } else {
		dl_local han [lindex $args 1] 
	    }
	    set ovlp [lindex $args 2]
	    set nfft [lindex $args 3]
	    set fs 1
	}
	if {[llength $args] == 5} {
	    dl_local dat [lindex $args 0]
	   if {[dl_length [lindex $args 1]] == 1 && [dl_depth [lindex $args 1]] == 0} {
		dl_local han [::helpspec::hanningWindow [lindex $args 1]]
	    } else {
		dl_local han [lindex $args 1] 
	    }
	    set ovlp [lindex $args 2]
	    set nfft [lindex $args 3]
	    set fs [lindex $args 4]
	}
	if {[llength $args] > 5} {
	    puts "Error: too many input arguements";
	    use ::spec::spectrogram;update;
	    return 0
	}
	###End of checking inputs###
	puts $ovlp
	###Setting up certain flags and loop variables###
	
	set totlen [dl_length ${dat}:0];
	#set loopnum [expr $totlen / [expr [dl_length $han] - $ovlp]];
	if {[expr int(fmod($totlen , [dl_length $han]))] != 0} {
	    puts "Warning: since the total length of data is not\n evenly divisible by the length of the window\ndata will be truncated."
	}
	puts "totlen is $totlen"
	#puts "loopnum is $loopnum"
	if {[expr $nfft > [dl_length $han]]} {
	    set zpflag 1;
	    set znum [expr $nfft - [dl_length $han]]
	    puts "Will use zero padding since nfft length greater than\nddata segment length."
	} else {
	    set zpflag 0
	}
	
	set sampstart 0
	set sampstop [expr [dl_length $han] - 1]
	dl_local getsamp [dl_series $sampstart $sampstop]
	dl_local power [dl_llist]
	dl_local start [dl_flist]
	dl_local stop [dl_flist]
	while {$sampstop < $totlen} {
	    
	    dl_local dataseg [dl_mult [dl_choose $dat [dl_llist $getsamp]] [dl_llist $han]]


	    ##note to future programmers. I always thought that an extra
	    ##argument could be added to specify a method for the calculation
	    ##of the power spectral density, however, than you have
	    ##to have global variables or pass around in an array or something
	    ##all the arguements and values specified above.
	    ##so at the moment it is not implemented.
	
	    if $zpflag {
		dl_local dataseg [dl_combineLists $dataseg [dl_float [dl_zeros [dl_repeat $znum [dl_length $dataseg]]]]]
	    }
	    
	    set fft_len  $nfft
	    dl_local fe [::fftw::1d $dataseg $fft_len]
	    dl_local pe [::helpspec::complexMult $fe $fe]
	    dl_append $power [dl_meanList [dl_collapse [dl_div [dl_pow [dl_choose $pe [dl_llist [dl_ilist 0]]] 2] [dl_float $fft_len]]]]
	    dl_append $start [expr $sampstart * 1000. / $fs]
	    dl_append $stop [expr $sampstop * 1000. / $fs]
	    
	   
	    incr sampstart [expr [dl_length $han] - $ovlp]
	    incr sampstop [expr [dl_length $han] - $ovlp]
	    dl_local getsamp [dl_series $sampstart $sampstop]
	    puts "start is $sampstart, end is $sampstop"
	}
	dl_return [dl_llist $power $start $stop [dl_series 0. [expr $fs /2.] [expr double($fs) / double($nfft)]]]
    }
    


	
    
    proc psdBy {g lfpchan args } {
	#add use function
	#Error Checking and Housekeeping
	#Can't compute the power if no eeg, check for it
	if {![dl_exists $g:eeg]} {
	    ::helplfp::getAllEeg $g
	}
	
	#Always start with a clean slate
	    ::helpspec::buildPSDArray

	#If optional arguements parse them
	if {[llength $args]} {
	    if {![::helpspec::parseArgsPSD [eval list $args]]} {return 0}
	}
	set ::spec::psdVar(lfpchan) $lfpchan

	#Run the proper psd function based on the method
	::spec::psd$::spec::psdVar(method) $g "$lfpchan" "$args"

	if {$::spec::psdVar(plotspec)} {
	    ::helpspec::plotPSD $g $lfpchan
	}
	return 1
    }
    proc psdpgram {g lfpchan args} {
	dl_local e [::helpspec::getEeg $g $lfpchan $::spec::psdVar(start) $::spec::psdVar(stop) $::spec::psdVar(align)]
	if {![string match $::spec::psdVar(window) "none"]} {
	    dl_local taper [::helpspec::${::spec::psdVar(window)}Window [dl_length $e:0:0]]
	    dl_local e [dl_mult $e [dl_pack [dl_llist $taper]]]
	}
	set fft_len  [dl_min [dl_lengths [dl_collapse $e]]]
	dl_local fe [dl_collapse [::fftw::1d $e $fft_len]]
	dl_local pe [::helpspec::complexMult $fe $fe]
	dl_set $g:power_names [dl_slist]
	dl_set $g:power_settings [dl_slist]
	foreach pn [array names ::spec::psdVar] {
	    dl_append $g:power_names $pn
	    dl_append $g:power_settings $::spec::psdVar($pn)
	}
	dl_set $g:power [dl_collapse [dl_div [dl_pow [dl_choose $pe [dl_llist [dl_ilist 0]]] 2] [dl_float $fft_len]]]
	set freq_res [expr 1000. / ($::spec::psdVar(stop) - $::spec::psdVar(start))]
	dl_set $g:freqs [dl_repeat [dl_llist [dl_mult [dl_fromto 0 [dl_length $g:power:0]] $freq_res]] [dl_length $g:power]]
	return 1
    }
    
}



