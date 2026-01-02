# This file contains all the code necessary to build an event viewer given
# an input parent widget and servername 

package require qpcs
package require Tktable
package require BWidget

namespace eval oev {
    
    set server ""; set table ""; set pause ""; set spinbox ""
    set viewmode 0; set currentview 0; set lasttime 0; set lastevent -1
    set trace 0

    dg_create oevgroup
    dl_set oevgroup:types [dl_llist]; dl_set oevgroup:subtypes [dl_llist]
    dl_set oevgroup:times [dl_llist]; dl_set oevgroup:params [dl_llist]
    
    proc buildEventViewer { parent server } {
	set oev::server $server

	set oev::table [table $parent.table -rows 50 -cols 4 -width 4 \
		-height 10 -colwidth 15 -roworigin -1 -colorigin -1 \
		-titlerows 1 -colstretchmode last -selecttype row \
		-sparsearray 0 -state disabled -cursor arrow \
		-bordercursor arrow]

	pack [set bigframe [frame $parent.bigframe]] -pady 3
	pack [set smallframe [frame $bigframe.smallframe]] -side left -padx 10
	pack [set oev::pause [button $smallframe.pause -text "Pause" \
		-command {oev::pauseProc} -width 8]] -side bottom -pady 5


	pack [set oev::spinbox [SpinBox $smallframe.sb -label \
		"Group to View" -textvariable oev::currentview \
		-command [list $oev::table configure -variable \
		oev::$oev::currentview] -width 2]] -pady 5 -side bottom

	pack [set radframe [LabelFrame $bigframe.radframe -text "Trace Level"\
		-side top -anchor w -relief sunken -borderwidth 1]] -side left
	pack [radiobutton [$radframe getframe].n -text "None" \
		-variable oev::trace -value 0 -command {oev::setTrace}] \
		-anchor w  
	pack [radiobutton [$radframe getframe].ao -text "Action Only" \
		-variable oev::trace -value 1 -command {oev::setTrace}] \
		-anchor w  
	pack [radiobutton [$radframe getframe].aat -text "Action and \
		Transition" -variable oev::trace -value 2 \
		-command {oev::setTrace}] -anchor w  
	set oev::trace 0	
	pack $oev::table
	bind $oev::table <Destroy> { oev::cleanUpProc }
	qpcs::listenTo $server -1
	qpcs::addCallback oev::eventViewerCallback ev
    }
    
    proc setTrace { } {
	set running [lindex [qpcs::user_queryState $oev::server] 1]
	qpcs::user_stop $oev::server
	qpcs::user_setTrace $oev::server $oev::trace
	if {$running} {	qpcs::user_start $oev::server}
    }

    proc eventViewerCallback { tag dg } {
	if {[expr [dl_tcllist $dg:type] == 3] && \
		[expr [dl_tcllist $dg:subtype] == 2]} {
	    dg_delete oevgroup
	    dg_create oevgroup
	    dl_set oevgroup:types [dl_llist]
	    dl_set oevgroup:subtypes [dl_llist]
	    dl_set oevgroup:times [dl_llist]
	    dl_set oevgroup:params [dl_llist]
	    for {set i 0} {$i <= $oev::lastevent} {incr i 1} {
		oev::cleanTableArray $i
	    }
	    set oev::lastevent -1
	}
	if {[dl_tcllist $dg:type] == 19} {
	    incr oev::lastevent 1
	    $oev::spinbox configure -range [list 0 $oev::lastevent 1]
	    dl_insert oevgroup:types $oev::lastevent [dl_ilist]
	    dl_insert oevgroup:subtypes $oev::lastevent [dl_ilist]
	    dl_insert oevgroup:times $oev::lastevent [dl_ilist]
	    dl_insert oevgroup:params $oev::lastevent [dl_llist]
	    oev::setTableValue $oev::lastevent -1 -1 "Type"
	    oev::setTableValue $oev::lastevent -1 0 "Subtype"
	    oev::setTableValue $oev::lastevent -1 1 "Time"
	    oev::setTableValue $oev::lastevent -1 2 "Parameters"	
	    set oev::lasttime 0
	    if {$oev::viewmode == 0} {
		$oev::table configure -variable oev::$oev::lastevent
		set oev::currentview $oev::lastevent
	    }
	}
	if {$oev::lastevent == -1} { return }
	set num $oev::lastevent
	dl_append oevgroup:types:$num [dl_tcllist $dg:type]
	dl_append oevgroup:subtypes:$num [dl_tcllist $dg:subtype]
	dl_append oevgroup:times:$num [dl_tcllist $dg:time]
	dl_append oevgroup:params:$num $dg:params
	set enum [expr [dl_length oevgroup:types:$num] - 1]
	oev::setTableValue $num $enum -1 \
		$qpcs::evttypes([dl_tcllist $dg:type],name)
	oev::setTableValue $num $enum 0 [dl_tcllist $dg:subtype]
	oev::setTableValue $num $enum 1 [expr [dl_tcllist $dg:time]\
		- $oev::lasttime]
	oev::setTableValue $num $enum 2 [dl_tcllist $dg:params]
	set oev::lasttime [dl_tcllist $dg:time]
    }
    
    proc setTableValue { array x y info } {
	upvar #0 oev::$array a
	set a($x,$y) $info
    }
    
    proc cleanTableArray { array } {
	upvar #0 oev::$array a
	foreach val [array names a] {
	    unset a($val)
	}
	unset a
    }

    proc cleanUpProc { } {
	qpcs::removeCallback oev::eventViewerCallback ev
	dg_delete oevgroup
	dg_create oevgroup
	dl_set oevgroup:types [dl_llist]
	dl_set oevgroup:subtypes [dl_llist]
	dl_set oevgroup:times [dl_llist]
	dl_set oevgroup:params [dl_llist]
	for {set i 0} {$i <= $oev::lastevent} {incr i 1} {
	    oev::cleanTableArray $i
	}
	set oev::lastevent -1
    }
    
    # Called when the event viewer's pause button is pressed 
    proc pauseProc { } {
	if {$oev::viewmode == 0} {
	    set oev::viewmode 1
	    $oev::pause configure -text "Unpause"
	} else {
	    set oev::viewmode 0
	    $oev::pause configure -text "Pause"
	}
    }
}

