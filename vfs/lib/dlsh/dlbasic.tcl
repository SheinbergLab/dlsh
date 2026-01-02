#
# NAME
#    dlbasic.tcl
#
# DESCRIPTION
#    Basic dl_funcs that are derived directly from other primitives
#
#

package provide dlsh 1.2

proc dl_between { dl low high } {
    dl_return [dl_and [dl_lt $dl $high] [dl_gt $dl $low]]
}
proc dl_betweenEq { dl low high } {
    dl_return [dl_and [dl_lte $dl $high] [dl_gte $dl $low]]
}
proc dl_betweenEqLow { dl low high } {
    dl_return [dl_and [dl_lt $dl $high] [dl_gte $dl $low]]
}
proc dl_betweenEqHigh { dl low high } {
    dl_return [dl_and [dl_lte $dl $high] [dl_gt $dl $low]]
}

proc dl_closeto { dl val {epsilon .0001} } {
    # %HELP%
    # NAME
    #  dl_closeto
    #
    # USAGE
    #  dl_closeto <dl> <val> ?epsilon?
    #
    # DESCRIPTION
    #  takes lists ints or floats (they can be packed) and returns a boolean value list or same structure.  return value is 1 if dl value is <val>+/- ?epsilon?.  By default, epsilon is very small (so dl_closeto can be used like dl_eq for floats without rounding errors) but it can be changed to any int or float value.  Note that dl_closeto uses dl_between, so value cannot be exactly <val>+/-?epsilon?
    #
    # EXAMPLE
    #    % dl_set a "0.012 7.0000000001 9.5"
    #    > a
    #    % dl_tcllist [dl_closeto a 7]
    #    > 0 1 0
    #    % dl_tcllist [dl_closeto a 8 1.5]
    #    > 0 1 0
    #    % dl_tcllist [dl_closeto a 8 1.51]
    #    > 0 1 1
    # %END%
    
    # old and slow algorithm
    #dl_return [dl_between $dl [dl_sub $val $epsilon] [dl_add $val $epsilon]]

    # this algorithm for calculating dl_closeto is 40% faster than the current algorithm, which uses [dl_between [dl_lt] [dl_gt]]
    dl_return [dl_lt [dl_abs [dl_sub $dl $val]] $epsilon]
}

proc dl_to_dg { l } { 
    set g [dg_create]
    for { set i 0 } { $i < [dl_length $l] } { incr i } { 
	dl_set $g:list$i $l:$i
    } 
    return $g 
}


#
# NAME
#  dl_median(s)
#
# DESCRIPTION
#  Find median value of list
#

proc dl_median { data } {
    set n [dl_length $data]
    if { !$n } { return 0.0 }
    if { [expr $n%2] } {
	return [dl_get [dl_sort $data] [expr [dl_length $data]/2]]
    } else {
	set i [expr $n/2]
	dl_local s [dl_sort $data]
	return [expr ([dl_get $s $i]+[dl_get $s [expr $i-1]])/2.0]
    }
}

proc dl_medians { l } {
    dl_local retlist [dl_flist]
    foreach i [dl_tcllist [dl_fromto 0 [dl_length $l]]] {
	dl_append $retlist [dl_median $l:$i]
    }
    dl_return $retlist
}

proc dl_getPercentile { dl percentile } {
    # %HELP%
    # NAME
    #  dl_getPercentile <dl> <percentile>
    #
    # DESCRIPTION
    #  returns the value of list <dl> at the percentile <percentile>.
    #  Percentile is a value 0-100
    #  dl_getPercentile <dl> 50 is equivilent to dl_median
    #
    #  <dl> should be int or float and this funciton returns a scalar
    # %END%
    return [dl_first [dl_getPercentiles [dl_llist $dl] $percentile]]
}
proc dl_getPercentiles { dl percentile } {
    # %HELP%
    # NAME
    #  dl_getPercentiles <dl> <percentile>
    #
    # DESCRIPTION
    #  returns the values of lists in <dl> at the percentile(s) <percentile>.
    #  Percentile is a value 0-100 or a list of percentiles (0-100) for each correspoding element in <dl>.
    #  dl_getPercentiles <dl> 50 is equivilent to dl_medians
    #
    #  <dl> should be list of ints or floats and this funciton returns an int/float list
    # %END%
    if { $percentile > 100 || $percentile < 0 } {
	error "dl_getPercentiles - percentile must be within range of 0 - 100 (not including extremes)."
    }

    if {[dl_length $percentile] == 1} {
	# unnecessary as dl_div can handle the scaler across a list (below)
	#dl_local percentile [dl_repeat $percentile [dl_length $dl]]
    } elseif {[dl_length $percentile] != [dl_length $dl]} {
	error "dl_getPercentiles - percentile list must be scalar or same length as value list"
    }
    
    switch $percentile {
	0 {
	    dl_local ret [dl_mins $dl]
	}
	100 {
	    dl_local ret [dl_maxs $dl]
	}
	default {
	    # get closest index to this percentile
	    dl_local idxs [dl_mult [dl_lengths $dl] [dl_div $percentile 100.]]
	    dl_local idxs [dl_int [dl_add $idxs .5]]; # round off to nearest index

	    # check to make sure we are trying to take from valid index (or push it to max/min)
	    dl_local idxs [dl_min [dl_sub [dl_lengths $dl] 1] $idxs]

	    # if $idxs == -1 at any value, then there is no valid value to take.  we need to account for this and return an empty list there.
	    # thus, we need to use dl_select below and not dl_choose, so we'll change idxs to ref
	    dl_local ref [dl_eq [dl_index $dl] $idxs]
	    dl_local ret [dl_unpack [dl_select [dl_sort $dl] $ref]]
	}
    }
    dl_return $ret
}

#
# NAME
#  dprime
#
# DESCRIPTION
#  Return dprime Z(H)-Z(F)
#

proc dprime { H F } {
    if { $H <= 0.0 || $H >= 1.0 } { 
	error "hitrate must be between 0 and 1"
    }
    if { $F <= 0.0 || $F >= 1.0 } { 
	error "false alarm rate must be between 0 and 1"
    }
    return [expr [dl_critz $H]-[dl_critz $F]]
}


proc dl_combineLists { args } {
    # %HELP%
    # NAME
    #  dl_combineLists
    #
    # DESCRIPTION
    #  takes lists of lists as arguments and returns single list created by
    #     combining all inner lists of same index.
    #
    # EXAMPLE
    #  dl_set a [dl_llist [dl_fromto 0 5] [dl_fromto 10 15]]
    #  dl_set b [dl_llist [dl_fromto 20 25] [dl_fromto 30 35]]
    #  dl_tcllist [dl_combineLists a b]
    #  > {{0 1 2 3 4} {20 21 22 23 24}} {{10 11 12 13 14} {30 31 32 33 34}}
    #
    #  in the above example, you can do a further unpacking step to get:
    #  dl_tcllist [dl_unpack [dl_combineLists a b]]
    #  > {0 1 2 3 4 20 21 22 23 24} {10 11 12 13 14 30 31 32 33 34}
    # %END%
    dl_local combined [dl_llist]
 
    # first see if we are going to need to pack lists of scalars
    dl_local depths [dl_ilist]
    foreach l $args {
	dl_append $depths [dl_depth $l]
    }
    if {![dl_max $depths] == 0} {
	# then some lists are deep so we need to pack all lowlevel ones
	set packem 1
    } else {
	set packem 0
    }
 
    # now put em all together, packing scalars if we need to
    foreach l $args {
	if {[dl_depth $l] == 0 && $packem} {
	    dl_local l [dl_pack $l]
	}
	dl_append $combined $l
    }
    if $packem {
	dl_local ret [dl_unpack [dl_transpose $combined]]
    } else {
	dl_local ret [dl_transpose $combined]
    }
    dl_return $ret
}
#  proc dl_combineLists { args } {
#      dl_local combined [dl_llist]
#      foreach l $args {
#  	if {[dl_depth $l] == 0} {
#  	    dl_local l [dl_pack $l]
#  	}
#  	dl_append $combined $l
#      }
#      dl_return [dl_unpack [dl_transpose $combined]]
#  }

proc dg_dirDump { } {
    # %HELP%
    # NAME
    #  dg_dirDump
    #
    # DESCRIPTION
    #  returns a list of all current dgs, one in each line
    # %END%
    puts [join [lsort [dg_dir]] "\n"]
}

proc use { proc } {
    # %HELP%
    # use:
    #   Usage: use <proc>
    #   Description: Provides information from headers defined within tcl procedure
    #   Header Format (all lines commented):
    #         %_HElP_% (remove underscores)
    #         <any text you want to see>
    #         %_END_% (remove underscores)
    #   Returns: help info for given proc
    #   Example: info body use
    #                 to see how structure of this use message is created
    # %END%

    if [catch {info body $proc} body] {
	error "$proc is not a procedure"
    }
    # if we are this far then we have a body for the given proc, check for use message %HELP% and %END%
    if {![regexp -- {%HELP%(.*?)%END%} $body all help_text]} {
	error "use could not find a valid header for $proc"
    }
    regsub -all -- \# $help_text "" help_text

    # the rest of this is to remove left spaces that are due to the indent of the comments in the file
    set k 1
    set i -1
    set trim ""
    while { $k } {
	set s [string index $help_text [incr i]]
	if {$s == "\n" || $s == " " || $s == "\t"} {
	    set trim ${trim}$s
	} else {
	    set k 0
	}
    }
    regsub -all -- $trim $help_text "\n" help_text
    regsub -- "\n" $help_text "" help_text
    #puts [string trim $help_text "\n\s\t"]
    puts $help_text
}

proc dl_distance { xy1 xy2 } {
    # %HELP%
    # dl_distance:
    #   Usage: dl_distance <xy1> <xy2>
    #   Description: Calculates the distance between pairs of coordinates.
    #                xy1 and xy2 represent lists of (x,y) pairs.
    #   Returns: list of floats that represent the distance between xy1
    #            and xy2 pairs of coordinates
    #   Example: 
    #            % dl_local xy1 [dl_llist [dl_flist 0 0] [dl_flist 4 0]]
    #              &xy1_30&
    #            % dl_local xy2 [dl_llist [dl_flist -5 0] [dl_flist 4 7]]
    #              &xy2_31&
    #            % dl_tcllist [dl_distance $xy1 $xy2]
    #              5.000000 7.000000
    # %END%

    if {[dl_length $xy1] != [dl_length $xy2]} {
	error "dl_distance: xy1 and xy2 must be the same length (include same number of coordinate pairs)"
    }
    
    dl_local x1 [dl_unpack [dl_select $xy1 [dl_llist "1 0"]]]
    dl_local y1 [dl_unpack [dl_select $xy1 [dl_llist "0 1"]]]
    dl_local x2 [dl_unpack [dl_select $xy2 [dl_llist "1 0"]]]
    dl_local y2 [dl_unpack [dl_select $xy2 [dl_llist "0 1"]]]
    dl_return [dl_sqrt [dl_add [dl_pow [dl_sub $x1 $x2] 2] \
			    [dl_pow [dl_sub $y1 $y2] 2]]]
}

proc dl_remap { dl_tomap dl_oldval dl_newval } {
    # %HELP%
    # dl_remap:
    #   Usage: dl_remap <dl_tomap> <dl_oldval> <dl_newval>
    #   Description: replaces all values in <dl_oldval> in the list ,dl_tomap> to those supplied by <dl_newval>.
    #                this can even be used to change the list type (see example)
    #                
    #   Returns: new list with replaced values
    # 
    #   Example: 
    #            % dl_local a [dl_ilist 0 1 2 3 0 1]
    #            &a_121&
    #            % dl_local old [dl_ilist 0 1 2 3]
    #            &old_122&
    #            % dl_local new [dl_slist zero one two three]
    #            &new_123&
    #            % dl_tcllist [dl_remap $a $old $new]
    #            zero one two three zero one
    # %END%
    if {[dl_length $dl_oldval] != [dl_length $dl_newval]} {
	error "dl_remap <dl_tomap> <dl_oldval> <dl_newval>\n     length dl_oldval and dl_newval must be the same."
    }
    dl_local new [dl_create [dl_datatype $dl_newval]]
    
    for {set i 0} {$i < [dl_length $dl_tomap]} {incr i} {

	# simplified 2/07 REHBM.  Always use more explicit $toget definition
	#if {[dl_datatype $dl_oldval] == "string"} {
	#    # accounts for oldvals being strings, which causes problems with dl_find
	#    set toget [dl_get $dl_tomap $i];
	#} else {
	#    # this fixes a problem when an slist supplies a string of numbers, which is then treated as an integer.
	#    dl_local toget [dl_create [dl_datatype $dl_oldval] [dl_get $dl_tomap $i]];
	#}
	
	# this fixes a problem when an slist supplies a string of numbers, which is then treated as an integer.
	dl_local toget [dl_create [dl_datatype $dl_oldval] [dl_get $dl_tomap $i]];
	
	if {[set idx [dl_find $dl_oldval $toget]] != -1} {
	    dl_append $new [dl_get $dl_newval $idx]
	} else {
	    dl_append $new [dl_get $dl_tomap $i]
	}
    }
    dl_return $new
}


proc dl_paste { dl1 dl2 args } {
    # %HELP%
    # dl_remap:
    #   Usage: dl_paste dl1 dl2 [dl3 ...]
    #   Description: paste dl1 and dl2 together (and others) to create new list
    #                
    #   Returns: new list with concatenated values
    # 
    #   Example: 
    #            % dl_local a [dl_ilist 0 1 2 3 0 1]
    #            % dl_local b [dl_slist a b c d e f]
    #            % dl_local c [dl_slist !]
    #            % dl_local new [dl_paste $a $b $c]
    #            % dl_tcllist $new
    #            0a! 1b! 2c! 3d! 0e! 1f!
    # %END%

    proc do_paste { dl1 dl2 } {
	set n1 [dl_length $dl1]
	set n2 [dl_length $dl2]
	if { $n2 == 1 } {
	    dl_local dl2a [dl_repeat $dl2 $n1]
	    set newlist [lmap x [dl_tcllist $dl1] y [dl_tcllist $dl2a] \
			     { string cat $x$y }];
	} elseif { $n1 == 1 } {
	    dl_local dl1a [dl_repeat $dl1 $n2]
	    set newlist [lmap x [dl_tcllist $dl1a] y [dl_tcllist $dl2] \
			     { string cat $x$y }];
	} else {
	    set newlist [lmap x [dl_tcllist $dl1] y [dl_tcllist $dl2] \
			     { string cat $x$y }];
	}
	dl_return [dl_slist {*}$newlist]
    }

    dl_local pasted [do_paste $dl1 $dl2]
    foreach l $args {
	dl_local pasted [do_paste $pasted $l]
    }
    dl_return $pasted
}
