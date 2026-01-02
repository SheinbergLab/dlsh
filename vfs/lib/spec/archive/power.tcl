package provide spec 0.9

namespace eval ::spec {

####Functions for Power Computation
  
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



