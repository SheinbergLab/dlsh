# This package provides basic tools to perform autocorrelogram and crosscorrelogram analysis on point process and LFP data

# AUTHORS
#   May 2010 - LW

package provide corrgram 1.0

namespace eval ::corrgram { } {
    
    proc spikecc { g chan1 chan2 sortby args } {
	# %HELP%
	# ::corrgram::spikecc <g> <chan1> <chan2> <sortby> ?<args>?
	#
	#
	#    Construct correlogram for channels <chan1> and <chan2> in group <g>  
	#    sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables, although considering the number
	#                  of spikes required to construct these things, it's unlikely
	#                  you'll want to use two sort variables
	#
	#    Optional Arguments <DEFAULTS>:
	#    -pretime <50> time before align variable to get; this is absolute time, 
	#                  so negative values correspond to times BEFORE align event;
	#                  here, the +50 ms roughly corrects for the response latency 
	#                  of IT neurons
	#    -posttime <250> time after align variable to get
	#    -align <stimon> on what event to align spikes
	#    -lags <50> lag, in ms, over which to compute the correlogram; meaningful
	#                  values exist over the range +/-[postime - pretime - 1]
	#    -mintrialspikes <4> minimum number of spikes in either channel in order for
	#                  a trial to contribute to the final correlogram
	#    -plot <1> plot correlogram?
	#    -correction <allway> correction term to use to account for stimulus 
	#                  correlation: "allway" or "oneway"
	#                  *only makes difference for plotting because group returns both
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#                  a dl list for selection
	#
	#    For info, see Bair et al. (2001), Smith and Kohn (2005, 2008) in JNeurosci
	#
	# %END%

	#do we have spike data to analyze?
	if ![dl_exists $g:spk_times] {
	    error "No spike data to analyze"
	}

	#build initial array
	::helpcorrgram::CreateInitArgsArray 
	
	#modify the options array according to user set options
	if { [llength $args] } {
	    ::helpcorrgram::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::corrgram::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::corrgram::Args(limit) 1]
	} 

	#create sortlists
	dl_local sortlists [::helpcorrgram::MakeSortLists $g $sortby]

	#sort the spikes
	set g1 [::helpcorrgram::CreateSpikeAnalGroup $g "$chan1 $chan2" $sortlists]

	#do the actual correlogram analysis
	set g2 [::helpcorrgram::CorrSpikes $g1]

	#delete the copied list
	if {$::corrgram::Args(limit) != "none"} {
	    dg_delete $g
	}

	#plotting
	if {$::corrgram::Args(plot)} {

	    if {$::corrgram::Args(correction)=="allway"} {
		dl_local data $g2:smoothAllwayCorrectedCorr
	    } elseif {$::corrgram::Args(correction)=="oneway"} {
		dl_local data $g2:smoothOnewayCorrectedCorr
	    }
	    
	    dl_local label1 [dl_unique $g2:label1]
	    dl_local label2 [dl_unique $g2:label2]

	    set npanels [dl_length $label2]
	    clearwin
	    dlp_setpanels 1 $npanels
	    set counter 0
	    for {set i 0} {$i < $npanels} {incr i} {
		set p [dlp_newplot]
		set x [dlp_addXData $p $g2:lags:0]
		for {set j 0} {$j < [dl_length $label1]} {incr j} {
		    dl_local y $data:$counter
		    if {[dl_datatype $y] == "string"} {
			dlp_wincmd $p {0 0 1 1} "dlg_text .9 [expr .9 - $j*.02] {not enough data - [dl_tcllist $label1:$j]} -color $::helpmtspec::cols($j) -just 1"
			incr counter
			continue
		    }
		    set y [dlp_addYData $p $y]
		    dlp_draw $p lines "$x $y" -linecolor $::helpcorrgram::cols($j) -lwidth 200
		    dlp_wincmd $p {0 0 1 1} "dlg_text .9 [expr .9 - $j*.02] $label1:$j -color $::helpmtspec::cols($j) -just 1"
		    incr counter
		}
		dlp_set $p xtitle "Lag (ms)"
		dlp_set $p ytitle "Coinc. / Spike"
		dlp_setxrange $p [dl_min $g2:lags:0] [dl_max $g2:lags:0]
		
		#main title
		if {$npanels > 1} {
		    dlp_set $p title "Correlogram for [lindex $sortby 1] = [dl_tcllist $label2:$i]"
		} else {
		    dlp_set $p title "Correlogram"
		}
		dlp_subplot $p $i
	    }
	}
	return $g2	
    }

    #Normalization isn't quite finalized, partly due to the fact there is no definitive reference
    proc lfpcc { g chan1 chan2 sortby args } {
	# %HELP%
	# ::corrgram::lfpcc <g> <chan1> <chan2> <sortby> ?<args>?
	#
	# 
	#    Construct correlogram for channels <chan1> and <chan2> in group <g>  
	#    sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables  
	#
	#    Optional Arguments <DEFAULTS>:
	#    -pretime <50> time before align variable to get; this is absolute time, 
	#                  so negative values correspond to times BEFORE align event;
	#    -posttime <250> time after align variable to get
	#    -align <stimon> on what events to align spikes
	#    -lags <50> lag, in ms, over which to compute the correlogram
	#    -zero_align <same> if you want to zero relative to something other than -align list
	#    -zero_range <"-50 0"> enter a two item tcl list of the 
	#      times to use as the basis for zeroing the eeg sample.  
	#      Times are relative to -align or overwritten by -zero_align list
	#    -plot <1> plot correlogram?
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#                  a dl list for selection
	#
	# %END%

	#do we have lfp data to analyze?
	if ![dl_exists $g:eeg] {
	    error "No lfp data to analyze"
	}

	#build initial array
	::helpcorrgram::CreateInitArgsArray 
	
	#modify the options array according to user set options
	if { [llength $args] } {
	    ::helpcorrgram::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::corrgram::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::corrgram::Args(limit) 1]
	} 

	#create sortlists
	dl_local sortlists [::helpcorrgram::MakeSortLists $g $sortby]

	#get two sets of LFP
	set g1 [::helpcorrgram::CreateLFPAnalGroup $g $chan1 $sortlists]
	set g2 [::helpcorrgram::CreateLFPAnalGroup $g $chan2 $sortlists]
	
	#do the correlation analysis
	set g3 [::helpcorrgram::CorrLFP $g1 $g2 ]
	
	#delete the copied list
	if {$::corrgram::Args(limit) != "none"} {
	    dg_delete $g
	}

	#plotting
	if {$corrgram::Args(plot)} {
	    dl_local label1 [dl_unique $g3:label1]
	    dl_local label2 [dl_unique $g3:label2]
	    dl_local data $g3:onewayCorrectedCorr

	    set npanels [dl_length $label2]
	    clearwin
	    dlp_setpanels 1 $npanels
	    set counter 0
	    for {set i 0} {$i < $npanels} {incr i} {
		set p [dlp_newplot]
		set x [dlp_addXData $p $g3:lags:0]
		for {set j 0} {$j < [dl_length $label1]} {incr j} {
		    dl_local y $data:$counter
		    if {[dl_datatype $y] == "string"} {
			dlp_wincmd $p {0 0 1 1} "dlg_text .9 [expr .9 - $j*.02] {not enough data - [dl_tcllist $label1:$j]} -color $::helpmtspec::cols($j) -just 1"
			incr counter
			continue
		    }
		    set y [dlp_addYData $p $y]
		    dlp_draw $p lines "$x $y" -linecolor $::helpcorrgram::cols($j) -lwidth 200
		    dlp_wincmd $p {0 0 1 1} "dlg_text .9 [expr .9 - $j*.02] $label1:$j -color $::helpmtspec::cols($j) -just 1"
		    incr counter
		}
		dlp_set $p xtitle "Lag (ms)"
		dlp_set $p ytitle "Normalized Correlation"
		dlp_setxrange $p [dl_min $g3:lags:0] [dl_max $g3:lags:0]
		
		#main title
		if {$npanels > 1} {
		    dlp_set $p title "Correlogram for [lindex $sortby 1] = [dl_tcllist $label2:$i]"
		} else {
		    dlp_set $p title "Correlogram"
		}
		dlp_subplot $p $i
	    }
	}
	return $g3
    }
}

namespace eval ::helpcorrgram { } {

    set cols(0) 3
    set cols(1) 5
    set cols(2) 10
    set cols(3) 14
    set cols(4) 4
    set cols(5) 2
    set cols(6) 1
    set cols(7) 12
    set cols(99) 8
     
    proc MakeSortLists {g sb} {
	# are we sorting by 1 or 2 lists?
	if {[llength $sb] > 2} {
	    error "::corrgram:: - can't handle more than two sort variables at a time"
	}
	if {[llength $sb] == 1} {
	    set label1 [lindex $sb 0]
	    dl_local split_vals1 $g:$label1 
	    set label2 sorted_by
	    dl_local split_vals2 [dl_repeat [dl_slist "$label1"] [dl_length $g:spk_times]]
	} else {
	    set label1 [lindex $sb 0]
	    dl_local split_vals1 $g:$label1
	    set label2 [lindex $sb 1]
	    dl_local split_vals2 $g:$label2
	}
	dl_return [dl_llist [dl_llist [dl_slist $label1] $split_vals1] [dl_llist [dl_slist $label2] $split_vals2]]
    }

    proc CorrSpikes { g } {
	set epochLength [expr $::corrgram::Args(posttime) - $::corrgram::Args(pretime)]
	set n_lags [expr $::corrgram::Args(lags) * 2 + 1]
	dl_local spikes1 $g:raw_spktimes:0
	dl_local spikes2 $g:raw_spktimes:1
	dl_local label1 $g:label1:0
	dl_local label2 $g:label2:0
	
	#ROWS = TRIALS, COLUMNS = CONDITIONS
	
	#for each condition...
	dl_local rawCorrByCond [dl_llist]
	dl_local normCorrByCond [dl_llist]
	dl_local rawAllwayShuffleByCond [dl_llist]
	dl_local normAllwayShuffleByCond [dl_llist]
	dl_local rawOnewayShuffleByCond [dl_llist]
	dl_local normOnewayShuffleByCond [dl_llist]
	dl_local allwayCorrectedCorrByCond [dl_llist]
	dl_local onewayCorrectedCorrByCond [dl_llist]
	dl_local smoothAllwayCorrectedCorrByCond [dl_llist]
	dl_local smoothOnewayCorrectedCorrByCond [dl_llist]
	dl_local spk1countsByCond [dl_llist]
	dl_local spk2countsByCond [dl_llist]
	for { set i 0 } {$i < [dl_length $spikes1]} {incr i} {
	    
	    set n_trials [dl_length $spikes1:$i].
	 
	    #for each trial...
	    dl_local overlap [dl_llist]
	    dl_local overlapShuff [dl_llist]
	    dl_local spk1count [dl_ilist]
	    dl_local spk2count [dl_ilist]
	    dl_local psth1 [dl_llist]
	    dl_local psth2 [dl_llist]
	    dl_local validTrials [dl_ilist] 
	    set n_trials_included 0
	    for { set j 0 } {$j < $n_trials} {incr j} {
		
		#let's make sure we only include trials in which there were at least a minimum number of spikes
		if { [dl_length $spikes1:$i:$j] < $::corrgram::Args(mintrialspikes) ||\
			 [dl_length $spikes2:$i:$j] < $::corrgram::Args(mintrialspikes)} {
		    continue
		}
		
		dl_append $validTrials $j
				
		#let's construct actual spike train where 0 = no spike and 1 = spike with a resolution of 1ms
		dl_local v [dl_zeros $epochLength]
		dl_local spktrain1 [dl_replaceByIndex $v [dl_sub [dl_floor $spikes1:$i:$j] $::corrgram::Args(pretime)] 1]
		dl_local spktrain2 [dl_replaceByIndex $v [dl_sub [dl_floor $spikes2:$i:$j] $::corrgram::Args(pretime)] 1]
		dl_append $psth1 $spktrain1
		dl_append $psth2 $spktrain2
		
		#let's store spike count numbers for potential later use
		dl_append $spk1count [dl_sum $spktrain1]
		dl_append $spk2count [dl_sum $spktrain2]
		
		#do actual cross-correlation
		dl_append $overlap [::helpcorrgram::crosscorr $spktrain1 $spktrain2 $::corrgram::Args(lags)]
		
		#up counter
		incr n_trials_included
	    }
	    
	    #take average over trials
	    dl_local rawCorr [dl_meanList $overlap]
	    
	    #repeat step above but now offset by one trial the pair of cells, resulting in shift predictor (oneway shuffle)
	    if {[dl_length $validTrials] >0 } {
		set zero [dl_tcllist $validTrials:0]
	    }
	    for { set j 0 } {$j < $n_trials_included} {incr j} {
		set curTrial [dl_tcllist $validTrials:$j]
		
		#let's construct actual spike train where 0 = no spike and 1 = spike with a resolution of 1ms
		dl_local v [dl_zeros $epochLength]
		dl_local spktrain1 [dl_replaceByIndex $v [dl_sub [dl_floor $spikes1:$i:$curTrial] $::corrgram::Args(pretime)] 1]
		if { $j == [expr $n_trials_included - 1] } {
		    dl_local spktrain2 [dl_replaceByIndex $v [dl_sub [dl_floor $spikes2:$i:$zero] $::corrgram::Args(pretime)] 1]
		} else {
		    set offsetTrial [dl_tcllist $validTrials:[expr $j + 1]]
		    dl_local spktrain2 [dl_replaceByIndex $v [dl_sub [dl_floor $spikes2:$i:$offsetTrial] $::corrgram::Args(pretime)] 1]
		}
		
		dl_append $overlapShuff [::helpcorrgram::crosscorr $spktrain1 $spktrain2 $::corrgram::Args(lags)]
	    }
			     
	    #sum and divide by number of trials
	    dl_local rawOnewayShuffle [dl_meanList $overlapShuff]
			     
	    #All-way shuffle predictor
	    dl_local psth1 [dl_meanList $psth1]
	    dl_local psth2 [dl_meanList $psth2]
	    dl_local rawAllwayShuffle [::helpcorrgram::crosscorr $psth1 $psth2 $::corrgram::Args(lags)]
	    dl_local rawAllwayShuffle [dl_div [dl_sub [dl_mult $rawAllwayShuffle $n_trials_included] \
						   $rawCorr] [expr $n_trials_included - 1].]

	    #do normalization (notice the inclusion of the geometric mean of the spike rates in the normalization factor
	    #as well as the triangular taper function)
	    dl_local spk1rate [dl_div [dl_mean $spk1count] [expr .001*$epochLength]]
	    dl_local spk2rate [dl_div [dl_mean $spk2count] [expr .001*$epochLength]]
	    dl_local geomean [dl_sqrt [dl_mult $spk1rate $spk2rate]]
	    dl_local tri_taper [dl_mult [dl_sub [dl_repeat $epochLength $n_lags] [dl_abs [dl_series [expr -$::corrgram::Args(lags)]\
											      $::corrgram::Args(lags)]]] .001]
	    dl_local normCorr [dl_div $rawCorr [dl_mult $tri_taper $geomean]]
	    dl_local normAllwayShuffle [dl_div $rawAllwayShuffle [dl_mult $tri_taper $geomean]]
	    dl_local normOnewayShuffle [dl_div $rawOnewayShuffle [dl_mult $tri_taper $geomean]]

	    #correct by both the allway and oneway shuffle
	    dl_local allwayCorrectedCorr [dl_sub $normCorr $normAllwayShuffle]
	    dl_local onewayCorrectedCorr [dl_sub $normCorr $normOnewayShuffle]

	    #store results
	    dl_append $rawCorrByCond $rawCorr
	    dl_append $normCorrByCond $normCorr
	    dl_append $rawAllwayShuffleByCond $rawAllwayShuffle
	    dl_append $normAllwayShuffleByCond $normAllwayShuffle
	    dl_append $rawOnewayShuffleByCond $rawOnewayShuffle
	    dl_append $normOnewayShuffleByCond $normOnewayShuffle
	    dl_append $allwayCorrectedCorrByCond $allwayCorrectedCorr
	    dl_append $onewayCorrectedCorrByCond $onewayCorrectedCorr

	    #some book-keeping and smoothing of the CCG
	    if {![dl_length $allwayCorrectedCorr]} {
		dl_append $smoothAllwayCorrectedCorrByCond [dl_slist "not enough data"]
	    } else {
		dl_append $smoothAllwayCorrectedCorrByCond [dl_conv2 $allwayCorrectedCorr [dl_flist .05 .25 .4 .25 .05]]
	    }
	    if {![dl_length $onewayCorrectedCorr]} {
		dl_append $smoothOnewayCorrectedCorrByCond [dl_slist "not enough data"]
	    } else {
		dl_append $smoothOnewayCorrectedCorrByCond [dl_conv2 $onewayCorrectedCorr [dl_flist .05 .25 .4 .25 .05]]
	    }

	    #keep track of the spike counts
	    dl_append $spk1countsByCond $spk1count
	    dl_append $spk2countsByCond $spk2count

	}

	set out [dg_create]
	dl_set $out:label1 $label1
	dl_set $out:label2 $label2
	dl_set $out:lags [dl_llist [dl_series [expr -$::corrgram::Args(lags)] $::corrgram::Args(lags)]]
	dl_set $out:rawCorr $rawCorrByCond
	dl_set $out:normCorr $normCorrByCond
	dl_set $out:rawAllwayShuffle $rawAllwayShuffleByCond 
	dl_set $out:normAllwayShuffle $normAllwayShuffleByCond 
	dl_set $out:rawOnewayShuffle $rawOnewayShuffleByCond 
	dl_set $out:normOnewayShuffle $normOnewayShuffleByCond 
	dl_set $out:allwayCorrectedCorr $allwayCorrectedCorrByCond
	dl_set $out:smoothAllwayCorrectedCorr $smoothAllwayCorrectedCorrByCond
	dl_set $out:onewayCorrectedCorr $onewayCorrectedCorrByCond
	dl_set $out:smoothOnewayCorrectedCorr $smoothOnewayCorrectedCorrByCond
	dl_set $out:spk1counts $spk1countsByCond
	dl_set $out:spk2counts $spk2countsByCond
	return $out
	
    }

    proc CorrLFP { g1 g2 } {

	#do not multiply lags * 2 because we only sample the LFP every 2 ms
	set n_lags [expr $::corrgram::Args(lags) + 1]

	set epochLength [expr $::corrgram::Args(posttime) - $::corrgram::Args(pretime)]
	dl_local lfp1 [dl_unpack [dl_transpose $g1:ydata]]

	dl_local lfp2 [dl_unpack [dl_transpose $g2:ydata]]
	dl_local lfp1mean [dl_unpack [dl_transpose $g1:ydatam]]
	dl_local lfp2mean [dl_unpack [dl_transpose $g2:ydatam]]
	dl_local label1 $g1:label1:0
	dl_local label2 $g1:label2:0
	
	#ROWS = TRIALS, COLUMNS = CONDITIONS
	
	#for each condition...
	dl_local normCorrByCond [dl_llist]
	dl_local normOnewayShuffleByCond [dl_llist]
	dl_local onewayCorrectedCorrByCond [dl_llist]
	for { set i 0 } {$i < [dl_length $lfp1]} {incr i} {
	    set n_trials [dl_length $lfp1:$i].

	    #for each trial...
	    dl_local corr [dl_llist]
	    dl_local var1 [dl_llist]
	    dl_local var2 [dl_llist]
	    for { set j 0 } {$j < $n_trials} {incr j} {
		#subtract mean signal value
		dl_local signal1 [dl_sub $lfp1:$i:$j [dl_mean $lfp1:$i:$j]]
		dl_local signal2 [dl_sub $lfp2:$i:$j [dl_mean $lfp2:$i:$j]]
	
		#correlate mean-subtracted time-series
		dl_append $corr [::helpcorrgram::crosscorr $signal1 $signal2 [expr $::corrgram::Args(lags)/2]]
		
		#calculate variance of each time-series
		dl_append $var1 [dl_sum [dl_pow $signal1 2]]
		dl_append $var2 [dl_sum [dl_pow $signal2 2]]
	    }

	    # repeat step above but with a one trial offset for a one way shuffle	    
	    dl_local corrShuff [dl_llist]
	    dl_local var1Shuff [dl_llist]
	    dl_local var2Shuff [dl_llist]
	    for { set j 0 } {$j < $n_trials} {incr j} {
		
		dl_local signal1 [dl_sub $lfp1:$i:$j [dl_mean $lfp1:$i:$j]]

		if { $j == [expr $n_trials - 1] } {
		    dl_local signal2 [dl_sub $lfp2:$i:0 [dl_mean $lfp2:$i:0]]
		} else {
		    dl_local signal2 [dl_sub $lfp2:$i:[expr $j+1] [dl_mean $lfp2:$i:[expr $j+1]]]
		}
		
		dl_append $corrShuff [::helpcorrgram::crosscorr $signal1 $signal2 [expr $::corrgram::Args(lags)/2]]
		dl_append $var1Shuff [dl_sum [dl_pow $signal1 2]]
		dl_append $var2Shuff [dl_sum [dl_pow $signal2 2]]

	    }

	    #take average over trials
	    dl_local corr [dl_meanList $corr]
	    dl_local var1 [dl_meanList $var1]
	    dl_local var2 [dl_meanList $var2]
	    dl_local corrShuff [dl_meanList $corrShuff]
	    dl_local var1Shuff [dl_meanList $var1Shuff]
	    dl_local var2Shuff [dl_meanList $var2Shuff]
	    dl_append $normCorrByCond [dl_div $corr [dl_sqrt [dl_mult $var1 $var2]]]
	    dl_append $normOnewayShuffleByCond [dl_div $corrShuff [dl_sqrt [dl_mult $var1Shuff $var2Shuff]]]
	}

	set out [dg_create]
	dl_set $out:label1 $label1
	dl_set $out:label2 $label2
	dl_set $out:lags [dl_llist [dl_series [expr -$::corrgram::Args(lags)] $::corrgram::Args(lags) 2]]
	dl_set $out:normCorr $normCorrByCond
	dl_set $out:normOnewayShuff $normOnewayShuffleByCond
	dl_set $out:onewayCorrectedCorr [dl_sub $normCorrByCond $normOnewayShuffleByCond]
	return $out
	
    }

    proc crosscorr {ts1 ts2 lags} {
	set n_lags [expr $lags * 2 + 1]

	#reshape to ease computation
	dl_local ts1 [dl_repeat [dl_llist $ts1] $n_lags]
	dl_local ts2_shifted [dl_llist]
		
	#construct shifted versions of the second spike train
	for { set i 0 } {$i < $n_lags} {incr i}  {
	    dl_append $ts2_shifted [dl_shift $ts2 [expr -$lags + $i]]
	}
		
	#do the actual cross-correlation
	dl_return [dl_sums [dl_mult $ts1 $ts2_shifted]]
    }
    
    proc CreateSpikeAnalGroup {g chans sortlists} {
	
	set align $::corrgram::Args(align)
	
	dl_local label1 [dl_llist]
	dl_local label2 [dl_llist]
	dl_local sorted_times [dl_llist]
	dl_local sorted_counts [dl_llist]
	dl_local sorted_counts_mean [dl_llist]
	dl_local channel_ids [dl_ilist]
	
	foreach chan $chans {

	    dl_local aligned_spikes     [dl_sub [::helpcorrgram::spike_extract $g $chan] $g:$align]
	    dl_local restricted_spikes  [dl_select $aligned_spikes [dl_betweenEqLow $aligned_spikes \
									$::corrgram::Args(pretime) $::corrgram::Args(posttime)]]
	    dl_local restricted_spkcounts  [dl_counts $restricted_spikes $::corrgram::Args(pretime) $::corrgram::Args(posttime)]
	    
	    #sorting
	    set name1 [dl_get $sortlists:0 0]
	    dl_local sb1 [dl_get $sortlists:0 1]
	    set name2 [dl_get $sortlists:1 0]
	    dl_local sb2 [dl_get $sortlists:1 1]

	    dl_append $sorted_times        [dl_sortByLists $restricted_spikes $sb1 $sb2]
	    dl_append $sorted_counts       [dl_sortByLists $restricted_spkcounts $sb1 $sb2]
	    dl_local  sorted_means         [dl_sortedFunc $restricted_spkcounts "$sb1 $sb2"]
	    dl_append $sorted_counts_mean  $sorted_means:2
	    dl_append $channel_ids $chan
	    dl_append $label1 $sorted_means:0
	    dl_append $label2 $sorted_means:1

	}
	
	#creating out group
	set out [dg_create]
	dl_set $out:channel_ids              $channel_ids
	dl_set $out:raw_spktimes             $sorted_times
	dl_set $out:spkcounts_mean           $sorted_counts_mean
	dl_set $out:spkcounts                $sorted_counts
	dl_set $out:label1                   $label1
	dl_set $out:label2                   $label2
	return $out
    }

    proc CreateLFPAnalGroup {g chan sortlists} {
	
	#get lfp signal first
	dl_local alleeg [::helpcorrgram::get_lfp $g $chan]

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]	
	dl_local eeg [dl_sortByLists $alleeg $sb1 $sb2]
	
	#creating out group
	set out [dg_create]
	set samp_rate [expr [lindex [split [dl_tcllist $g:eeg_info]] 2] /1000.]
	dl_set $out:ydata [dl_llist $eeg]
	dl_set $out:ydatam [dl_llist [dl_means $eeg]]
	dl_set $out:label1 [dl_llist [dl_cross [dl_unique $sb1] [dl_unique $sb2]]:0]
	dl_set $out:label2 [dl_llist [dl_cross [dl_unique $sb1] [dl_unique $sb2]]:1]
	return $out
    }

    proc CreateInitArgsArray {} {
	set ::corrgram::Args(pretime) 50
	set ::corrgram::Args(posttime) 250
	set ::corrgram::Args(align) "stimon"
	set ::corrgram::Args(lags) 50 
	set ::corrgram::Args(mintrialspikes) 4
	set ::corrgram::Args(plot) 1
	set ::corrgram::Args(correction) "allway"
	set ::corrgram::Args(zero_align) "same"
	set ::corrgram::Args(zero_range) "-50 0"
	set ::corrgram::Args(limit) "none"
    }
    
    proc ParseArgs {inargs} {
	set args [lindex $inargs 0]
	foreach a $args i [dl_tcllist [dl_fromto 0 [llength $args]]] {
	    if {(![string match [string index $a 0] "-"]) || \
		     [string is integer $a]} {
		continue
	    } else {
		switch -exact -- "$a" {
		    -reset {::helpcorrgram::CreateInitArgsArray}
		    -pretime { set ::corrgram::Args(pretime) [lindex $args [expr $i + 1]]}
		    -posttime { set ::corrgram::Args(posttime) [lindex $args [expr $i + 1]]}
		    -align { set ::corrgram::Args(align) [lindex $args [expr $i + 1]]}
		    -lags { set ::corrgram::Args(lags) [lindex $args [expr $i + 1]]}
		    -mintrialspikes { set ::corrgram::Args(mintrialspikes) [lindex $args [expr $i + 1]]}
		    -plot { set ::corrgram::Args(plot) [lindex $args [expr $i + 1]]}
		    -correction { set ::corrgram::Args(correction) [lindex $args [expr $i + 1]]}
		    -zero_align { set ::corrgram::Args(zero_align) [lindex $args [expr $i + 1]]}
		    -zero_range { set ::corrgram::Args(zero_range) [lindex $args [expr $i + 1]]}
		    -limit { set ::corrgram::Args(limit) [lindex $args [expr $i + 1]]}
		    default {puts "Illegal option: $a\nShould be reset, pretime, posttime, align, lags, mintrialspikes, plot, correction, zero_align, zero_range,limit.\nWill work ignoring your entry."}
		}
	    }
	}
    }
    
    proc spike_extract { g channel } {
	dl_local spk_selector [dl_eq $g:spk_channels $channel]
	dl_local all_spk_times [dl_select $g:spk_times $spk_selector]
	dl_return $all_spk_times
    }
    
    #extract the desired raw LFP signal
    proc get_lfp { g chan } {
	set range "$::corrgram::Args(pretime) $::corrgram::Args(posttime)"
	set align $::corrgram::Args(align)
	set z_r $::corrgram::Args(zero_range)
	set z_2 $::corrgram::Args(zero_align)
	
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
}


