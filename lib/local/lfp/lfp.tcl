# AUTHORS
#   FEB 2006 - REHBM
#
# DESCRIPTION
#    this package provides basic tools for ploting lfps
#    all functions are provided under the ::lfp namespace.  all helper
#       functions (called by ::lfp procs, but not directly by user)
#       are provided under the ::helplfp namespace.  this will help users
#       find appropriate procs to call from the command line.
#    at a minimum, all procs in ::lfp namespace should contain a %HELP%
#       section that can be parsed by the 'use' proc.
#
# TODO
#   

package provide lfp 2.0
package require loaddata; # for getting eeg data

namespace eval ::lfp { } {
    proc get_data { g args } {
	# %HELP%
	# ::lfp::get_data: <g> ?options...?
	#    calls ::eeg::get_eeg_data using an epoch of stimon+$from to stimon+$to
	#    will center and rescale data as well.
	#
	#    Options to include at end of proc; format: -option option_value
	#    Default value is shown in []
	#       -align : which dl in group <g> is time 0 for pre/post [stimon]
	#              NOTE - this is not time=0 in eeg_time.  eeg_time is always relative to beginobs
	#       -pre : amount of time (positive ms) before 'align' to extract data from [100]
	#       -post : amount of time (positive ms) after 'align' to extract data to [400]
	#         : Data is extracted from (align-pre) to (align+post) for each trial
	#       -gain : gain signal was recorded at [5000]
	#       -baseline_samps : number of samples (from earliest obtained sample)
	#                         to average to get a baseline, to zero data. [250]
	#       -raw : boolean, do you want the raw data (no centering or rescaleing)? [0]
	# %END%

	# reset default values
	helplfp::reset_defaults

	# parse switches
 	for {set i 0} {$i < [llength $args]} {incr i} {
 	    set a [lindex $args $i]
 	    if {[string range $a 0 0] != "-"} {
 		set globargs [lrange $args $i end]
 		break
 	    } elseif {[string range $a 0 1] == "--"} {
 		set globargs [lrange $args [incr i] end]
 		break
 	    } else {
 		# below we define any tags that will be used
 		switch -exact [string range $a 1 end] {
 		    align {
			set val [lindex $args [incr i]]
			if ![dl_exists $g:$val] {
			    error "::lfp::get_data - align dl $align does not exist in group $g"
			}
			set ::helplfp::vars(align) $val
		    }
		    pre -
		    post -
		    gain -
		    baseline_samps {
			# just take next arg as value
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set ::helplfp::vars(${var}) $val
		    }
		    raw {
			# these are all booleans
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {$val != 0 && $val != 1 } {
			    error "::lfp::get_data - expect boolean (0 or 1) for $var"
			}
 			set ::helplfp::vars(${var}) $val
 		    }
		    default {
 			error "::lfp::get_data: invalid switch $a \n   must be 'align' 'pre' 'post' 'gain' 'baseline_samps' 'raw'.\n   use ::lfp::get_data for more details."
 		    }
 		}
 	    }
 	}
	puts "     getting lfp data for $g from ($helplfp::vars(align) - $helplfp::vars(pre)) to ($helplfp::vars(align) + $helplfp::vars(post))"
	dl_local epochs [dl_combineLists [dl_sub $g:$helplfp::vars(align) $helplfp::vars(pre)] [dl_add $g:$helplfp::vars(align) $helplfp::vars(post)]]
	::eeg::get_eeg_data $g $epochs; # this is from the load_data package
	if !$::helplfp::vars(raw) {
	    puts "          centering and rescaling data"
	    ::helplfp::center $g $::helplfp::vars(baseline_samps)
	    ::helplfp::rescale $g $::helplfp::vars(gain)
	}
	return $g
    }

    
    proc onset { g chan sortby } {
	# %HELP%
	# ::lfp::onset: <g> <chan> <sortby>
	#    take average lfp for channel <chan> in group <g> sorted by <sortby> - dl(s) in group <g>.
	#    if <sortby> contains more than one variable, then each unique combination is 
	#    plotted as a different trace on same plot.
	# %END%
	if ![dl_exists $g:eeg] {error "::lfp::onset - eeg data does not exist for group $g. (try ::lfp::get_data $g)"}

	dl_local alleeg [dl_unpack [dl_select $g:eeg [dl_eq $g:eeg_chans $chan]]]

	set sb [join $g:$sortby " $g:"]
	dl_local eeg [dl_sortedFunc $alleeg $sb]
	dl_local cnt [dl_sortedFunc $alleeg $sb dl_lengths]

	set p [dlp_newplot]
	
	set n [expr [dl_length $eeg]-1]
	set npoints [dl_length $eeg:$n:0]
	dlp_addXData $p [dl_add  [dl_mult [dl_fromto 0 $npoints] [expr 1/2.5]] -50]
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
	clearwin; dlp_plot $p
	helplfp::mark_plots "::lfp::onset $g $chan $sortby" $g

	return $p
    }
    proc onset_split { g chan sortby } {
	# %HELP%
	# ::lfp::onset: g chan sortby
	#    take average lfp for channel <chan> in group <g> sorted by <sortby> - dl(s) in group <g>.
	#    if <sortby> provides two lists then they are split up (by second list) into seperate plots.
	# %END%

	# check for eeg lists and get them if we can
	if ![dl_exists $g:eeg] {error "::lfp::onset - eeg data does not exist for group $g. (try ::lfp::get_data $g)"}
	dl_local chanref [dl_eq $g:eeg_chans $chan]
	dl_local eeg [dl_unpack [dl_select $g:eeg $chanref]]

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
	foreach vs1 [dl_tcllist [dl_unique $split_sort_vals]] {
	    if {$vs1 != ""} {
		dl_local thiseeg1_ref [dl_eq $g:$split_sort_label $vs1]
		set title ""
	    } else {
		dl_local thiseeg1_ref [dl_ones [dl_length $eeg]]
		set title "$split_sort_label = $vs1"
	    }
	    set g2 [dg_copySelected $g $thiseeg1_ref]
	    set p [::lfp::onset $g2 $chan $plot_sort_label]
	    dlp_set $p title "$split_sort_label $vs1"
	    dg_delete $g2
	    lappend plots $p
	}
	# replot the split plots
	clearwin
	dlp_setpanels [dl_length [dl_unique $split_sort_vals]] 1
	set j -1
	foreach p $plots {
	    dlp_subplot $p [incr j]
	}
	plots::mark_plots "::lfp::onset_split $g $chan $sortby"	$g

	return $plots
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
    proc reset_defaults { } {
	# array to hold all info needed to get and plot lfps (changable through option tags)
	array set ::helplfp::vars ""
	set ::helplfp::vars(align) stimon;  # dl in group to align data to
	set ::helplfp::vars(pre)   100;      # time before align to get
	set ::helplfp::vars(post)  400;     # time after align to get
	set ::helplfp::vars(gain)  5000;    # gain signal was recorded at
	set ::helplfp::vars(baseline_samps) 250; # number of samples to average over to get baseline and center eeg
	set ::helplfp::vars(raw) 0; # should we just get raw data? (no center or rescale)
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

}