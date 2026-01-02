# This is a tcl script program to display dynamic groups, two
# bindings exist for use: double left click for viewing and 
# double right for opening other files

package provide dlsh 1.2

if { ![catch {package present Tk}] } {
    package require Tktable
}

namespace eval groupview {

    variable groups
    set count 0
    set runstate 0
    
    # This procedure opens a table-based window for group viewing.
    proc viewGroup { group {openflag 0} { parent "" } } {

	# When a group is opened for viewing, a new toplevel is 
	# created (named after a combination of the groupname and
	# the count). The group's name is also placed in the array
	# of groupnames under this key. A global variable named after
	# the count is declared for entry purposes.
	
	set groupkey dgview
	append groupkey $::groupview::count
	set ::groupview::groups($groupkey) $group
	if { $parent == "" } {
	    set t [toplevel .$groupkey]
	} else {
	    set t $parent
	}

	global ::groupview::count

	# Next, the associated widgets are created, as well as the
	# array to contain the group data.

	set table [table $t.table -width 0 -height 0 -yscrollcommand \
		[list $t.vs set ] -xscrollcommand [list $t.hs set ] \
		-roworigin 0 -colorigin 0 -colstretchmode none -titlerows 1 \
		-state disabled -cursor arrow -borderwidth 2 -pady 1 \
		-colwidth 15 -variable ::groupview::$groupkey \
		-selectmode extended \
		-selecttype cell \
		-rowseparator \n -colseparator \t  \
		-browsecommand [list ::groupview::updateEntry $t.table $t.text %S]]
	scrollbar $t.vs -orient vertical -command [list $table yview ] 
	scrollbar $t.hs -orient horizontal -command [list $table xview ]

	label $t.text -background orange -relief sunken -justify left \
		-anchor sw 
	
	grid $table $t.vs -sticky news 
	grid $t.text -sticky news -columnspan 2
	grid $t.hs -sticky ew -columnspan 2
	grid columnconfig $t 0 -weight 1
	grid rowconfig $t 0 -weight 1    

	if { [winfo toplevel $t] == $t } {
	    wm title $t "Dynamic Group Viewer: $group"   
	}

	# Now bindings are added to give the viewer some functionality,
	# as well as to allow it to clean up after itself.

	bind $table <Button-2> \
	    "switch_selection_mode $table;
             event generate $table <Button-1> -button 1 -state %s -x %x -y %y"

	bind $table <Destroy> [list ::groupview::cleanUpAfterGroup \
		$groupkey $openflag ]

	bind $table <Double-1> [list ::groupview::openListMouse $table %x %y \
		$group ]

	bind $table <Double-3> [list ::groupview::openFile ]

	bind $table <Return> [list ::groupview::openListKeys $table $group ]

	# Finally, the group's data is filled into the array.

	upvar #0 ::groupview::$groupkey array 

	dl_local nodenames [dg_listnames $group]
	set maxlength 0
	for {set i 0} {$i < [dl_length $nodenames]} {incr i} {
	    set listname [dl_get $nodenames $i]	
	    if {[dl_length $group:$listname] > $maxlength} {
		set maxlength [dl_length $group:$listname]
	    }
	}
	if {$maxlength == 0} {
	    $table configure -rows 5 -cols 3
	} else {
	    $table configure -rows [expr $maxlength + 1] \
		    -cols [dl_length $nodenames]
	}
	for {set i 0} {$i < [dl_length $nodenames]} {incr i} {
	    set listname [dl_get $nodenames $i]
	    set array(0,$i) $listname
	    for {set j 0} {$j < [dl_length $group:$listname]} {incr j} {
		if {[string match [dl_datatype $group:$listname] list]} {
		    set array([expr $j+1],$i) \
			    "[dl_datatype $group:$listname:$j]\
			    ([dl_length $group:$listname:$j])" 
		} else {
		    set array([expr $j+1],$i) [dl_get $group:$listname $j]
		}
	    }
	}

	incr ::groupview::count
	return $t
    }

    # This procedure opens a table-based window for list viewing.
    proc viewList { list } {

	# When a list is opened for viewing, a new toplevel is 
	# created (named after a combination of the listname and
	# the count). The count is also incremented
	
	set listkey dlview
	append listkey $::groupview::count
	set t [toplevel .$listkey]

	# Next, the associated widgets are created, as well as the
	# array to contain the list data.

	set table [table $t.table -width 0 -height 0 -yscrollcommand \
		[list $t.vs set ] -xscrollcommand [list $t.hs set ] \
		-roworigin 0 -colorigin 0 -colstretchmode none -titlerows 1 \
		-state disabled -cursor arrow -borderwidth 2 -pady 1 \
		-colwidth 15 -variable ::groupview::$listkey \
		-selectmode extended \
		-selecttype cell \
		-rowseparator \n -colseparator \t  \
		-browsecommand [list ::groupview::updateEntry $t.table $t.text %S]]
	scrollbar $t.vs -orient vertical -command [list $table yview ] 
	scrollbar $t.hs -orient horizontal -command [list $table xview ]
	label $t.text -background lightgray -relief sunken -justify left \
		-anchor w
	
	grid $table $t.vs -sticky news 
	grid $t.text -sticky news -columnspan 2
	grid $t.hs -sticky ew -columnspan 2
	grid columnconfig $t 0 -weight 1
	grid rowconfig $t 0 -weight 1    
	
	wm title .$listkey "Dynamic List Viewer: $list"   

	# Now bindings are added to give the viewer some functionality,
	# as well as to allow it to clean up after itself.

	bind $t.table <Destroy> [list ::groupview::cleanUpAfterList $listkey ]

	bind $table <Double-1> [list ::groupview::openListMouse $table %x %y ]

	bind $table <Return> [list ::groupview::openListKeys $table ]


	# Finally, the group's data is filled into the array. 
	# The count is also incremented.

	upvar #0 ::groupview::$listkey array 

	set maxlength [dl_length $list]
	$table configure -rows [expr $maxlength + 1] -cols 1

	set array(0,0) $list

	if {[string match [dl_datatype $list] list]} {
	    for {set j 0} {$j < $maxlength} {incr j} {
		set array([expr $j+1],0)  "[dl_datatype $list] \
			([dl_length $list:$j])" 
	    }
	} else {
	    for {set j 0} {$j < $maxlength} {incr j} {
		set array([expr $j+1],0) [dl_get $list $j]
	    }
	}

	incr ::groupview::count

    }
        
    # This procedure frees the memory associated with the appropriate
    # table's array just as it is destroyed. The group is also deleted
    # if it was opened by this program (the openflag is set).
    proc cleanUpAfterGroup { groupkey openflag } {
	upvar #0 ::groupview::$groupkey array1
	upvar #0 ::groupview::groups array2
	foreach cell [array names array1] {
	    unset array1($cell)
	}
	
	if { $openflag } {
	    set group $array2($groupkey) 
	    dg_delete $group
	}
	
	unset array1
	unset array2($groupkey) 

	if {[llength [array names array2]] == 0} {
	    if {$::groupview::runstate} {
		exit
	    }
	}
	
    }

    # This procedure frees the memory associated with the appropriate
    # table's array just as it is destroyed. This is almost identical 
    # to the above procedure, but is called to clean up after list
    # viewers.
    proc cleanUpAfterList { listkey } {
	upvar #0 ::groupview::$listkey array
	foreach cell [array names array] {
	    unset array($cell)
	}
	unset array
    }

    # This procedure determines which list the user has seleceted to 
    # view, and calls viewList upon it.
    proc openListMouse { w x y {group ""} } {	
	set xcor [$w index @$x,$y col]
	set xcor [$w get 0,$xcor]
	set ycor [expr [$w index @$x,$y row] - 1]

	if {[string match $group ""]} {
	    if {[string match [dl_datatype $xcor] list]} {
		::groupview::viewList $xcor:$ycor 
	    }	
	} else {
	    if {[dg_exists $group]} {
		if {[string match [dl_datatype $group:$xcor] list]} {
		    ::groupview::viewList $group:$xcor:$ycor 
		}	
	    }
	}
    }

    # This procedure determines which list the user has seleceted to 
    # view, and calls viewList upon it.
    proc openListKeys { w {group ""} } {	
	set cords [$w curselection]
	set xcor [$w index $cords col]
	set xcor [$w get 0,$xcor]
	set ycor [expr [$w index $cords row] - 1]
	
	if {[string match $group ""]} {
	    if {[string match [dl_datatype $xcor] list]} {
		::groupview::viewList $xcor:$ycor 
	    }	
	} else {
	    if {[dg_exists $group]} {
		if {[string match [dl_datatype $group:$xcor] list]} {
		    ::groupview::viewList $group:$xcor:$ycor 
		}	
	    }
	}
    }

    # This procedure is used to keep the viewers' entries up to date.
    proc updateEntry { w entryw index} {
	set label [$w get $index]
	$entryw configure -text $label
    }
    
    # This procedure opens a dg file through a dialog.
    proc openFile { } {
	set filename [tk_getOpenFile -filetype {{"Dynamic Groups" {".dgz"}}}]
	if {$filename != ""} {
	    ::groupview::viewGroup [dg_read $filename]
	}
    }
    proc getRow { g } {
	set table $g.table
	if { [scan [$table curselection] "%d,%d" row col] != 2 } { return -1 }
	return [expr $row-1]
    }

    proc getCol { g } {
	set table $g.table
	if { [scan [$table curselection] "%d,%d" row col] != 2 } { return -1 }
	return [expr $col]
    }
}

proc dg_view { g { parent "" } } {
    if {![dg_exists $g]} { error "dyngroup $g does not exist" }
    if { $parent != "" } { 
	set w [::groupview::viewGroup $g 0 $parent]
    } else {
	set w [::groupview::viewGroup $g]
    }
    return $w
}

proc dl_view { l } { 
    if {![dl_exists $l]} { error "dynlist $l does not exist" }
    set g [dl_to_dg $l]
    set w [::groupview::viewGroup $g]
    return $w
}

proc dg_closeViewers { {which sub} } {
    # %HELP%
    #  dg_closeViewers ?which?
    #     closes all dl_view windows, which are sub windows of dg_view.
    #     if ?which? == all, then all dg_view windows will also be closed
    # %END%
    foreach w [winfo children .] {
	if {[string range $w 0 6] == ".dlview"} {
	    destroy $w
	}
	if {[string range $w 0 6] == ".dgview" && \
		$which == "all"} {
	    destroy $w
	}
    }
}

if { 0 } { 
    wm withdraw .
    
    if {$argc == 1} {
	set ::groupview::runstate 1
	::groupview::viewGroup [dg_read [lindex $argv 0]]
    } 
}

# Added by Sergey. Switches the cell selection mode between extended (area select)
# and row (whole row is selected.)
proc switch_selection_mode { t } {
    if [string equal [$t cget -selecttype] cell ] {
	$t configure -selecttype row 
    } elseif [string equal [$t cget -selecttype] row ] {
	$t configure -selecttype col
    } elseif [string equal [$t cget -selecttype] col] {
	$t configure -selecttype cell
    }
    
}




# A hack to override tkTable.tcl's MoveCell proc.
# This was added by Sergey to fix the problem where tkTable jumps the active cell
# to the top-left corner of the table when using the keyboard to move between cells.
if { ![catch {package present Tk}] } {
proc ::tk::table::MoveCell {w x y} {
    if {[catch {$w index active row} r]} return
    set c [$w index active col]
    set cell [$w index [incr r $x],[incr c $y]]
    while {[string compare [set true [$w hidden $cell]] {}]} {
	# The cell is in some way hidden
	if {[string compare $true [$w index active]]} {
	    # The span cell wasn't the previous cell, so go to that
	    set cell $true
	    break
	}
	if {$x > 0} {incr r} elseif {$x < 0} {incr r -1}
	if {$y > 0} {incr c} elseif {$y < 0} {incr c -1}
	if {[string compare $cell [$w index $r,$c]]} {
	    set cell [$w index $r,$c]
	} else {
	    # We couldn't find a non-hidden cell, just don't move
	    return
	}
    }
    $w activate $cell
    #    $w see active      This line caused the old problem

    # These lines handle reaching the edge of visible screen much better.
    if {$c >= [$w index bottomright col]} {$w xview scroll 1 units}
    if {$c <= [$w index topleft col]} {$w xview scroll -1 units}
    if {$r >= [$w index bottomright row]} {$w yview scroll 1 units}
    if {$r <= [$w index topleft row]} {$w yview scroll -1 units}


    switch [$w cget -selectmode] {
	browse {
	    $w selection clear all
	    $w selection set active
	}
	extended {
	    variable Priv
	    $w selection clear all
	    $w selection set active
	    $w selection anchor active
	    set Priv(tablePrev) [$w index active]
	}
    }
}
}
