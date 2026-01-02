package provide mtspec 1.4

namespace eval ::helpmtspec { } {
    set cols(0) 3
    set cols(1) 5
    set cols(2) 10
    set cols(3) 14
    set cols(4) 4
    set cols(5) 2
    set cols(6) 1
    set cols(7) 7
    set cols(99) 8

    proc computeNFFT {n pad} {
	if {$pad < 0} {
	    return $n
	} 
	set nextpow2 [expr ceil(1/log10(2)*log10($n))]
	return [expr int(pow(2,$nextpow2+$pad))]
    }

    #########################################################################
    #
    #
    #     mt___________ functions are the workhorses, 
    #     
    #     be careful, these functions take lists of lists, hence the depth check
    #
    #########################################################################     

    proc mtspectrum { dynlist flag } {
	if {[dl_depth $dynlist] != 1} {
	    error "Input dynlist wrong depth. Will give wrong answer."
	}
	set samp_freq $::mtspec::Args(samp_freq)
	set fmin [lindex $::mtspec::Args(fpass) 0]
	set fmax [expr min($samp_freq/2.,[lindex $::mtspec::Args(fpass) 1])]
	set pretime [expr $::mtspec::Args(pretime)/1000.]
	set posttime [expr $::mtspec::Args(posttime)/1000.]
	set nw $::mtspec::Args(nw)
	set ntapers $::mtspec::Args(ntapers)
	set pad $::mtspec::Args(pad)
	set nfft [::helpmtspec::computeNFFT [expr round(($posttime-$pretime)*$samp_freq)] $pad]

	if {$flag == "continuous"} {
	    dl_local J [::fftw::mtfftC $dynlist $nw $ntapers $pad $samp_freq]
	} elseif {$flag == "point"} {
	    dl_local J [::fftw::mtfftPb $dynlist $nw $ntapers $pad $samp_freq]
	}

	dl_local S [dl_means [dl_deepUnpack [dl_add \
						 [dl_mult [dl_choose $J [dl_llist [dl_llist 0]]] \
						      [dl_choose $J [dl_llist [dl_llist 0]]]] \
						 [dl_mult [dl_choose $J [dl_llist [dl_llist 1]]] \
						      [dl_choose $J [dl_llist [dl_llist 1]]]]]]]

	dl_local freqs [dl_series 0 $samp_freq [expr $samp_freq/$nfft.]]
	dl_local fpass [dl_and [dl_gte $freqs $fmin] [dl_lte $freqs $fmax]]
	dl_local freqs [dl_select $freqs $fpass]
	dl_local S [dl_select $S [dl_llist $fpass]]
	dl_local J [dl_select $J [dl_llist [dl_llist [dl_llist $fpass]]]]
	dl_return [dl_llist $S $J $freqs]
    }

    proc mtcoherence { dynlist1 dynlist2 flag1 flag2} {
	if {[dl_depth $dynlist1] != 1 || [dl_depth $dynlist2] != 1} {
	    error "Input dynlist wrong depth. Will give wrong answer."
	}
	set samp_freq $::mtspec::Args(samp_freq)
	set fmin [lindex $::mtspec::Args(fpass) 0]
	set fmax [expr min($samp_freq/2.,[lindex $::mtspec::Args(fpass) 1])]
	set pretime [expr $::mtspec::Args(pretime)/1000.]
	set posttime [expr $::mtspec::Args(posttime)/1000.]
	set nw $::mtspec::Args(nw)
	set ntapers $::mtspec::Args(ntapers)
	set pad $::mtspec::Args(pad)
	set nfft [::helpmtspec::computeNFFT [expr round(($posttime-$pretime)*$samp_freq)] $pad]

	if {$flag1 == "continuous"} {
	    dl_local J1 [::fftw::mtfftC $dynlist1 $nw $ntapers $pad $samp_freq]
	} elseif {$flag1 == "point"} {
	    dl_local J1 [::fftw::mtfftPb $dynlist1 $nw $ntapers $pad $samp_freq]
	}

	if {$flag2 == "continuous"} {
	    dl_local J2 [::fftw::mtfftC $dynlist2 $nw $ntapers $pad $samp_freq]
	} elseif {$flag2 == "point"} {
	    dl_local J2 [::fftw::mtfftPb $dynlist2 $nw $ntapers $pad $samp_freq]
	}

	dl_local S12r [dl_means [dl_deepUnpack [dl_add \
						 [dl_mult [dl_choose $J1 [dl_llist [dl_llist 0]]] \
						      [dl_choose $J2 [dl_llist [dl_llist 0]]]] \
						 [dl_mult [dl_choose $J1 [dl_llist [dl_llist 1]]] \
						      [dl_choose $J2 [dl_llist [dl_llist 1]]]]]]]
	dl_local S12i [dl_means [dl_deepUnpack [dl_sub \
						 [dl_mult [dl_choose $J1 [dl_llist [dl_llist 0]]] \
						      [dl_choose $J2 [dl_llist [dl_llist 1]]]] \
						 [dl_mult [dl_choose $J1 [dl_llist [dl_llist 1]]] \
						      [dl_choose $J2 [dl_llist [dl_llist 0]]]]]]]
	dl_local S1 [dl_means [dl_deepUnpack [dl_add \
						   [dl_mult [dl_choose $J1 [dl_llist [dl_llist 0]]] \
							[dl_choose $J1 [dl_llist [dl_llist 0]]]] \
						   [dl_mult [dl_choose $J1 [dl_llist [dl_llist 1]]] \
							[dl_choose $J1 [dl_llist [dl_llist 1]]]]]]]
	dl_local S2 [dl_means [dl_deepUnpack [dl_add \
						 [dl_mult [dl_choose $J2 [dl_llist [dl_llist 0]]] \
						      [dl_choose $J2 [dl_llist [dl_llist 0]]]] \
						 [dl_mult [dl_choose $J2 [dl_llist [dl_llist 1]]] \
						      [dl_choose $J2 [dl_llist [dl_llist 1]]]]]]]

	dl_local C12r [dl_div $S12r [dl_sqrt [dl_mult $S1 $S2]]]
	dl_local C12i [dl_div $S12i [dl_sqrt [dl_mult $S1 $S2]]]
	dl_local cohmag [dl_sqrt [dl_add [dl_mult $C12r $C12r] [dl_mult $C12i $C12i]]]
	dl_local cohphase [dl_atan [dl_div $C12i $C12r]]

	dl_local freqs [dl_series 0 $samp_freq [expr $samp_freq/$nfft.]]
	dl_local fpass [dl_and [dl_gte $freqs $fmin] [dl_lte $freqs $fmax]]
	dl_local freqs [dl_select $freqs $fpass]

	dl_local cohmag [dl_select $cohmag [dl_llist $fpass]]
	dl_local cohphase [dl_select $cohphase [dl_llist $fpass]]
	dl_local S12r [dl_select $S12r [dl_llist $fpass]]
	dl_local S12i [dl_select $S12i [dl_llist $fpass]]
	dl_local S1 [dl_select $S1 [dl_llist $fpass]]
	dl_local S2 [dl_select $S2 [dl_llist $fpass]]
	dl_local J1 [dl_select $J1 [dl_llist [dl_llist [dl_llist $fpass]]]]
	dl_local J2 [dl_select $J2 [dl_llist [dl_llist [dl_llist $fpass]]]]

	dl_return [dl_llist $cohmag $cohphase $S12r $S12i $S1 $S2 $J1 $J2 $freqs]
    }

    proc mtspecgram { dynlist stepsize winsize flag} {
	if {[dl_depth $dynlist] != 1} {
	    error "Input dynlist wrong depth. Will give wrong answer."
	}
	set samp_freq $::mtspec::Args(samp_freq)
	set fmin [lindex $::mtspec::Args(fpass) 0]
	set fmax [expr min($samp_freq/2.,[lindex $::mtspec::Args(fpass) 1])]
	set pretime [expr $::mtspec::Args(pretime)/1000.]
	set posttime [expr $::mtspec::Args(posttime)/1000.]
	set winsize [expr $winsize / 1000.]
	set stepsize [expr $stepsize / 1000.]
	set nw $::mtspec::Args(nw)
	set ntapers $::mtspec::Args(ntapers)
	set pad $::mtspec::Args(pad)

	set nSamp [expr int(($posttime-$pretime)*$samp_freq)]
	set nSampWin [expr int($winsize*$samp_freq)]
	set nSampStep [expr int($stepsize*$samp_freq)] 
	dl_local allSamps [dl_fromto 0 $nSamp]
	dl_local sampWinStart [dl_series 0 [expr $nSamp-$nSampWin] $nSampStep]
	set nWins [dl_length $sampWinStart]
	
	#times correspond to the centers of the windows
	dl_local times [dl_mult [dl_add [dl_div $sampWinStart [expr double($samp_freq)]] \
				     $pretime [expr $winsize/2.]] 1000]	

	#Split up the eeg signal into overlapping windows for the spectrogram; also do this in batches of trials to ease
	# memory demands
	set ncomp [expr [dl_length $dynlist] / 100]
	dl_local S [dl_llist]
	for {set i 0} {$i <= $ncomp} {incr i} {
	    dl_pushTemps
	    if {$i == $ncomp} {
		if ([expr [dl_length $dynlist]%100==0]) break
		dl_local tempDynlist [dl_choose $dynlist [dl_fromto [expr $i*100] [dl_length $dynlist]]]
	    } else {
		dl_local tempDynlist [dl_choose $dynlist [dl_fromto [expr $i*100] [expr ($i+1)*100]]]
	    }

	    dl_local windowIndices [dl_fromto $sampWinStart [dl_add $sampWinStart $nSampWin]]
	    dl_local windowedData [dl_choose $tempDynlist [dl_llist $windowIndices]]
	    
	    #Do the mtFFT
	    if {$flag == "continuous"} {
		dl_local J [::fftw::mtfftC $windowedData $nw $ntapers $pad $samp_freq]
	    } elseif {$flag == "point"} {
		dl_local J [::fftw::mtfftPb $windowedData $nw $ntapers $pad $samp_freq]
	    }

	    dl_local S [dl_combine $S [dl_hmeans [dl_deepUnpack [dl_add \
						      [dl_mult [dl_choose $J [dl_llist [dl_llist [dl_llist 0]]]] \
							   [dl_choose $J [dl_llist [dl_llist [dl_llist 0]]]]] \
						      [dl_mult [dl_choose $J [dl_llist [dl_llist [dl_llist 1]]]] \
							   [dl_choose $J [dl_llist [dl_llist [dl_llist 1]]]]]]]]]
	    dl_popTemps
	}
	set nfft [::helpmtspec::computeNFFT $nSampWin $pad]
	dl_local freqs [dl_series 0 $samp_freq [expr $samp_freq/$nfft.]]
	dl_local fpass [dl_and [dl_gte $freqs $fmin] [dl_lte $freqs $fmax]]
	dl_local freqs [dl_select $freqs $fpass]
	dl_local S [dl_select $S [dl_llist [dl_llist $fpass]]]
	dl_return [dl_llist $S $freqs $times]
    }

    #########################################################################
    #
    #     
    #                         elementary statistics
    #
    #
    #
    #########################################################################
    
    proc specerror {mean_spectra ind_mtfft ntrials p type {numspks ""}} {
	
	# type = 1 for asymptotic, 2 for jacknife

	set pp [expr 1-$p/2.]
	set qq [expr 1-$pp]

	if {$type == 1} {
	    dl_local dfs [dl_mult $ntrials $::mtspec::Args(ntapers) 2]
	    if {$numspks != ""} {
		dl_local dfs [dl_float [dl_floor [dl_div 1. [dl_add [dl_div 1. $dfs] [dl_div 1. [dl_mult 2 $numspks]]]]]]
	    }
	    dl_local Qp [dcdf::chi2inv [dl_repeat $pp [dl_length $dfs]] $dfs]
	    dl_local Qq [dcdf::chi2inv [dl_repeat $qq [dl_length $dfs]] $dfs]
	    dl_local Serrl [dl_div [dl_mult $mean_spectra $dfs] $Qp]
	    dl_local Serru [dl_div [dl_mult $mean_spectra $dfs] $Qq]
	} elseif {$type == 2} {
	    set ncond [dl_length $ntrials]
	    dl_local Serrl [dl_llist]
	    dl_local Serru [dl_llist]
	    for {set i 0} {$i < $ncond} {incr i} {
		dl_local cur_mtfft [dl_collapse $ind_mtfft:$i]
		set n [dl_length $cur_mtfft]
		dl_local tcrit [dcdf::tinv $p [expr $n-1].]
		dl_local Sj [dl_llist]
		for {set j 0} {$j < $n} {incr j} {
		    dl_pushTemps
		    dl_local select_notj [dl_noteq [dl_fromto 0 $n] $j]
		    dl_local J_notj [dl_select $cur_mtfft $select_notj]
		    dl_local S_notj [dl_meanList [dl_unpack [dl_add \
								 [dl_mult [dl_choose $J_notj [dl_llist  0]] \
								      [dl_choose $J_notj [dl_llist 0]]] \
								 [dl_mult [dl_choose $J_notj [dl_llist 1]] \
								      [dl_choose $J_notj [dl_llist 1]]]]]]
		    dl_append $Sj $S_notj
		    dl_popTemps
		}
		dl_local sdlogSj [dl_sqrt [dl_mult [dl_vars [dl_log [dl_transpose $Sj]]] [expr ($n.-1)/$n.]]];
		dl_local sigma [dl_mult $sdlogSj [expr sqrt($n-1)]]
		dl_local conf [dl_mult $sigma $tcrit]
		dl_append $Serrl [dl_mult $mean_spectra:$i [dl_exp [dl_mult $conf -1]]]
		dl_append $Serru [dl_mult $mean_spectra:$i [dl_exp $conf]]
	    }
	} else { 
	    error "Invalid method for confidence bar calculation"
	}

	dl_return [dl_llist $Serrl $Serru]
    }

    proc coherror {mean_cohmag ind_mtfft1 ind_mtfft2 ntrials p type {numspks1 ""} {numspks2 ""}} {
	
	# type = 1 for asymptotic, 2 for jacknife

	set nfft [dl_tcllist [dl_lengths $mean_cohmag]:0]

	set pp [expr 1-$p/2.]
	set qq [expr 1-$p]

	# compute confidence level at the specified pvalue for coherence magnitude
	dl_local dfs [dl_mult $ntrials $::mtspec::Args(ntapers) 2]
	dl_local dfs1 $dfs; dl_local dfs2 $dfs;
	if {$numspks1 != ""} {
	    dl_local dfs1 [dl_float [dl_floor [dl_div [dl_mult 2 $numspks1 $dfs1] [dl_add [dl_mult 2 $numspks1] $dfs]]]]
	}
	if {$numspks2 != ""} {
	    dl_local dfs2 [dl_float [dl_floor [dl_div [dl_mult 2 $numspks2 $dfs2] [dl_add [dl_mult 2 $numspks2] $dfs]]]]
	}
	dl_local dfs [dl_min $dfs1 $dfs2]
	dl_local df [dl_div 1. [dl_sub [dl_div $dfs 2.] 1.]]
	dl_local confC [dl_sqrt [dl_sub 1. [dl_pow $p $df]]]; #this value determines at what value magnitudes are stat. sig. > 0

	# theoretical phase standard deviation
	if {$type == 1} {
	    dl_local phistd [dl_float [dl_reshape [dl_zeros [expr [dl_length $mean_cohmag]*$nfft]] - $nfft]]
	    dl_local dfs [dl_reshape [dl_repeat $dfs $nfft] - $nfft]
	    dl_local idx [dl_gte [dl_abs [dl_sub $mean_cohmag 1]] [expr 1.e-16]]
	    dl_local temp [dl_sqrt [dl_mult [dl_div 2. [dl_select $dfs $idx]] \
					[dl_sub [dl_div 1. [dl_pow [dl_select $mean_cohmag $idx] 2]] 1]]]
	    dl_local phistd [dl_replace $phistd $idx [dl_select $temp $idx]]
	    dl_return [dl_llist $confC [dl_llist [dl_slist "specify jackknife"] \
					    [dl_slist "specify jackknife"]] $phistd]
	} elseif {$type == 2} {; # jacknife confidence bars for magnitude and jacknife phase standard deviation
	    set ncond [dl_length $ntrials]
	    dl_local Cerrl [dl_llist]
	    dl_local Cerru [dl_llist]
	    dl_local phistd [dl_llist]
	    for {set i 0} {$i < $ncond} {incr i} {
		dl_local cur_mtfft1 [dl_collapse $ind_mtfft1:$i]
		dl_local cur_mtfft2 [dl_collapse $ind_mtfft2:$i]
		set n [dl_length $cur_mtfft1]
		dl_local tcrit [dcdf::tinv $pp [expr [dl_tcllist $dfs:$i]-1]]
		dl_local atanhCj [dl_llist]
		dl_local phasefactorrj [dl_llist]
		dl_local phasefactorij [dl_llist]
		for {set j 0} {$j < $n} {incr j} {
		    dl_pushTemps
		    dl_local select_notj [dl_noteq [dl_fromto 0 $n] $j]
		    dl_local J1_notj [dl_select $cur_mtfft1 $select_notj]
		    dl_local J2_notj [dl_select $cur_mtfft2 $select_notj]
		    dl_local S12r_notj [dl_meanList [dl_unpack [dl_add \
								 [dl_mult [dl_choose $J1_notj [dl_llist  0]] \
								      [dl_choose $J2_notj [dl_llist 0]]] \
								 [dl_mult [dl_choose $J1_notj [dl_llist 1]] \
								      [dl_choose $J2_notj [dl_llist 1]]]]]]
		    dl_local S12i_notj [dl_meanList [dl_unpack [dl_sub \
								 [dl_mult [dl_choose $J1_notj [dl_llist  0]] \
								      [dl_choose $J2_notj [dl_llist 1]]] \
								 [dl_mult [dl_choose $J1_notj [dl_llist 1]] \
								      [dl_choose $J2_notj [dl_llist 0]]]]]]
		    dl_local S1_notj [dl_meanList [dl_unpack [dl_add \
								 [dl_mult [dl_choose $J1_notj [dl_llist  0]] \
								      [dl_choose $J1_notj [dl_llist 0]]] \
								 [dl_mult [dl_choose $J1_notj [dl_llist 1]] \
								      [dl_choose $J1_notj [dl_llist 1]]]]]]
		    dl_local S2_notj [dl_meanList [dl_unpack [dl_add \
								 [dl_mult [dl_choose $J2_notj [dl_llist  0]] \
								      [dl_choose $J2_notj [dl_llist 0]]] \
								 [dl_mult [dl_choose $J2_notj [dl_llist 1]] \
								      [dl_choose $J2_notj [dl_llist 1]]]]]]
		    dl_local C12r_notj [dl_div $S12r_notj [dl_sqrt [dl_mult $S1_notj $S2_notj]]]
		    dl_local C12i_notj [dl_div $S12i_notj [dl_sqrt [dl_mult $S1_notj $S2_notj]]]
		    dl_local cohmag_notj [dl_sqrt [dl_add [dl_mult $C12r_notj $C12r_notj] [dl_mult $C12i_notj $C12i_notj]]]
		    dl_local cohphase_notj [dl_atan [dl_div $C12i_notj $C12r_notj]]
		    dl_append $atanhCj [dl_mult .5 [expr sqrt(2*$n-1)] \
					    [dl_log [dl_div [dl_add 1 $cohmag_notj] [dl_sub 1 $cohmag_notj]]]]
		    dl_append $phasefactorrj [dl_div $C12r_notj $cohmag_notj]
		    dl_append $phasefactorij [dl_div $C12i_notj $cohmag_notj]
		    dl_popTemps
		}
		dl_local atanhC [dl_mult .5 [expr sqrt(2*$n-1)] \
				     [dl_log [dl_div [dl_add 1 $mean_cohmag:$i] [dl_sub 1 $mean_cohmag:$i]]]]
		dl_local sigma12 [dl_mult [dl_sqrt [dl_mult [dl_vars [dl_transpose $atanhCj]] [expr ($n.-1)/$n.]]] \
				      [expr sqrt($n-1)]]
		dl_local Cl [dl_sub $atanhC [dl_mult $tcrit $sigma12]]
		dl_local Cu [dl_add $atanhC [dl_mult $tcrit $sigma12]]
		dl_append $Cerrl [dl_tanh [dl_div $Cl [expr sqrt(2*$n-1)]]]
		dl_append $Cerru [dl_tanh [dl_div $Cu [expr sqrt(2*$n-1)]]]

		dl_local mphasefactorr [dl_meanList $phasefactorrj]
		dl_local mphasefactori [dl_meanList $phasefactorij]
		dl_local phasefactormag [dl_sqrt [dl_add [dl_mult $mphasefactorr $mphasefactorr] \
						      [dl_mult $mphasefactori $mphasefactori]]]
		dl_append $phistd [dl_mult [expr 2*$n-2] [dl_sub 1 $phasefactormag]]
	    }
	    dl_return [dl_llist $confC [dl_llist $Cerrl $Cerru] $phistd]
	} else { 
	    error "Invalid method for statistics calculation"
	}
    }

    ###############################################################################
    #
    #
    # The "Create" functions essentially sort and store analyzed data in groups
    #
    #
    ############################################################################### 

    proc CreateSpectrumGroupC {g chan sortlists} {

	#get lfp signal first
	dl_local alleeg [::helpmtspec::get_lfp $g $chan]

	#now do spectral analysis
	dl_local data [::helpmtspec::mtspectrum $alleeg "continuous"]
	dl_local allspectra $data:0

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]
	dl_local spectra [dl_sortedFunc $allspectra "$sb1 $sb2"]

	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_local ntrials [dl_float [dl_lengths [dl_sortByLists $allspectra $sb1 $sb2]]]
	    dl_local mtfft [dl_sortByLists $data:1 $sb1 $sb2]
	    dl_local Serr [::helpmtspec::specerror $spectra:2 $mtfft $ntrials [lindex $::mtspec::Args(p) 1] [lindex $::mtspec::Args(p) 0]]
	}
			   

	#creating out group
	set out [dg_create]
	dl_set $out:freqs $data:2
	dl_set $out:power $spectra
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_set $out:Serrl [dl_llist $spectra:0 $spectra:1 $Serr:0]
	    dl_set $out:Serru [dl_llist $spectra:0 $spectra:1 $Serr:1]
	}
	dl_set $out:individual $allspectra
	dl_set $out:label1 [dl_llist $name1 $sb1]
	dl_set $out:label2 [dl_llist $name2 $sb2]
	return $out
    }

    proc CreateSpecgramGroupC {g chan stepsize winsize sortlists } {
	#get lfp signal
	dl_local alleeg [::helpmtspec::get_lfp $g $chan]

	#now do spectral analysis
	dl_local data [::helpmtspec::mtspecgram $alleeg $stepsize $winsize "continuous"]
	dl_local allspecgrams $data:0

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]

	dl_local specgrams [dl_sortedFunc $allspecgrams "$sb1 $sb2"]

	#frequency and time indices
	dl_local fnts [dl_llist $data:1 $data:2]

	#out group
	set out [dg_create]
	dl_set $out:xydata $fnts
	dl_set $out:zdata $specgrams
	dl_set $out:zind $allspecgrams
	dl_set $out:label1 [dl_llist $name1 $sb1]
	dl_set $out:label2 [dl_llist $name2 $sb2]
	return $out
    }

    proc CreateSpectrumGroupPb {g chan sortlists} {

	#get spikes first
	dl_local spikes [::helpmtspec::spike_extract $g $chan]
	set ntrials [dl_length $spikes]

	#discretize
	dl_local binnedSpikes [dl_llist]
	for { set i 0} {$i < $ntrials} {incr i} {
	    dl_local zeros [dl_zeros [expr $::mtspec::Args(posttime) - \
					  $::mtspec::Args(pretime)]]
	    dl_local occurences [dl_sub [dl_floor $spikes:$i] $::mtspec::Args(pretime)]
	    dl_append $binnedSpikes [dl_replaceByIndex $zeros $occurences 1]
	}
	dl_local binnedSpikes [dl_float $binnedSpikes]

	#now do spectral analysis
	dl_local data [::helpmtspec::mtspectrum $binnedSpikes "point"]
	dl_local allspectra $data:0

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]
	dl_local spectra [dl_sortedFunc $allspectra "$sb1 $sb2"]

	#Error bars
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_local ntrials [dl_float [dl_lengths [dl_sortByLists $allspectra $sb1 $sb2]]]
	    dl_local mtfft [dl_sortByLists $data:1 $sb1 $sb2]
	    if {$::mtspec::Args(fscorr)} {
		dl_local numspks [dl_sums [dl_sortByLists [dl_sums $binnedSpikes] $sb1 $sb2]]
	    } else  {
		set numspks ""
	    }
	    dl_local Serr [::helpmtspec::specerror $spectra:2 $mtfft $ntrials [lindex $::mtspec::Args(p) 1] [lindex $::mtspec::Args(p) 0] $numspks]
	}

	#creating out group
	set out [dg_create]
	dl_set $out:freqs $data:2
	dl_set $out:power $spectra
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_set $out:Serrl [dl_llist $spectra:0 $spectra:1 $Serr:0]
	    dl_set $out:Serru [dl_llist $spectra:0 $spectra:1 $Serr:1]
	}
	dl_set $out:individual $allspectra
	dl_set $out:label1 [dl_llist $name1 $sb1]
	dl_set $out:label2 [dl_llist $name2 $sb2]
	return $out
    }

    proc CreateSpecgramGroupPb {g chan stepsize winsize sortlists } {

	#get spikes first
	dl_local spikes [::helpmtspec::spike_extract $g $chan]
	set ntrials [dl_length $spikes]
	
	#discretize
	dl_local binnedSpikes [dl_llist]
	for { set i 0} {$i < $ntrials} {incr i} {
	    dl_local zeros [dl_zeros [expr $::mtspec::Args(posttime) - \
					  $::mtspec::Args(pretime)]]
	    dl_local occurences [dl_sub [dl_floor $spikes:$i] $::mtspec::Args(pretime)]
	    dl_append $binnedSpikes [dl_replaceByIndex $zeros $occurences 1]
	}
	dl_local binnedSpikes [dl_float $binnedSpikes]

	#now do spectral analysis
	dl_local data [::helpmtspec::mtspecgram $binnedSpikes $stepsize $winsize "point"]
	dl_local allspecgrams $data:0

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]
	dl_local specgrams [dl_sortedFunc $allspecgrams "$sb1 $sb2"]

	#frequency and time indices
	dl_local fnts [dl_llist $data:1 $data:2]

	#out group
	set out [dg_create]
	dl_set $out:xydata $fnts
	dl_set $out:zdata $specgrams
	dl_set $out:zind $allspecgrams
	dl_set $out:label1 [dl_llist $name1 $sb1]
	dl_set $out:label2 [dl_llist $name2 $sb2]
	return $out
    }

    proc CreateCoherenceGroupC {g chan1 chan2 sortlists} {

	#get lfp signal first
	dl_local alleeg1 [::helpmtspec::get_lfp $g $chan1]
	dl_local alleeg2 [::helpmtspec::get_lfp $g $chan2]

	#Now do coherence analysis.
	#To get average coherence, you can't simply average individual coherences!
	# Instead, you must average the trial cross-spectra and trial power spectra, 
	# then get coherence from that average; same for phase.
	dl_local data [::helpmtspec::mtcoherence $alleeg1 $alleeg2 "continuous" "continuous"]
	dl_local allcohmag   $data:0
	dl_local allcohphase $data:1
	dl_local allS12r     $data:2
	dl_local allS12i     $data:3
	dl_local allS1       $data:4
	dl_local allS2       $data:5

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]
	dl_local S12r [dl_sortedFunc $allS12r "$sb1 $sb2"]
	dl_local S12i [dl_sortedFunc $allS12i "$sb1 $sb2"]
	dl_local S1 [dl_sortedFunc $allS1 "$sb1 $sb2"]
	dl_local S2 [dl_sortedFunc $allS2 "$sb1 $sb2"]

	dl_local C12r [dl_div $S12r:2 [dl_sqrt [dl_mult $S1:2 $S2:2]]]
	dl_local C12i [dl_div $S12i:2 [dl_sqrt [dl_mult $S1:2 $S2:2]]]
	dl_local cohmag [dl_sqrt [dl_add [dl_mult $C12r $C12r] [dl_mult $C12i $C12i]]]
	dl_local cohphase [dl_atan [dl_div $C12i $C12r]]

	#Error bars
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_local ntrials [dl_float [dl_lengths [dl_sortByLists $allcohmag $sb1 $sb2]]]
	    dl_local mtfft1 [dl_sortByLists $data:6 $sb1 $sb2]
	    dl_local mtfft2 [dl_sortByLists $data:7 $sb1 $sb2]
	    dl_local stats [::helpmtspec::coherror $cohmag $mtfft1 $mtfft2 $ntrials [lindex $::mtspec::Args(p) 1] [lindex $::mtspec::Args(p) 0]]
	    dl_local confC $stats:0
	    dl_local Cerr $stats:1
	    dl_local phistd $stats:2
	}

	#creating out group
	set out [dg_create]
	dl_set $out:freqs $data:8
	dl_set $out:cohmag [dl_llist $S12r:0 $S12r:1 $cohmag]
	dl_set $out:cohphase [dl_llist $S12r:0 $S12r:1 $cohphase]
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_set $out:confC [dl_llist $S12r:0 $S12r:1 $confC]
	    dl_set $out:Cerrl [dl_llist $S12r:0 $S12r:1 $Cerr:0]
	    dl_set $out:Cerru [dl_llist $S12r:0 $S12r:1 $Cerr:1]
	    dl_set $out:phistd [dl_llist $S12r:0 $S12r:1 $phistd]
	}
	dl_set $out:S12r $S12r
	dl_set $out:S12i $S12i
	dl_set $out:S1 $S1
	dl_set $out:S2 $S2
	dl_set $out:individualmag $allcohmag
	dl_set $out:individualphase $allcohphase
	dl_set $out:label1 [dl_llist $name1 $sb1]
	dl_set $out:label2 [dl_llist $name2 $sb2]
	return $out
    }

    proc CreateCoherenceGroupCPb {g chan1 chan2 sortlists} {

	#Get lfp signal first
	#
	#We're going to linearly interpolate the LFP signal up to 1000 samples/second
	# to make it equivalent to the binned spike data.  In order to do that, we need to 
	# get one more sample at the present sampling rate. One could fix this by changing
	# the load_data function to downsample the LFP signal from 2.5kHz to 1kHz instead 
	# of from 2.5 kHz to 500Hz.
	set ::mtspec::Args(posttime) [expr $::mtspec::Args(posttime) + 2]
	dl_local alleeg  [::helpmtspec::get_lfp $g $chan1]
	set ::mtspec::Args(posttime) [expr $::mtspec::Args(posttime) - 2]
	set ntrials [dl_length $alleeg]
	dl_local upsEEG [dl_llist]
	for {set i 0} {$i < $ntrials} {incr i} {
	    dl_local shift [dl_shift $alleeg:$i -1]
	    dl_local inter [dl_div [dl_add $shift $alleeg:$i] 2.]
	    dl_append $upsEEG [dl_choose [dl_interleave $alleeg:$i $inter] [dl_fromto 0 [expr 2*[dl_length $shift] - 2]]]
	} 

	#now get spikes 
	dl_local spikes [::helpmtspec::spike_extract $g $chan2]

	#discretize
	dl_local binnedSpikes [dl_llist]
	for { set i 0} {$i < $ntrials} {incr i} {
	    dl_local zeros [dl_zeros [expr $::mtspec::Args(posttime) - \
					  $::mtspec::Args(pretime)]]
	    dl_local occurences [dl_sub [dl_floor $spikes:$i] $::mtspec::Args(pretime)]
	    dl_append $binnedSpikes [dl_replaceByIndex $zeros $occurences 1]
	}
	dl_local binnedSpikes [dl_float $binnedSpikes]

	#Now do coherence analysis.
	#To get average coherence, you can't simply average individual coherences!
	# Instead, you must average the trial cross-spectra and trial power spectra, 
	# then get coherence from that average; same for phase.
	dl_local data [::helpmtspec::mtcoherence $upsEEG $binnedSpikes "continuous" "point"]
	dl_local allcohmag   $data:0
	dl_local allcohphase $data:1
	dl_local allS12r     $data:2
	dl_local allS12i     $data:3
	dl_local allS1       $data:4
	dl_local allS2       $data:5

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]
	dl_local S12r [dl_sortedFunc $allS12r "$sb1 $sb2"]
	dl_local S12i [dl_sortedFunc $allS12i "$sb1 $sb2"]
	dl_local S1 [dl_sortedFunc $allS1 "$sb1 $sb2"]
	dl_local S2 [dl_sortedFunc $allS2 "$sb1 $sb2"]

	dl_local C12r [dl_div $S12r:2 [dl_sqrt [dl_mult $S1:2 $S2:2]]]
	dl_local C12i [dl_div $S12i:2 [dl_sqrt [dl_mult $S1:2 $S2:2]]]
	dl_local cohmag [dl_sqrt [dl_add [dl_mult $C12r $C12r] [dl_mult $C12i $C12i]]]
	dl_local cohphase [dl_atan [dl_div $C12i $C12r]]

	#Error bars
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_local ntrials [dl_float [dl_lengths [dl_sortByLists $allcohmag $sb1 $sb2]]]
	    dl_local mtfft1 [dl_sortByLists $data:6 $sb1 $sb2]
	    dl_local mtfft2 [dl_sortByLists $data:7 $sb1 $sb2]
	    if {$::mtspec::Args(fscorr)} {
		dl_local numspks [dl_sums [dl_sortByLists [dl_sums $binnedSpikes] $sb1 $sb2]]
	    } else  {
		set numspks ""
	    }
	    dl_local stats [::helpmtspec::coherror $cohmag $mtfft1 $mtfft2 $ntrials [lindex $::mtspec::Args(p) 1] [lindex $::mtspec::Args(p) 0] "" $numspks]
	    dl_local confC $stats:0 
	    dl_local Cerr $stats:1
	    dl_local phistd $stats:2
	}

	#creating out group
	set out [dg_create]
	dl_set $out:freqs $data:8
	dl_set $out:cohmag [dl_llist $S12r:0 $S12r:1 $cohmag]
	dl_set $out:cohphase [dl_llist $S12r:0 $S12r:1 $cohphase]
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_set $out:confC [dl_llist $S12r:0 $S12r:1 $confC]
	    dl_set $out:Cerrl [dl_llist $S12r:0 $S12r:1 $Cerr:0]
	    dl_set $out:Cerru [dl_llist $S12r:0 $S12r:1 $Cerr:1]
	    dl_set $out:phistd [dl_llist $S12r:0 $S12r:1 $phistd]
	}
	dl_set $out:S12r $S12r
	dl_set $out:S12i $S12i
	dl_set $out:S1 $S1
	dl_set $out:S2 $S2
	dl_set $out:individualmag $allcohmag
	dl_set $out:individualphase $allcohphase
	dl_set $out:label1 [dl_llist $name1 $sb1]
	dl_set $out:label2 [dl_llist $name2 $sb2]
	return $out
    }

    proc CreateCoherenceGroupPb {g chan1 chan2 sortlists} {

	#get spikes 
	dl_local spikes1 [::helpmtspec::spike_extract $g $chan1]
	dl_local spikes2 [::helpmtspec::spike_extract $g $chan2]
	set ntrials [dl_length $spikes1]

	#discretize
	dl_local binnedSpikes1 [dl_llist]
	dl_local binnedSpikes2 [dl_llist]
	for { set i 0} {$i < $ntrials} {incr i} {
	    dl_local zeros1 [dl_zeros [expr $::mtspec::Args(posttime) - \
					  $::mtspec::Args(pretime)]]
	    dl_local zeros2 [dl_zeros [expr $::mtspec::Args(posttime) - \
					   $::mtspec::Args(pretime)]]
	    dl_local occurences1 [dl_sub [dl_floor $spikes1:$i] $::mtspec::Args(pretime)]
	    dl_local occurences2 [dl_sub [dl_floor $spikes2:$i] $::mtspec::Args(pretime)]
	    dl_append $binnedSpikes1 [dl_replaceByIndex $zeros1 $occurences1 1]
	    dl_append $binnedSpikes2 [dl_replaceByIndex $zeros1 $occurences2 1]
	}
	dl_local binnedSpikes1 [dl_float $binnedSpikes1]
	dl_local binnedSpikes2 [dl_float $binnedSpikes2]

	#Now do coherence analysis
	#To get average coherence, you can't simply average individual coherences!
	# Instead, you must average the trial cross-spectra and trial power spectra, 
	# then get coherence from that average; same for phase.
	dl_local data [::helpmtspec::mtcoherence $binnedSpikes1 $binnedSpikes2 "point" "point"]
	dl_local allcohmag   $data:0
	dl_local allcohphase $data:1
	dl_local allS12r     $data:2
	dl_local allS12i     $data:3
	dl_local allS1       $data:4
	dl_local allS2       $data:5

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]
	dl_local S12r [dl_sortedFunc $allS12r "$sb1 $sb2"]
	dl_local S12i [dl_sortedFunc $allS12i "$sb1 $sb2"]
	dl_local S1 [dl_sortedFunc $allS1 "$sb1 $sb2"]
	dl_local S2 [dl_sortedFunc $allS2 "$sb1 $sb2"]

	dl_local C12r [dl_div $S12r:2 [dl_sqrt [dl_mult $S1:2 $S2:2]]]
	dl_local C12i [dl_div $S12i:2 [dl_sqrt [dl_mult $S1:2 $S2:2]]]
	dl_local cohmag [dl_sqrt [dl_add [dl_mult $C12r $C12r] [dl_mult $C12i $C12i]]]
	dl_local cohphase [dl_atan [dl_div $C12i $C12r]]

	#Error bars
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_local ntrials [dl_float [dl_lengths [dl_sortByLists $allcohmag $sb1 $sb2]]]
	    dl_local mtfft1 [dl_sortByLists $data:6 $sb1 $sb2]
	    dl_local mtfft2 [dl_sortByLists $data:7 $sb1 $sb2]
	    if {$::mtspec::Args(fscorr)} {
		dl_local numspks1 [dl_sums [dl_sortByLists [dl_sums $binnedSpikes1] $sb1 $sb2]]
		dl_local numspks2 [dl_sums [dl_sortByLists [dl_sums $binnedSpikes2] $sb1 $sb2]]
	    } else  {
		set numspks1 ""
		set numspks2 ""
	    }
	    dl_local stats [::helpmtspec::coherror $cohmag $mtfft1 $mtfft2 $ntrials [lindex $::mtspec::Args(p) 1] [lindex $::mtspec::Args(p) 0] $numspks1 $numspks2]
	    dl_local confC $stats:0
	    dl_local Cerr $stats:1
	    dl_local phistd $stats:2
	}

	#creating out group
	set out [dg_create]
	dl_set $out:freqs $data:8
	dl_set $out:cohmag [dl_llist $S12r:0 $S12r:1 $cohmag]
	dl_set $out:cohphase [dl_llist $S12r:0 $S12r:1 $cohphase]
	if {[lindex $::mtspec::Args(p) 0] != 0} {
	    dl_set $out:confC [dl_llist $S12r:0 $S12r:1 $confC]
	    dl_set $out:Cerrl [dl_llist $S12r:0 $S12r:1 $Cerr:0]
	    dl_set $out:Cerru [dl_llist $S12r:0 $S12r:1 $Cerr:1]
	    dl_set $out:phistd [dl_llist $S12r:0 $S12r:1 $phistd]
	}
	dl_set $out:S12r $S12r
	dl_set $out:S12i $S12i
	dl_set $out:S1 $S1
	dl_set $out:S2 $S2
	dl_set $out:individualmag $allcohmag
	dl_set $out:individualphase $allcohphase
	dl_set $out:label1 [dl_llist $name1 $sb1]
	dl_set $out:label2 [dl_llist $name2 $sb2]
	return $out
    }
    
    proc CreateSTAlfpSFCGroup {g chan1 chan2 segHalfLength sortlists} {
	#basic parameters 
	set lfpchan $chan1
	set spkchan $chan2
	dl_local sb1 [dl_get $sortlists:0 1]
	dl_local sb2 [dl_get $sortlists:1 1]

	#actually get the spike-triggered lfp snippets
	dl_local stLFPsnips [::helpmtspec::sta_snips $g $lfpchan $spkchan $segHalfLength]
	
	#we need to compute the power of all the individual st LFP snippets, but first we need to sort them;
	dl_local sortedSTLFPsnips [dl_unpack [dl_sortByLists $stLFPsnips $sb1 $sb2]]
	dl_local STALFP [dl_means $sortedSTLFPsnips]
	dl_local labels [dl_uniqueCross $sb1 $sb2]

	#average power per individual st lfp segment computed here
	dl_local STApower [dl_llist]
	dl_local SFC [dl_llist]
	for { set i 0 } {$i < [dl_length $sortedSTLFPsnips] } { incr i } {
	    if { [dl_length $sortedSTLFPsnips:$i] == 0} {
		dl_append $SFC [dl_slist "no spikes in this condition"]
		continue
	    }
	    dl_local tempSTApower [::helpmtspec::mtspectrum [dl_llist $STALFP:$i] "continuous"]
	    dl_local freqs $tempSTApower:2
	    dl_append $STApower [dl_unpack $tempSTApower:0]
	    dl_local snipPowers [::helpmtspec::mtspectrum $sortedSTLFPsnips:$i "continuous"]:0
	    dl_local avgSnipPower [dl_meanList $snipPowers]
	    dl_append $SFC [dl_div $STApower:$i $avgSnipPower]; #the actual sfc computed here
	}

	#create out group
	set out [dg_create]
	dl_set $out:time [dl_fromto -$segHalfLength $segHalfLength 2]
	dl_set $out:STALFP [dl_llist $labels:0 $labels:1 $STALFP]
	dl_set $out:freqs $freqs
	dl_set $out:STApower [dl_llist $labels:0 $labels:1 $STApower]
	dl_set $out:cohmag [dl_llist $labels:0 $labels:1 $SFC]
	set name1  [dl_get $sortlists:0 0]
	set name2  [dl_get $sortlists:0 0]
	dl_set $out:label1 [dl_llist $name1 $sb1]
	dl_set $out:label2 [dl_llist $name2 $sb2]
	return $out
    }

    ###############################################################################
    #
    #
    #                              PLOTTING
    #
    #
    ############################################################################### 

    proc GenerateSpectrumPlots {g} {
	set plots ""
	foreach main [dl_tcllist [dl_unique $g:power:1]] {
	    dl_local splits [dl_select $g:power:0 [dl_eq $g:power:1 $main]]
	    set p [dlp_newplot]
	    set x [dlp_addXData $p $g:freqs]
	    set plotcntr 0
	    foreach split [dl_tcllist $splits] {
		dl_local y [dl_select $g:power:2 [dl_and [dl_eq $g:power:0 $split] [dl_eq $g:power:1 $main]]]
		if { $::mtspec::Args(log)} {
		    dl_local y [dl_mult [dl_log10 $y] 10]
		}
		set y [dlp_addYData $p $y]
		set colorcntr [expr $plotcntr%8]
		dlp_draw $p lines "$x $y" -linecolor $::helpmtspec::cols($colorcntr) -lwidth 20
		set n [dl_sum [dl_and [dl_eq $g:label1:1 $split] [dl_eq $g:label2:1 $main]]]
		
		dlp_wincmd $p {0 0 1 1} "dlg_text .95 [expr .9-$plotcntr*.05] {$split ($n)} -color $::helpmtspec::cols($colorcntr) -just 1"

		if {[lindex $::mtspec::Args(p) 0] != 0} {
		    dl_local y1 [dl_select $g:Serrl:2 [dl_and [dl_eq $g:Serrl:0 $split] [dl_eq $g:Serrl:1 $main]]]
		    dl_local y2 [dl_select $g:Serru:2 [dl_and [dl_eq $g:Serru:0 $split] [dl_eq $g:Serru:1 $main]]]
		    if { $::mtspec::Args(log)} {
			dl_local y1 [dl_mult [dl_log10 $y1] 10]
			dl_local y2 [dl_mult [dl_log10 $y2] 10]
		    }
		    set y1 [dlp_addYData $p $y1]
		    set y2 [dlp_addYData $p $y2]
		    dlp_draw $p lines "$x $y1" -linecolor $::helpmtspec::cols($colorcntr) -lwidth 20 -lstyle 2
		    dlp_draw $p lines "$x $y2" -linecolor $::helpmtspec::cols($colorcntr) -lwidth 20 -lstyle 2
		}

		set n [dl_sum [dl_and [dl_eq $g:label1:1 $split] [dl_eq $g:label2:1 $main]]]
		dlp_wincmd $p {0 0 1 1} "dlg_text .95 [expr .9-$plotcntr*.05] {$split ($n)} -color $::helpmtspec::cols($colorcntr) -just 1"
		incr plotcntr
	    }

	    dlp_wincmd $p {0 0 1 1} "dlg_text .95 .95 {[dl_tcllist $g:label1:0] (N)} -color $::colors(gray) -just 1"
	    dlp_set $p xtitle "Frequency (Hz)"
	    if { $::mtspec::Args(log)} {
		dlp_set $p ytitle "Power (dB)"
	    } else {
		dlp_set $p ytitle "Power"
	    }

	    #main title
	    dlp_set $p title "[dl_tcllist $g:label2:0] :: $main"
	    lappend plots $p
    	}
	return $plots
    }

    proc GenerateCoherencePlots {g} {
	set plots ""
	foreach main [dl_tcllist [dl_unique $g:cohmag:1]] {
	    dl_local splits [dl_select $g:cohmag:0 [dl_eq $g:cohmag:1 $main]]
	    set p [dlp_newplot]
	    set x [dlp_addXData $p $g:freqs]
	    set plotcntr 0
	    foreach split [dl_tcllist $splits] {
		dl_local y [dl_select $g:cohmag:2 [dl_and [dl_eq $g:cohmag:0 $split] [dl_eq $g:cohmag:1 $main]]]
		set y [dlp_addYData $p $y]
		dlp_draw $p lines "$x $y" -linecolor $::helpmtspec::cols($plotcntr) -lwidth 20
		set n [dl_sum [dl_and [dl_eq $g:label1:1 $split] [dl_eq $g:label2:1 $main]]]
		dlp_wincmd $p {0 0 1 1} "dlg_text .95 [expr .9-$plotcntr*.05] {$split ($n)} -color $::helpmtspec::cols($plotcntr) -just 1"

		if {[lindex $::mtspec::Args(p) 0] != 0} {
		    set confC [dl_tcllist [dl_select $g:confC:2 [dl_and [dl_eq $g:confC:0 $split] [dl_eq $g:confC:1 $main]]]]
		    dl_local y1 [dl_select $g:Cerrl:2 [dl_and [dl_eq $g:Cerrl:0 $split] [dl_eq $g:Cerrl:1 $main]]]
		    dl_local y2 [dl_select $g:Cerru:2 [dl_and [dl_eq $g:Cerru:0 $split] [dl_eq $g:Cerru:1 $main]]]
		    if {[lindex $::mtspec::Args(p) 0] == 1} {
			# for theoretical, draw only the confidence level line
			set yval [dlp_addYData $p $confC]
			dlp_draw $p lines "$x $yval" -linecolor $::helpmtspec::cols($plotcntr) -lwidth 20 -lstyle 3
		    } else {
			# for jackknife, draw the error bars
			set y1 [dlp_addYData $p $y1]
			set y2 [dlp_addYData $p $y2]
			dlp_draw $p lines "$x $y1" -linecolor $::helpmtspec::cols($plotcntr) -lwidth 20 -lstyle 2
			dlp_draw $p lines "$x $y2" -linecolor $::helpmtspec::cols($plotcntr) -lwidth 20 -lstyle 2
		    }
		}

		incr plotcntr
	    }

	    dlp_wincmd $p {0 0 1 1} "dlg_text .95 .95 {[dl_tcllist $g:label1:0] (N)} -color $::colors(gray) -just 1"
	    dlp_setyrange $p 0 [expr min([expr 1.25 * [dl_max $p:ydata]],1)]
	    dlp_set $p xtitle "Frequency (Hz)"
	    dlp_set $p ytitle "Coherence"

	    #main title
	    dlp_set $p title "[dl_tcllist $g:label2:0] :: $main"
	    lappend plots $p
    	}
	return $plots
    }

    proc GenerateSpecgramPlot { data freqs times plotmin plotmax {realmax ""}} {
	dl_local data [dl_unpack [dl_transpose $data]]

	set factor 63; # for the 64 different rgb combinations comprising colormap
	if {$realmax == ""} {
	    set realmax $plotmax
	}
	dl_local colormap [::helpmtspec::create_colormap $::mtspec::Args(colormap)]
	dl_local subtracted [dl_sub $data $plotmin]
	dl_local normalized [dl_div $subtracted $plotmax]
	dl_local scaled [dl_int [dl_mult $normalized $factor]]
	dl_local str [dl_unpack [dl_transpose [dl_breverse $scaled]]]
  	dl_local reds [dl_choose $colormap:0 $str]
	dl_local greens [dl_choose $colormap:1 $str]
	dl_local blues [dl_choose $colormap:2 $str]
	dl_local rgbs [dl_char [dl_unpack [dl_combineLists $reds $greens $blues]]]
	dl_local size [dl_ilist [dl_length $times] [dl_length $freqs]]
	dl_local image [dl_llist $size $rgbs]

	set p [dlp_newplot]
	dlp_setxrange $p [dl_min $times] [dl_max $times]
	dlp_setyrange $p [dl_min $freqs] [dl_max $freqs]
	dlp_set $p image $image
	dlp_cmd $p "dlg_image [dl_min $times] [dl_min $freqs] %p:image [expr [dl_max $times] - [dl_min $times]]  [expr [dl_max $freqs] - [dl_min $freqs]]"

	dlp_set $p xtitle "time (ms)"
	dlp_set $p ytitle "Frequencies (Hz)"

	###bar showing the colorscale
	dl_local rgbs_legend [dl_char [dl_unpack [dl_reverse [dl_combineLists $colormap:0 $colormap:1 $colormap:2]]]]
	dl_local image_legend [dl_llist "1 64" $rgbs_legend]
	dlp_set $p image_legend $image_legend
	dlp_winpostcmd $p {0 0 1 1} "dlg_image 1.025 .75 %p:image_legend .05 .25"
	dlp_winpostcmd $p {0 0 1 1} "dlg_text 1.0475 .77 { $plotmin} -just 0 -size 6"
	dlp_winpostcmd $p {0 0 1 1} "dlg_text 1.0475 .99 { $realmax} -just 0 -size 6 -color $::colors(black)"
	return $p
    }

    ###############################################################################
    #
    #
    #        Basic parameters, argument parsing, sortlists, data extraction, etc
    #
    #
    ############################################################################### 

    proc CreateInitArgsArray {flag} {
	set ::mtspec::Args(plot) 1
	set ::mtspec::Args(tdt) 1
	set ::mtspec::Args(pretime) -50
	set ::mtspec::Args(posttime) 500
	set ::mtspec::Args(align) "stimon"
	set ::mtspec::Args(zero_align) "same"
	set ::mtspec::Args(zero_range) "-50 0"
	set ::mtspec::Args(nw) 3
	set ::mtspec::Args(ntapers) 5
	set ::mtspec::Args(fpass) "0 100"
	set ::mtspec::Args(log) 1
	set ::mtspec::Args(pad) 0
	set ::mtspec::Args(limit) "none"
	if {$flag == "continuous"} {
	    set ::mtspec::Args(samp_freq) 1017.252604167
	} elseif {$flag == "point"} {
	    set ::mtspec::Args(samp_freq) 1000
	}
	set ::mtspec::Args(colormap) "hot"
	set ::mtspec::Args(p) "0 0"
	set ::mtspec::Args(fscorr) 0
    }

    proc ParseArgs {inargs} {
	set args [lindex $inargs 0]
	#set args $inargs
	foreach a $args i [dl_tcllist [dl_fromto 0 [llength $args]]] {
	    if {(![string match [string index $a 0] "-"]) || \
		    [string is integer [lindex $a 0]]} {
		continue
	    } else {
		switch -exact -- "$a" {
		    -reset {::helpmtspec::CreateInitArgsArray}
		    -plot {set ::mtspec::Args(plot) [lindex $args [expr $i + 1]]}
		    -tdt { set ::mtspec::Args(tdt) [lindex $args [expr $i + 1]]}
		    -pretime { set ::mtspec::Args(pretime) [lindex $args [expr $i + 1]]}
		    -posttime { set ::mtspec::Args(posttime) [lindex $args [expr $i + 1]]}
		    -start { set ::mtspec::Args(pretime) [lindex $args [expr $i + 1]]}
		    -stop { set ::mtspec::Args(posttime) [lindex $args [expr $i + 1]]}
		    -align { set ::mtspec::Args(align) [lindex $args [expr $i + 1]]}
		    -zero_align { set ::mtspec::Args(zero_align) [lindex $args [expr $i + 1]]}
		    -zero_range { set ::mtspec::Args(zero_range) [lindex $args [expr $i + 1]]}
		    -ntapers { set ::mtspec::Args(ntapers) [lindex $args [expr $i + 1]]}
		    -nw { set ::mtspec::Args(nw) [lindex $args [expr $i + 1]]}
		    -fpass { set ::mtspec::Args(fpass) [lindex $args [expr $i + 1]]}
		    -log { set ::mtspec::Args(log) [lindex $args [expr $i + 1]] }
		    -pad { set ::mtspec::Args(pad) [lindex $args [expr $i + 1]]}
		    -limit { set ::mtspec::Args(limit) [lindex $args [expr $i + 1]]}
		    -samp_freq { set ::mtspec::Args(samp_freq) [lindex $args [expr $i + 1]]}
		    -colormap { set ::mtspec::Args(colormap) [lindex $args [expr $i + 1]]}
		    -p { set ::mtspec::Args(p) [lindex $args [expr $i + 1]]}
		    -fscorr { set ::mtspec::Args(fscorr) [lindex $args [expr $i + 1]]}
		    default {puts "Illegal option: $a\nShould be reset, plot, tdt, pretime, posttime, align, zero_align, zero_range, nw, ntapers, fpass, log, pad, limit, samp_freq, colormap, p, fscorr.\nWill analyze ignoring your entry."}
		}
	    }
	}
    }

    proc MakeSortLists {g sb} {
	# are we sorting by 1 or 2 lists?
	if {[llength $sb] > 2} {
	    error "::mtspec:: - can't handle more than two sort variables at a time"
	}
	if {[llength $sb] == 1} {
	    set label1 [lindex $sb 0]
	    dl_local split_vals1 $g:$label1
	    set label2 sorted_by
	    dl_local split_vals2 [dl_repeat [dl_slist "$label1"] [dl_length $g:ids]]
	} else {
	    set label1 [lindex $sb 0]
	    dl_local split_vals1 $g:$label1
	    set label2 [lindex $sb 1]
	    dl_local split_vals2 $g:$label2
	}
	dl_return [dl_llist [dl_llist [dl_slist $label1] $split_vals1] [dl_llist [dl_slist $label2] $split_vals2]]
    }

    #extract the desired raw LFP signal
    proc get_lfp { g chan } {
	set pre  $::mtspec::Args(pretime)
	set post $::mtspec::Args(posttime)
	set range "$pre $post"
	set align $::mtspec::Args(align)
	set z_r $::mtspec::Args(zero_range)
	set z_2 $::mtspec::Args(zero_align)
	set samp_rate $::mtspec::Args(samp_freq)
	set samps_per_ms [expr $samp_rate/1000.]
	
	set minus_pre [expr -1*$pre]

	set dglists [dg_tclListnames $g]
	set LFPs [lsearch -all $dglists LFP_${align}_*]
	if { $LFPs == "-1" } { error "LFPs not found in $g" }

	set lfpdata ""
	foreach match $LFPs {
	    set this_list [lindex $dglists $match]
	    if { [scan $this_list LFP_${align}_%d_%d this_pre this_post] == 2 } {
		if { $pre >= [expr -1*$this_pre] && $post <= $this_post } {
		    set this_start [expr -1*$this_pre]
		    set this_stop $this_post
		    set lfpdata $this_list
		    break
		}
	    }
	}
	if { $lfpdata == "" } { error "no valid LFP data found" }

	# TDT channels start from 1
	if { $::mtspec::Args(tdt) } {
	    set this_chan [expr $chan-1]
	} else {
	    set this_chan $chan
	}
	dl_local alleeg [dl_unpack [dl_choose $g:$lfpdata [dl_llist $this_chan]]]
	
	###where zeroing takes place
	if {[llength $z_r] == 2} {
	    set zeroDur [expr [lindex $z_r 1]-[lindex $z_r 0]]
	    set startInd [expr max(0, int(([lindex $z_r 0]-$this_start)/$samps_per_ms))]
	    set stopInd [expr $startInd+int($zeroDur/$samps_per_ms)]
	    puts "$z_r $startInd $stopInd ($this_start/$this_stop)"
	    dl_local zeroInd [dl_fromto $startInd $stopInd]
	    dl_local zr [dl_choose $alleeg [dl_llist $zeroInd]]
	    dl_local alleeg [dl_sub $alleeg [dl_means $zr]]
	}

	# now choose selected range
	set startInd [expr int(($pre-$this_start)/$samps_per_ms)]
	set dur [expr $post-$pre]
	set stopInd [expr int($dur/$samps_per_ms)+$startInd]
	dl_local selInd [dl_llist [dl_fromto $startInd $stopInd]]
	
	dl_return [dl_choose $alleeg $selInd]
    }

    proc get_lfp_orig { g chan } {
	set range "$::mtspec::Args(pretime) $::mtspec::Args(posttime)"
	set align $::mtspec::Args(align)
	set z_r $::mtspec::Args(zero_range)
	set z_2 $::mtspec::Args(zero_align)

	#get the lfp
	dl_local alleeg_raw [dl_unpack [dl_select $g:eeg [dl_eq $g:eeg_chans $chan]]]
	dl_local alleeg_time [dl_unpack [dl_select $g:eeg_time [dl_eq $g:eeg_chans $chan]]]

	#to what are we zeroing?
	if { $z_2 != "same" } {
	    dl_local alleeg_time_zeroed [eval dl_sub $alleeg_time $g:$z_2]
	} else {
	    dl_local alleeg_time_zeroed [eval dl_sub $alleeg_time $g:$align]
	}
	set samp_rate [expr [lindex [split [dl_tcllist $g:eeg_info]] 2] /1000.]
	set samp_num [expr ([lindex $range 1] - [lindex $range 0]) / $samp_rate]
	dl_local first_samps [dl_minIndices [dl_abs [dl_sub $alleeg_time_zeroed [lindex $range 0]]]]
	dl_local add2 [dl_repeat [dl_llist [dl_fromto 0  $samp_num]] [dl_length $first_samps]]
	dl_local get_list [dl_int [dl_add $add2 $first_samps]]
	dl_local alleeg [dl_choose $alleeg_raw $get_list]

	###where zeroing takes place
	if {[llength $z_r] == 2} {
	    set samp_num_zero [expr ([lindex $z_r 1] - [lindex $z_r 0]) / $samp_rate]
	    dl_local zero_pre [dl_minIndices [dl_abs [dl_sub $alleeg_time_zeroed [lindex $z_r 0]]]]
	    dl_local add2z [dl_repeat [dl_llist [dl_fromto 0  $samp_num_zero]] [dl_length $zero_pre]]
	    dl_local get_zero [dl_int [dl_add $add2z $zero_pre]]
	    dl_local zr [dl_choose $alleeg_raw $get_zero]
	    dl_local alleeg [dl_sub $alleeg [dl_means $zr]]
	}
	dl_return $alleeg
    }

    proc spike_extract { g channel } {
	dl_local spk_selector [dl_eq $g:spk_channels $channel]
	dl_local aligned_spikes [dl_sub [dl_select $g:spk_times $spk_selector] $g:$::mtspec::Args(align)]
	dl_local restricted_spikes [dl_select $aligned_spikes [dl_betweenEqLow $aligned_spikes $::mtspec::Args(pretime) \
								   $::mtspec::Args(posttime)]]
	dl_return $restricted_spikes
    }

    #extract all the individual spike-triggered LFPs and leave them in raw trial from
    proc sta_snips { g lfpchan spkchan segHalfLength} {
	set range "$::mtspec::Args(pretime) $::mtspec::Args(posttime)"
	set align $::mtspec::Args(align)
	set samp_rate [expr 1./$::mtspec::Args(samp_freq) * 1000]

	dl_local spk_times [::helpmtspec::spike_extract $g $spkchan]

	#We need a few extra samples prior to the pretime and after the posttime,
	# but make sure to immediately reset the parameters to their original values
	# You could also do it the other way around - narrow the window from which spikes are extracted
	set ::mtspec::Args(pretime) [expr $::mtspec::Args(pretime) - $segHalfLength]
	set ::mtspec::Args(posttime) [expr $::mtspec::Args(posttime) + $segHalfLength]
	dl_local all_eeg [::helpmtspec::get_lfp $g $lfpchan]
	set ::mtspec::Args(pretime) [lindex $range 0]
	set ::mtspec::Args(posttime) [lindex $range 1]
	############################################################################

	#this is my attempt at slight vectorization of the extraction process
	dl_local eeg_times [dl_repeat [dl_llist [dl_fromto [expr [lindex $range 0] - $segHalfLength] [expr [lindex $range 1] + $segHalfLength] $samp_rate]] [dl_length $all_eeg]]
	dl_local str_eeg_times [dl_choose [dl_pack $eeg_times] [dl_zeros [dl_lengths $spk_times]]]
	dl_local str_eeg_values [dl_choose [dl_pack $all_eeg] [dl_zeros [dl_lengths $spk_times]]]
	dl_local str_rel_times [dl_sub $str_eeg_times $spk_times]
	dl_local selector [dl_betweenEqLow $str_rel_times -$segHalfLength $segHalfLength]
	dl_local sta_snips [dl_select $str_eeg_values $selector]

	#Normalize each segment by its time-average
	dl_local sta_snips [dl_sub $sta_snips [dl_bmeans $sta_snips]]
	dl_return $sta_snips
    }

    #code for creating combinations
    proc doublets { n } {
	# First create all possible combinations
	dl_local l0 [dl_replicate [dl_fromto 0 $n] $n]
	dl_local l1 [dl_repeat [dl_fromto 0 $n] $n]

	# Now combine into doublets and sort
	dl_local all [dl_combineLists $l0 $l1]
	dl_local sorted [dl_sort $all]

	# Now create unique ids by multiplying and summing
	dl_local mult_sorted [dl_mult $sorted [dl_llist "1 $n"]]
	dl_local unique_ids [dl_sums $mult_sorted]

	# Find indices of first doublet for each unique element
	dl_local u [dl_eq [dl_rank $unique_ids] 0]

	# Find doublets that don't have repeats
	dl_local alldiff [dl_eq [dl_lengths [dl_unique $sorted]] 2]

	dl_local good [dl_and $u $alldiff]

	# Return selected combinations
	dl_return [dl_llist \
		       [dl_select $l0 $good] \
		       [dl_select $l1 $good]]
    }

    proc all_doublets { l } {
	set n [dl_length $l]
	dl_local indices [::helpmtspec::doublets $n]
	dl_return [dl_choose $l $indices]
    }
}
