########################################################################
# NAME
#   stats.tcl
#
# DESCRIPTION
#   Routines for statistical methods
#
# AUTHOR
#   REHBM,DLS, 1/05
#
########################################################################

package provide dlsh 1.2

proc dl_stdList { dl } {
    if ![dl_length $dl] {
	dl_return $dl
    } else {
	dl_return [dl_stds [dl_transpose $dl]]
    }
}
proc dl_semList { dl } {
    if ![dl_length $dl] {
	dl_return $dl
    } else {
	dl_return [dl_sems [dl_transpose $dl]]
    }
}
proc dl_95ciList { dl } {
    if ![dl_length $dl] {
	dl_return $dl
    } else {
	dl_return [dl_95cis [dl_transpose $dl]]
    }
}


proc dl_95cis { lists } {
    # %HELP%
    # dl_95cis <lists>
    #    given a list of flists or ilists, supplies a single value for each list
    #    which is the distance from the mean (+/-) of the 95% confidence interval.
    #    i.e. Confidence Interval = (dl_mean of lists) +/- (dl_95cis lists)
    #    Assumes a large n, so eqivilent to 95%Ci = 1.96 * SEM
    # %END%
    dl_local ns [dl_lengths $lists]
    dl_local stds [dl_stds $lists]
    
    # t needs to come from a table - or alternatively we could bootstrap it out of the data
    dl_local t 1.96; # dl_local t [lookup t?]
    dl_return [dl_mult $t [dl_div $stds [dl_sqrt $ns]]]
}
proc dl_95ci { list } {
    # %HELP%
    # dl_95ci <lists>
    #    given an flist or ilist, supplies a single value which is the
    #    distance from the mean (+/-) of the 95% confidence interval.
    #    i.e. Confidence Interval = (dl_mean of lists) +/- (dl_95cis lists)
    #    Assumes a large n, so eqivilent to 95%Ci = 1.96 * SEM
    # %END%
    return [dl_tcllist [dl_95cis [dl_llist $list]]]
}

proc dl_sems { lists } {
    # %HELP%
    # dl_sems <lists>
    #    given a list of flists or ilists, supplies a single value for each list
    #    which is the standard error of the mean (SEM) for that list.
    #    SEM = (standard deviation) / sqrt(N)
    # %END%
    dl_local ns [dl_lengths $lists]
    dl_local stds [dl_stds $lists]
    dl_return [dl_div $stds [dl_sqrt $ns]]
}
proc dl_sem { list } {
    # %HELP%
    # dl_sem <lists>
    #    given an flist or ilist, supplies a single value which is
    #    the standard error of the mean (SEM) for that list.
    #    SEM = (standard deviation) / sqrt(N)
    # %END%
    return [dl_tcllist [dl_sems [dl_llist $list]]]
}


proc dl_spearman { xs ys } {
    # %HELP%
    #  dl_spearman <xs> <ys>
    #     takes vectors of ints or floats and returns a list of two values.
    #     the first representating the spearman correlation coefficient, and 
    #     the second representing z. the algorithm used here is to first rank
    #     the xs and ys independantly, using a mid-rank ties algorithm, and
    #     then passing the ranked data into dl_pearson.
    # %END%
    if {[dl_depth $xs] || [dl_depth $ys]} {
 	error "dl_spearman - you can only use lists of depth zero for now due to limitations of dl_recodeWithTies"
    }
    dl_local xrank [dl_recodeWithTies $xs]
    dl_local yrank [dl_recodeWithTies $ys]
    dl_return [dl_pearson $xrank $yrank]	  
}


####################################################################
#
#  FUNCTION
#     dl_pearson(s)
#
#  DESCRIPTION
#     calc correlation coefficient between two vectors (or list of vectors)
#
#  SEE ALSO
#     for a .c implementation of pearson correlation see dl_pearson in load_Stats
#######################################################################
# proc dl_pearson { x y } {
#     if {[dl_depth $x] || [dl_depth $y]} {
# 	error "dl_pearson x y: x and y must each be a list ints or floats"
#     }
#     return [dl_tcllist [dl_pearsons [dl_llist $x] [dl_llist $y]]]
# }
# proc dl_pearsons { x y } {

#     if {![dl_depth $x] || ![dl_depth $y]} {
# 	error "dl_pearsons x y: x and y must each be a list of lists"
#     }

#     # make sure we are working with floats
#     dl_local x [dl_float $x]
#     dl_local y [dl_float $y]

#     dl_local N [dl_lengths $x]
#     dl_local sum_x [dl_sums $x]
#     dl_local sum_y [dl_sums $y]

#     dl_local xy [dl_mult $x $y]
#     dl_local sum_xy [dl_sums $xy]

#     dl_local xx [dl_mult $x $x]
#     dl_local yy [dl_mult $y $y]
#     dl_local sum_xx [dl_sums $xx]
#     dl_local sum_yy [dl_sums $yy]
    
#     dl_return [dl_div \
# 		   [dl_sub $sum_xy [dl_div [dl_mult $sum_x $sum_y] $N]] \
# 		   [dl_sqrt [dl_mult [dl_sub $sum_xx [dl_div [dl_pow $sum_x 2] $N]] \
# 				 [dl_sub $sum_yy [dl_div [dl_pow $sum_y 2] $N]]]]]
# }

