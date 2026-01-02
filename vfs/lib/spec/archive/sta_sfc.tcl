package provide spec 0.9

namespace eval ::spec {

####Spike Triggered Average (STA) and helper functions    
    
    proc sta {g spkch lfpch args} {
	# %HELP%
	# use:
	# Computes a spike triggered average
	#   Input groupid, spike channel, lfp channel, and, optionally, arguments
	#   Optional arguements:
	#   -reset : restores all defaults, overrides any other options
	#   -subslct : name of list to subselect output by
	#   -align : time list in group for zeroing to
	#   -pre : how much before zero time to grab spikes (e.g. -50 is fifty seconds before zero in align)
	#   -post : how much time after zero to grab spikes (e.g. 200 is two hundred seconds after zero)
	#   -lfpdur : duration of lfp to grab
        # %END%
	puts "Resetting ::spec::staVar array to default values"
	::helpspec::buildStaVarArray; #creates default variable array
	if {[llength $args]} {
	    #if you gave optional arguements it will parse them and output new setting so you can verify
	    ::helpspec::parseArgsSta "$args"
	    puts "Updated ::spec::staVar value"
	    puts [array get ::spec::staVar]
	}
	if {$::spec::staVar(limit) != "none"} {
	    set g [dg_copySelected $g $::spec::staVar(limit) 1]
	    puts $g
	}
	if {[::helpspec::staVarCheck $g]} {
	    #checks to make sure that you are asking for eeg before 0 or after endobs
	    #and that you have the eeg data
	    return 0
	}
	#Remove any old sta samples.  This list will be added below so that you can generate new
	#partitions of the data manually without rerunning this proc, if desired.
	foreach n [list sta_info sta_xaxis sta_samps] {
	    if {[dl_exists $g:$n]} {
		dg_remove $g $n
	    }
	}
	#Get the EEG if we need it.
	if ![dl_exists $g:eeg] {
	    ::helplfp::getAllEeg $g
	}
	
	#Create local list of align times for zeroing
	eval dl_local align $g:$::spec::staVar(align)
	#get the spikes from the right lfp channel that are within the proper time window
	dl_local sts [dl_sub [dl_select $g:spk_times [dl_mult [dl_eq $g:spk_channels $spkch] \
							  [dl_lte [dl_sub $g:spk_times $align] $::spec::staVar(post)] \
							  [dl_gt [dl_sub $g:spk_times $align] $::spec::staVar(pre)]]] \
			  $align]
	#Need the sampling rate for grabbing the lfp samples
	set samp_rate [expr [lindex [dl_tcllist [dl_get $g:eeg_info 0]] 2] / 1000.]; #assumse same samp rate all files
	set ::spec::staVar(samp_rate) "$samp_rate"
	set num_samps [expr $::spec::staVar(lfpdur) / $samp_rate];#pre and post samples
	set num_samps_prehalf [expr int($num_samps / 2)]
	set num_samps_posthalf [expr $num_samps_prehalf]
	dl_set $g:sta_info [eval dl_slist "spkch" $spkch "lfpch" $lfpch [array get ::spec::staVar]]
	#A series of steps to get the subset of eeg needed.  Could be collapsed, but hopefully
	#this makes the operations more transparent
	
  
	dl_local eeg_onech [dl_choose $g:eeg [dl_indices [dl_eq $g:eeg_chans $lfpch]]];#get the correct channel of eeg
	dl_set eeg_onech $eeg_onech
	dl_local eeg_time_onech [dl_choose $g:eeg_time [dl_indices [dl_eq $g:eeg_chans $lfpch]]];#get the times to match
	dl_local eeg_samps [dl_llist]
	#since different numbers of spike in each obs (including zero spikes), easiest to just loop through each
	#observation grabbing the lfp necessary for each spike
	set xaxisflag 1
	for {set i 0} {$i < [dl_length $sts]} {incr i} {
	    #one loop for each observation period
	    if {[dl_length $sts:$i]} {
		#if at least one spike in this obs period then loop through each spike
		dl_local looplist [dl_llist]
		for {set j 0} {$j < [dl_length $sts:$i]} {incr j} {
		    #the following if is used only for plotting
		    if {$xaxisflag} {
			dl_set $g:sta_xaxis [dl_llist [dl_mult [dl_series -$num_samps_prehalf $num_samps_posthalf] $samp_rate]]
			set xaxisflag 0		    
		    }
		    set min_idx [dl_minIndex [dl_unpack [dl_abs [dl_sub $eeg_time_onech:$i [dl_get $g:$::spec::staVar(align) $i] $sts:$i:$j]]]]
		    dl_append $looplist [dl_collapse [dl_choose $eeg_onech:$i [dl_llist [dl_series [expr $min_idx - $num_samps_prehalf] [expr $min_idx + $num_samps_posthalf]]]]]
		}
		dl_append $eeg_samps $looplist
	    } else {
		dl_append $eeg_samps [dl_llist]
	    }
	}
	#this list has a sample of eeg to match each included spike and keeping the list structure of the
	#group to faciliate subsequent dl_select commands
	#Make each eeg sample zero mean
	dl_set $g:sta_samps [dl_sub $eeg_samps [dl_hmeans $eeg_samps]]
	#Two slightly different output functions depending on whether we are subselecting
	if {[string match $::spec::staVar(subslct) "none"]}  {
	    dl_local slcts [eval dl_slist "none"]
	    dl_local stas [dl_meanList [dl_collapse $g:sta_samps]]
	    dl_local outdat [dl_llist $slcts [dl_sum [dl_lengths $g:sta_samps]] [dl_llist $stas]]
	} else {
	    dl_local slcts [dl_create [dl_datatype $g:$::spec::staVar(subslct)]]
	    dl_local numspks [dl_ilist]
	    dl_local stas [dl_llist]
	    dl_local subslctlist [dl_unique $g:$::spec::staVar(subslct)]
	    foreach ss [dl_tcllist $subslctlist] {
		dl_append $slcts $ss
		dl_append $numspks [dl_sum [dl_lengths [dl_select $g:sta_samps [dl_eq $g:$::spec::staVar(subslct) $ss]]]]
		dl_append $stas [dl_meanList [dl_collapse [dl_select $g:sta_samps [dl_eq $g:$::spec::staVar(subslct) $ss]]]]
	    }
	    dl_local outdat [dl_llist $slcts $numspks $stas]
	}
	set outg [dg_create]
	foreach n [list subslct spknum sta] i [list 0 1 2] {
	    dl_set $outg:$n $outdat:$i
	}
	if $::spec::staVar(plot) {
	    ::spec::plotSta $outg $g
	}
	return "$outg $g"
    }

   proc sta4sfc {g spkch lfpch args} {
	# %HELP%
	# This is a special version of sta for calling by the sfc. If want to do sta only, use ::spec::sta
        # %END%
	puts "Resetting ::spec::staVar array to default values"
	::helpspec::buildStaVarArray; #creates default variable array
	if {[llength $args]} {
	    #if you gave optional arguements it will parse them and output new setting so you can verify
	    ::helpspec::parseArgsSta "$args"
	}
	if {[::helpspec::staVarCheck $g]} {
	    #checks to make sure that you are asking for eeg before 0 or after endobs and that you have eeg
	    return 0
	}
	#Remove any old sta samples.  This list will be added below so that you can generate new
	#partitions of the data manually without rerunning this proc, if desired.
	foreach n [list sta_info sta_xaxis sta_samps] {
	    if {[dl_exists $g:$n]} {
		dg_remove $g $n
	    }
	}
	#Create local list of align times for zeroing
	eval dl_local align $g:$::spec::staVar(align)
	#get the spikes from the right lfp channel that are within the proper time window
	dl_local sts [dl_sub [dl_select $g:spk_times [dl_mult [dl_eq $g:spk_channels $spkch] \
							  [dl_lte [dl_sub $g:spk_times $align] $::spec::staVar(post)] \
							  [dl_gt [dl_sub $g:spk_times $align] $::spec::staVar(pre)]]] \
			  $align]
	#Need the sampling rate for grabbing the lfp samples
	set samp_rate [expr [lindex [dl_tcllist [dl_get $g:eeg_info 0]] 2] / 1000.]; #assumse same samp rate all files
	set ::spec::staVar(samp_rate) "$samp_rate"
	set num_samps [expr $::spec::staVar(lfpdur) / $samp_rate];#pre and post samples
	set num_samps_prehalf [expr int($num_samps / 2)]
	set num_samps_posthalf [expr $num_samps_prehalf]
	dl_set $g:sta_info [eval dl_slist "spkch" $spkch "lfpch" $lfpch [array get ::spec::staVar]]
	#A series of steps to get the subset of eeg needed.  Could be collapsed, but hopefully
	#this makes the operations more transparent
	dl_local eeg_onech [dl_choose $g:eeg [dl_indices [dl_eq $g:eeg_chans $lfpch]]];#get the correct channel of eeg
	dl_set eeg_onech $eeg_onech
	dl_local eeg_time_onech [dl_choose $g:eeg_time [dl_indices [dl_eq $g:eeg_chans $lfpch]]];#get the times to match
	dl_local eeg_samps [dl_llist]
	#since different numbers of spike in each obs (including zero spikes), easiest to just loop through each
	#observation grabbing the lfp necessary for each spike
	set xaxisflag 1
	for {set i 0} {$i < [dl_length $sts]} {incr i} {
	    #one loop for each observation period
	    if {[dl_length $sts:$i]} {
		#if at least one spike in this obs period then loop through each spike
		dl_local looplist [dl_llist]
		for {set j 0} {$j < [dl_length $sts:$i]} {incr j} {
		    #the following if is used only for plotting
		    if {$xaxisflag} {
			dl_set $g:sta_xaxis [dl_llist [dl_mult [dl_series -$num_samps_prehalf $num_samps_posthalf] $samp_rate]]
			set xaxisflag 0		    
		    }
		    set min_idx [dl_minIndex [dl_unpack [dl_abs [dl_sub $eeg_time_onech:$i [dl_get $g:$::spec::staVar(align) $i] $sts:$i:$j]]]]
		    dl_append $looplist [dl_collapse [dl_choose $eeg_onech:$i [dl_llist [dl_series [expr $min_idx - $num_samps_prehalf] [expr $min_idx + $num_samps_posthalf]]]]]
		}
		dl_append $eeg_samps $looplist
	    } else {
		dl_append $eeg_samps [dl_llist]
	    }
	}
	#this list has a sample of eeg to match each included spike and keeping the list structure of the
	#group to faciliate subsequent dl_select commands
	#Make each eeg sample zero mean
	dl_set $g:sta_samps [dl_sub $eeg_samps [dl_hmeans $eeg_samps]]
	#Two slightly different output functions depending on whether we are subselecting
	if {[string match $::spec::staVar(subslct) "none"]}  {
	    dl_local slcts [eval dl_slist "none"]
	    dl_local stas [dl_meanList [dl_collapse $g:sta_samps]]
	    dl_local outdat [dl_llist $slcts [dl_sum [dl_lengths $g:sta_samps]] [dl_llist $stas]]
	} else {
	    dl_local slcts [dl_create [dl_datatype $g:$::spec::staVar(subslct)]]
	    dl_local numspks [dl_ilist]
	    dl_local stas [dl_llist]
	    dl_local subslctlist [dl_unique $g:$::spec::staVar(subslct)]
	    foreach ss [dl_tcllist $subslctlist] {
		dl_append $slcts $ss
		dl_append $numspks [dl_sum [dl_lengths [dl_select $g:sta_samps [dl_eq $g:$::spec::staVar(subslct) $ss]]]]
		dl_append $stas [dl_meanList [dl_collapse [dl_select $g:sta_samps [dl_eq $g:$::spec::staVar(subslct) $ss]]]]
	    }
	    dl_local outdat [dl_llist $slcts $numspks $stas]
	}
	set outg [dg_create]
	foreach n [list subslct spknum sta] i [list 0 1 2] {
	    dl_set $outg:$n $outdat:$i
	}
	return "$outg $g"
    }


    proc plotSta {stag datag} {
	# %HELP%
	# plotSta stag datag
	# stag is the group out put by ::spec::sta that includes the list of avearaged eeg and the subslct names
	# datag is the load_data group and is used to get filename and other details for plotting
	# %END%
	set p [dlp_newplot]
	dlp_addXData $p $datag:sta_xaxis
	#wrote the above, because protects against lists being off by one due to rounding when dividing by samp_rate
	set k -1
	set w 0
	#bookeeping for each subslcted plot
	#Almost all the following comes from Ryan's ::lfp::onset functions
	for {set i 0} {$i < [dl_length $stag:subslct]} {incr i} {
	    set y [dlp_addYData $p $stag:sta:$i]
	    if {[incr k] == 8} {set k 0; incr w 2}
	    dlp_draw $p lines $y -linecolor $::helplfp::cols($k) -lstyle $w
	    set cmd [eval list dlg_text .01 [expr .93-$i*.05] \{cond: [dl_tcllist $stag:subslct:${i}] [dl_tcllist $stag:spknum:$i]\} -color $::helplfp::cols($k) -just -1]
	    dlp_wincmd $p {0 0 1 1} $cmd
	}
	# legend title
	dlp_wincmd $p {0 0 1 1} "dlg_text .01 .98 \{$::spec::staVar(subslct) (N)\} -color $::colors(gray) -just -1"

	dlp_set $p xtitle "Time (ms) "
	dlp_set $p ytitle "Spike Triggered Average (uV)"

	dlp_winycmd $p {0 1} "dlg_lines {0 0} {0 1} -linecolor $::colors(gray)"
	# plot it
	if {$::spec::staVar(plot) == 2} {
	    newWindow; 
	    dlp_plot $p
	} else {
	    clearwin; 
	    dlp_plot $p
	}
	::helplfp::mark_plots "[dl_tcllist $datag:sta_info]" $datag
	return $p
    }

    proc sta_split { g spkch lfpch args} {
	# %HELP%
	# Basically just a plotting function, to get the actual numbers back use ::spec::sta with appropriate selectors and limits
	# use: ::spec::sta_split g spkch lfpch args
	# g is load_data group, spkch is spike channel id, lfpch is lfp channel id, args are all the sta options and the -sortby option
	# which allows you to provide the name of a list for which the graph will be split for each unique value.
	# %END%
	#Most of this code is a hybrid of my sta function above and Ryan's ::lfp::onset_split
	::helpspec::buildStaVarArray
	::helpspec::parseArgsSta $args
	if {$::spec::staVar(limit) != "none"} {
	    set g [dg_copySelected $g $::spec::staVar(limit) 1]
	}
	# are we sorting by 1 or 2 lists?
	if {[llength $::spec::staVar(sortby)] > 1} {
	    error "::spec::sta_split - can't handle more than one sort variables at a time"
	}
	if {[llength $::spec::staVar(sortby)] == 1} {
	    set split_sort_label [lindex $::spec::staVar(sortby) 0]
	    dl_local split_sort_vals $g:$split_sort_label
	    set plot_sort_label [lindex $::spec::staVar(sortby) 0]; # zeroth sort list defines each plot
	}
	
	set plots ""
	foreach vs1 [dl_tcllist [dl_unique $split_sort_vals]] {
	    puts $vs1
	    if {$vs1 != ""} {
		dl_local thiseeg1_ref [dl_eq $g:$split_sort_label $vs1]
		set title ""
	    } else {
		dl_local thiseeg1_ref [dl_ones [dl_length $g:eeg]]
		set title "$split_sort_label = $vs1"
	    }
	    set g2 [dg_copySelected $g $thiseeg1_ref 1]
	    set sta2 [eval ::spec::sta4sfc $g2 $spkch $lfpch $args]
	    set p [eval ::spec::plotSta $sta2]
	    dlp_set $p title "$split_sort_label $vs1"
	    dg_delete $g2
	    lappend plots $p
	}
	# replot the split plots
	clearwin
	dl_local ymaxs [dl_flist]
	dl_local ymins [dl_flist]
	foreach p $plots {
	    dl_concat $ymaxs $p:cur_ymax
	    dl_concat $ymins $p:cur_ymin
	}
	set ymax [dl_max $ymaxs]
	set ymin [dl_min $ymins]
	dlp_setpanels [dl_length [dl_unique $split_sort_vals]] 1
	set j -1
	foreach p $plots {
	    dlp_setyrange $p $ymin $ymax
	    dlp_subplot $p [incr j]
	}
	::helplfp::mark_plots "::spec::sta_split $g $spkch $lfpch $::spec::staVar(sortby) $::spec::staVar(subslct)"	$g
	
	return $plots
    }


    ####Spike Field Coherence (SFC)####
    
    proc sfc {g spkch lfpch args} {
	# %HELP%
	# use:
	# Computes the spike field coherence
	#   Input groupid, spike channel, lfp channel, and, optionally, arguments
	#   Optional arguements:
	#   -reset : restores all defaults, overrides any other options
	#   -subslct : name of list to subselect output by
	#   -align : time list in group for zeroing to
	#   -pre : how much before zero time to grab spikes (e.g. -50 is fifty seconds before zero in align)
	#   -post : how much time after zero to grab spikes (e.g. 200 is two hundred seconds after zero)
	#   -lfpdur : duration of lfp to grab
	#   -window : what to taper data by for fourier transform
	#   -max2plot (-min2plot) : specify maximum (minimum) freq in Hz for plotting
	#   -plotsfc : no plot (0), plot in analysis (1), plot in new window (2)
        # %END%
	::helpspec::buildSfcVarArray 
	::helpspec::parseArgsSfc $args
	#Get the eeg
	#::spec::getEeg $g
	#Should not be gotten from load_data with option -eeg 1 or 2
	if {$::spec::sfcVar(limit) != "none"} {
	    set g [dg_copySelected $g $::spec::sfcVar(limit) 1]
	}
	set staret [eval ::spec::sta4sfc $g $spkch $lfpch $args]
	set stadat [lindex $staret 0]
	set g [lindex $staret 1]
	dl_set $stadat:sfc_numerator [dl_llist]
	dl_set $stadat:sfc_denominators [dl_llist]
	dl_set $stadat:sfc [dl_llist]
	foreach n [dl_tcllist $stadat:subslct] i [dl_tcllist [dl_fromto 0 [dl_length $stadat:subslct]]] {
	    dl_local temp_denoms [dl_llist]
	    dl_append $stadat:sfc_numerator [::spec::stasfcpgram [dl_get $stadat:sta $i]]
	    if {$::spec::staVar(subslct) == "none"} {
		for {set j 0} {$j < [dl_length $g:obsid]} {incr j} {
		    if {[dl_length [dl_get $g:sta_samps $j]] == 0} {
			dl_append $temp_denoms [dl_llist]
		    } else {
			dl_append $temp_denoms [::spec::stasfcpgram [dl_get $g:sta_samps $j]]
		    }
		}
	    } else {
		foreach j [dl_tcllist [dl_indices [dl_eq $g:$::spec::staVar(subslct) $n]]] {
		    if {[dl_length [dl_get $g:sta_samps $j]] == 0} {
			dl_append $temp_denoms [dl_llist]
		    } else {
			dl_append $temp_denoms [::spec::stasfcpgram [dl_get $g:sta_samps $j]]
		    }
		}
	    }
	    dl_append $stadat:sfc_denominators [dl_llist $temp_denoms]
	    dl_append $stadat:sfc [dl_collapse [dl_div [dl_get $stadat:sfc_numerator $i] [dl_meanList [dl_collapse [dl_collapse [dl_get $stadat:sfc_denominators $i]]]]]]
	}
	if $::spec::staVar(plot) {
	    ::spec::plotSta $stadat $g
	}
	if $::spec::sfcVar(plot) {
	    ::spec::plotSfc $stadat $g
	}
	return $staret
    }

    


    proc plotSfc {stag datag} {
	# %HELP%
	# ::spec::plotSfc stag datag
	# stag is the group output by sta/sfc that includes lists of eeg samples and their fourier transforms for plotting
	# data is the load data group which also contains some information used in the plot
	# if you want plot in a new window set ::spec::sfcVar(plot) 2
	# %END%
	set p [dlp_newplot]
	set frqres [expr 1000. / $::spec::staVar(lfpdur)]
	dl_local freqs [dl_fromto 0 [expr 1000. / (2. * $::spec::staVar(samp_rate))] $frqres] 
	set start [dl_minIndex [dl_abs [dl_sub $freqs $::spec::sfcVar(min2plot)]]]
	set stop [dl_minIndex [dl_abs [dl_sub $freqs $::spec::sfcVar(max2plot)]]]
	dl_local slct2plot [dl_series $start $stop]
	dl_local sfcx [dl_choose $freqs $slct2plot]
	dlp_addXData $p $sfcx
	set k -1
	set w 0
	for {set i 0} {$i < [dl_length $stag:subslct]} {incr i} {
	    set y [dlp_addYData $p [dl_choose [dl_get $stag:sfc $i] $slct2plot]]
	    if {[incr k] == 8} {set k 0; incr w 2}
	    dlp_draw $p lines $y -linecolor $::helplfp::cols($k) -lstyle $w
	    set cmd [eval list dlg_text .01 [expr .93-$i*.05] \{cond: [dl_tcllist $stag:subslct:${i}] [dl_tcllist $stag:spknum:$i]\} -color $::helplfp::cols($k) -just -1]
	    dlp_wincmd $p {0 0 1 1} $cmd
	}
	# legend title
	dlp_wincmd $p {0 0 1 1} "dlg_text .01 .98 \{$::spec::staVar(subslct) (N)\} -color $::colors(gray) -just -1"

	dlp_set $p xtitle "Freq (Hz) "
	dlp_set $p ytitle "Spike Field Coherence"

	dlp_winycmd $p {0 1} "dlg_lines {0 0} {0 1} -linecolor $::colors(gray)"
	# plot it
	if {$::spec::sfcVar(plot) == 2} {
	    newWindow; 
	    dlp_plot $p
	} else {
	    clearwin; 
	    dlp_plot $p
	}
	::helplfp::mark_plots "[dl_tcllist $datag:sta_info]" $datag
	return $p
    }

    proc sfc_split { g spkch lfpch args} {
	# %HELP%
	# Basically just a plotting function, to get the actual numbers back use ::spec::sfc with appropriate selectors and limits
	# use: ::spec::sfc_split g spkch lfpch args
	# g is load_data group, spkch is spike channel id, lfpch is lfp channel id, args are all the sta options and the -sortby option
	# which allows you to provide the name of a list for which the graph will be split for each unique value.
	# %END%
	#Most of this code is a hybrid of my sfc function above and Ryan's ::lfp::onset_split
	::helpspec::buildSfcVarArray 
	::helpspec::parseArgsSfc $args
	if {$::spec::sfcVar(limit) != "none"} {
	    set g [dg_copySelected $g $::spec::sfcVar(limit) 1]
	}
	# are we sorting by 1 or 2 lists?
	if {[llength $::spec::sfcVar(sortby)] > 1} {
	    error "::spec::sfc_split - can't handle more than one sort variables at a time"
	}
	if {[llength $::spec::sfcVar(sortby)] == 1} {
	    set split_sort_label [lindex $::spec::sfcVar(sortby) 0]
	    dl_local split_sort_vals $g:$split_sort_label
	    set plot_sort_label [lindex $::spec::sfcVar(sortby) 0]; # zeroth sort list defines each plot
	}
	
	set plots ""
	foreach vs1 [dl_tcllist [dl_unique $split_sort_vals]] {
	    puts $vs1
	    if {$vs1 != ""} {
		dl_local thiseeg1_ref [dl_eq $g:$split_sort_label $vs1]
		set title ""
	    } else {
		dl_local thiseeg1_ref [dl_ones [dl_length $g:eeg]]
		set title "$split_sort_label = $vs1"
	    }
	    set g2 [dg_copySelected $g $thiseeg1_ref 1]
	    set sta2 [eval ::spec::sfc $g2 $spkch $lfpch $args]
	    set p [eval ::spec::plotSfc $sta2]
	    dlp_set $p title "$split_sort_label $vs1"
	    dg_delete $g2
	    lappend plots $p
	}
	# replot the split plots
	clearwin
	dl_local ymaxs [dl_flist]
	foreach p $plots {
	    dl_concat $ymaxs $p:cur_ymax
	}
	set ym [dl_max $ymaxs] 
	dlp_setpanels [dl_length [dl_unique $split_sort_vals]] 1
	set j -1
	foreach p $plots {
	    dlp_setyrange $p 0 $ym
	    dlp_subplot $p [incr j]
	}
	::helplfp::mark_plots "::spec::sfc_split $g $spkch $lfpch $::spec::sfcVar(sortby) $::spec::sfcVar(subslct)"	$g
	
	return $plots
    }
    
    #This next is a little bit specific to the data that comes
    #in for a sta or sfc calculation where same eeg is needed for 
    #possibly mutiple spikes
    proc stasfcpgram {indat args} {
	#       %HELP%
	# 	This is simple periodogram estimate which you can use to calculate a power spectral density
	#       Will handle lists for vectorizing
	# 	You enter the data as a list and then you have optional arguments:
       	#       -window: declares a window to use for tapering, only hanning supported at moment
	# 	example: stasfcpgram $datalist -window hanning
	# 	%END%
	if {![array exists ::spec::stasfcpgramVar]} {
	    ::helpspec::buildStasfcpgramVarArray
	}
	::helpspec::parseArgsStasfcpgram  $args
	dl_local inlist $indat
	if {![string match [dl_datatype $inlist] "list"]} {
	    puts "Warning: input data not of datatype list. Converting input data to list"
	    dl_local inlist [dl_llist $inlist]
	}
	set normalize [dl_length $inlist:0]
	if {$::spec::stasfcpgramVar(window) != "none"} {
	    set cmd "::helpspec::${::spec::stasfcpgramVar(window)}Window"
	    dl_local taper [eval $cmd [dl_length $inlist:0]]
	    dl_local inlist [dl_mult $inlist [dl_llist $taper]]
	}
	dl_local fftin [::fftw::1d $inlist]
	dl_local sqfftin [dl_mult [dl_transpose $fftin] [dl_mult [dl_llist 1 -1] [dl_transpose $fftin]]]
	dl_local pow [dl_div [dl_sub $sqfftin:0 $sqfftin:1] $normalize]
	dl_return $pow
    }    
    

}



