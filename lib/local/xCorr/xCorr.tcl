package provide xCorr 0.2
package require lfp; #use this for some of the sorting and plotting procs, note this sources lfp_britt not lfp

namespace eval ::xCorr {

    proc by {g chnids sortby args } {
	# %HELP%
	# ::xCorr::by <g> <chnids> <sortby> ?<args>?
	# 	Designed to produce a cross-correlation plot from a load
	# 	data group that contains spikes. You provide groupid, tcl list of 
	# 	channel ids and the variables to sort by. A number of optional arguments
	# 	are available:
	# 	   -prex <-20> Crosscorrelation plot runs from prex to postx
	# 	   -postx <20>
	# 	   -align <stimon> name of list to use as basis of zeroing for the
	# 	      grabbing of spikes from prealign to postalign
	# 	   -prealign <50>
	# 	   -postalign <350>
	# 	   -binsize <5> Spikes within +- 1/2 binsize are counted as synchronous
	# 	   -subdivide <none>
	# 	   -limit <none> if a list is provided it will be used to limit
	# 	      the data used for generating the cross correlograms
	#          -norm <trnum> normalizes output by number of trials, other choice is raw with no normalization
	# %END%
	
	#using lfp plotting functions
	set ::helplfp::vars(colorcntr) 0
	set ::helplfp::vars(linecntr) 0
	set ::helplfp::vars(plotcntr) 0

	::helpXCorr::parseByArgs [list -reset]
	#parse input arguements
	if {[llength $args]} {
	    ::helpXCorr::parseByArgs [list $args]
	}

	#setting additional array values
	set ::helpXCorr::byArgs(chnids) "$chnids"
	set ::helpXCorr::byArgs(sortby) $sortby

	#if we wish to limit the data analyzed do so by working on a truncated group
	if {$::helpXCorr::byArgs(limit) != "none"} {
	    set g [dg_copySelected $g $::helpXCorr::byArgs(limit) 1]
	} else {
	    set g [dg_copy $g]
	}
	
	#Now selecting the spike times and channels to be those within the prealign and postalign time values relative to align list
	dl_local slctlist [dl_between $g:spk_times [dl_add $g:$::helpXCorr::byArgs(align) $::helpXCorr::byArgs(prealign)] \
			       [dl_add $g:$::helpXCorr::byArgs(align) $::helpXCorr::byArgs(postalign)]]
	dl_set $g:spk_times [dl_select $g:spk_times $slctlist]
	dl_set $g:spk_channels [dl_select $g:spk_channels $slctlist]

	set pannum 0
	if {[string match $::helpXCorr::byArgs(subdivide) "none"]} {
	    dl_local sortlists [::helplfp::makeSortLists $g $sortby]
	    #dl_set sl $sortlists
	    set plots "[::helpXCorr::genPlots $sortlists $g]"
	    dlp_setpanels 1 1
	    clearwin
	    set minmax_counter 0
	    foreach p $plots {
		dlp_subplot $p $pannum
		dlp_set $p title $sortby
		if {$minmax_counter == 0} {
		    set ymin [dl_tcllist $p:cur_ymin]
		    set ymax [dl_tcllist $p:cur_ymax]
		} else {
		    set ymin [lindex [lsort -real -increasing "[dl_tcllist $p:cur_ymin] $ymin"] 0]
		    set ymax [lindex [lsort -real -decreasing "[dl_tcllist $p:cur_ymax] $ymax"] 0]
		}
		incr minmax_counter
	    }
	    clearwin
	    foreach p $plots {
		dlp_setyrange $p $ymin $ymax
		dlp_subplot $p $pannum
	    }
	    helplfp::mark_plots "::xCorr::by $g $chnids $sortby"
	} else {
	    dlp_setpanels [dl_length [dl_unique $g:$::helpXCorr::byArgs(subdivide)]]  1
	    set plots [list]
	    set minmax_counter 0
	    foreach uv [dl_tcllist [dl_unique $g:$::helpXCorr::byArgs(subdivide)]] {
		set subgrp [dg_copySelected $g [dl_eq $g:$::helpXCorr::byArgs(subdivide) $uv] 1]
		dl_local sortlists [::helplfp::makeSortLists $subgrp $sortby]
		set tempplot "[::helpXCorr::genPlots $sortlists $subgrp]"
		foreach p $tempplot {
		    dlp_set $p title [dl_get [dl_unique $g:$::helpXCorr::byArgs(subdivide)] $pannum]
		    incr pannum 
		    dlp_plot $p
		    if {$minmax_counter == 0} {
			set ymin [dl_tcllist $p:cur_ymin]
			set ymax [dl_tcllist $p:cur_ymax]
			incr minmax_counter
		    } else {
			set ymin [lindex [lsort -real -increasing "[dl_tcllist $p:cur_ymin] $ymin"] 0]
			set ymax [lindex [lsort -real -decreasing "[dl_tcllist $p:cur_ymax] $ymax"] 0]
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
	    helplfp::mark_plots "::lfp::by $g $chnids $sortby $::helpXCorr::byArgs(subdivide)"
	}
	
	# replot the split plots
	dg_delete $g
	return $plots
    }
}
#---Helper Functions for xCorr package---#

namespace eval ::helpXCorr {
    proc init_xc_params { } {
	#initializes the array of values needed by ::xCorr::by
	set ::helpXCorr::byArgs(prex) -20
	set ::helpXCorr::byArgs(postx) 20
	set ::helpXCorr::byArgs(align) stimon
	set ::helpXCorr::byArgs(prealign) 50
	set ::helpXCorr::byArgs(postalign) 350
	set ::helpXCorr::byArgs(binsize) 5
	set ::helpXCorr::byArgs(subdivide) none
	set ::helpXCorr::byArgs(limit) none
	set ::helpXCorr::byArgs(norm) trnum
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
		    -reset {::helpXCorr::init_xc_params}
		    -prex { set ::helpXCorr::byArgs(prex) [lindex $args [expr $i + 1]]}
		    -postx { set ::helpXCorr::byArgs(postx) [lindex $args [expr $i + 1]]}
		    -prealign { set ::helpXCorr::byArgs(prealign) [lindex $args [expr $i + 1]]}
		    -postalign { set ::helpXCorr::byArgs(postalign) [lindex $args [expr $i + 1]]}
		    -align { set ::helpXCorr::byArgs(align) [lindex $args [expr $i + 1]]}
		    -subdivide { set ::helpXCorr::byArgs(subdivide) [lindex $args [expr $i + 1]]}
		    -limit { set ::helpXCorr::byArgs(limit) [lindex $args [expr $i + 1]]}
		    -norm { set ::helpXCorr::byArgs(norm) [lindex $args [expr $i + 1]]}
		}
	    }
	}
    }
    
    proc makeXCorr {spk_times spk_chns} {
	#%HELP%
	#A small function to yield the basics of a cross correlogram.
	# The input is a list of spike times and a list of the channels of each
	# spike, such as comes out of adfgui.  Then comes a tcl list of two channels that
	# you want to be the basis of the cross correlogram.  Lastly, you give a minimum and maximum lag
	# and a resolution, all of which are optional.
	# The function uses the dl_idiff to encode all order differences between each item of the first list
	# and each item of the second list in a big long list.  These are then truncated by the maximum and minimum
	# values.  Then the resolution value is used to set the size of the bins used for getting the counts.
	# You then sum over all the trials, fixations, or obs periods.  When using with fixations, such as comes
	# you may have to collapse a list.
	# Output is not normalized by spike or trial number
	#%END%
	set min $::helpXCorr::byArgs(prex)
	set max $::helpXCorr::byArgs(postx)
	set res $::helpXCorr::byArgs(binsize)
	set chnlist $::helpXCorr::byArgs(chnids)
	if {[expr int(fmod([expr $max - $min], $res ) )] != 0} {puts {Num bins [($postx - $prex) / $binsize] must be integer};return}
	set binnum [expr int([expr ($max - $min)/$res])]
	dl_local t0 [dl_select $spk_times [dl_eq $spk_chns [lindex $chnlist 0]]]	
	dl_local t1 [dl_select $spk_times [dl_eq $spk_chns [lindex $chnlist 1]]]	
	dl_local diffs [dl_idiff $t0 $t1]
	dl_local w_diffs [dl_select $diffs [dl_between $diffs $min $max]]
	dl_local bincounts [dl_hists $w_diffs $min $max $binnum]
	if {[string match $::helpXCorr::byArgs(norm) trnum]} {
	    dl_return [dl_meanList $bincounts]
	} elseif  {[string match $::helpXCorr::byArgs(norm) raw]} {
	    dl_return [dl_sums [dl_transpose $bincounts]]
	} else {
	    return 0
	}
    }

    
    proc genPlots {sort_lists g} {
	set split_sort_label [dl_tcllist $sort_lists:0]
	dl_local split_sort_vals [dl_get $sort_lists 1]
	set plot_sort_label [dl_tcllist $sort_lists:2]
	set plots ""
	foreach vs1 [dl_tcllist [dl_unique $split_sort_vals]] {
	    if {$vs1 != ""} {
		dl_local thisxc1_ref [dl_eq $g:$split_sort_label $vs1]
	    } else {
		dl_local thisxc1_ref [dl_ones [dl_length $g:spk_times]]
	    }
	    set g2 [dg_copySelected $g $thisxc1_ref 1]
	    set p [eval ::helpXCorr::genOnePlot $g2 $plot_sort_label $vs1]
	    dg_delete $g2
	    lappend plots $p
	}
	return $plots
    }
    
    proc genOnePlot {g  sortby {ssl ""}} {
	set p [dlp_newplot]
	dlp_addXData $p [dl_series [expr $::helpXCorr::byArgs(prex) + ($::helpXCorr::byArgs(binsize) / 2.)] \
			     [expr $::helpXCorr::byArgs(postx) - ($::helpXCorr::byArgs(binsize) / 2.)] $::helpXCorr::byArgs(binsize)]
	set uniqval [dl_tcllist [dl_unique $g:$sortby]]
	dl_local xcdat [dl_llist]
	foreach uv $uniqval {
	    set wg [dg_copySelected $g [dl_eq $g:$sortby $uv] 1]
	    dl_append $xcdat [::helpXCorr::makeXCorr $wg:spk_times $wg:spk_channels]
	    dg_delete $wg
	    #puts $wg
	}
	for {set i 0} {$i < [dl_length $xcdat]} {incr i} {
	    set y [dlp_addYData $p $xcdat:$i]
	    set w $::helplfp::vars(linecntr)
	    set k [::helplfp::getColor]
	    dlp_draw $p lines $y -linecolor $::helplfp::cols($k) -lstyle $w
	    set xspot 0.01
	    if {$ssl != ""} {
		dlp_wincmd $p {0 0 1 1} "dlg_text $xspot [expr .93-${::helplfp::vars(plotcntr)}*.05] $ssl -color $::helplfp::cols($k) -just -1"
		set xspot 0.15
      	    }
	    dlp_wincmd $p {0 0 1 1} "dlg_text $xspot [expr .93-${::helplfp::vars(plotcntr)}*.05] [lindex $uniqval $i] -color $::helplfp::cols($k) -just -1"
	    incr ::helplfp::vars(plotcntr)
	}

	# legend title
	dlp_wincmd $p {0 0 1 1} "dlg_text .01 .98 \{$sortby (N)\} -color $::colors(gray) -just -1"
	dlp_set $p xtitle "Time (ms)"
	if {[string match $::helpXCorr::byArgs(norm) raw]} {
	    dlp_set $p ytitle "\# Coins"
	} elseif {[string match $::helpXCorr::byArgs(norm) trnum]} {
	       dlp_set $p ytitle "Mean Coins per Trial"
	}
	dlp_winycmd $p {0 1} "dlg_lines {0 0} {0 1} -linecolor $::colors(gray)"
	return $p
    }


}

::helpXCorr::init_xc_params 




