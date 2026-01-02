#
# NAME
#    dlsort.tcl
#
# DESCRIPTION
#    accessory functions for dealing with categorical sorts
#

package provide dlsh 1.2

proc dl_sortedFunc { data categs { func dl_means } } {
    dl_return [dl_append [eval dl_uniqueCross $categs] \
	    [$func [eval dl_sortByLists $data $categs]]]
}

proc dl_selectSortedFunc { data variables selects { func dl_means } } {
    dl_return [dl_append [eval dl_uniqueCross $selects] \
	    [$func [dl_sortBySelected [subst $data] [subst $variables] \
	    [subst $selects]]]]
}

proc dl_multisort { args } {
    set i -1
    if { $args == "" } { error "usage: dl_multisort list0 list1 ..." }

    # First recode all lists in terms of consecutive integers
    dl_local recoded [dl_llist]
    set l [dl_length [lindex $args 0]]
    foreach list $args {
	if { [dl_length $list] != $l } { 
	    error "dl_multisort: lists must all be of equal length"
	}
	dl_append $recoded [dl_recode $list]
    }

    # Now calculate the number of unique elements in each list
    dl_local ns [dl_add [dl_shift [dl_maxs $recoded] -1] 1]

    # Each sort list is multiplied by offsets, which will accentuate
    #  the first list, and leave the last list at face value
    dl_local offsets [dl_reverse [dl_cumprod [dl_reverse $ns]]]
    dl_set $recoded [dl_mult $recoded $offsets]

    # Add up the recoded lists, which gives a sortable list
    dl_local sums [dl_sums [dl_transpose $recoded]]
    dl_return [dl_sortIndices $sums]
}

proc dg_permute { g order } {
    foreach l [dg_tclListnames $g] {
	if { [dl_length $g:$l] == [dl_length $order] } {
	    dl_set $g:$l [dl_permute $g:$l $order]
	}
    }
    return $g
}

proc dg_sort { g args } {
    foreach arg $args {
	lappend sargs "$g:$arg"
    }
    dl_local order [eval dl_multisort $sargs]
    return [dg_permute $g $order]
}

proc dg_choose { g s { ref "" } } {
    dl_local sel $s
    set g1 [dg_create]
    set rlen 0
    if { $ref != "" } { set rlen [dl_length $g:$ref] }
    foreach l [dg_tclListnames $g] {
	if { !$rlen || [expr $rlen==[dl_length $g:$l]] } {
	    dl_set $g1:$l [dl_choose $g:$l $sel]
	} else {
	    dl_set $g1:$l $g:$l
	}
    }
    return $g1
}

proc dg_select { g s } {
    set n [dl_length $s]
    dl_local sel $s
    foreach l [dg_tclListnames $g] {
	if { [dl_length $g:$l] == $n } { 
	    dl_set $g:$l [dl_select $g:$l $sel]
	}
    }
    return $g
}

proc dg_copySelected { g s {save_noteq 0}} {
    set n [dl_length $s]
    set g1 [dg_create]
    foreach l [dg_tclListnames $g] {
	if { [dl_length $g:$l] == $n } { 
	    dl_set $g1:$l [dl_select $g:$l $s]
	} elseif $save_noteq {
	    dl_set $g1:$l $g:$l
	}
    }
    return $g1
}

# proc dg_copySelected { g s } {
#     set n [dl_length $s]
#     set g1 [dg_create]
#     foreach l [dg_tclListnames $g] {
# 	if { [dl_length $g:$l] == $n } { 
# 	    dl_set $g1:$l [dl_select $g:$l $s]
# 	}
#     }
#     return $g1
# }


#
# NAME
#  dl_shuffleLists
#
# DESCRIPTION
#  Takes a list of lists and shuffles each list individually, but maintains
#     the order of the highest list level.
#
proc dl_shuffleLists { l } {
    if {[dl_datatype $l] != "list"} {
	error "dl_shuffleLists: list must be a list of lists"
    }
    dl_return [dl_choose $l [dl_randfill [dl_lengths $l]]]
}








