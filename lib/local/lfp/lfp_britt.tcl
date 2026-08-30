# this package provides basic tools for ploting lfps
# it has been altered from the original lfp.tcl
# to try and take into account that all eeg data may be loaded.
# all functions are provided under the ::lfp namespace.  all helper
#    functions (called by ::lfp procs, but not directly by user)
#    are provided under the ::helplfp namespace.  this will help users
#    find appropriate procs to call from the command line.
# at a minimum, all procs in ::lfp namespace should contain a %HELP%
#    section that can be parsed by the 'use' proc.

# AUTHORS
#   FEB 2006 - REHBM
#   May 2006 (last modified July 21, 2006)- Britt

# TODO
#   

package provide lfp 2.3
package require loaddata; # for getting eeg data
package require spec

namespace eval ::lfp { } {
    
    proc demo { } {
	puts "The following are the available demos:\n   psdDemo\n   specGramDemo\n\nTo get more specifics on each type: use ::lfp::demoName\n\nWhere you substitute the demoName with one of the options above."
    }
    

    proc psdDemo { } {
	puts "do: info body ::lfp::psdDemo to see the commands"
	set g [load_data -eeg egz m_nfCon_011806*]
	::lfp::psd $g 2 -sortby stim_contrasts -window hanning -pow_range "0 30"
    }
  
    proc specGramDemo { {fz 60 } {k -15} {nmag 0.01} } {
	puts "This is demo of the specgram.\nIt uses a linear chirp signal.\nThat is, a signal where the dominant frequency\ndecreases with time. You can use three optional\narguments to change this program.\nTry info body ::lfp::specGramDemo."
	set cmd "load_data -eeg egz  m_nfcon_011806*"
	puts $cmd
	set g [eval $cmd]
	puts "replacing eeg with chirp signal"
	set cmd {dl_local time [::helpspec::noisychirp $g:eeg_time 60 -30 0.005]}
	puts $cmd
	eval $cmd
	set cmd {dl_set $g:eeg [dl_deepUnpack $time]}
	puts $cmd
	eval $cmd
	set cmd {dl_set $g:start [dl_zeros [dl_length $g:endobs]]}
	puts $cmd
	eval $cmd
	set cmd {::lfp::specgram $g 2 -pow_range "0 100" -start 0 -stop 1500 -seglen 500 -stepsize 50 -window hanning -align start}
	puts $cmd
	eval $cmd
    }

	
    proc psd {g lfpchan args } {
	# %HELP%
	# ::lfp::psd <g> <lfpchan> ?<args>?
	#    take lfp for channel <chan> in group <g> sorted by <sortby> in group <g>.
	#    Plots the periodogram (power spectrum) computed by averaging all trials in 
	#    each combination. Can change multiple features of this computation by using
	#    optional arguements.
	#    if <sortby> contains more than one variable, then each unique combination is 
	#    plotted as a different trace on same plot.
	#    optional arguments <defaults>:
	#    -start <-50> time before align variable to get eeg data
	#    -stop <250> time after align variable to get eeg data
	#    -align <stimon> what time to align lfp on.
	#    -sortby <subj> list name to sort by.
	#    -subdivide <none> if subdivide is other than none than each unique value of list
	#      subdivide will be plotted separately 
	#    -limit <none> if you wish to limit the group to a subset of trials enter a dl list for selection
	#      this will call dg_copySelected and work on the subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero relative to something other than -align list
	#    -zero_range <"-50 0"> enter a two item tcl list of the 
	#      times to use as the basis for zeroing the eeg sample.  
	#      Times are relative to -align list
	#    -pow_range <"0 80"> sets the range of frequency bands to plot.
	#    -window <"none"> if you want to taper the lfp before computing the power,
	#      only currently available window is Hanning
	#    -method <"per"> uses periodogram methods, only one available at present.
	# %END%
	set ::helplfp::vars(colorcntr) 0
	set ::helplfp::vars(linecntr) 0
	set ::helplfp::vars(plotcntr) 0
	# check for eeg lists and get them if we can
	if ![dl_exists $g:eeg] {
	    ::helplfp::getAllEeg $g
	}
	::helpspec::buildPSDArray
	::helpspec::parseArgsPSD $args
	set ::spec::psdVar(lfpchan) $lfpchan
	dl_local e [::helpspec::getEeg $g $::spec::psdVar(lfpchan) $::spec::psdVar(start) $::spec::psdVar(stop) $::spec::psdVar(align)]
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
	::helpspec::plotPSD $g $lfpchan
	return 1
	

    }

      proc specgram {g lfpchan args } {
	# %HELP%
	# ::lfp::psd <g> <lfpchan> ?<args>?
	#    take lfp for channel <chan> in group <g> sorted by <sortby> in group <g>.
	#    Plots the periodogram (power spectrum) computed by averaging all trials in 
	#    each combination. Can change multiple features of this computation by using
	#    optional arguements.
	#    if <sortby> contains more than one variable, then each unique combination is 
	#    plotted as a different trace on same plot.
	#    optional arguments <defaults>:
	#    -start <-50> time before align variable to get eeg data
	#    -stop <250> time after align variable to get eeg data
	#    IMPORTANT--STOP FOR SPECGRAM IS DIFFERENT THAN FOR PSD
	#      for psd, the eeg segment runs from start to stop
	#      for specgram it is from start to (start + seglen) incrementing 
	#      by stepsize until it reaches stop
	#    -seglen <250> length of segment for analysis
	#    -stepsize <50> number of milliseconds to step forward for each new psd
	#    -align <stimon> what time to align lfp on.
	#    -sortby <subj> list name to sort by.
	#    -subdivide <none> if subdivide is other than none than each unique value of list
	#      subdivide will be plotted separately 
	#    -limit <none> if you wish to limit the group to a subset of trials enter a dl list for selection
	#      this will call dg_copySelected and work on the subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero relative to something other than -align list
	#    -zero_range <"-50 0"> enter a two item tcl list of the 
	#      times to use as the basis for zeroing the eeg sample.  
	#      Times are relative to -align list
	#    -pow_range <"0 80"> sets the range of frequency bands to plot.
	#    -window <"none"> if you want to taper the lfp before computing the power,
	#      only currently available window is Hanning
	#    -method <"per"> uses periodogram methods, only one available at present.
	# %END%
	set ::helplfp::vars(colorcntr) 0
	set ::helplfp::vars(linecntr) 0
	set ::helplfp::vars(plotcntr) 0
	# check for eeg lists and get them if we can
	if ![dl_exists $g:eeg] {
	    ::helplfp::getAllEeg $g
	}
	::helpspec::buildPSDArray
	::helpspec::parseArgsPSD $args
	set ::spec::psdVar(lfpchan) $lfpchan
	dl_local starts [dl_series $::spec::psdVar(start) [expr $::spec::psdVar(stop) - $::spec::psdVar(seglen)] $::spec::psdVar(stepsize)]
	dl_local stops [dl_add $starts $::spec::psdVar(seglen)]
	if {[dg_exists specgrp]} {
	    dg_delete specgrp
	}
	dg_create specgrp
	dl_set specgrp:power [dl_llist]
	dl_set specgrp:freqs [dl_llist]
	dl_set specgrp:start [dl_ilist]
	dl_set specgrp:stop [dl_ilist]
	set freq_res [expr 1000. / $::spec::psdVar(seglen)];#NOTE: this is different calculation than ::lfp::psd
	foreach art [dl_tcllist $starts] op [dl_tcllist $stops] {
	    puts "$art $op"
	    dl_local e [::helpspec::getEeg $g $::spec::psdVar(lfpchan) $art $op $::spec::psdVar(align)]
	    #dl_set e $e
	    #return 0
	    if {![string match $::spec::psdVar(window) "none"]} {
		dl_local taper [::helpspec::${::spec::psdVar(window)}Window [dl_length $e:0:0]]
		dl_local e [dl_mult $e [dl_pack [dl_llist $taper]]]
	    }
	    set fft_len  [dl_min [dl_lengths [dl_collapse $e]]]
	    dl_local fe [dl_collapse [::fftw::1d $e $fft_len]]
	    dl_local pe [::helpspec::complexMult $fe $fe]
	    dl_append specgrp:power [dl_collapse [dl_div [dl_pow [dl_choose $pe [dl_llist [dl_ilist 0]]] 2] [dl_float $fft_len]]]
	    dl_append specgrp:start $art
	    dl_append specgrp:stop  $op 
	}
	dl_set specgrp:power [dl_mult [dl_log10 specgrp:power] 20];#turns into decibels
	dl_local plotlist [dl_slist]
	foreach n [dl_tcllist [dl_unique ${g}:$::spec::psdVar(sortby)]] {
	    set tempspec [dg_copy specgrp]
	    dl_set $tempspec:power [dl_choose specgrp:power [dl_llist [dl_indices [dl_eq ${g}:$::spec::psdVar(sortby) $n]]]]
	    puts $tempspec
	    dl_set $tempspec:freqs [dl_llist [dl_repeat [dl_llist [dl_mult [dl_fromto 0 [dl_length $tempspec:power:0:0]] $freq_res]] [dl_length $tempspec:power:0]]]
	    dl_append $plotlist [::helpspec::plotSpecgram $g $tempspec $::spec::psdVar(lfpchan) ]
	    dg_delete $tempspec
	}
	dg_delete specgrp
	clearwin
	set r [expr int(sqrt(double([dl_length $plotlist])))]
	set c [expr [dl_length $plotlist] / $r]
	dlp_setpanels $r $c
	if {$::spec::psdVar(globnorm)} {
	    set maxs [list]
	    set mins [list]
	    foreach n [dl_tcllist $plotlist] {
		lappend maxs [dl_max $n:plotdat]
		lappend mins [dl_min $n:plotdat]
	    }
	    set globmax [dl_max $maxs]
	    set globmin [dl_min $mins]
	}
	for {set pc 0} {$pc < [dl_length $plotlist]} {incr pc} {
	    dlp_set [dl_get $plotlist $pc] title [dl_get [dl_unique ${g}:$::spec::psdVar(sortby)] $pc]
	    if {$::spec::psdVar(globnorm)} {
		dl_set [dl_get $plotlist $pc]:plotdat [dl_char [dl_mult [dl_div [dl_sub [dl_get $plotlist $pc]:plotdat $globmin] [expr $globmax - $globmin]] 255]]
	    } else {
		dl_set [dl_get $plotlist $pc]:plotdat [dl_char [dl_mult [dl_div [dl_sub [dl_get $plotlist $pc]:plotdat [dl_min [dl_get $plotlist $pc]:plotdat]] [expr [dl_max [dl_get $plotlist $pc]:plotdat] - [dl_min  [dl_get $plotlist $pc]:plotdat]]] 255]]
	    }
	    dlp_subplot [dl_get $plotlist $pc] $pc
	}
	return [dl_tcllist $plotlist]
	

    }

    proc by { g chan sortby args } {
	# %HELP%
	# ::lfp::by <g> <chan> <sortby> ?<args>?
	#    take average lfp for channel <chan> in group <g> sorted by <sortby> - dl(s) in group <g>.
	#    if <sortby> contains more than one variable, then each unique combination is 
	#    plotted as a different trace on same plot.
	#    optional arguments <defaults>:
	#    -pretime <-50> time before align variable to get eeg data
	#    -posttime <400> time after align variable to get eeg data
	#    -align <stimon> what time to align lfp on.
	#    -subdivide <none> if subdivide is other than none than each unique value of list
	#      subdivide will be plotted separately 
	#    -limit <none> if you wish to limit the group to a subset of trials enter a dl list for selection
	#      this will call dg_copySelected and work on the subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero relative to something other than -align list
	#    -zero_range <"-50 0"> enter a two item tcl list of the 
	#      times to use as the basis for zeroing the eeg sample.  
	#      Times are relative to -align list
	# %END%
	set ::helplfp::vars(colorcntr) 0
	set ::helplfp::vars(linecntr) 0
	set ::helplfp::vars(plotcntr) 0
	# check for eeg lists and get them if we can
	if ![dl_exists $g:eeg] {
	    ::helplfp::getAllEeg $g
	}
# 	if {![array exists ::lfp::byArgs]} {
# 	    ::helplfp::buildByArgsArray
# 	}
	::helplfp::buildByArgsArray
	if {[llength $args]} {
	    ::helplfp::parseByArgs [list $args]
	}
	if {$::lfp::byArgs(limit) != "none"} {
	    set g [dg_copySelected $g $::lfp::byArgs(limit) 1]
	}
	set pannum 0
	if {[string match $::lfp::byArgs(subdivide) "none"]} {
	    dl_local sortlists [::helplfp::makeSortLists $g $sortby]
	    dl_set sl $sortlists
	    set plots "[::helplfp::genPlots $sortlists $g $chan]"
	    dlp_setpanels 1 1
	    clearwin
	    if {[llength $::lfp::byArgs(yrange)] > 1} {
		set minmax_counter -1
	    } else {
		set minmax_counter 0
	    }
	    foreach p $plots {
		dlp_subplot $p $pannum
		dlp_set $p title $sortby
		if {$minmax_counter == 0} {
		    set ymin [dl_tcllist $p:cur_ymin]
		    set ymax [dl_tcllist $p:cur_ymax]
		    incr minmax_counter
		} elseif {$minmax_counter > 0} {
		    set ymin [lindex [lsort -real -increasing "[dl_tcllist $p:cur_ymin] $ymin"] 0]
		    set ymax [lindex [lsort -real -decreasing "[dl_tcllist $p:cur_ymax] $ymax"] 0]
		    incr minmax_counter
		} elseif {$minmax_counter < 0 } {
		    set ymin [lindex $::lfp::byArgs(yrange) 0]
		    set ymax [lindex $::lfp::byArgs(yrange) 1]
		}
	    }
	    clearwin
	    foreach p $plots {
		dlp_setyrange $p $ymin $ymax
		dlp_subplot $p $pannum
	    }
	    helplfp::mark_plots "::lfp::by $g $chan $sortby"
	} else {
	    dlp_setpanels [dl_length [dl_unique $g:$::lfp::byArgs(subdivide)]]  1
	    set plots [list]
	    if {[llength $::lfp::byArgs(yrange)] > 1} {
		set minmax_counter -1
	    } else {
		set minmax_counter 0
	    }
	    foreach uv [dl_tcllist [dl_unique $g:$::lfp::byArgs(subdivide)]] {
		set subgrp [dg_copySelected $g [dl_eq $g:$::lfp::byArgs(subdivide) $uv] 1]
		dl_local sortlists [::helplfp::makeSortLists $subgrp $sortby]
		set tempplot "[::helplfp::genPlots $sortlists $subgrp $chan]"
		foreach p $tempplot {
		    dlp_set $p title [dl_get [dl_unique $g:$::lfp::byArgs(subdivide)] $pannum]
		    incr pannum 
		    dlp_plot $p
		    if {$minmax_counter == 0} {
			set ymin [dl_tcllist $p:cur_ymin]
			set ymax [dl_tcllist $p:cur_ymax]
			incr minmax_counter
		    } elseif {$minmax_counter > 0} {
			set ymin [lindex [lsort -real -increasing "[dl_tcllist $p:cur_ymin] $ymin"] 0]
			set ymax [lindex [lsort -real -decreasing "[dl_tcllist $p:cur_ymax] $ymax"] 0]
		    } elseif {$minmax_counter < 0} {
			  set ymin [lindex $::lfp::byArgs(yrange) 0]
			set ymax [lindex $::lfp::byArgs(yrange) 1]
		    }
		}
		lappend plots $tempplot
		dg_delete $subgrp
		set ::helplfp::vars(colorcntr) 0
		set ::helplfp::vars(linecntr) 0
	    }
	    clearwin
	    set pannum 0
	    foreach p $plots {
		dlp_setyrange $p $ymin $ymax
		dlp_subplot $p $pannum
		incr pannum
	    }
	    helplfp::mark_plots "::lfp::by $g $chan $sortby $::lfp::byArgs(subdivide)"
	}
	
	# replot the split plots
	if {$::lfp::byArgs(limit) != "none"} {
	    dg_delete $g
	}
	return $plots
    }

    #####Deprecated Functions####
    #These might still be used by some analysis programs so they are retained, but
    #new uses should default to the ::lpf::by function to keep all standardized in one function and
    #common nomenclature for arguments.


    proc onset { g chan sortby args} {
	# %HELP%
	# ::lfp::onset: <g> <chan> <sortby>
	#    take average lfp for channel <chan> in group <g> sorted by <sortby> - dl(s) in group <g>.
	#    if <sortby> contains more than one variable, then each unique combination is 
	#    plotted as a different trace on same plot.
	#    optional arguments <defaults>:
	#    -pretime <-50> time before align variable to get eeg data
	#    -posttime <400> time after align variable to get eeg data
	#    -align <stimon> what time to align lfp on.
	#    -subdivide <none> if subdivide is other than none than each unique value of list
	#      subdivide will be plotted separately 
	# %END%
	if ![dl_exists $g:eeg] {
	    ::helplfp::getAllEeg $g
	}
	if {![array exists ::lfp::onsetArgs]} {
	    ::helplfp::buildOnsetArgsArray
	}
	if {[llength $args]} {
	    ::helplfp::parseOnsetArgs [list $args]
	}
	
	dl_local alleeg_raw [dl_unpack [dl_select $g:eeg [dl_eq $g:eeg_chans $chan]]]
	dl_local alleeg_time [dl_unpack [dl_select $g:eeg_time [dl_eq $g:eeg_chans $chan]]]
	dl_set aet $alleeg_time
	dl_local alleeg_time_zeroed [eval dl_sub $alleeg_time $g:$::lfp::onsetArgs(align)]
	set samp_rate [expr [lindex [split [dl_tcllist $g:eeg_info]] 2] /1000.]
	set samp_num [expr ($::lfp::onsetArgs(posttime) - $::lfp::onsetArgs(pretime)) / $samp_rate]
	dl_local first_samps [dl_minIndices [dl_abs [dl_sub $alleeg_time_zeroed $::lfp::onsetArgs(pretime)]]]
	dl_local add2 [dl_repeat [dl_llist [dl_fromto 0  $samp_num]] [dl_length $first_samps]]
	dl_local get_list [dl_int [dl_add $add2 $first_samps]]
	dl_local alleeg [dl_choose $alleeg_raw $get_list]
	set sb [join $g:$sortby " $g:"]
	dl_local eeg [dl_sortedFunc $alleeg $sb]
	dl_local cnt [dl_sortedFunc $alleeg $sb dl_lengths]

	set p [dlp_newplot]
	
	set n [expr [dl_length $eeg]-1]
	set npoints [dl_length $eeg:$n:0]
	dlp_addXData $p [dl_add  [dl_mult [dl_fromto 0 $npoints] $samp_rate]  $::lfp::onsetArgs(pretime)]
	dl_local t [dl_combine [dl_choose $eeg [dl_fromto 0 $n]] [dl_llist [eval dl_slist [join "([dl_tcllist $cnt:$n])" ") ("]]]]
	dl_local tags [dl_llist]
	for {set i 0} {$i < [dl_length $t]} {incr i} {
	    dl_append $tags [eval dl_slist [dl_tcllist [dl_get $t $i]]]
	}
	dl_set $p:tags [eval dl_slist [dl_tcllist [dl_transpose $tags]]]

	set k -1
	set w 0
	for {set i 0} {$i < [dl_length $eeg:$n]} {incr i} {
	    
	    set y [dlp_addYData $p $eeg:$n:$i]
	    if {[incr k] == 8} {set k 0; incr w 2}
	    dlp_draw $p lines $y -linecolor $::helplfp::cols($k) -lstyle $w
	    dlp_wincmd $p {0 0 1 1} "dlg_text .01 [expr .93-$i*.05] %p:tags:$i -color $::helplfp::cols($k) -just -1"
	}
	# legend title
	dlp_wincmd $p {0 0 1 1} "dlg_text .01 .98 \{$sortby (N)\} -color $::colors(gray) -just -1"

	dlp_set $p xtitle "Time (ms)"
	dlp_set $p ytitle "Evoked Potential (uV)"

	dlp_winycmd $p {0 1} "dlg_lines {0 0} {0 1} -linecolor $::colors(gray)"

	# plot it
	if $::lfp::onsetArgs(draw) {
	    clearwin; dlp_plot $p
	    helplfp::mark_plots "::lfp::onset $g $chan $sortby" $g
	}

	return $p
    }
    proc onset_split { g chan sortby args } {
	# %HELP%
	# ::lfp::onset: g chan sortby
	#    take average lfp for channel <chan> in group <g> sorted by <sortby> - dl(s) in group <g>.
	#    if <sortby> provides two lists then they are split up (by second list) into seperate plots.
	# %END%

	# check for eeg lists and get them if we can
	if ![dl_exists $g:eeg] {
	    ::helplfp::getAllEeg $g
	}

	# are we sorting by 1 or 2 lists?
	if {[llength $sortby] > 2} {
	    error "::lfp:onset - can't handle more than two sort variables at a time"
	}
	if {[llength $sortby] == 1} {
	    set split_sort_label ""
	    dl_local split_sort_vals [dl_slist {}]
	    set plot_sort_label [lindex $sortby 0]; # zeroth sort list defines each plot
	} else {
	    set split_sort_label [lindex $sortby 1]
	    dl_local split_sort_vals $g:$split_sort_label
	    set plot_sort_label [lindex $sortby 0]; # zeroth sort list defines each plot
	}

	set plots ""
	set minmax_counter 0
	foreach vs1 [dl_tcllist [dl_unique $split_sort_vals]] {
	    if {$vs1 != ""} {
		dl_local thiseeg1_ref [dl_eq $g:$split_sort_label $vs1]
		set title "$vs1"
	    } else {
		dl_local thiseeg1_ref [dl_ones [dl_length $g:eeg]]
		set title [lindex $sortby 0]
	    }
	    set g2 [dg_copySelected $g $thiseeg1_ref 1]
	    set p [eval ::lfp::onset $g2 $chan $plot_sort_label $args]
	    dlp_set $p title $title
	    dg_delete $g2
	    lappend plots $p
	    if {$minmax_counter == 0} {
		set ymin [dl_tcllist $p:cur_ymin]
		set ymax [dl_tcllist $p:cur_ymax]
	    } else {
		set ymin [lindex [lsort -real -increasing "$ymin $p:cur_ymin"] 0]
		set ymax [lindex [lsort -real -decreasing "$ymax $p:cur_ymax"] 0]
	    }
	}
	# replot the split plots
	clearwin
	dlp_setpanels [dl_length [dl_unique $split_sort_vals]] 1
	set j -1
	foreach p $plots {
	    dlp_setyrange $p $ymin $ymax
	    dlp_subplot $p [incr j]
	}
	helplfp::mark_plots "::lfp::onset_split $g $chan $sortby"  $g

	return $plots
    }



    ####End Deprecated####

    ####Comment June 13, 2006####
    #     Most of the above stuff really deals with visualization, and so do many of the following ::helplfp
    #     procs.  It was written gradually by several different users and needs to be simplified and streamlined
    #     and made more modular.  However, it works and so I am not changing any of it. But as we begin to start doing
    #     data analysis on lfp's we have different requirements, so I am beginning to write a series of procs
    #     which should more directly support this application.
    ##### End Comment ####

    proc createLfpAnalGrp { g range2get  zero2 zero_range chan } {
	# %HELP%
	#::lfp::createLfpAnalGrp g <range2get "-50 450"> <zero2 stimon> <zero_range "-50 0"> <chan 99> 
	#     Function to create a group of lfp data for analysis and stats. This function creates
	#     a separate group called lfpgrp (which gets overwritten each time you call this function).
	#     Optional arguments are the range of eeg to get and what time frame to zero to. If you
	#     set zero_range to all, it uses the all the data. You only have to change the chan
	#     argument if you have more than one channel of eeg and you need to specify a particular
	#     one for analysis.
	# %END%

	#Do we have eeg in this original group? If not, get it.
	if ![dl_exists $g:eeg] {
	    ::helplfp::getAllEeg $g
	}
	
	if [dg_exists lfpgrp] {dg_delete lfpgrp}

	dg_create lfpgrp
	::helplfp::addEegLfpGrp $g "$range2get" "$zero2" "$zero_range" $chan
	return "lfpgrp"
    }	
}

namespace eval ::helplfp { } {
    set cols(0) 3
    set cols(1) 5
    set cols(2) 10
    set cols(3) 14
    set cols(4) 4
    set cols(5) 2
    set cols(6) 1
    set cols(7) 12
    set cols(99) 8
    
    proc makeSortLists {g sb} {
	# are we sorting by 1 or 2 lists?
	if {[llength $sb] > 2} {
	    error "::lfp:by - can't handle more than two sort variables at a time"
	}
	if {[llength $sb] == 1} {
	    set split_sort_label ""
	    dl_local split_sort_vals [dl_slist {}]
	    set plot_sort_label [lindex $sb 0]; # zeroth sort list defines each plot
	} else {
	    set split_sort_label [lindex $sb 1]
	    dl_local split_sort_vals $g:$split_sort_label
	    set plot_sort_label [lindex $sb 0]; # zeroth sort list defines each plot
	}
	dl_return [dl_llist [eval dl_slist $split_sort_label] $split_sort_vals [eval dl_slist $plot_sort_label]]
    }

    proc genPlots {sort_lists g chan} {
	set split_sort_label [dl_tcllist $sort_lists:0]
	dl_local split_sort_vals [dl_get $sort_lists 1]
	set plot_sort_label [dl_tcllist $sort_lists:2]
	set plots ""
	foreach vs1 [dl_tcllist [dl_unique $split_sort_vals]] {
	    if {$vs1 != ""} {
		dl_local thiseeg1_ref [dl_eq $g:$split_sort_label $vs1]
	    } else {
		dl_local thiseeg1_ref [dl_ones [dl_length $g:eeg]]
	    }
	    set g2 [dg_copySelected $g $thiseeg1_ref 1]
	    set p [eval ::helplfp::genOnePlot $g2 $chan $plot_sort_label $vs1]
	    #dlp_set $p title $title
	    dg_delete $g2
	    lappend plots $p
	}
	return $plots
    }

    proc genOnePlot {g  chan sortby {ssl ""}} {
	dl_local alleeg_raw [dl_unpack [dl_select $g:eeg [dl_eq $g:eeg_chans $chan]]]
	dl_local alleeg_time [dl_unpack [dl_select $g:eeg_time [dl_eq $g:eeg_chans $chan]]]
	dl_set aet $alleeg_time
	dl_local alleeg_time_zeroed [eval dl_sub $alleeg_time $g:$::lfp::byArgs(align)]
	set samp_rate [expr [lindex [split [dl_tcllist $g:eeg_info]] 2] /1000.]
	set samp_num [expr ($::lfp::byArgs(posttime) - $::lfp::byArgs(pretime)) / $samp_rate]
	dl_local first_samps [dl_minIndices [dl_abs [dl_sub $alleeg_time_zeroed $::lfp::byArgs(pretime)]]]
	dl_local add2 [dl_repeat [dl_llist [dl_fromto 0  $samp_num]] [dl_length $first_samps]]
	dl_local get_list [dl_int [dl_add $add2 $first_samps]]
	dl_local alleeg [dl_choose $alleeg_raw $get_list]
	###where zeroing takes place
	if { $::lfp::byArgs(zero_align) != "same" } {
	    dl_local alleeg_time_zeroed [eval dl_sub $alleeg_time $g:$::lfp::byArgs(zero_align)]
	}
	set samp_num_zero [expr ([lindex $::lfp::byArgs(zero_range) 1] - [lindex $::lfp::byArgs(zero_range) 0]) / $samp_rate]
	dl_local zero_pre [dl_minIndices [dl_abs [dl_sub $alleeg_time_zeroed [lindex $::lfp::byArgs(zero_range) 0]]]]
	dl_local add2z [dl_repeat [dl_llist [dl_fromto 0  $samp_num_zero]] [dl_length $zero_pre]]
	dl_local get_zero [dl_int [dl_add $add2z $zero_pre]]
	dl_local zr [dl_choose $alleeg_raw $get_zero]
	dl_local alleeg [dl_sub $alleeg [dl_means $zr]]
	###
	set sb [join $g:$sortby " $g:"]
	dl_local eeg [dl_sortedFunc $alleeg $sb]
	dl_local cnt [dl_sortedFunc $alleeg $sb dl_lengths]
	set p [dlp_newplot]
	set n [expr [dl_length $eeg]-1]
	set npoints [dl_length $eeg:$n:0]
	dlp_addXData $p [dl_add  [dl_mult [dl_fromto 0 $npoints] $samp_rate]  $::lfp::byArgs(pretime)]
	dl_local t [dl_combine [dl_choose $eeg [dl_fromto 0 $n]] [dl_llist [eval dl_slist [join "([dl_tcllist $cnt:$n])" ") ("]]]]
	dl_local tags [dl_llist]
	for {set i 0} {$i < [dl_length $t]} {incr i} {
	    dl_append $tags [eval dl_slist [dl_tcllist [dl_get $t $i]]]
	}
	dl_set $p:tags [eval dl_slist [dl_tcllist [dl_transpose $tags]]]
	for {set i 0} {$i < [dl_length $eeg:$n]} {incr i} {
	    set y [dlp_addYData $p $eeg:$n:$i]
	    set w $::helplfp::vars(linecntr)
	    set k [::helplfp::getColor]
	    dlp_draw $p lines $y -linecolor $::helplfp::cols($k) -lstyle $w
	    set xspot 0.01
	    if {$ssl != ""} {
		dlp_wincmd $p {0 0 1 1} "dlg_text $xspot [expr .93-${::helplfp::vars(plotcntr)}*.05] $ssl -color $::helplfp::cols($k) -just -1"
		set xspot 0.15
	    }
	    dlp_wincmd $p {0 0 1 1} "dlg_text $xspot [expr .93-${::helplfp::vars(plotcntr)}*.05] %p:tags:$i -color $::helplfp::cols($k) -just -1"
	    incr ::helplfp::vars(plotcntr)
	}
	# legend title
	dlp_wincmd $p {0 0 1 1} "dlg_text .01 .98 \{$sortby (N)\} -color $::colors(gray) -just -1"
	dlp_set $p xtitle "Time (ms)"
	dlp_set $p ytitle "Evoked Potential (uV)"
	dlp_winycmd $p {0 1} "dlg_lines {0 0} {0 1} -linecolor $::colors(gray)"
	return $p
    }
    
    proc getMonkeyName {init} {
	switch -exact -- $init {
	    o {return "oliver"}
	    j {return "jack"}
	    t {return "tiger"}
	    e {return "eddy"}
	    s {return "simon"}
	    g {return "gallagher"}
	    r {return "reggie"}
	    m {return "max"}
	}
    }

    proc reset_defaults { } {
	# array to hold all info needed to get and plot lfps (changable through option tags)
	array set ::helplfp::vars ""
	set ::helplfp::vars(align) stimon;  # dl in group to align data to
	set ::helplfp::vars(pre)   100;      # time before align to get
	set ::helplfp::vars(post)  400;     # time after align to get
	set ::helplfp::vars(gain)  5000;    # gain signal was recorded at
	set ::helplfp::vars(baseline_samps) 250; # number of samples to average over to get baseline and center eeg
	set ::helplfp::vars(raw) 0; # should we just get raw data? (no center or rescale)
	set ::helplfp::vars(colorcntr) 0;
	set ::helplfp::vars(linecntr) 0;
    }
    
    proc getColor { } {
	incr ::helplfp::vars(colorcntr)
	if {$::helplfp::vars(colorcntr) > 7} {set ::helplfp::vars(colorcntr) 0; incr ::helplfp::vars(linecntr) 2}
	return $::helplfp::vars(colorcntr)
    }

    proc center { g samples } {
	dl_local pre [dl_choose $g:eeg [dl_llist [dl_llist [dl_fromto 0 $samples]]]]
	dl_local baselines [dl_bmeans $pre]
	dl_local corrected [dl_sub $g:eeg $baselines]
	dl_set $g:eeg $corrected
    }
    proc rescale { g gain } {
	set scale [expr 10*1000000./($gain*16348.)]
	dl_set $g:eeg [dl_mult $g:eeg $scale]
    }
    
    proc getAllEeg { g } {
	puts "No eeg in  $g. Trying to load"
	set origloadvar [set ::loaddata::loadvar(eeg)]
	set ::loaddata::loadvar(eeg) "egz"
	for {set j 0} {$j < [dl_length $g:filename]} {incr j} {
	    set fn [dl_get $g:filename $j]
	    set f [dg_read [file join $::datadir [::helplfp::getMonkeyName [string index $fn 0]] $fn]]
	    dl_local valid_idx [::loaddata::get_completed_obs_idx $f]
	    dg_delete $f
	    set trialsperobs [expr [dl_length [dl_select $g:obsid [dl_eq $g:fileid $j]]] / [dl_length [dl_unique [dl_select $g:obsid [dl_eq $g:fileid $j]]]]]
	    if {[catch "::eeg::getEeg $g [file root $fn] $valid_idx $trialsperobs"]} {
		error "Can't load eeg data. You figure out what's wrong."
	    }
	}
	puts "Eeg successfully loaded"
	set ::loaddata::loadvar(eeg) $origloadvar
	return 1
    }

    proc mark_plots { text {g ""} } {
	# %HELP%
	# FROM - ::plots::mark_plots: text ?g?
	#    adds some text info to plots.  always adds date.  
	#    if a group g is supplied will try to add load string
	# %END%
	set win [getwindow]
	setwindow 0 0 1 1
	set date [clock format [clock seconds] -format %m/%d/%y]
	if {$g !="" && [dl_exists $g:load_params]} {
	    set date "$date     [dl_get $g:load_params 0]"
	}
	dlg_text .95 .5 "$date\n$text" -ori 1 -just 0 -spacing .015 -color 156204255
	eval setwindow $win
    }
    
    proc parseOnsetArgs {inargs} {
	set args [lindex $inargs 0]
	#set args $inargs
	foreach a $args i [dl_tcllist [dl_fromto 0 [llength $args]]] {
	    if {![string match [string index $a 0] "-"]} {
		continue
	    } else {
		switch -exact -- "$a" {
		    -reset {::helplfp::buildOnsetArgsArray}
		    -pretime  { set ::lfp::onsetArgs(pretime) [lindex $args [expr $i + 1]]}
		    -posttime { set ::lfp::onsetArgs(posttime) [lindex $args [expr $i + 1]]}
		    -align { set ::lfp::onsetArgs(align) [lindex $args [expr $i + 1]]}
		    -draw { set ::lfp::onsetArgs(draw) [lindex $args [expr $i + 1]]}
		}
	    }
	}
    }

    proc buildOnsetArgsArray { } {
	set ::lfp::onsetArgs(pretime) -50
	set ::lfp::onsetArgs(posttime) 400
	set ::lfp::onsetArgs(align) "stimon"
	set ::lfp::onsetArgs(draw) 1
	return 1
    }

    proc parseByArgs {inargs} {
	set args [lindex $inargs 0]
	#set args $inargs
	foreach a $args i [dl_tcllist [dl_fromto 0 [llength $args]]] {
	    if {![string match [string index $a 0] "-"]} {
		continue
	    } else {
		switch -exact -- "$a" {
		    -reset {::helplfp::buildByArgsArray}
		    -pretime  { set ::lfp::byArgs(pretime) [lindex $args [expr $i + 1]]}
		    -posttime { set ::lfp::byArgs(posttime) [lindex $args [expr $i + 1]]}
		    -align { set ::lfp::byArgs(align) [lindex $args [expr $i + 1]]}
		    -subdivide { set ::lfp::byArgs(subdivide) [lindex $args [expr $i + 1]]}
		    -zero_align { set ::lfp::byArgs(zero_align) [lindex $args [expr $i + 1]]}
		    -zero_range { set ::lfp::byArgs(zero_range) [lindex $args [expr $i + 1]]}
		    -limit { set ::lfp::byArgs(limit) [lindex $args [expr $i + 1]]}
		    -yrange {set ::lfp::byArgs(yrange) [lindex $args [expr $i + 1]]}
		    default {puts "Illegal option: $a\nShould be reset, pretime, posttime, align, subdivide, zero_align, zero_range, or limit.\nWill plot ignoring your entry."}
		}
	    }
	}
    }

    proc buildByArgsArray { } {
	set ::lfp::byArgs(pretime) -50
	set ::lfp::byArgs(posttime) 400
	set ::lfp::byArgs(align) "stimon"
	set ::lfp::byArgs(subdivide) "none"
	set ::lfp::byArgs(zero_align) "same"
	set ::lfp::byArgs(zero_range) "-50 0"
	set ::lfp::byArgs(limit) "none"
	set ::lfp::byArgs(yrange) "99."
	return 1
    }


    ####New Helper Functions for lfpgrp Creation and Analysis
    
    proc addEegLfpGrp {g range z2 z_r chan} {
	dl_local alleeg_raw [dl_unpack [dl_select $g:eeg [dl_eq $g:eeg_chans $chan]]]
	dl_local alleeg_time [dl_unpack [dl_select $g:eeg_time [dl_eq $g:eeg_chans $chan]]]
	dl_set aet $alleeg_time
	dl_local alleeg_time_zeroed [eval dl_sub $alleeg_time $g:$z2]
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
	} else {
	    #leave it as is
	    #dl_local alleeg [dl_sub $alleeg [dl_hmeans $alleeg]]
	}
	dl_set lfpgrp:eeg $alleeg
	dl_set lfpgrp:eeg_time [dl_add [dl_mult [dl_float [dl_fromto 0 [dl_lengths $alleeg]]] $samp_rate] [lindex $range 0]]
    }
}
::helplfp::reset_defaults
