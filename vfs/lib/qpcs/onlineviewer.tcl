# This file contains all the code necessary to build any number of widgets
# useful for viewing the current ess state system and stimulator activites.

package require qpcs
package require BWidget

set SUBJECTS [list tiger jack simon eddie]

set JUICE [list 10 20 30 40 50 60 70 80 90 100 150 200]

set BLINK_ALLOWANCE [list 0 32 64 96 128 160 192 224 256 288 320 800]

set EYE_CHANNELS [list 0/1 2/3 4/5 6/7]

set EM_LOG_RATE [list 0 1 2 3 4 5 10 15 20]

namespace eval onlineviewer {

################################## Functions ##################################

    # This procedure sends a command to give juice 
    proc juiceCommand { } {
	qpcs::juice $onlineviewer::server1 $onlineviewer::juiceAmount
    }

    # This procedure is called to load a param file
    proc loadParamFile { } {
	set pfiletoplevel [toplevel .paramfile]
	wm resizable $pfiletoplevel 0 0
	wm title $pfiletoplevel "Load Parameter File" 
	onlineviewer::paramFileDialog $pfiletoplevel 1 
	focus $pfiletoplevel
	grab $pfiletoplevel
	tkwait window $pfiletoplevel
	onlineviewer::updateStatus
    }

    # This procedure is called to save a param file
    proc saveParamFile { } {
	set pfiletoplevel [toplevel .paramfile]
	wm resizable $pfiletoplevel 0 0
	wm title $pfiletoplevel "Save Parameter File" 
	onlineviewer::paramFileDialog $pfiletoplevel 0
	focus $pfiletoplevel
	grab $pfiletoplevel
	tkwait window $pfiletoplevel
	onlineviewer::updateStatus
   }

    # This procedure opens the dialog for a parameter file loading and saving
    proc paramFileDialog { parent loading } {
	bind $parent <Escape> [list destroy $parent ]
	pack [entry $parent.filename -textvariable \
		onlineviewer::currentParamFile] -side left -padx 3
	pack [button $parent.saveload -text "Save" -command [list \
		qpcs::user_fileio $onlineviewer::server1 $::ESS_PARAM_FILE \
		$::ESS_FILE_CLOSE 0 $onlineviewer::currentParamFile]] \
		-side left -padx 3
	if {$loading} {
	    $parent.saveload configure -text "Load" -command [list \
		    qpcs::user_fileio $onlineviewer::server1 $::ESS_PARAM_FILE\
		    $::ESS_FILE_OPEN 0 $onlineviewer::currentParamFile]
	    bind $parent <Return> [list onlineviewer::paramFileDialogBinding \
		    1 $parent]
	} else {
	    bind $parent <Return> [list onlineviewer::paramFileDialogBinding \
		    0 $parent]
	}
    }

    # This is called by the binding of the Param File dialog box
    proc paramFileDialogBinding { loading parent } {
	if {$loading} {
	    qpcs::user_fileio $onlineviewer::server1 $::ESS_PARAM_FILE \
		    $::ESS_FILE_OPEN 0 $onlineviewer::currentParamFile
	    destroy $parent
	} else {
	    qpcs::user_fileio $onlineviewer::server1 $::ESS_PARAM_FILE \
		    $::ESS_FILE_CLOSE 0 $onlineviewer::currentParamFile
	    destroy $parent
	}
    }

    # This procedure is called to open a data file
    proc openDataFile { } {
	onlineviewer::openDataFileDialog
	onlineviewer::updateStatus
    }
    
    # This procedure opens the dialog for a data file openeing
    proc openDataFileDialog { } {
	set dfiletoplevel [toplevel .opendatafile]
	wm resizable $dfiletoplevel 0 0
	wm title $dfiletoplevel "Open Data File" 

	bind $dfiletoplevel <Escape> [list destroy $dfiletoplevel ]
	bind $dfiletoplevel <Return> {onlineviewer::openWithCheck 1}

	pack [entry $dfiletoplevel.filename -textvariable \
		onlineviewer::workingDataFile] -side left -padx 3 -pady 3
	pack [button $dfiletoplevel.open -text "Open" -command \
		[list onlineviewer::openWithCheck 1 ]] \
		-side left -padx 3 -pady 3

	focus $dfiletoplevel
	grab $dfiletoplevel
	tkwait window $dfiletoplevel
    }

    # This is a helper procedure to check if the datafile exists when
    # openeing it
    proc openWithCheck { overwrite } {
	set response [qpcs::user_fileio $onlineviewer::server1 \
		$::ESS_DATA_FILE $::ESS_FILE_OPEN $overwrite \
		$onlineviewer::workingDataFile]
	if {![lindex $response 1] && $overwrite } {
	    onlineviewer::overwriteDataFileDialog
	} 
	destroy .opendatafile
	if {$overwrite == 0} {
	    destroy .overwritedatafile
	}
    }


    
    # This procedure opens the dialog for a data file overwrite
    proc overwriteDataFileDialog { } {
	set dfiletoplevel [toplevel .overwritedatafile]
	wm resizable $dfiletoplevel 0 0
	wm title $dfiletoplevel "Load Data File" 

	bind $dfiletoplevel <Escape> [list destroy $dfiletoplevel ]
	bind $dfiletoplevel <Return> {onlineviewer::openWithCheck 0}

	pack [label $dfiletoplevel.label -text "File Exists: Overwrite?"] \
		-side top -pady 3 -padx 3
	pack [set buttonframe [frame $dfiletoplevel.buttonframe]] \
		-side top -pady 3
	pack [button $buttonframe.ok -text "Ok" -command \
		[list onlineviewer::openWithCheck 0] -width 8 -height 1 ] \
		-side left 	
	pack [button $buttonframe.cancel -text "Cancel" -command \
		[list destroy $dfiletoplevel] -width 8 -height 1 ] -side left
	focus $dfiletoplevel
	grab $dfiletoplevel
	tkwait window $dfiletoplevel
    }

    # This procedure is called to close a data file
    proc closeDataFile { } {
	qpcs::user_fileio $onlineviewer::server1 $::ESS_DATA_FILE \
		$::ESS_FILE_CLOSE 0 $onlineviewer::currentDataFile
	onlineviewer::updateStatus
    }

   
    # This procedure is called when the params menu option is selected
    proc editParams { } {
	set paramtoplevel [toplevel .param]
	wm resizable $paramtoplevel 0 0
	wm title $paramtoplevel "Parameter Viewer" 
	onlineviewer::buildParamEditer $paramtoplevel
	focus $paramtoplevel
	grab $paramtoplevel
	tkwait window $paramtoplevel
    }

    # This procedure gets the names of the current statesystems available
    proc getSystems { } {
	set list [list]
	set index 0
	while {![string match [set name [lindex [qpcs::user_queryNameIndex \
		$onlineviewer::server1 $index] 1]] ""]} {
	    lappend list $name
	    incr index
	}
	return $list
    }

    # This procedure pops up the help dialog
    proc helpCommand { } {
	set helptoplevel [toplevel .help]
	bind $helptoplevel <Escape> [list destroy $helptoplevel ]
	wm title $helptoplevel "About..." 
	pack [label $helptoplevel.label1 -text "Online Viewer v1.0" -font \
		{"Times New Roman" 16} -relief raised] -side top \
		-pady 20 -padx 10
	pack [label $helptoplevel.label2 -text "DLS,CML 2001" -font \
		{"Times New Roman" 10} -relief raised] -side top -pady 5 \
		-padx 5
	focus $helptoplevel
	grab $helptoplevel
	tkwait window $helptoplevel
    }

    # This function returns the current subject if possible 
    proc getSubject { } {
	set response [qpcs::user_queryParam $onlineviewer::server1 -1 3]
	if {[llength $response] == 6} {
	    return [lindex $response 2]
	}
	return ""
    }

    # This function is called both in polling for external changes and 
    # whenever an internal change is made to ensure that the program
    # and gui reflect the actual system status.
    proc updateStatus { } {
	set server $onlineviewer::server1
	set running 0
	set exists [qpcs::systemExists $server]
	if {$exists} {
	    set running [lindex [qpcs::user_queryState $server] 1]
	}
	set matching 1
	if {$onlineviewer::systemExists != $exists} {
	    set onlineviewer::systemExists $exists
	    set matching 0
	}
	if {$onlineviewer::systemRunning != $running} {
	    set onlineviewer::systemRunning $running
	    set matching 0
	} else { 
	    if {$onlineviewer::systemRunning} {
		after 5000 onlineviewer::updateStatus
		return
	    }
	}
	set name ""
	set data ""
	if {$exists} {
	    set name [lindex [qpcs::user_queryName $server] 1]
	    set data [qpcs::user_queryDatafile $server]
	    if {[llength $data] == 2} { 
		set data [lindex $data 1] 
	    } else {
		set data ""
	    }
	}
	if {$onlineviewer::systemName != $name} {
	    set onlineviewer::systemName $name
	    set matching 0
	}
	if {$onlineviewer::currentDataFile != $data} {
	    set onlineviewer::currentDataFile $data
	    $onlineviewer::widgets(Data) configure -text $data
	    set matching 0
	}
	if {!$matching} {
	    onlineviewer::updateStatusLabel
	    onlineviewer::updateDataLabel
	    onlineviewer::updateWidgetStates
	}
	after 5000 onlineviewer::updateStatus
    }
    
    # This function updates the datafile name label
    proc updateDataLabel { } {
	$onlineviewer::widgets(Data) configure -text \
		$onlineviewer::currentDataFile
    }

    # This procedure is called to update the contents of the status label
    proc updateStatusLabel { } {
	set text "Stopped"
	set color red
	if {$onlineviewer::systemRunning} {
	    set text "Running"
	    set color green
	}
	if {!$onlineviewer::systemExists} {
	    set text "Inactive"
	    set color black
	}
	$onlineviewer::widgets(SSStatus) configure -text $text -foreground \
		$color
    }

    # This procedure handles updating the states (disabled or normal) 
    # of the various appropriate widgets
    proc updateWidgetStates { } {
	if {!$onlineviewer::systemExists || $onlineviewer::systemRunning} {
	    $onlineviewer::widgets(SSCombo) configure -state disabled
	    $onlineviewer::widgets(Subject) configure -state disabled
	    $onlineviewer::widgets(File) entryconfigure "Data File" \
		    -state disabled
	    $onlineviewer::widgets(File) entryconfigure "Param File" \
		    -state disabled
	    $onlineviewer::widgets(Edit) entryconfigure "Eye Movements" \
		    -state disabled
	    $onlineviewer::widgets(Edit) entryconfigure "Parameters" \
		    -state disabled
	    $onlineviewer::widgets(Edit) entryconfigure "Levers" \
		    -state disabled
	} else {
	    $onlineviewer::widgets(SSCombo) configure -state normal
	    $onlineviewer::widgets(Subject) configure -state normal
	    $onlineviewer::widgets(File) entryconfigure "Data File" \
		    -state normal
	    $onlineviewer::widgets(File) entryconfigure "Param File" \
		    -state normal
	    $onlineviewer::widgets(Edit) entryconfigure "Eye Movements" \
		    -state normal
	    $onlineviewer::widgets(Edit) entryconfigure "Parameters" \
		    -state normal
	    $onlineviewer::widgets(Edit) entryconfigure "Levers" \
		    -state normal
	}
	if {![string match $onlineviewer::currentDataFile ""]} {
	    $onlineviewer::widgets(SSCombo) configure -state disabled
	    $onlineviewer::widgets(Subject) configure -state disabled
	    $onlineviewer::widgets(DataCascade) entryconfigure "Open" \
		    -state disabled
	    $onlineviewer::widgets(DataCascade) entryconfigure "Close" \
		    -state normal
	} else {
	    $onlineviewer::widgets(DataCascade) entryconfigure "Close" \
		    -state disabled
	    $onlineviewer::widgets(DataCascade) entryconfigure "Open" \
		    -state normal
	}
    }    

    # This function is called whenever the subject is switched from the 
    # subjects combobox
    proc switchSubject { } {
	qpcs::user_setParam $onlineviewer::server1 "ESS Subject" \
		$onlineviewer::currentSubject
	set onlineviewer::currentSubject [onlineviewer::getSubject]	
    }
    
    # The command called when the go button (or g) is pressed
    proc goCommand { } {
	qpcs::user_start $onlineviewer::server1
	onlineviewer::updateStatus
    }

    # The command called when the quit button (or q) is pressed
    proc quitCommand { } {
	qpcs::user_stop $onlineviewer::server1
    	onlineviewer::updateStatus
    }

    # The command called when the reset button (or r) is pressed
    proc resetCommand { } {
	qpcs::user_reset $onlineviewer::server1
	onlineviewer::updateStatus
    }

    # This procedure is called when the user switches state systems via the
    # combo box widget
    proc switchSystem { } {
	qpcs::user_setSystem $onlineviewer::server1 \
		[$onlineviewer::widgets(SSCombo) getvalue]
	onlineviewer::updateStatus
    }

################################# Initialization ##############################
    
    qpcs::buildServer

    # Default servers to connect to: 1 = QNX, 2 = STIM
    set server1 ""
    set systemExists 0
    set connectedToQNX 0
    if {$argc == 0} {
	if {[llength [array names env QNXSERVER]] == 1} {
	    set server1 $env(QNXSERVER)
	    if {[set systemExists [qpcs::systemExists $server1]] == 1} {
		if {[qpcs::registerWithQNX $server1] != -1} {
		    set connectedToQNX 1
		}
	    }
	}
    } else {
	set server1 [lindex $argv 0]
	if {[set systemExists [qpcs::systemExists $server1]] == 1} {
	    if {[qpcs::registerWithQNX $server1] != -1} {
		set connectedToQNX 1
	    }
	}
    }
    
    # By now the server (for receiving) should be build, the server variables
    # (for sending) should be set (possibly empty), and if possible, the
    # program should be connected (status stored in the appropriate variables).
    # Now we get the properties of the current state-system (if applicable).
    
    set systemName "" 
    if {$systemExists} {
	set systemName [lindex [qpcs::user_queryName $server1] 1]
    }
    
    set systemRunning 0
    if {$systemExists} {
	set systemRunning [lindex [qpcs::user_queryState $server1] 1]
    }
    
    array set params [list]

    set currentSubject ""
    set subjects $SUBJECTS
    if {$systemExists} {
	set currentSubject [onlineviewer::getSubject]
    }    

    set currentDataFile [lindex [qpcs::user_queryDatafile $server1] 1]
    set workingDataFile ""
    set currentParamFile ""

    set juiceAmount 40
    
    set systems [onlineviewer::getSystems]

    # The following initializes varaibles that are widget specific
    
    set widgets(toplevel) .toplevel
	    
################################## Helper Functions ###########################
    
    # This procedure prints the current status of this program
    proc printStatus { } {
	puts "Server1: $onlineviewer::server1"
	puts "Server2: $onlineviewer::server2"
	puts "QNX Connection: $onlineviewer::connectedToQNX"
	puts "Stim Connection: $onlineviewer::connectedToStim"
	puts "System Exists: $onlineviewer::systemExists"
	puts "System Name: $onlineviewer::systemName"
	puts "System Running: $onlineviewer::systemRunning"
    }

################################## Widget Building ############################

    proc buildBasicWidget { } {
	# First a toplevel is created and some setup is done 
	set toplevel [toplevel .toplevel]
	bind $toplevel <Destroy> { if {"%W" == ".toplevel"} {destroy . }}
	bind $toplevel <g> { onlineviewer::goCommand }
	bind $toplevel <G> { onlineviewer::goCommand }
	bind $toplevel <q> { onlineviewer::quitCommand }
	bind $toplevel <Q> { onlineviewer::quitCommand }
	bind $toplevel <r> { onlineviewer::resetCommand }
	bind $toplevel <R> { onlineviewer::resetCommand }
	bind $toplevel <j> { onlineviewer::juiceCommand }
	bind $toplevel <J> { onlineviewer::juiceCommand }
	bind $toplevel <x> { onlineviewer::buildExecuteMenu }
	bind $toplevel <X> { onlineviewer::buildExecuteMenu }
	bind $toplevel <p> { if {$onlineviewer::systemExists && \
		!$onlineviewer::systemRunning} {onlineviewer::editParams} }
	bind $toplevel <P> { if {$onlineviewer::systemExists && \
		!$onlineviewer::systemRunning} {onlineviewer::editParams} }
	bind $toplevel <v> { if {$onlineviewer::systemExists && \
		!$onlineviewer::systemRunning} {onlineviewer::editParams} }
	bind $toplevel <V> { if {$onlineviewer::systemExists && \
		!$onlineviewer::systemRunning} {onlineviewer::editParams} }
	bind $toplevel <t> { if {$onlineviewer::systemExists && \
		!$onlineviewer::systemRunning} {onlineviewer::editParams} }
	bind $toplevel <T> { if {$onlineviewer::systemExists && \
		!$onlineviewer::systemRunning} {onlineviewer::editParams} }
	bind $toplevel <e> { if {$onlineviewer::systemExists && \
		!$onlineviewer::systemRunning} {onlineviewer::buildIOMenu} }
	bind $toplevel <E> { if {$onlineviewer::systemExists && \
		!$onlineviewer::systemRunning} {onlineviewer::buildIOMenu} }
	
	if {[string match $::tcl_platform(platform) windows]} {
	    bind $toplevel <Control-h> { console show }
	    bind $toplevel <Control-H> { console show }
	}

	wm title $toplevel "Online Viewer"
	wm resizable $toplevel 0 0
	wm withdraw .

	# A menubar with some options (server info...etc)
	set onlineviewer::widgets(Menu) [set menu [menu $toplevel.menu \
		-tearoff 0]]
	$toplevel configure -menu $menu
	foreach m {File Edit View Help} {
	    set $m [menu $menu.menu$m -tearoff 0]
	    $menu add cascade -label $m -menu $menu.menu$m
	    set onlineviewer::widgets($m) .toplevel.menu.menu$m
	}


	$File add cascade -label "Data File" -menu $File.datacascade
	set onlineviewer::widgets(DataCascade) [set DataCascade \
		[menu $File.datacascade -tearoff 0]]
	$DataCascade add command -label "Open" -command \
		onlineviewer::openDataFile	
	$DataCascade add command -label "Close" -command \
		onlineviewer::closeDataFile

	$File add cascade -label "Param File" -menu $File.paramcascade
	set ParamCascade [menu $File.paramcascade -tearoff 0]
	$ParamCascade add command -label "Load" -command \
		onlineviewer::loadParamFile	
	$ParamCascade add command -label "Save" -command \
		onlineviewer::saveParamFile	

	$File add command -label Execute -command \
		onlineviewer::buildExecuteMenu
	$File add command -label Quit -command exit	

	$Edit add command -label Parameters -command onlineviewer::editParams
	$Edit add command -label "Eye Movements" -command \
		onlineviewer::buildIOMenu
	
	$Edit add cascade -label Levers -menu $Edit.leverscascade
	set LeversCascade [menu $Edit.leverscascade -tearoff 0]
	$LeversCascade add radio -label "  0/1" -value "0/1" \
		-variable onlineviewer::leverchannels \
		-command onlineviewer::setLeversCommand
	$LeversCascade add radio -label "  2/3" -value "2/3" \
		-variable onlineviewer::leverchannels \
		-command onlineviewer::setLeversCommand
	$LeversCascade add radio -label "  4/5" -value "4/5" \
		-variable onlineviewer::leverchannels \
		-command onlineviewer::setLeversCommand
	$LeversCascade add radio -label "  6/7" -value "6/7" \
		-variable onlineviewer::leverchannels  \
		-command onlineviewer::setLeversCommand
	
	$View add command -label "Event Viewer" -command \
		[list eventviewer::buildEventViewer $onlineviewer::server1]
	$View add command -label "Trace Viewer" -command \
		[list traceviewer::buildTraceViewer $onlineviewer::server1]

	$Help add command -label About -command onlineviewer::helpCommand
	

	pack [set loadframe [frame $toplevel.load]] -side top -pady 3

	set ss_fr [LabelFrame $loadframe.ss_frame -text "State System" \
		       -width 12 -anchor w -padx 3]
	pack [set onlineviewer::widgets(SSCombo) [ComboBox $ss_fr.sscombo \
		-textvariable onlineviewer::systemName \
		-values $onlineviewer::systems -editable false \
		-modifycmd onlineviewer::switchSystem]] \
		-side left -padx 5 -pady 3
	pack $ss_fr
	
	pack [set controlframe [frame $toplevel.control]] -side top -pady 5
	pack [button $controlframe.go -text "Go" -font {default 12}\
		-command { onlineviewer::goCommand } \
		-width 5 -height 1 ] -side left 
	pack [button $controlframe.quit -text "Quit" -font {default 12} \
		-command { onlineviewer::quitCommand } \
		-width 5 -height 1 ] -side left 
	pack [button $controlframe.reset -text "Reset" -font {default 12} \
		-command { onlineviewer::resetCommand } \
		-width 5 -height 1 ] -side left 
	
	pack [set statusframe [frame $toplevel.status]] -side top \
		-pady 3 -padx 20 -anchor w
	pack [label $statusframe.text -text "ESS Status: "] -side left 
	pack [set onlineviewer::widgets(SSStatus) [label $statusframe.status \
		-width 10 -font { default 9 bold } ]] -padx 3 
	
	pack [set subjectframe [frame $toplevel.subject]] -side top \
		-pady 3 -padx 20 -anchor w
	set subject_fr [LabelFrame $subjectframe.ss_frame -text "Subject" \
			    -width 8 -anchor w -padx 3]
	pack [set onlineviewer::widgets(Subject) [ComboBox \
		$subject_fr.subject \
		-textvariable onlineviewer::currentSubject \
		-values $onlineviewer::subjects -editable false \
	        -modifycmd onlineviewer::switchSubject]] -side left -pady 3
	pack $subject_fr

	pack [set dataframe [frame $toplevel.data]] -side top \
		-pady 3 -padx 20 -anchor w
    	pack [label $dataframe.text -text "Data File: "] -side left 
	pack [set onlineviewer::widgets(Data) [label $dataframe.data \
		-width 18 -font { Helvetica 10 bold } ]] -padx 3

	pack [set juiceframe [frame $toplevel.juice]] -side top -pady 2
	pack [button $juiceframe.juicebutton -text "Juice" -command \
		onlineviewer::juiceCommand] -side left -padx 5
	pack [ComboBox $juiceframe.amount -values $::JUICE -textvariable \
		onlineviewer::juiceAmount -width 5] -side left -padx 5 -pady 3

	onlineviewer::updateStatusLabel
	onlineviewer::updateDataLabel
	onlineviewer::updateWidgetStates
	onlineviewer::getSubject
	onlineviewer::updateStatus
    }

################################## Parameter Editer ###########################

    # This procedure creates the widgets used to view/edit the parameters.
    # It takes a parent (which will most likely be a toplevel to be focused
    # on).
    proc buildParamEditer { parent } {
	onlineviewer::clearParams
	onlineviewer::getSystemParams

	bind $parent <Escape> [list destroy $parent ]
	bind $parent <Return> [list onlineviewer::updateParams $parent ]

	set ptype 1
	foreach name {time variable subject storage} {
	    set index 0
	    pack [set sframe [ScrollableFrame $parent.$name]] -side top -pady 3
	    pack [label $sframe.title -text "[string totitle $name] \
		    Parameters"] -pady 3
	    foreach param [array names onlineviewer::params $ptype,*] {
		set pname [lindex [split $param ,] 1]
		set pvalu [lindex $onlineviewer::params($param) 1]
		set dtype [lindex $onlineviewer::params($param) 0]
		
		set ::PARAM.$pname $pvalu
		
		pack [set frame [frame $sframe.frame$index]] -side top
		pack [label $frame.label$index -text $pname -width 20 \
			-anchor w] -side left -padx 3 -anchor w
		pack [entry $frame.entry$index -width 20 -textvariable \
			::PARAM.$pname ] -side left -padx 3 -anchor w
		incr index
	    }
	    incr ptype
	}

	pack [set buttonframe [frame $parent.buttonframe]] -side top -pady 5
	pack [button $buttonframe.ok -text "Ok" -command \
		[list onlineviewer::updateParams $parent] \
		-width 8 -height 1 ] -side left 	
	pack [button $buttonframe.update -text "Update" -command \
		{ onlineviewer::updateParams } -width 8 -height 1 ] -side left 
	pack [button $buttonframe.cancel -text "Cancel" -command \
		[list onlineviewer::destroyParamViewer $parent] \
		-width 8 -height 1 ] -side left 	
    }

    # This procedure determines which parameters need to be updated on
    # the QNX side and sends the messages to update them.
    proc updateParams { {destroyParent 0} } {
	foreach param [array names onlineviewer::params] {
	    set pname [lindex [split $param ,] 1]
	    set pvalu [lindex $onlineviewer::params($param) 1]
	    set globalvar ::PARAM.$pname
	    if {[set $globalvar] != $pvalu} {
		qpcs::user_setParam $onlineviewer::server1 $pname \
			[set $globalvar]
	    }
	}
	onlineviewer::getSystemParams
	foreach param [array names onlineviewer::params] {
	    set pname [lindex [split $param ,] 1]
	    set pvalu [lindex $onlineviewer::params($param) 1]
	    set ::PARAM.$pname $pvalu   
	}
	set onlineviewer::currentSubject [onlineviewer::getSubject] 
	if {$destroyParent != 0} { destroy $destroyParent }
    }

    # This procedure retrieves the parameter information from the state
    # system and places the data in an array for later reference.
    proc getSystemParams { } {
	for {set ptype 1} {$ptype < 5} {incr ptype} {
	    set index -1
	    set response [qpcs::user_queryParam $onlineviewer::server1 \
		    $index $ptype]
	    while {[llength $response] == 6} {
		set pname [lindex $response 1]
		set pvalu [lindex $response 2]
		set index [lindex $response 3]
		set dtype [lindex $response 4]
		set onlineviewer::params($ptype,$pname) [list $dtype $pvalu]
			    set response [qpcs::user_queryParam \
				    $onlineviewer::server1 $index $ptype]
	    }
	}
    }

    # This procedure cleans up after the paramter viewer
    proc destroyParamViewer { parent } { 
	destroy $parent
	foreach param [array names onlineviewer::params] { 
	    set pname [lindex [split $param ,] 1]
	    set globalvar ::PARAM.$pname
	    unset [set globalvar]  
	    unset onlineviewer::params($param)
	}
    }

    # This procedure clears the param array 
    proc clearParams { } {
	foreach param [array names onlineviewer::params] { 
	    unset onlineviewer::params($param)
	}
    }

################################## Execute Menu ############################## 

    set executable ""
    set remoteSystem ""
    set execnode 2

    # This procedure builds the widgets for the execute menu
    proc buildExecuteMenu { } {
	set parent [toplevel .exec]
	wm resizable $parent 0 0
	wm title $parent "Execute Menu" 
	set onlineviewer::executable [lindex [qpcs::user_queryExecname \
		$onlineviewer::server1] 1]
	set onlineviewer::remoteSystem [lindex [qpcs::user_queryRemote \
		$onlineviewer::server1] 1]

	bind $parent <Escape> [list destroy $parent ]
	bind $parent <Return> [list onlineviewer::executeReturnBinding \
		$parent ]

	pack [set eframe1 [frame $parent.eframe1]] -side top -pady 3
	pack [label $eframe1.text -text "System Executable" -width 20 \
		-anchor w] -side left -padx 3
	pack [entry $eframe1.entry -textvariable onlineviewer::executable] \
		-side left -padx 3

	pack [set eframe2 [frame $parent.eframe2]] -side top -pady 3
	pack [label $eframe2.text -text "Remote System" -width 20 -anchor w] \
		-side left -padx 3
	pack [entry $eframe2.entry -textvariable onlineviewer::remoteSystem] \
		-side left -padx 3
    
	pack [set buttonframe [frame $parent.buttonframe]] -side top -pady 5
	pack [set onlineviewer::widgets(ExecStart) [button $buttonframe.start \
		-text "Start" -command onlineviewer::executeStart -width 8 \
		-state normal -height 1 ]] -side left
	pack [set onlineviewer::widgets(ExecStop) [button $buttonframe.stop \
		-text "Stop" -command onlineviewer::executeStop -width 8 \
		-state disabled -height 1 ]] -side left 
	pack [set onlineviewer::widgets(ExecRestart) [button \
		$buttonframe.reset -text "Restart" -command \
		onlineviewer::executeRestart -width 8 -height 1 \
		-state disabled]] -side left
	
	if {[qpcs::systemExists $onlineviewer::server1]} {
	    $onlineviewer::widgets(ExecStart) configure -state disabled
	    $onlineviewer::widgets(ExecStop) configure -state normal
	    $onlineviewer::widgets(ExecRestart) configure -state normal	    
	}

	focus $parent
	grab $parent
	tkwait window $parent
    }

    # This procedure is bound to the return command 
    proc executeReturnBinding { parent } {
	if {$onlineviewer::systemExists} {
	    onlineviewer::executeStop
	} else {
	    onlineviewer::executeStart
	    destroy $parent
	}
    }

    
    # This procedure is called by the Execut Menu's start button
    proc executeStart { } {
	if {[string match $onlineviewer::remoteSystem ""]} {
	    qpcs::user_spawn $onlineviewer::server1 $onlineviewer::execnode $onlineviewer::executable
	} else {
	    qpcs::user_spawn $onlineviewer::server1 $onlineviewer::execnode \
		    $onlineviewer::executable $onlineviewer::remoteSystem
	}
	set onlineviewer::subjects [onlineviewer::getSystems]
	$onlineviewer::widgets(SSCombo) configure -values \
		$onlineviewer::subjects
	$onlineviewer::widgets(ExecStart) configure -state disabled
	$onlineviewer::widgets(ExecStop) configure -state normal
	$onlineviewer::widgets(ExecRestart) configure -state normal
	onlineviewer::updateStatus
    }

    # This procedure is called by the Execut Menu's stop button
    proc executeStop { } {
	qpcs::user_kill $onlineviewer::server1
	$onlineviewer::widgets(ExecStart) configure -state normal
	$onlineviewer::widgets(ExecStop) configure -state disabled
	$onlineviewer::widgets(ExecRestart) configure -state disabled
	onlineviewer::updateStatus
    }

    # This procedure is called by the Execut Menu's restart button
    proc executeRestart { } {
	onlineviewer::executeStop
	onlineviewer::executeStart
    }

################################## I/O Settings Menu ######################### 
    
    set eyechannels "0/1"
    set emlograte [lindex [qpcs::user_queryDetails $onlineviewer::server1] 4]
    set blinkallowance 0
    set leverchannels [lindex [qpcs::user_queryDetails \
	    $onlineviewer::server1] 2]/[lindex [qpcs::user_queryDetails \
	    $onlineviewer::server1] 3]

    # This procedure builds the widgets for the I/O Settings menu
    proc buildIOMenu { } {	
	set onlineviewer::eyechannels "0/1"
	set onlineviewer::emlograte [lindex [qpcs::user_queryDetails \
		$onlineviewer::server1] 4]
	set onlineviewer::leverchannels [lindex [qpcs::user_queryDetails \
		$onlineviewer::server1] 2]/[lindex [qpcs::user_queryDetails \
		$onlineviewer::server1] 3]
	if {[string match $onlineviewer::leverchannels /]} {
	    set onlineviewer::leverchannels ""
	}
    
	set parent [toplevel .iosettings]
	wm resizable $parent 0 0
	wm title $parent "Eye Movements" 

	bind $parent <Escape> [list destroy $parent ]
	
	pack [set frame1 [frame $parent.frame1]] -side top -pady 2
	pack [label $frame1.text -text "Eye Channels" -width 15] \
		-side left -padx 5 -anchor w
	pack [ComboBox $frame1.field -values $::EYE_CHANNELS -textvariable \
		onlineviewer::eyechannels -width 5 -editable false] \
		-side left -padx 5 -pady 3

	pack [set frame2 [frame $parent.frame2]] -side top -pady 2
	pack [label $frame2.text -text "EM Log Rate" -width 15] \
		-side left -padx 5 -anchor w
	pack [ComboBox $frame2.field -values $::EM_LOG_RATE -textvariable \
		onlineviewer::emlograte -width 5 -editable false] \
		-side left -padx 5 -pady 3

	pack [set frame3 [frame $parent.frame3]] -side top -pady 2
	pack [label $frame3.text -text "Blink Allowance" -width 15] \
		-side left -padx 5 -anchor w
	pack [ComboBox $frame3.field -values $::BLINK_ALLOWANCE -textvariable \
		onlineviewer::blinkallowance -width 5 -editable false] \
		-side left \
		-padx 5 -pady 3

	focus $parent
	grab $parent
	tkwait window $parent
    }

    # This procedure sets the levers detail on the QNX side
    proc setLeversCommand { } {
	set left [lindex [split $onlineviewer::leverchannels /] 0]
	set right [lindex [split $onlineviewer::leverchannels /] 1]
	qpcs::user_setLevers $onlineviewer::server1 $left $right
    }

}

onlineviewer::buildBasicWidget

set beast beast.neuro.brown.edu









