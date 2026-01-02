# AUTHORS
#   REHBM
#
# DESCRIPTION
#   procs for runnig roc analysis on two lists

package provide roc 1.0

namespace eval roc {
    proc details { lo hi {crit_step optimize} } {
	# %HELP%
	# ::roc::details <lo> <hi> ?crit_step?
	#    Returns a group with hit rate, false alarm rate, and area under ROC curve that describes how well
	#      an ideal observer knows, trial-by-trial, that the 'hi' distribution is higher
	#    In this case, hit rate is how often a trial in hi is classified as hi;  false alarm is how often
	#      a trial in lo is classified as hi
	#    if crit_step is "optimize" [default value], then thresholds are set at the value of every unique value of lo+hi.
	#      This way, we avoid having to evaluate at threshold criterias that end up being the same point on the fa v. hr curve.
	#      This should give us the best estimate of ROC area possible while optimizing for speed (fewest threshold evaulations).
	#    if crit_step is a value, then step though from min to max of lo+hi by crit_step
	#
	#    See Also - ::roc::plot - the return group from ::roc::details can be used as an arg to ::roc::plot to visualize ROC curve
	# %END%
	return [helproc::analyze $lo $hi $crit_step 1]
    }
    proc area { lo hi {crit_step optimize} } {
	# %HELP%
	# ::roc::area <lo> <hi> ?crit_step?
	#    Returns a group with hit rate, false alarm rate, and area under ROC curve that describes how well
	#      an ideal observer knows, trial-by-trial, that the 'hi' distribution is higher
	#    In this case, hit rate is how often a trial in hi is classified as hi;  false alarm is how often
	#      a trial in lo is classified as hi
	#    if crit_step is "optimize" [default value], then thresholds are set at the value of every unique value of lo+hi.
	#      This way, we avoid having to evaluate at threshold criterias that end up being the same point on the fa v. hr curve.
	#      This should give us the best estimate of ROC area possible while optimizing for speed (fewest threshold evaulations).
	#    if crit_step is a value, then step though from min to max of lo+hi by crit_step
	#
	#    See Also - ::roc::plot - the return group from ::roc::details can be used as an arg to ::roc::plot to visualize ROC curve
	# %END%
	return [helproc::analyze $lo $hi $crit_step 0]
    }
    proc plot { g } {
	# %HELP%
	# ::roc::plot <g>
	#    takes a group (return group from ::roc::details) and returns a plot of the corresponding roc curve.
	# %END%
	set p [dlp_xyplot $g:fa $g:hr]
	dlp_setxrange $p 0 1.
	dlp_setyrange $p 0 1.
	dlp_set $p xtitle "FALSE ALARM : ('lo' trials classified as 'hi')"
	dlp_set $p ytitle "HIT RATE: ('hi' trials classified as 'hi')"
	dlp_set $p title "area = [dl_first $g:area]"

	# redraw data with shaded bottom
	dlp_draw $p lines "0 0" -fillcolor $::colors(light_gray)
	dlp_draw $p markers "0 0" -marker fcircle -color $::colors(red) -size 10

	# add dotted line at x=y
	dlp_draw $p lines "[dlp_addXData $p {0 1}] [dlp_addYData $p {0 1}]" -lstyle 2

	return $p
    }
}
namespace eval helproc {
    proc analyze { lo hi crit_step return_group } {
	if {![dl_length $lo] || ![dl_length $hi]} {
	    set crit_step 1; # this will stop errors that occur when trying to optimize without data (will yield area = 0.5)
	}

	dl_local vals [dl_combine [dl_float $lo] [dl_float $hi]]; # need floats in case our steps are floats
	dl_local tags [dl_combine [dl_zeros [dl_length $lo]] [dl_ones [dl_length $hi]]]

	if {$crit_step == "optimize"} {
	    # calculate at every step that yields a unique fa-hr point on the ROC curve (we'll asdd  0,0 and 1,1 later)
	    dl_local uv [dl_unique $vals]
	    if {[dl_length $uv] == 1} {
		# then all values are the same, so can't do analysis
		set mindiff 0
	    } else {
		set mindiff [dl_min [dl_diff [dl_sort $uv]]]
	    }
	    dl_local crit_steps [dl_sub $uv [expr .5 * $mindiff]]; # make sure we are always just below the value to avoid rounding errors for floats
	    
	} else {
	    # step through raising criterion based on $crit_step
	    set start [dl_min [dl_sub $vals $crit_step]]
	    set stop  [dl_max [dl_add $vals $crit_step]]
	    dl_local crit_steps [dl_series $start $stop $crit_step]
	}

	# dls to store 'hit rate' and 'false alarms'
	dl_local hr [dl_flist]
	dl_local fa [dl_flist]

	# caluculate false alarm and hit rate for each criterion
	dl_append $fa 1.0
	dl_append $hr 1.0
	foreach crit [dl_tcllist $crit_steps] {
	    if { ![catch { fahr_at_crit $vals $crit $tags } tmp] } {
		dl_append $fa [lindex $tmp 0]
		dl_append $hr [lindex $tmp 1]
	    }
	}
	dl_append $fa 0.0
	dl_append $hr 0.0

	# now get the area under the ROC curve using trapezoid function
	set area [trapz $fa $hr]

	if $return_group {
	    set gout [dg_create]
	    dl_set $gout:hr $hr
	    dl_set $gout:fa $fa
	    dl_set $gout:area $area
	    return $gout
	} else {
	    return $area
	}
    }

    proc fahr_at_crit { vals crit class } {
	dl_local s [dl_sortedFunc $vals "[dl_gt $vals $crit] $class" dl_lengths]
	set fa [expr [dl_get $s:2 1]./([dl_get $s:2 0]+[dl_get $s:2 1])]
	set hr [expr [dl_get $s:2 3]./([dl_get $s:2 2]+[dl_get $s:2 3])]
	return "$fa $hr"
    }

    proc trapz { x y } {
	dl_local x1 [dl_abs [dl_diff $x]]
	dl_local y1 [dl_div [dl_add $y [dl_shift $y -1]] 2.0]

	dl_append $x1 0.0;		# last position doesn't count
	return [dl_sum [dl_mult $x1 $y1]]
    }
}

