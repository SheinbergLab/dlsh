# This package provides basic tools to perform spectral analysis
#
# AUTHORS
#   DEC 2007, APR 2008, MAY, JUNE 2010 - LW

package provide mtspec 1.4
#source /Volumes/Labfiles/projects/analysis/packages/mtspec/helpmtspec.tcl
#source /Volumes/Labfiles/projects/analysis/packages/mtspec/colormap.tcl

namespace eval ::mtspec { } {

    proc spectrumC { g chan sortby args } {
	# %HELP%
	# ::mtspec::spectrumC <g> <chan> <sortby> ?<args>?
	#
	#    take lfp for channel <chan> in group <g> and return spectrum
	#    sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables.
	#
	#    optional arguments <defaults>:
	#    -plot <1> plot results?
	#    -pretime <-50> time before align variable to get eeg data
	#    -posttime <500> time after align variable to get eeg data
	#    -align <stimon> what time to align lfp on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#      a dl list for selection this will call dg_copySelected and work on the 
	#      subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero relative to something other than -align
	#    -zero_range <"-50 0"> enter a two item tcl list of the
	#      times to use as the basis for zeroing the eeg sample.
	#      Times are relative to -align or -zero_align list
	#    -nw <3> time-bandwidth product
	#    -ntapers <5> number of tapers to use for the multitapered spectral analysis
	#    -fpass <"0 100"> range of frequencies to return
	#    -log <1> log or absolute scale when plotting
	#    -pad <0> pad to the next power of 2? -1=no, 0=next power, 1=power after that, so on  
	#    -samp_freq <1017> sampling rate (samples/second)
	#    -p <"0 0"> two item tcl list where the first item specifies the method for
	#      calculating the confidence bars and the second item specifies the p-level.
	#      1 = asymptotic estimates based on chi-square distribution.
	#      2 = jackknife estimates (for loop intensive)
	#
	# %END%

	#do we have EEG data to analyze?
#	if ![dl_sum [dl_eq $g:eeg_chans $chan]] {
#	    error "No eeg data to analyze.  Try loading with -eeg egz or different channel."
#	}

	#build initial array
	::helpmtspec::CreateInitArgsArray "continuous"

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpmtspec::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::mtspec::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::mtspec::Args(limit) 1]
	}

	#create sortlists
	dl_local sortlists [::helpmtspec::MakeSortLists $g $sortby]

	#create out group with the data
	set g1 [::helpmtspec::CreateSpectrumGroupC $g $chan $sortlists]
		
	#delete the copied list
	if {$::mtspec::Args(limit) != "none"} {
	    dg_delete $g
	}

	#plot?
	if {$::mtspec::Args(plot)} {
	    clearwin
	    set plots "[::helpmtspec::GenerateSpectrumPlots $g1]"

	    #determine how many panels we're going to need
	    if {[llength $sortby] == 1} {
		dlp_setpanels 1 1
		set p [lindex $plots 0]
		dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		dlp_plot $p
	    } else {
		set cur_ymax 0
		set cur_ymin 0
		foreach p $plots {
		    if { [dl_max $p:ydata] > $cur_ymax } { set cur_ymax [dl_max $p:ydata] }
		    if { [dl_min $p:ydata] < $cur_ymin } { set cur_ymin [dl_min $p:ydata] }
		}
		set panel_num 0
		dlp_setpanels 1 [dl_length [dl_unique $sortlists:1:1]]
		foreach p $plots {
		    #the xrange is usually pretty static whereas the yrange not so much
		    dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		    dlp_setyrange $p [expr $cur_ymin - .1*abs($cur_ymin)] [expr $cur_ymax + .1*abs($cur_ymax)]
		    dlp_subplot $p $panel_num
		    incr panel_num
		}
	    }
	}
	return $g1
    }

    proc specgramC { g chan stepsize winsize sortby args } {
	# %HELP%
	# ::mtspec::specgramC <g> <chan> <stepsize> <winsize> <sortby> ?<args>?
	#
	#    take lfp for channel <chan> in group <g> and return spectrogram
	#    with <stepsize> and <winsize> (in ms) sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables.
	#
	#    optional arguments <defaults>:
	#    -plot <1> plot results?
	#    -pretime <-50> time before align variable to get eeg data
	#    -posttime <500> time after align variable to get eeg data
	#    -align <stimon> what time to align lfp on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#      a dl list for selection this will call dg_copySelected and work on the 
	#      subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero relative to something other than -align
	#    -zero_range <"-50 0"> enter a two item tcl list of the
	#      times to use as the basis for zeroing the eeg sample.
	#      Times are relative to -align or -zero_align list
	#    -nw <3> time-bandwidth product
	#    -ntapers <5> number of tapers to use for the multitapered spectral analysis
	#    -fpass <"0 100"> range of frequencies to return
	#    -log <1> log or absolute scale when plotting
	#    -pad <0> pad to the next power of 2? -1=no, 0=next power, 1=power after that, so on  
	#    -samp_freq <1017> sampling frequency (samples/second)
	#    -colormap <hot> colormap to use when plotting (hot/jet)
	#
	# %END%

	#do we have eeg data to analyze?
#	if ![dl_sum [dl_eq $g:eeg_chans $chan]] {
#	    error "No eeg data to analyze.  Try loading with -eeg egz or different channel."
#	}

	#build initial array
	::helpmtspec::CreateInitArgsArray "continuous"

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpmtspec::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::mtspec::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::mtspec::Args(limit) 1]
	}

	#create sortlists
	dl_local sortlists [::helpmtspec::MakeSortLists $g $sortby]

	#create out group with the data
	set g1 [::helpmtspec::CreateSpecgramGroupC $g $chan $stepsize $winsize $sortlists]
		
	#delete the copied list
	if {$::mtspec::Args(limit) != "none"} {
	    dg_delete $g
	}

	#find the minimum and maximum data values so we can scale the colors of the plot appropriately
	if { $::mtspec::Args(log) } {
	    dl_local data [dl_mult [dl_log10 $g1:zdata:2] 10]
	} else {
	    dl_local data $g1:zdata:2
	}
	set plotmin [dl_min [dl_mins $data]]
	set realmax [dl_max [dl_maxs $data]]
	set plotmax [dl_max [dl_maxs [dl_sub $data $plotmin]]]

	set plots ""

	dl_local freqs $g1:xydata:0
	dl_local times $g1:xydata:1
	#create the plots according to the sort variables
	foreach main [dl_tcllist [dl_unique $g1:zdata:1]] {
	    dl_local splits [dl_select $g1:zdata:0 [dl_eq $g1:zdata:1 $main]]
	    foreach split [dl_tcllist $splits] {
		dl_local curData [dl_select $data [dl_and [dl_eq $g1:zdata:0 $split] [dl_eq $g1:zdata:1 $main]]]
		set p "[::helpmtspec::GenerateSpecgramPlot $curData $freqs $times $plotmin $plotmax $realmax]"
		dlp_set $p title " $main :: $split "
		lappend plots $p
	    }
	}
	
	#actually plot the plots
	clearwin
	set panel_num 0
	if {[llength $sortby] == 1} {
	    dlp_setpanels 1 [dl_length [dl_unique $sortlists:0:1]]
	    foreach p $plots {
		dlp_subplot $p $panel_num
		incr panel_num
	    }
	} else {
	    dlp_setpanels [dl_length [dl_unique $sortlists:1:1]] [dl_length [dl_unique $sortlists:0:1]]
	    foreach p $plots {
		dlp_subplot $p $panel_num
		incr panel_num
	    }
	}
	return $g1
    }
    
    proc spectrumPb { g chan sortby args } {
	# %HELP%
	# ::mtspec::spectrumPb <g> <chan> <sortby> ?<args>?
	#
	#    take spikes for channel <chan> in group <g> and return spectrum 
	#    sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables.
	#
	#    optional arguments <defaults>:
	#    -plot <1> plot results?
	#    -pretime <-50> time before align variable to get spike data
	#    -posttime <500> time after align variable to get spike data
	#    -align <stimon> what time to align spikes on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#      a dl list for selection this will call dg_copySelected and work on the 
	#      subgroup which is destroyed at the end.
	#    -nw <3> time-bandwidth product
	#    -ntapers <5> number of tapers to use for the multitapered spectral analysis
	#    -fpass <"0 100"> range of frequencies to return
	#    -log <1> log or absolute scale when plotting
	#    -pad <0> pad to the next power of 2? -1=no, 0=next power, 1=power after that, so on  
	#    -samp_freq <1000> sampling frequency (samples/second)
	#    -p <"0 0"> two item tcl list where the first item specifies the method for
	#      calculating the confidence bars and the second item specifies the p-level.
	#      1 = asymptotic estimates based on chi-square distribution.
	#      2 = jackknife estimates (for loop intensive)
	#    -fscorr <0> use finite size correction for calculation of error bars?
	#
	# %END%

	#do we have spike data to analyze?
	if ![dl_sum [dl_eq $g:spk_channels $chan]] {
	    error "No spike data to analyze. Try different channel or adfspkdir."
	}
	#build initial array
	::helpmtspec::CreateInitArgsArray "point"

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpmtspec::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::mtspec::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::mtspec::Args(limit) 1]
	}

	#create sortlists
	dl_local sortlists [::helpmtspec::MakeSortLists $g $sortby]

	#create out group with the data
	set g1 [::helpmtspec::CreateSpectrumGroupPb $g $chan $sortlists]
		
	#delete the copied list
	if {$::mtspec::Args(limit) != "none"} {
	    dg_delete $g
	}

	#plot?
	if {$::mtspec::Args(plot)} {
	    clearwin
	    set plots "[::helpmtspec::GenerateSpectrumPlots $g1]"

	    #determine how many panels we're going to need
	    if {[llength $sortby] == 1} {
		dlp_setpanels 1 1
		set p [lindex $plots 0]
		dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		dlp_plot $p
	    } else {
		set cur_ymax 0
		set cur_ymin 0
		foreach p $plots {
		    if { [dl_max $p:ydata] > $cur_ymax } { set cur_ymax [dl_max $p:ydata] }
		    if { [dl_min $p:ydata] < $cur_ymin } { set cur_ymin [dl_min $p:ydata] }
		}
		set panel_num 0
		dlp_setpanels 1 [dl_length [dl_unique $sortlists:1:1]]
		foreach p $plots {
		    #the xrange is usually pretty static whereas the yrange not so much
		    dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		    dlp_setyrange $p [expr $cur_ymin - .1*abs($cur_ymin)] [expr $cur_ymax + .1*abs($cur_ymax)]
		    dlp_subplot $p $panel_num
		    incr panel_num
		}
	    }
	}
	return $g1
    }

    proc specgramPb { g chan stepsize winsize sortby args } {
	# %HELP%
	# ::mtspec::specgramC <g> <chan> <stepsize> <winsize> <sortby> ?<args>?
	#
	#    take spikes for channel <chan> in group <g> and return spectrogram
	#    with <stepsize> and <winsize> (in ms) sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables.
	#
	#    optional arguments <defaults>:
	#    -plot <1> plot results?
	#    -pretime <-50> time before align variable to get spike data
	#    -posttime <500> time after align variable to get spike data
	#    -align <stimon> what time to align spikes on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#      a dl list for selection this will call dg_copySelected and work on the 
	#      subgroup which is destroyed at the end.
	#    -nw <3> time-bandwidth product
	#    -ntapers <5> number of tapers to use for the multitapered spectral analysis
	#    -fpass <"0 100"> range of frequencies to return
	#    -log <1> log or absolute scale when plotting
	#    -pad <0> pad to the next power of 2? -1=no, 0=next power, 1=power after that, so on  
	#    -samp_freq <1000> sampling frequency (samples/second)
	#    -colormap <hot> colormap to use when plotting (hot/jet)
	#
	# %END%

	#do we have spike data to analyze?
	if ![dl_sum [dl_eq $g:spk_channels $chan]] {
	    error "No spike data to analyze. Try different channel or adfspkdir."
	}
	
	#build initial array
	::helpmtspec::CreateInitArgsArray "point"
	
	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpmtspec::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::mtspec::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::mtspec::Args(limit) 1]
	}

	#create sortlists
	dl_local sortlists [::helpmtspec::MakeSortLists $g $sortby]

	#create out group with the data
	set g1 [::helpmtspec::CreateSpecgramGroupPb $g $chan $stepsize $winsize $sortlists]
		
	#delete the copied list
	if {$::mtspec::Args(limit) != "none"} {
	    dg_delete $g
	}

	#find the minimum and maximum data values so we can scale the colors of the plot appropriately
	if { $::mtspec::Args(log) } {
	    dl_local data [dl_mult [dl_log10 $g1:zdata:2] 10]
	} else {
	    dl_local data $g1:zdata:2
	}
	set plotmin [dl_min [dl_mins $data]]
	set realmax [dl_max [dl_maxs $data]]
	set plotmax [dl_max [dl_maxs [dl_sub $data $plotmin]]]

	set plots ""

	dl_local freqs $g1:xydata:0
	dl_local times $g1:xydata:1
	#create the plots according to the sort variables
	foreach main [dl_tcllist [dl_unique $g1:zdata:1]] {
	    dl_local splits [dl_select $g1:zdata:0 [dl_eq $g1:zdata:1 $main]]
	    foreach split [dl_tcllist $splits] {
		dl_local curData [dl_select $data [dl_and [dl_eq $g1:zdata:0 $split] [dl_eq $g1:zdata:1 $main]]]
		set p "[::helpmtspec::GenerateSpecgramPlot $curData $freqs $times $plotmin $plotmax $realmax]"
		dlp_set $p title " $main :: $split "
		lappend plots $p
	    }
	}
	
	#actually plot the plots
	clearwin
	set panel_num 0
	if {[llength $sortby] == 1} {
	    dlp_setpanels 1 [dl_length [dl_unique $sortlists:0:1]]
	    foreach p $plots {
		dlp_subplot $p $panel_num
		incr panel_num
	    }
	} else {
	    dlp_setpanels [dl_length [dl_unique $sortlists:1:1]] [dl_length [dl_unique $sortlists:0:1]]
	    foreach p $plots {
		dlp_subplot $p $panel_num
		incr panel_num
	    }
	}
	return $g1
    }
    
    proc coherenceC { g chan1 chan2 sortby args } {
	# %HELP%
	# ::mtspec::coherenceC <g> <chan1> <chan2> <sortby> ?<args>?
	#
	#    take lfps in <chan1> and <chan2> in group <g> and return coherence 
	#    sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables.
	#
	#    optional arguments <defaults>:
	#    -plot <1> plot results?
	#    -pretime <-50> time before align variable to get eeg data
	#    -posttime <500> time after align variable to get eeg data
	#    -align <stimon> what time to align spikes on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#      a dl list for selection this will call dg_copySelected and work on the 
	#      subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero relative to something other than -align
	#    -zero_range <"-50 0"> enter a two item tcl list of the
	#      times to use as the basis for zeroing the eeg sample.
	#      Times are relative to -align or -zero_align list
	#    -nw <3> time-bandwidth product
	#    -ntapers <5> number of tapers to use for the multitapered spectral analysis
	#    -fpass <"0 100"> range of frequencies to plot
	#    -pad <0> pad to the next power of 2? -1=no, 0=next power, 1=power after that, so on  
	#    -samp_freq <500> sampling frequency (samples/second)
	#    -p <"0 0"> two item tcl list where the first item specifies the method for
	#      calculating the statistic and the second item specifies the p-level.
	#      1 = asymptotic estimates for phase standard deviation (no magnitude)
	#      2 = jackknife error bars for magnitude/jackknife phase standard dev.
	#          jacknife estimates use big for loops so be patient.
	#
	# %END%

	#do we have eeg data to analyze?
#	if ![dl_sum [dl_eq $g:eeg_chans $chan1]] {
#	    error "No eeg data to analyze.  Try loading with -eeg egz or different channel."
#	}

	#build initial array
	::helpmtspec::CreateInitArgsArray "continuous"

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpmtspec::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::mtspec::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::mtspec::Args(limit) 1]
	}

	#create sortlists
	dl_local sortlists [::helpmtspec::MakeSortLists $g $sortby]

	#create out group with the data
	set g1 [::helpmtspec::CreateCoherenceGroupC $g $chan1 $chan2 $sortlists]
		
	#delete the copied list
	if {$::mtspec::Args(limit) != "none"} {
	    dg_delete $g
	}

	if {$::mtspec::Args(plot)} {
	    clearwin
	    set plots "[::helpmtspec::GenerateCoherencePlots $g1]"

	    #determine how many panels we're going to need
	    if {[llength $sortby] == 1} {
		dlp_setpanels 1 1
		set p [lindex $plots 0]
		dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		dlp_plot $p
	    } else {
		set cur_ymax 0
		set cur_ymin 0
		foreach p $plots {
		    if { [dl_max $p:ydata] > $cur_ymax } { set cur_ymax [dl_max $p:ydata] }
		    if { [dl_min $p:ydata] < $cur_ymin } { set cur_ymin [dl_min $p:ydata] }
		}
		set panel_num 0
		dlp_setpanels 1 [dl_length [dl_unique $sortlists:1:1]]
		foreach p $plots {
		    #the xrange is usually pretty static whereas the yrange not so much
		    dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		    dlp_setyrange $p [expr $cur_ymin - .1*abs($cur_ymin)] [expr $cur_ymax + .1*abs($cur_ymax)]
		    dlp_subplot $p $panel_num
		    incr panel_num
		}
	    }
	}
	return $g1
    }

    proc coherenceCPb { g chan1 chan2 sortby args } {
	# %HELP%
	# ::mtspec::coherenceCPb <g> <chan1> <chan2> <sortby> ?<args>?
	#
	#    take lfp in <chan1> and and spikes in <chan2> in group <g> and return coherence 
	#    sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables.
	#
	#    optional arguments <defaults>:
	#    -plot <1> plot results?
	#    -pretime <-50> time before align variable to get data
	#    -posttime <500> time after align variable to get data
	#    -align <stimon> what time to align data on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#      a dl list for selection this will call dg_copySelected and work on the 
	#      subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero the eeg relative to something other than -align
	#    -zero_range <"-50 0"> enter a two item tcl list of the
	#      times to use as the basis for zeroing the eeg sample.
	#      Times are relative to -align or -zero_align list
	#    -nw <3> time-bandwidth product
	#    -ntapers <5> number of tapers to use for the multitapered spectral analysis
	#    -fpass <"0 100"> range of frequencies to plot
	#    -pad <0> pad to the next power of 2? -1=no, 0=next power, 1=power after that, so on  
	#    -samp_freq <1000> sampling frequency (samples/second)
	#    -p <"0 0"> two item tcl list where the first item specifies the method for
	#      calculating the statistic and the second item specifies the p-level.
	#      1 = asymptotic estimates for phase standard deviation (no magnitude)
	#      2 = jackknife error bars for magnitude/jackknife phase standard dev.
	#          jacknife estimates use big for loops so be patient.
	#    -fscorr <0> use finite size correction for point process data?
	#
	# %END%

	#do we have EEG data to analyze?
#	if ![dl_sum [dl_eq $g:eeg_chans $chan1]] {
#	    error "No eeg data to analyze.  Try loading with -eeg egz or different channel."
#	}

	#do we have spike data to analyze?
	if ![dl_sum [dl_eq $g:spk_channels $chan2]] {
	    error "No spike data to analyze. Try different channel or adfspkdir."
	}

	#build initial array (we're going to have to upsample the lfp data)
	::helpmtspec::CreateInitArgsArray "point"

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpmtspec::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::mtspec::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::mtspec::Args(limit) 1]
	}

	#create sortlists
	dl_local sortlists [::helpmtspec::MakeSortLists $g $sortby]

	#create out group with the data
	set g1 [::helpmtspec::CreateCoherenceGroupCPb $g $chan1 $chan2 $sortlists]
		
	#delete the copied list
	if {$::mtspec::Args(limit) != "none"} {
	    dg_delete $g
	}

	if {$::mtspec::Args(plot)} {
	    clearwin
	    set plots "[::helpmtspec::GenerateCoherencePlots $g1]"

	    #determine how many panels we're going to need
	    if {[llength $sortby] == 1} {
		dlp_setpanels 1 1
		set p [lindex $plots 0]
		dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		dlp_plot $p
	    } else {
		set cur_ymax 0
		set cur_ymin 0
		foreach p $plots {
		    if { [dl_max $p:ydata] > $cur_ymax } { set cur_ymax [dl_max $p:ydata] }
		    if { [dl_min $p:ydata] < $cur_ymin } { set cur_ymin [dl_min $p:ydata] }
		}
		set panel_num 0
		dlp_setpanels 1 [dl_length [dl_unique $sortlists:1:1]]
		foreach p $plots {
		    #the xrange is usually pretty static whereas the yrange not so much
		    dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		    dlp_setyrange $p [expr $cur_ymin - .1*abs($cur_ymin)] [expr $cur_ymax + .1*abs($cur_ymax)]
		    dlp_subplot $p $panel_num
		    incr panel_num
		}
	    }
	}
	return $g1
    }

    proc coherencePb { g chan1 chan2 sortby args } {
	# %HELP%
	# ::mtspec::coherencePb <g> <chan1> <chan2> <sortby> ?<args>?
	#
	#    take spikes in <chan1> and spikes in <chan2> in group <g> and return coherence 
	#    sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables.
	#
	#    optional arguments <defaults>:
	#    -plot <1> plot results?
	#    -pretime <-50> time before align variable to get spike data
	#    -posttime <500> time after align variable to get spike data
	#    -align <stimon> what time to align data on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#      a dl list for selection this will call dg_copySelected and work on the 
	#      subgroup which is destroyed at the end.
	#    -nw <3> time-bandwidth product
	#    -ntapers <5> number of tapers to use for the multitapered spectral analysis
	#    -fpass <"0 100"> range of frequencies to plot
	#    -pad <0> pad to the next power of 2? -1=no, 0=next power, 1=power after that, so on  
	#    -samp_freq <1000> sampling frequency (samples/second)
	#    -p <"0 0"> two item tcl list where the first item specifies the method for
	#      calculating the statistic and the second item specifies the p-level. jacknife
	#      estimates are similar to bootstrap and take a while to compute (for loops)
	#      1 = asymptotic estimates for phase standard deviation (no magnitude)
	#      2 = jackknife error bars for magnitude/jackknife phase standard dev.
	#          jacknife estimates use big for loops so be patient.
	#    -fscorr <0> use finite size correction for point process data?
	#
	# %END%

	#do we have spike data to analyze?
	if ![dl_sum [dl_eq $g:spk_channels $chan1]] {
	    error "No spike data on channel $chan1. Try different channel or adfspkdir."
	}
	if ![dl_sum [dl_eq $g:spk_channels $chan2]] {
	    error "No spike data on channel $chan2. Try different channel or adfspkdir."
	}

	#build initial array (we're going to have to upsample the lfp data)
	::helpmtspec::CreateInitArgsArray "point"

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpmtspec::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::mtspec::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::mtspec::Args(limit) 1]
	}

	#create sortlists
	dl_local sortlists [::helpmtspec::MakeSortLists $g $sortby]

	#create out group with the data
	set g1 [::helpmtspec::CreateCoherenceGroupPb $g $chan1 $chan2 $sortlists]
		
	#delete the copied list
	if {$::mtspec::Args(limit) != "none"} {
	    dg_delete $g
	}

	if {$::mtspec::Args(plot)} {
	    clearwin
	    set plots "[::helpmtspec::GenerateCoherencePlots $g1]"

	    #determine how many panels we're going to need
	    if {[llength $sortby] == 1} {
		dlp_setpanels 1 1
		set p [lindex $plots 0]
		dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		dlp_plot $p
	    } else {
		set cur_ymax 0
		set cur_ymin 0
		foreach p $plots {
		    if { [dl_max $p:ydata] > $cur_ymax } { set cur_ymax [dl_max $p:ydata] }
		    if { [dl_min $p:ydata] < $cur_ymin } { set cur_ymin [dl_min $p:ydata] }
		}
		set panel_num 0
		dlp_setpanels 1 [dl_length [dl_unique $sortlists:1:1]]
		foreach p $plots {
		    #the xrange is usually pretty static whereas the yrange not so much
		    dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		    dlp_setyrange $p [expr $cur_ymin - .1*abs($cur_ymin)] [expr $cur_ymax + .1*abs($cur_ymax)]
		    dlp_subplot $p $panel_num
		    incr panel_num
		}
	    }
	}
	return $g1
    }

    proc STAlfpSFC { g chan1 chan2 segHalfLength sortby args } {
	# %HELP%
	# ::mtspec::STAlfpSFC <g> <chan1> <chan2> <sortby> ?<args>?
	#
	#    take lfp in <chan1> and and spikes in <chan2> in group <g> and construct
	#    the spike triggered average LFP, STA power, and Spike-Field Coherence 
	#    +/- <segHalfLength> sorted by <sortby>
	#
	#    <sortby> can have up to two sort variables.
	#
	#    optional arguments <defaults>:
	#    -plot <1> plot results?
	#    -pretime <-50> time before align variable to get data
	#    -posttime <500> time after align variable to get data
	#    -align <stimon> what time to align data on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter
	#      a dl list for selection this will call dg_copySelected and work on the 
	#      subgroup which is destroyed at the end.
	#    -pad <0> pad to the next power of 2? -1=no, 0=next power, 1=power after that, so on  
	#    -samp_freq <500> sampling frequency (samples/second)
	#
	# %END%

	#do we have EEG data to analyze?
#	if ![dl_sum [dl_eq $g:eeg_chans $chan1]] {
#	    error "No eeg data to analyze.  Try loading with -eeg egz or different channel."
#	}

	#do we have spike data to analyze?
	if ![dl_sum [dl_eq $g:spk_channels $chan2]] {
	    error "No spike data to analyze.  Try different channel or adfspkdir."
	}

	#build initial array 
	::helpmtspec::CreateInitArgsArray "continuous"

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpmtspec::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::mtspec::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::mtspec::Args(limit) 1]
	}

	#create sortlists
	dl_local sortlists [::helpmtspec::MakeSortLists $g $sortby]

	#create out group with the data
	set g1 [::helpmtspec::CreateSTAlfpSFCGroup $g $chan1 $chan2 $segHalfLength $sortlists]
		
	#delete the copied list
	if {$::mtspec::Args(limit) != "none"} {
	    dg_delete $g
	}

	if {$::mtspec::Args(plot)} {
	    clearwin
	    set plots "[::helpmtspec::GenerateCoherencePlots $g1]"

	    #determine how many panels we're going to need
	    if {[llength $sortby] == 1} {
		dlp_setpanels 1 1
		set p [lindex $plots 0]
		dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		dlp_plot $p
	    } else {
		set cur_ymax 0
		set cur_ymin 0
		foreach p $plots {
		    if { [dl_max $p:ydata] > $cur_ymax } { set cur_ymax [dl_max $p:ydata] }
		    if { [dl_min $p:ydata] < $cur_ymin } { set cur_ymin [dl_min $p:ydata] }
		}
		set panel_num 0
		dlp_setpanels 1 [dl_length [dl_unique $sortlists:1:1]]
		foreach p $plots {
		    #the xrange is usually pretty static whereas the yrange not so much
		    dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		    dlp_setyrange $p [expr $cur_ymin - .1*abs($cur_ymin)] [expr $cur_ymax + .1*abs($cur_ymax)]
		    dlp_subplot $p $panel_num
		    incr panel_num
		}
	    }
	}
	return $g1
    }

    #############################################################################################
    # 
    #
    #
    #                                        DEMO DEMO DEMO
    #
    #
    #
    #############################################################################################


    proc demo {} {
	# %HELP%
	# ::mtspec::demo
	#
	#    Demonstration of some of the underlying computations performed by mtspec
	#
	# %END%
	
	# This demo has been heavily ripped from Ryan's masterpiece

	# Demo best viewed at max cgraph size for readability.
	closeAllTabs;
#	wm state . zoom
	set c "green cyan red yellow magenta blue light_blue gray bright_white"

	puts "*****Multitapered Spectral Package Demo*****"
	puts " This demo will open up newTabs and create a series of plots.\n"

	set TABIDX 0

	#########################################################################################
	#
	#
	#
	#                                Basics of DFT
	#
	#
	#
	#########################################################################################

	puts " Tab [incr TABIDX] - Window Functions and their Frequency Response.\n This figure illustrates the perils of doing Fourier analysis on finite length data.\n"
	flushwin
	newTab "$TABIDX. Window Functions"

	# Basic parameters
	set n 64
	set nw 3; #time-bandwidth for the Slepians
	set ntapers 5; #number of tapers
	set npad [expr $n*4]
	set nzeros [expr $npad - $n]
	dlp_setpanels 2 4
	set i 0
	
	foreach winFunc {
	    {"rectangular (implicit)" {dl_repeat 1. $n}} 
	    {"triangular" {dl_mult [expr 2/($n.-1)] [dl_sub [expr ($n-1)/2.] [dl_abs [dl_sub [dl_fromto 0 $n] [expr ($n-1)/2.]]]]}}
	    {"Hamming" {dl_sub .54 [dl_mult [dl_cos [dl_div [dl_mult [dl_fromto 0 $n] [expr 2*$::pi]] [expr $n-1]]] .46]}}
	    
	} {
	    eval dl_local w \[[lindex $winFunc 1]\]
	    dl_local wpad [dl_combine $w [dl_repeat 0. $nzeros]]
	    dl_local X [fftw::1d $wpad]
	    dl_local S [dl_add [dl_mult $X:0 $X:0] [dl_mult $X:1 $X:1]]
	    dl_local S [dl_div [dl_add $S .0000000001] [dl_max $S]]; #normalize to max power, prevent -inf
	    dl_local fs [dl_series 0 .5 [expr 1/$npad.]]

	    #Plot actual taper function
	    set p [dlp_newplot]
	    set xdata [dlp_addXData $p [dl_fromto 0 $n]]
	    set ydata [dlp_addYData $p $w]
	    dlp_setxrange $p 0 [expr $n-1]
	    dlp_setyrange $p 0 1.01
	    dlp_draw $p lines "$xdata $ydata" -lwidth 250 -linecolor $::colors(green)
	    dlp_set $p xtitle "Time bin"
	    dlp_set $p ytitle "Amplitude"
	    dlp_set $p title "[lindex $winFunc 0] taper"
	    dlp_subplot $p $i

	    #Plot its frequency response
	    set p [dlp_newplot]
	    set xdata [dlp_addXData $p $fs]
	    set ydata [dlp_addYData $p [dl_mult [dl_log10 $S] 10]]
	    dlp_setxrange $p 0 [dl_max $fs]
	    dlp_setyrange $p -100 0
	    dlp_draw $p lines "$xdata $ydata" -lwidth 250 -linecolor $::colors(cyan)
	    dlp_set $p xtitle "Normalized Frequency"
	    dlp_set $p ytitle "Relative Power (dB)"
	    dlp_set $p title "Frequency response"
	    dlp_subplot $p [expr $i+4]
	    incr i
	}

	dl_local w [fftw::dpss $n $nw $ntapers]
	dl_local w [dl_div $w [dl_max $w:0]]
	dl_local wpad [dl_combineLists $w [dl_repeat [dl_llist [dl_repeat 0. $nzeros]] $ntapers]]
	dl_local X [fftw::1d $wpad]
	dl_local Xr [dl_choose $X [dl_llist 0]]
	dl_local Xi [dl_choose $X [dl_llist 1]]
	dl_local S [dl_unpack [dl_add [dl_mult $Xr $Xr] [dl_mult $Xi $Xi]]]
	dl_local S [dl_div [dl_add $S .0000000001] [dl_maxs $S]]; #normalize to max power, prevent -inf
	dl_local fs [dl_series 0 .5 [expr 1/$npad.]]

	#Plot tapers
	set p1 [dlp_newplot]
	set p2 [dlp_newplot]
	for {set j 0} {$j < $ntapers} {incr j} {
	    #Time window
	    set xdata [dlp_addXData $p1 [dl_fromto 0 $n]]
	    set ydata [dlp_addYData $p1 $w:$j]
	    dlp_setxrange $p1 0 [expr $n-1]
	    dlp_setyrange $p1 -1 1.01
	    dlp_draw $p1 lines "$xdata $ydata" -lwidth 100 -linecolor $::colors([lindex $c $j])

	    #Frequency response
	    set xdata [dlp_addXData $p2 $fs]
	    set ydata [dlp_addYData $p2 [dl_mult [dl_log10 $S:$j] 10]]
	    dlp_setxrange $p2 0 [dl_max $fs]
	    dlp_setyrange $p2 -100 0
	    dlp_draw $p2 lines "$xdata $ydata" -lwidth 100 -linecolor $::colors([lindex $c $j])
	    dlp_wincmd $p2 {0 0 1 1} "dlg_text .75 [expr .95-$j*.05] {${j}th order} -color $::colors([lindex $c $j])"
	}
	dlp_set $p1 xtitle "Time bin"
	dlp_set $p1 ytitle "Amplitude"
	dlp_set $p1 title "The 5 leading Slepians for TW=3"
	dlp_set $p2 xtitle "Normalized Frequency"
	dlp_set $p2 ytitle "Relative Power (dB)"
	dlp_set $p2 title "Frequency response"
	dlp_subplot $p1 $i
	dlp_subplot $p2 [expr $i+4]

	#########################################################################################
	#
	#
	#
	#                      Spectral estimation with Slepian tapers
	#
	#
	#
	#########################################################################################

	puts " Tab [incr TABIDX] - Multitapered Spectral Estimation. Note tradeoff between resolution and variance.\n To do similar analyses, see functions spectrumC and spectrumPb.\n"
	flushwin
	newTab "$TABIDX. Multitapered Spectral Analysis"
	dlp_setpanels 2 2

	::helpmtspec::CreateInitArgsArray "continuous"
	set i -1;  set f 25; set length 1; 
	set Fs $::mtspec::Args(samp_freq)
	set mtspec::Args(pretime) 0
	set mtspec::Args(posttime) [expr $length * 1000]
	set mtspec::Args(fpass) "0 250"

	# Create the cosine
	dl_local x [dl_fromto 0 $length [expr 1./$Fs]]
	dl_local y [dl_add [dl_cos [dl_mult $x $f [expr 2*$::pi]]] \
			[dl_zrand [dl_length $x]]]
	set n [dl_length $x]
	set p [dlp_newplot]
	set xdata [dlp_addXData $p $x]
	set ydata [dlp_addYData $p $y]
	dlp_setxrange $p 0 $length
	dlp_setyrange $p -5 5
	dlp_draw $p lines "$xdata $ydata" -lwidth 100
	dlp_set $p xtitle "time (seconds)"
	dlp_set $p ytitle "amplitude (a.u.)"
	dlp_set $p title "Cosine of 25 Hz, plus N(0,1) noise, sampled at $Fs Hz"
	dlp_subplot $p [incr i]
	
	# Multitapered estimates with different combinations of parameters
	foreach params {
	    {3 5} {4 7} {5 9}
	} {
	    set ::mtspec::Args(nw) [lindex $params 0]; 
	    set ntapers [lindex $params 1]; 

	    set p [dlp_newplot]
	    dl_local f [helpmtspec::mtspectrum [dl_llist $y] "continuous"]:2
	    set xdata [dlp_addXData $p $f]

	    for {set j 0} {$j < $ntapers} {incr j} {
		if {[expr $j%2] == 0} {
		    set ::mtspec::Args(ntapers) [expr $j+1]
		    dl_local S [dl_unpack [helpmtspec::mtspectrum [dl_llist $y] "continuous"]:0]
		    set ydata [dlp_addYData $p [dl_mult [dl_log10 $S] 10]]
		    dlp_draw $p lines "$xdata $ydata" -lwidth [expr 100+$j*25] -linecolor $::colors([lindex $c $j])
		    dlp_wincmd $p {0 0 1 1} "dlg_text .75 [expr .95-$j*.02] {Spectrum averaged across the [expr $j+1] leading Slepians} -color $::colors([lindex $c $j])"
		}
	    }
	    dlp_setxrange $p [dl_min $f] [dl_max $f]
	    dlp_setyrange $p -50 0
	    dlp_set $p xtitle "Frequency (Hz)"
	    dlp_set $p ytitle "Power (dB)"
	    dlp_set $p title "Multitapered power spectrum, TW=$ntapers"
	    dlp_subplot $p [incr i]
	}

	#########################################################################################
	#
	#
	#
	#                                     Spectrogram
	#
	#
	#
	#########################################################################################
	
	puts " Tab [incr TABIDX] - Spectrogram (sliding spectrum).\n To do similar analyses, see functions specgramC (continuous) and specgramPb (point).\n"
	flushwin
	newTab "$TABIDX. Multitapered spectrograms"
	dlp_setpanels 3 1
	set i -1; set length 2; set k 50; set f0 1; 
	::helpmtspec::CreateInitArgsArray "continuous"
	set Fs $::mtspec::Args(samp_freq)
	set ::mtspec::Args(pretime) 0
	set ::mtspec::Args(posttime) [expr 1000*$length]
	set ::mtspec::Args(fpass) "0 250";

	# Create the chirp
	dl_local x [dl_fromto 0 $length [expr 1./$Fs]]
	dl_local fs [dl_add $f0 [dl_mult [expr $k/2.] $x]]
	set n [dl_length $x]
	dl_local y [dl_cos [dl_mult $x $fs [expr 2*$::pi]]]

	dl_set y $y
	dl_set x $x
	
	set p [dlp_newplot]
	set xdata [dlp_addXData $p $x]
	set ydata [dlp_addYData $p $y]
	dlp_setxrange $p 0 $length
	dlp_setyrange $p -2 2
	dlp_draw $p lines "$xdata $ydata" -lwidth 50
	dlp_set $p xtitle "time (seconds)"
	dlp_set $p ytitle "amplitude (a.u.)"
	dlp_set $p title "Chirp with a $k Hz/s rate"
	dlp_subplot $p [incr i]

	set step 10; set window 500;
	dl_local Stf [helpmtspec::mtspecgram [dl_llist $y] $step $window "continuous"]
	dl_local S [dl_reverse [dl_mult [dl_log10 $Stf:0] 10]]
	dl_local freqs $Stf:1
	dl_local times $Stf:2

	set plotmin [dl_min [dl_mins $S]]
	set realmax [dl_max [dl_maxs $S]]
	set plotmax [dl_max [dl_maxs [dl_sub $S $plotmin]]]

	set ::mtspec::Args(colormap) "jet"
	set p [helpmtspec::GenerateSpecgramPlot $S $freqs $times $plotmin $plotmax $realmax]
	dlp_set $p title "Spectogram with stepsize=$step ms, winsize=$window ms, nw=$::mtspec::Args(nw), ntapers=$::mtspec::Args(ntapers), Jet Colormap"
	dlp_subplot $p [incr i]

	set ::mtspec::Args(colormap) "hot"
	set p [helpmtspec::GenerateSpecgramPlot $S $freqs $times $plotmin $plotmax $realmax]
	dlp_set $p title "Spectogram with stepsize=$step ms, winsize=$window ms, nw=$::mtspec::Args(nw), ntapers=$::mtspec::Args(ntapers), Hot Colormap"
	dlp_subplot $p [incr i]

	#########################################################################################
	#
	#
	#
	#                                     Coherence
	#
	#
	#
	#########################################################################################

	puts " Tab [incr TABIDX] - Coherence between two continuous processes.\n To do similar analyses, see functions coherenceC (two continuous), coherencePb (two point), coherenceCPb and STAlfpSFC (one continuous, one binned).\n"
	flushwin
	newTab "$TABIDX. Coherence example"
	dlp_setpanels 2 2
	set i -1; set f1 10; set f2 25; set f3 60; set length 1;
	::helpmtspec::CreateInitArgsArray "continuous"
	set Fs $::mtspec::Args(samp_freq)
	set ::mtspec::Args(pretime) 0
	set ::mtspec::Args(posttime) [expr 1000*$length]
	set ::mtspec::Args(fpass) "0 250"

	# Create the time varying cosine waves with multiple (and shared) frequencies
	dl_local x [dl_fromto 0 $length [expr 1./$Fs]]
	set n [dl_length $x]
	dl_local yf1 [dl_cos [dl_mult $x [expr 2*$::pi*$f1]]]
	dl_local yf2 [dl_cos [dl_mult $x [expr 2*$::pi*$f2]]]
	dl_local yf3 [dl_cos [dl_mult $x [expr 2*$::pi*$f3]]]
	dl_local y1 [dl_add $yf1 $yf2 [dl_zrand [dl_length $x]]]
	dl_local y2 [dl_add $yf2 $yf3 [dl_zrand [dl_length $x]]]
	
	#Plot the cosines
	set p [dlp_newplot]
	set xdata [dlp_addXData $p $x]
	set ydata1 [dlp_addYData $p [dl_add $y1 5]]
	set ydata2 [dlp_addYData $p [dl_add $y2 -5]]
	dlp_setxrange $p 0 $length
	dlp_setyrange $p -10 10
	dlp_draw $p lines "$xdata $ydata1" -lwidth 100 -linecolor $::colors(cyan)
	dlp_draw $p lines "$xdata $ydata2" -lwidth 100 -linecolor $::colors(green)
	dlp_wincmd $p {0 0 1 1} "dlg_text .5 .53 {Cosine with components at $f1 and $f2 Hz, plus N(0,1) noise} -color $::colors(cyan)"
	dlp_wincmd $p {0 0 1 1} "dlg_text .5 .47 {Cosine with components at $f2 and $f3 Hz, plus N(0,1) noise} -color $::colors(green)"
	dlp_set $p xtitle "time (seconds)"
	dlp_set $p ytitle "amplitude (a.u.)"
	dlp_set $p title "Two noisy cosines with a shared 25 Hz component"
	dlp_subplot $p [incr i]

	#Do coherency analysis
	dl_local Cs [helpmtspec::mtcoherence [dl_llist $y1] [dl_llist $y2] "continuous" "continuous"]
	dl_local f $Cs:8
	dl_local C $Cs:0
	set p [dlp_newplot]
	set xdata [dlp_addXData $p $f]
	set ydata [dlp_addYData $p $C]
	dlp_setxrange $p [dl_min $f] [dl_max $f]
	dlp_setyrange $p 0 1
	dlp_draw $p lines "$xdata $ydata" -lwidth 50
	dlp_set $p xtitle "Frequency (Hz)"
	dlp_set $p ytitle "Coherence Magnitude"
	dlp_set $p title "Single trial estimate of coherence, nw=$::mtspec::Args(nw), ntapers=$::mtspec::Args(ntapers)"
	dlp_subplot $p [incr i]

	#Multiple trials
	set ntrials 1000
	dl_local yf1 [dl_repeat [dl_llist [dl_cos [dl_mult $x [expr 2*$::pi*$f1]]]] $ntrials]
	dl_local yf2 [dl_repeat [dl_llist [dl_cos [dl_mult $x [expr 2*$::pi*$f2]]]] $ntrials]
	dl_local yf3 [dl_repeat [dl_llist [dl_cos [dl_mult $x [expr 2*$::pi*$f3]]]] $ntrials]
	dl_local noise1 [dl_reshape [dl_zrand [expr [dl_length $x]*$ntrials]] $ntrials -]
	dl_local noise2 [dl_reshape [dl_zrand [expr [dl_length $x]*$ntrials]] $ntrials -]
	dl_local y1 [dl_add $yf1 $yf2 $noise1]
	dl_local y2 [dl_add $yf2 $yf3 $noise2]
	dl_local Cs [helpmtspec::mtcoherence $y1 $y2 "continuous" "continuous"]
	dl_local cohmag_wrong [dl_meanList $Cs:0]
	dl_local S12r [dl_meanList $Cs:2]
	dl_local S12i [dl_meanList $Cs:3]
	dl_local S1   [dl_meanList $Cs:4]
	dl_local S2   [dl_meanList $Cs:5]

	#compute the actual coherence magnitude and phase
	dl_local C12r [dl_div $S12r [dl_sqrt [dl_mult $S1 $S2]]]
	dl_local C12i [dl_div $S12i [dl_sqrt [dl_mult $S1 $S2]]]
	dl_local cohmag [dl_sqrt [dl_add [dl_mult $C12r $C12r] [dl_mult $C12i $C12i]]]
	dl_local cohphase [dl_atan [dl_div $C12i $C12r]]

	set p [dlp_newplot]
	set xdata [dlp_addXData $p $f]
	set ydata [dlp_addYData $p $cohmag_wrong]
	dlp_setxrange $p [dl_min $f] [dl_max $f]
	dlp_setyrange $p 0 1
	dlp_draw $p lines "$xdata $ydata" -lwidth 50
	dlp_set $p xtitle "Frequency (Hz)"
	dlp_set $p ytitle "Coherence Magnitude"
	dlp_set $p title "Average of $ntrials noisy trials, nw=$::mtspec::Args(nw), ntapers=$::mtspec::Args(ntapers)"
	dlp_wincmd $p {0 0 1 1} "dlg_text .15 .75 {INCORRECTLY calculated as the average of individual trial magnitudes (note bias)} -just -1"
	dlp_subplot $p [incr i]

	set p [dlp_newplot]
	set xdata [dlp_addXData $p $f]
	set ydata [dlp_addYData $p $cohmag]
	dlp_setxrange $p [dl_min $f] [dl_max $f]
	dlp_setyrange $p 0 1
	dlp_draw $p lines "$xdata $ydata" -lwidth 50
	dlp_set $p xtitle "Frequency (Hz)"
	dlp_set $p ytitle "Coherence Magnitude"
	dlp_set $p title "Average of $ntrials noisy trials, nw=$::mtspec::Args(nw), ntapers=$::mtspec::Args(ntapers)"
	dlp_wincmd $p {0 0 1 1} "dlg_text .15 .75 {CORRECTLY calculated as the magnitude of the average cross/power spectrum} -just -1"
	dlp_subplot $p [incr i]

	puts " For more details on each procedure try the 'use' proc, e.g.:\n   use ::mtspec::spectrumC"
	puts "\n Enjoy..."
    }
}

