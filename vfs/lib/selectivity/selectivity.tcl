# AUTHORS
#   REHBM
#
# DESCRIPTION
#   various measures of selctivity

package provide selectivity 1.0

namespace eval selectivity {
    proc selectivity_index { g chan tags tag1 tag2 align start stop } {
	# %HELP%
	# ::selectivity::selectivity_index <g> <chan> <tags> <tag1> <tag2> <align> <start> <stop>
	#
	# your standard selectivity index.  positive means tag1 is more responsive, negaitve means tag2 is more responsive.
	#
	#                    Rate(tag1) - Rate(tag2)
	# selectivit_index = -----------------------
	#                    Rate(tag1) + Rate(tag2)
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'tag1' one specific value from tags
	#    'tag2' the second specific value from tags
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	# %END%

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec

	# get mean rate sorted by tags
	set tag1_rate [dl_mean [dl_select $rates [dl_eq $g:$tags $tag1]]]
	set tag2_rate [dl_mean [dl_select $rates [dl_eq $g:$tags $tag2]]]
	set ssi [expr ($tag1_rate - $tag2_rate) / ($tag1_rate + $tag2_rate)]

	return $ssi
    }
    proc bwsi { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::bwsi g chan tags align start stop
	#    shortcut to ::selectity::best_worst_selectivity_index
	# %END%	
	return [best_worst_selectivity_index $g $chan $tags $align $start $stop]
    }
    proc best_worst_selectivity_index { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::best_worst_selectivity_index <g> <chan> <tags> <align> <start> <stop>
	#
	# your standard selectivity index using best and worst stimuli (from tags).
	#    positive means 'best' is more responsive, negaitve means 'worst' is more responsive.
	#
	#                               Rate(best) - Rate(worst)
	# best_worst_selectivit_index = -----------------------
	#                               Rate(best) + Rate(worst)
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	# %END%

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec

	# get mean rate sorted by tags
	set best  [dl_max $rates]
	set worst [dl_min $rates]

	if {$best == $worst} {
	    set bwsi 0.
	} else {
	    set bwsi [expr ($best - $worst) / ($best + $worst)]
	}
	return $bwsi
    }

    proc bwsimr { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::bwsimr g chan tags align start stop
	#    shortcut to ::selectity::best_worst_selectivity_index_must_respond
	# %END%	
	return [best_worst_selectivity_index_must_respond $g $chan $tags $align $start $stop]
    }
    proc best_worst_selectivity_index_must_respond { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::best_worst_selectivity_index <g> <chan> <tags> <align> <start> <stop>
	#
	# smilar to ::selectivity::best_worst_selectivity_index but 'worst' stimulus must still be responsive compared to the background rate
	#
	#                                             Rate(best) - Rate(worst, but still responsive)
	# best_worst_selectivit_index_must_respond  = ----------------------------------------------
	#                                             Rate(best) + Rate(worst, but still responsive)
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	# %END%

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec

	dl_local rates [dl_select $rates [dl_gt $rates 0]]
	if ![dl_length $rates] {
	    dl_local rates [dl_flist 0. 0.]; # we need to have something there...
	    puts "   best_worst_selectivity_index_must_respond - it's a no go. no response from any stimulus"
	}
	
	# get mean rate sorted by tags
	set best  [dl_max $rates]
	set worst_responsive [dl_min $rates]

	if {$best == $worst_responsive} {
	    set bwsimr 0.
	} else {
	    set bwsimr [expr ($best - $worst_responsive) / ($best + $worst_responsive)]
	}
	return $bwsimr
    }

    proc broadness { g chan tags align start stop algorithm_and_args compare_to_all_bg {bgstart -50} {bgstop 50} } {
	# %HELP%
	# ::selectivity::broadness <g> <chan> <tags> <align> <start> <stop> <algorithm_and_args> <compare_to_all_bg> ?bgstart? ?bgstop?
	#
	# Ref: Freedman, Riesenhubner, Poggio and Miller (2006) or Kobatake, Wang, Tanaka (1998)
	# broadness is the proportion of stimuli (or whatever defined by 'tags') that cause a
	#   significant response relative to the background rate.
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	#    'algorithm_and_args' defines how to determine significance of a set of repsonses.  following are methods and associated args
	#         "wilcoxon <p_level>" - wilcoxon test across response and bg_rate
	#         "t_test <p_level>" - t_test between response and bg_rate
	#         "pc_max <p_level> <pc_max>" - does response reach percentage of maximum response? pc_max defines percentage threshold for significance.
	#         "sd_thresh <sd_level>" - does response surpass [mean_bgrate + (sd_level * bgrate_standard_deviation)]
	#    'compare_to_all_bg' is background defined for each set of respones separately or across all trials?
	#    'bgstart' start of window to measure bgrate relative to stimon [default is -50]
	#    'bgstop' end of window to measure bgrate relative to stimon [default is 50]
	# %END%

	set COMPARE_TO_ALL_BG $compare_to_all_bg
	set ALGORITHM [lindex $algorithm_and_args 0]
	switch $ALGORITHM {
	    wilcoxon -
	    t_test {
		set P_LEVEL [lindex $algorithm_and_args 1]
	    }
	    pc_max {
		set PC_LEVEL [lindex $algorithm_and_args 1]
		set PC_MAX [lindex $algorithm_and_args 2]
	    }
	    sd_thresh {
		set SD_LEVEL [lindex $algorithm_and_args 1]
	    }
	    default {
		error "selectivity::broadness - algorithm $ALGORITHM not defined"
	    }
	}

	# get spk rates for bg time window
	dl_local bgpre [dl_add $g:$align $bgstart]
	dl_local bgpost [dl_add $g:$align $bgstop]
	dl_local bgspks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $bgpre $bgpost]]]
	dl_local bgrates [dl_mult [dl_div [dl_lengths $bgspks] [expr $bgstop - $bgstart].] 1000.]; #spks/sec

	# get mean rate across all trials
	set  mean_bg [dl_mean $bgrates]
	set  sd_bg [dl_std $bgrates]
	set thresh [expr $mean_bg + 2. * $sd_bg]

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec
	
	# get mean rate sorted by tags
	dl_local tag_rates [dl_sortedFunc $rates $g:$tags]
	dl_local r $tag_rates:1
	dl_local t $tag_rates:0

#  	dl_local p [dl_flist]
  	dl_local m [dl_flist]
  	dl_local sig [dl_ilist]
  	foreach stim [dl_tcllist [dl_unique $g:$tags]] {
  	    dl_local this_r [dl_select $rates [dl_eq $g:$tags $stim]]
	    
	    if $COMPARE_TO_ALL_BG {
		# compare across all possible backgrounds (but can't do a paired test then)
		dl_local this_bg $bgrates
	    } else {
		dl_local this_bg [dl_select $bgrates [dl_eq $g:$tags $stim]]
	    }

	    switch $ALGORITHM {
		wilcoxon {
		    # wilcoxon sign-ranked test on rate distributions for each stimulus
		    if ![catch {set stat [imsls_wilcoxon_rank_sum $this_r $this_bg]} dummy] {
			dl_append $m [dl_median $this_r]
			set p [dl_get $stat:stats 2]
			dg_delete $stat
			if {$p < $P_LEVEL} {
			    dl_append $sig 1
			} else {
			    dl_append $sig 0
			}
		    } else {
			puts "skipping $stim.  not enough trials (<3)."
		    }
		}
		t_test {
		    # t-test on rate distributions for each stimulus
		    dl_append $m [dl_mean $this_r]
		    if ![catch {set stat [imsls_normal_two_sample $this_r $bgrates]} dummy] {
			set p [dl_get $stat:t_test_eq_var 2]
			dg_delete $stat
		    } else {
			set p 1.
		    }
		    if {$p < $P_LEVEL} {
			dl_append $sig 1
		    } else {
			dl_append $sig 0
		    }
		}
		pc_max {
		    dl_append $m [dl_mean $this_r]
		    #set t ... this is tricky because we don't know 
		    if {$m > $t} {
			dl_append $sig 1
		    } else {
			dl_append $sig 0
		    }
		}
		sd_thresh {
		    dl_append $m [dl_mean $this_r]
		    set t [expr [dl_mean $this_bg] + $SD_LEVEL * [dl_std $this_bg]]
		    if {$m > $t} {
			dl_append $sig 1
		    } else {
			dl_append $sig 0
		    }
		}
	    }

 	}

 	set n [dl_length $p]
 	set nhi [dl_sum [dl_and [dl_lt $p 0.01] [dl_gt $m $mean_bg]]]

	set broadness [expr [dl_sum $sig]. / [dl_length $sig]. ]
	return $broadness
    }

    proc breadth { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::breadth <g> <chan> <tags> <align> <start> <stop>
	#
	# Ref: Freedman, Riesenhubner, Poggio and Miller (2006)
	#   1. normalize response to all stimuli on a scale of 0 to 1 (0 being the worst response and 1 being the best?)
	#   2. find median normalized rate
	#   3. breadth = 1.0 - (median normalized rate)
	#      higher values (smaller median values) indicate sharper tuning (weak response to most stimuli)
	#      lower values (higher median values) indicate broader tuning
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	# %END%

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec
	
	# get mean rate sorted by tags
	dl_local tag_rates [dl_sortedFunc $rates $g:$tags]
	dl_local r $tag_rates:1
	dl_local t $tag_rates:0
	set n [dl_length $r]

	set max [dl_max $r]
	set min [dl_min $r]
	dl_local r_norm [dl_div [dl_sub $r $min] [expr $max-$min]]
	set median [dl_median $r_norm]
	set breadth [expr 1.0 - $median]

	return $breadth
    }

    proc dos { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::dos <g> <chan> <tags> <align> <start> <stop>
	#    shortcut to ::selectity::depth_of_selectivity
	# %END%	
	return [depth_of_selectivity $g $chan $tags $align $start $stop]
    }
    proc depth_of_selectivity { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::breadth <g> <chan> <tags> <align> <start> <stop>
	#
	# Ref: Rainer and Miller, 2000; Rainer et al 1998; Moody, Wise, di Pellegrino, Zipser (1998)
	#         n - (SUM Ri)/Rmax
	#    s = --------------------
	#              n - 1
	#
	# if (SUM Ri) = Rmax, then only response is to one stimulus and s = 1
	# if not selective, then R1= R2 = ... = Rmax and (SUM Ri)/Rmax = n, so s = 0
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	# %END%

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec
	
	# get mean rate sorted by tags
	dl_local tag_rates [dl_sortedFunc $rates $g:$tags]
	dl_local r $tag_rates:1
	dl_local t $tag_rates:0
	set n [dl_length $r]

	if {[dl_max $r] == 0} {
	    # this is a special case where the cell never responds, so dos = 0 (since sum and max are the same)
	    set dos 0.0
	} else {
	    set dos [expr ( $n- ([dl_sum $r] / [dl_max $r]) ) / ($n - 1) ]
	}
 	return $dos
    }

    proc crsa { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::crsa <g> <chan> <tags> <align> <start> <stop>
	#    shortcut to ::selectity::cumulative_ranked_sum_area
	# %END%	
	return [cumulative_ranked_sum_area $g $chan $tags $align $start $stop]
    }
    proc cumulative_ranked_sum_area { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::cumulative_ranked_sum_area <g> <chan> <tags> <align> <start> <stop>
	#
	# saw this at SFN on a poster about V4 Color selectivity.
	# 1. create a plot of the cummulative-ranked-normalized-sum (kind of similar to an ROC curve)
	#    x-axis = stimulus rank-index (range = 0-1)
	#             a. rank stimuli from best-to-worst so range is 1-to-N, where N=total # stimuli
	#             b. divide rank by N
	#    y-axis = cumulative response sum (range = 0-1)
	#             a. take responses for ranked stimuli
	#             b. normalize responses by total sum of all responses, so sum will add to 1
	#    curve will go from (0,0) [need to pad responses sums with a leading zero] to (1,1) and will be > than x=y the whole time
	# 2. cummlative_sum_area = area under the curve
	#    csa = 0.5 if all stimuli respond equally since curve runs along x=y
	#    csa = 1.0 if only one responds at all and all others zero.
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	# %END%

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec

	# get mean rate sorted by tags
	dl_local tag_rates [dl_sortedFunc $rates $g:$tags]
	dl_local r [dl_reverse [dl_sort $tag_rates:1]]
	dl_local t [dl_reverse [dl_sortByList $tag_rates:0 $tag_rates:1]]

	# normalize rates to the sum of the rates
	dl_local rsum [dl_sum $r]
	dl_local rnorm [dl_div $r $rsum]
	dl_local rcum [dl_cumsum $rnorm]
	dl_prepend $rcum 0.; # prepend a zeroth image so graph starts at zero
	dl_local xrank [dl_div [dl_index $rcum] [dl_length $rnorm].]

	# trapazoid method for getting the area
	dl_local x1 [dl_abs [dl_diff $xrank]]
	dl_local y1 [dl_div [dl_add $rcum [dl_shift $rcum -1]] 2.0]
	dl_append $x1 0;		# last position doesn't count
	set area [dl_sum [dl_mult $x1 $y1]]
	set area [trapz $xrank $rcum]
	return $area
    }
    proc trapz { x y } {
	dl_local x1 [dl_abs [dl_diff $x]]
	dl_local y1 [dl_div [dl_add $y [dl_shift $y -1]] 2.0]

	dl_append $x1 0;		# last position doesn't count
	return [dl_sum [dl_mult $x1 $y1]]
    }

    proc sparseness { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::sparseness <g> <chan> <tags> <align> <start> <stop>
	#
	# Ref: Suzuki et al (2006) refrencing Rolls et al (1995) and Olshausen and Field (2004)
	# Ref: Lehky, Desimone and Sejnowski (2005) notes that this index is sensitive to the distributions mean and variance.
	#   thus, to properly use this measure, the data should be standardized for mean and variance beforehand. (which isn't done here...yet)
	#
	# average response squared, divided by average squared response
	#    = 1 if all stimuli evoke responses of equal magnitude
	#    approaches 1/n when only one stimulus elicits a big response
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	# %END%

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec
	
	# get mean rate sorted by tags
	dl_local tag_rates [dl_sortedFunc $rates $g:$tags]
	dl_local r $tag_rates:1
	dl_local t $tag_rates:0

	set spareseness [dl_tcllist [dl_div [dl_pow [dl_mean $r] 2] [dl_mean [dl_pow $r 2]]]]

	return $spareseness
    }

    proc kurtosis { g chan tags align start stop } {
	# %HELP%
	# ::selectivity::kurtosis <g> <chan> <tags> <align> <start> <stop>
	#
	# Ref: Lehky, Desimone and Sejnowski (2005)
	#               _
	#          <(ri-r)^4>
	#    Sk = -----------  - 3
	#             sigma^4
	# 
	# where ri is the response to the ith image, r is the mean response over all images,
	# sigma is the standard deviation of the responses, and <.> is the mean value operator.
	# Subracting three normalizes the measure so that a Gaussian has a kurtosis of of zero.
	#
	# Larger values of Sk correspond to greater selectivity.
	#
	# Practival problem with using kurtosis is that it involves raising data to the fourth power,
	# which makes estimates of this measure highly sensitive
	#
	# args:
	#    'g' is data group
	#    'chan' is spike channel
	#    'tags' is the list to sort rates by (typically stim_names or image).  just like what you would put dl_sortedFunc
	#    'align' list of times that rate windows will be centered on
	#    'start' list of times relative to 'align' that denote the begining of the rate window
	#    'stop' list of times relative to 'align' that denote the end of the rate window
	# %END%

	# get spk rates for time window
	dl_local pre [dl_add $g:$align $start]
	dl_local post [dl_add $g:$align $stop]
	dl_local spks [dl_select $g:spk_times [dl_and [dl_eq $g:spk_channels $chan] [dl_between $g:spk_times $pre $post]]]
	dl_local rates [dl_mult [dl_div [dl_lengths $spks] [expr $stop - $start].] 1000.]; #spks/sec
	
	# get mean rate sorted by tags
	dl_local tag_rates [dl_sortedFunc $rates $g:$tags]
	dl_local r $tag_rates:1
	dl_local t $tag_rates:0

	set kurtosis [dl_tcllist [dl_sub [dl_div [dl_mean [dl_pow [dl_sub $r [dl_mean $r]] 4]] [dl_pow [dl_std $r] 4]] 3]]

	return $kurtosis
    }
}
