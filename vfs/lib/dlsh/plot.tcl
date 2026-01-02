########################################################################
# NAME
#   plot.tcl
#
# DESCRIPTION
#   Routines for making plots.  The plot objects are created using
# dynamic groups with lists corresponding to different plot attributes.
#
# AUTHOR
#   DLS, 12/95
#
########################################################################

package provide dlsh 1.2

#
# GLOBALS for the plot package
#

set dlpNROWS 2;		# For the panel commands these keep
set dlpNCOLS 2;		#  track of the # of specified panels

########################################################################
# PROC
#   dlp_create
#
# DESCRIPTION
#   Creates a plot group for storing info about a plot.  The update option
# allows a previously created plot (older version) to have any missing
# fields added.  If the update option is 0 (the default) then each
# list is added to a new plot group using dg_addNewList.
#
########################################################################

proc dlp_create { plot { update 0 } } {
    if { !$update } {
	if { [dg_exists $plot] } { dg_delete $plot }
	dg_create $plot
	set add dg_addNewList
    } else {
	if { ![dg_exists $plot] } { error "Plot $plot does not exist" }
	set add dlp_updategroup
    }
    
    $add $plot curviewport float
    $add $plot curwindow   float

    $add $plot fontfamily string

    $add $plot title      string
    $add $plot titlefontsize  float
    $add $plot titlefont  string
    $add $plot titlex     float
    $add $plot titley     float
    $add $plot titlejust  int
    $add $plot titlespacing   float

    $add $plot hscale     float
    $add $plot vscale     float
    $add $plot border     long

    $add $plot xaxis      long
    $add $plot x2axis     long
    $add $plot xmin       float
    $add $plot xmax       float
    $add $plot xticks     float
    $add $plot xsubticks  float
    $add $plot x2ticks    float
    $add $plot x2subticks float
    $add $plot xgrid      long
    $add $plot xgridplaces float
    $add $plot xgridopts  string
    $add $plot xlabels    int
    $add $plot xlabelswitch int
    $add $plot xnames     string
    $add $plot xplaces    float
    $add $plot xticklength     float
    $add $plot xsubticklength  float
    $add $plot x2ticklength    float
    $add $plot x2subticklength float
    $add $plot xloffset        float
    $add $plot xtitlefont      string
    $add $plot xtitlefontsize  float
    $add $plot xnamesfont      string
    $add $plot xnamesfontsize  float
    $add $plot xtitle      string
    $add $plot xtitlex     float
    $add $plot xtitley     float
    $add $plot xtitlespacing float
    $add $plot xtitleswitch int

    $add $plot yaxis      long
    $add $plot y2axis     long
    $add $plot ymin       float
    $add $plot ymax       float
    $add $plot yticks     float
    $add $plot ysubticks  float
    $add $plot y2ticks    float
    $add $plot y2subticks float
    $add $plot ygrid      long
    $add $plot ygridplaces float
    $add $plot ygridopts  string
    $add $plot ylabels    int
    $add $plot ylabelswitch int
    $add $plot ynames     string
    $add $plot yplaces    float
    $add $plot yticklength      float
    $add $plot ysubticklength   float
    $add $plot y2ticklength     float
    $add $plot y2subticklength  float
    $add $plot yloffset         float
    $add $plot ytitlefont       string
    $add $plot ytitlefontsize   float
    $add $plot ynamesfont       string
    $add $plot ynamesfontsize   float
    $add $plot ytitle      string
    $add $plot ytitlex     float
    $add $plot ytitley     float
    $add $plot ytitlespacing float
    $add $plot ytitleswitch int

    $add $plot adjustxy    long
    $add $plot lwidth      long

    $add $plot xdata       list
    $add $plot ydata       list
    $add $plot errdata     list
    $add $plot precmds     string
    $add $plot plotcmds    string
    $add $plot postcmds    string

    $add $plot cur_xmin    float
    $add $plot cur_xmax    float
    $add $plot cur_xticks  float

    $add $plot cur_ymin    float
    $add $plot cur_ymax    float
    $add $plot cur_yticks  float
    
    return $plot
}

proc dlp_update { plot } {
    dlp_create $plot 1
}

proc dlp_updategroup { plot field type } {
    if { ![dl_exists $plot:$field] } {
	dg_addNewList $plot $field $type
    } elseif { [dl_datatype $plot:$field] != $type } {
	dl_set $plot:$field [dl_create $type]
    }
}


proc dlp_exists { plot } {
    return [dg_exists $plot]
}

proc dlp_rename { plot newname } {
    return [dg_rename $plot $newname]
}

proc dlp_portrait { } {
    set vp [getfviewport]
    resizewin 612 792
    setpageori portrait
    eval setfviewport $vp
}

proc dlp_landscape { } {
    set vp [getfviewport]
    resizewin 640 480
    setpageori landscape
    eval setfviewport $vp
}

proc dlp_pagedims { x y } {
    set vp [getfviewport]
    resizewin $x $y
    setpageori portrait
    eval setfviewport $vp
}

proc dlp_delete { plot } {
    if { [dg_exists $plot] } { 
	dg_delete $plot 
    } else { 
	error "Plot $plot does not exist" 
    }
}

proc dlp_clean {} {
    foreach group [dg_dir] {
	if { [regexp {plot[0-9]+} $group] } { dg_delete $group }
    }
}

proc dlp_pop { { clear "" } } {
    popviewport all
    setfviewport 0 0 1 1
    dl_popTemps all
    if { $clear != "" } clearwin
}

proc dlp_set { p entry args } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if { $args == "" } {
	return [dl_tcllist $p:$entry]
    } elseif { [llength $args] == 1 } {
	if { [dl_exists $args] } {
	    dl_set ${p}:${entry} $args
	} else {
	    dl_reset ${p}:${entry}
	    eval dl_append ${p}:${entry} $args
	}
    } else {
	dl_reset ${p}:${entry}
	eval dl_append ${p}:${entry} $args
    }
}

proc dlp_reset { p args } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    foreach entry $args {
	dl_reset ${p}:${entry}
    }
}

proc dlp_resetdata { p } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    dl_reset ${p}:xdata
    dl_reset ${p}:ydata
    dl_reset ${p}:errdata
    dl_reset ${p}:plotcmds
}

proc dlp_setxrange { p xmin xmax } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    dl_set ${p}:xmin [dl_create float $xmin]
    dl_set ${p}:xmax [dl_create float $xmax]
    return $p
}

proc dlp_setyrange { p ymin ymax } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    dl_set ${p}:ymin [dl_create float $ymin]
    dl_set ${p}:ymax [dl_create float $ymax]
    return $p
}

proc dlp_getaspect { p } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if { ![dl_exists $p:curviewport] || ![dl_exists $p:curwindow] } {
	dlp_axes $p
    }
    set aspect [expr ([dl_get $p:curviewport 2]-[dl_get $p:curviewport 0])/ \
	    ([dl_get $p:curviewport 3]-[dl_get $p:curviewport 1])]
    set waspect  [expr ([dl_get $p:curwindow 2]-[dl_get $p:curwindow 0])/ \
	    ([dl_get $p:curwindow 3]-[dl_get $p:curwindow 1])]
    return [expr $waspect/$aspect]
}

proc dlp_wingetaspect { p w } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if { ![dl_exists $p:curviewport] || ![dl_exists $p:curwindow] } {
	dlp_axes $p
    }
    set aspect [expr ([dl_get $p:curviewport 2]-[dl_get $p:curviewport 0])/ \
	    ([dl_get $p:curviewport 3]-[dl_get $p:curviewport 1])]
    set waspect  [expr ([lindex $w 2]-[lindex $w 0])/ \
	    ([lindex $w 3]-[lindex $w 1])]
    return [expr $waspect/$aspect]
}

########################################################################
# PROC
#   dlp_copy
#
# DESCRIPTION
#   Copy a plot to another
#
########################################################################

proc dlp_copy { p new } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if {[dg_exists $new]} { dg_delete $new }
    dg_copy $p $new
}

########################################################################
# PROC
#   dlp_write
#
# DESCRIPTION
#   Save a plot to a file
#
########################################################################

proc dlp_write { p { file "" }} {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if { $file == "" } { set file $p.plt }
    dg_write $p $file
}

########################################################################
# PROC
#   dlp_read
#
# DESCRIPTION
#   Read a plot from a file
#
########################################################################

proc dlp_read { file { name "" } } {
    if { $name != "" } { dg_read $file $name } { dg_read $file }
}

########################################################################
# PROC
#   dlp_plot
#
# DESCRIPTION
#   Make a plot using the provided plot group
#
########################################################################

proc dlp_plot { p } {
    dl_pushTemps                ;# Keep track of temp lists created
    dlp_axes $p 1		;# The 1 means execute associate cmds
    dlp_postcmds $p		;#   with the dlp_plot cmds func
    dl_popTemps                 ;# Free any temp lists created
    return $p
}

########################################################################
# PROC
#   dlp_plotcmds
#
# DESCRIPTION
#   Execute commands associated with the plot
#
########################################################################

proc dlp_plotcmds { p } {
    set thisplot $p
    foreach cmd [dl_tcllist $p:precmds] { eval $cmd }
    foreach cmd [dl_tcllist $p:plotcmds] { eval $cmd }
}

########################################################################
# PROC
#   dlp_postcmds
#
# DESCRIPTION
#   Commands to be executed after the data and axis are plotted
#
########################################################################

proc dlp_postcmds { p } {
    set thisplot $p
    foreach cmd [dl_tcllist $p:postcmds] { eval $cmd }
}

########################################################################
# PROC
#   dlp_subplot
#
# DESCRIPTION
#   Make a subplot in a panel using the plot group specified
#
########################################################################

proc dlp_subplot { p panel } {
    dlp_pushpanel $panel
    if { [catch { dlp_plot $p } result] } {
	dlp_poppanel
	error $result
    }
    dlp_poppanel
    return $p
}
    
########################################################################
# PROC
#   dlp_axes
#
# DESCRIPTION
#   Creates border, axes, tick marks, etc.  This is the main function.
#
########################################################################

proc dlp_axes { p { exec_cmds 0 } } {
    set nxticks 5
    set nyticks 5

    scan [getviewport] "%f %f %f %f" vx0 vy0 vx1 vy1
    set tscale [expr (1.0+(1000.0-($vy1-$vy0))/1000.0)*0.15]
    set hscale [dlp_setvar $p:hscale $tscale]
    set vscale [dlp_setvar $p:vscale $tscale]

    # Somehow try to take into account the actual size of the
    # plot for picking things like fontsizes, ticklengths, etc.
    set scale [dl_mean [dl_flist [getxscale] [getyscale]]]

    # Don't even ask.  It seems to work better!
    set fontscale [expr sqrt($scale)]

    # Start with border
    set useborder [dlp_setvar $p:border  0]
    if { $useborder == 1 } { cgframe }

    set fontfamily [dlp_setvar $p:fontfamily "Helvetica"]
    set lw   [dlp_setvar $p:lwidth 100]

    set minx [dlp_setvar $p:xmin ""]
    set maxx [dlp_setvar $p:xmax ""]
    if { $minx == "" || $maxx == "" } {
	if { $minx == "" } { set minx [dlp_minxdata $p] }
	if { $minx == "" } { set minx 0 }

	if { $maxx == "" } { set maxx [dlp_maxxdata $p] }
	if { $maxx == "" } { set maxx 10 }

	if { $minx == $maxx } {
	    set off [expr 0.1*$minx]
	    set minx [expr $minx-$off]
	    set maxx [expr $maxx+$off]
	} else {
	    set off [expr 0.1*($maxx-$minx)]
	    # Haven't decided what's best here...
	    # set minx [expr $minx-$off]
	    set maxx [expr $maxx+$off]
	}
    }

    set adjustxy [dlp_setvar $p:adjustxy 0]
    if { $adjustxy } {
	pushviewport
	setpviewport [expr $hscale/2.] [expr $vscale/2.] \
		[expr 1.0-$hscale/2.] [expr 1.0-$vscale/2.]
	dl_local vp [eval dl_create float [getviewport]]
	set aspect [expr ([dl_get $vp 2]-[dl_get $vp 0]) / \
		([dl_get $vp 3]-[dl_get $vp 1])]
	popviewport
	set miny [expr $minx/$aspect]
	set maxy [expr $maxx/$aspect]
	dlp_set $p ymin $miny
	dlp_set $p ymax $maxy
    } else {
	set miny [dlp_setvar $p:ymin ""]
	set maxy [dlp_setvar $p:ymax ""]
	if { $miny == "" || $maxy == "" } {
	    if { $miny == "" } { set miny [dlp_minydata $p] }
	    if { $miny == "" } { set miny 0 }
	    
	    if { $maxy == "" } { set maxy [dlp_maxydata $p] }
	    if { $maxy == "" } { set maxy 10 }
	    
	    if { $miny == $maxy } {
		if { $miny } { set off [expr 0.2*$miny] } { set off 0.2 }
		set miny [expr $miny-$off]
		set maxy [expr $maxy+$off]
	    } else {
		set off [expr 0.2*($maxy-$miny)]
		# Haven't decided what's best here...
		# set miny [expr $miny-$off]
		set maxy [expr $maxy+$off]
	    }
	}
    }

    if { [dl_length $p:xmin] > 0 ||  [dl_length $p:xmax] > 0 } {
	set xlabel_func dlg_tight_label
    } else {
	set xlabel_func dlg_loose_label
    }
    if { [dl_length $p:ymin] > 0 ||  [dl_length $p:ymax] > 0 } {
	set ylabel_func dlg_tight_label
    } else {
	set ylabel_func dlg_loose_label
    }

    # If tick places aren't specified, calculate them
    if { [dl_length $p:xticks] == 0 } {
	dl_local xticks \
		[eval dl_create float [$xlabel_func $minx $maxx $nxticks]]
    } else {
	dl_local xticks $p:xticks
    }
    if { [dl_length $p:yticks] == 0 } {
	dl_local yticks \
		[eval dl_create float [$ylabel_func $miny $maxy $nyticks]]
    } else {
	dl_local yticks $p:yticks
    }
    if { [dl_length $p:x2ticks] == 0 } {
	dl_local x2ticks $xticks
    } else {
	dl_local x2ticks $p:x2ticks
    }
    if { [dl_length $p:y2ticks] == 0 } {
	dl_local y2ticks $yticks
    } else {
	dl_local y2ticks $p:y2ticks
    }

    set x0 [dlp_setvar $p:xmin [dl_min $xticks]]
    set x1 [dlp_setvar $p:xmax [dl_max $xticks]]
    set y0 [dlp_setvar $p:ymin [dl_min $yticks]]
    set y1 [dlp_setvar $p:ymax [dl_max $yticks]]


    # These can be used for reference (postcmds for example)
    dl_set $p:cur_xmin $x0
    dl_set $p:cur_xmax $x1
    dl_set $p:cur_ymin $y0
    dl_set $p:cur_ymax $y1
    
    dl_set $p:cur_xticks $xticks
    dl_set $p:cur_yticks $yticks


    # Now do subticks
    if { [dl_length $p:xsubticks] == 0 } {
	set tstart [dl_get $xticks 0]
	set tstop  [dl_last $xticks]
	if { [expr $tstart > $tstop ] } { 
	    set t $tstart; set tstart $tstop; set tstop $t
	}
	set xsubstep [expr abs([dl_get $xticks 0]-[dl_get $xticks 1])]
	set xsubstart [expr $tstart+(0.5*$xsubstep)]
	if { [expr $xsubstart-$xsubstep] >= $x0 } {
	    set xsubstart [expr $tstart-(0.5*$xsubstep)]
	}
	set xsubstop [expr $tstop-(0.5*$xsubstep)]
	if { [expr $xsubstop+$xsubstep] <= $x1 } {
	    set xsubstop [expr $tstop+(0.5*$xsubstep)]
	}
	dl_local xsubticks [dl_series $xsubstart $xsubstop $xsubstep]
    } else {
	dl_local xsubticks $p:xsubticks
    }
    
    if { [dl_length $p:x2subticks] == 0 } {
	dl_local x2subticks $xsubticks
    } else {
	dl_local x2subticks $p:x2subticks
    }

    if { [dl_length $p:ysubticks] == 0 } {
	set tstart [dl_get $yticks 0]
	set tstop  [dl_last $yticks]
	if { [expr $tstart > $tstop ] } { 
	    set t $tstart; set tstart $tstop; set tstop $t
	}
	set ysubstep [expr abs([dl_get $yticks 0]-[dl_get $yticks 1])]
	set ysubstart [expr $tstart+(0.5*$ysubstep)]
	if { [expr $ysubstart-$ysubstep] >= $y0 } {
	    set ysubstart [expr $tstart-(0.5*$ysubstep)]
	}
	set ysubstop [expr $tstop-(0.5*$ysubstep)]
	if { [expr $ysubstop+$ysubstep] <= $y1 } {
	    set ysubstop [expr $tstop+(0.5*$ysubstep)]
	}
	dl_local ysubticks [dl_series $ysubstart $ysubstop $ysubstep]
    } else {
	dl_local ysubticks $p:ysubticks
    }

    if { [dl_length $p:y2subticks] == 0 } {
	dl_local y2subticks $ysubticks
    } else {
	dl_local y2subticks $p:y2subticks
    }

    # If marker places aren't specified, calulate them
    if { [dl_length $p:xplaces] == 0 } {
	dl_local xplaces $xticks
    } else {
	dl_local xplaces $p:xplaces
    }
    if { [dl_length $p:yplaces] == 0 } {
	dl_local yplaces $yticks
    } else {
	dl_local yplaces $p:yplaces
    }


    # If tick names aren't specified, use tick value
    if { [dl_length $p:xnames] == 0 } {
	dl_local xnames $xplaces
    } else {
	dl_local xnames $p:xnames
    }
    if { [dl_length $p:ynames] == 0 } {
	dl_local ynames $yplaces
    } else {
	dl_local ynames $p:ynames
    }
    
    pushviewport
    setpviewport [expr $hscale/2.] [expr $vscale/2.] \
	    [expr 1.0-$hscale/2.] [expr 1.0-$vscale/2.]
    setwindow $x0 $y0 $x1 $y1
    dl_set $p:curviewport [eval dl_create float [getviewport]]
    dl_set $p:curwindow [dl_create float $x0 $y0 $x1 $y1]

    # Unlike the axes and tickmarks, we draw the grids before the data
    # so they don't obscure things
    if { [dlp_setvar $p:ygrid 0] } {
	if { [dl_length $p:ygridplaces] != 0 } {
	    dl_local gridplaces $p:ygridplaces
	} else {
	    dl_local gridplaces $yticks
	}
	set n [dl_length $gridplaces]
	set opts "-lstyle 4 [join [dl_tcllist $p:ygridopts]]"
	if { [catch { eval dlg_disjointLines \
		[dl_replicate [dl_flist $x0 $x1] $n] \
		[dl_repeat $gridplaces 2] $opts } result] } {
	    popviewport
	    error $result
	}
    }

    # If xgrid is set, draw vertical grid lines
    if { [dlp_setvar $p:xgrid 0] } {
	if { [dl_length $p:xgridplaces] != 0 } {
	    dl_local gridplaces $p:xgridplaces
	} else {
	    dl_local gridplaces $xticks
	}
	set n [dl_length $gridplaces]
	set opts "-lstyle 4 [join [dl_tcllist $p:xgridopts]]"
	if { [catch { eval dlg_disjointLines \
		[dl_repeat $gridplaces 2] \
		[dl_replicate [dl_flist $y0 $y1] $n] $opts } result] } {
	    popviewport
	    error $result
	}
    }

    # Now execute the precmds and plotcmds
    if { $exec_cmds == 1 } { dlp_plotcmds $p }

    # Draw the axes and the ticks
    setclip 0

    if { ![dlp_setvar $p:yaxis 1] || ![dlp_setvar $p:xaxis 1] || \
	    ![dlp_setvar $p:y2axis 1] || ![dlp_setvar $p:x2axis 1] } {
	set one_axis 0
    } else { set one_axis 1 }

    set ticklength [dlp_setvar $p:yticklength 16]
    set subticklength [dlp_setvar $p:ysubticklength 8]
    if { [dlp_setvar $p:yaxis 1] } {
	if { !$one_axis } {
	    dlg_lines [dl_create float $x0 $x0] [dl_create float $y0 $y1] 0 $lw
	}
	dlg_markers $x0 $yticks htick_r \
		[expr $ticklength*$scale] -lwidth $lw
	dlg_markers $x0 $ysubticks htick_r \
		[expr $subticklength*$scale] -lwidth $lw
    }
    set ticklength [dlp_setvar $p:y2ticklength $ticklength]
    set subticklength [dlp_setvar $p:y2subticklength $subticklength]
    
    if { [dlp_setvar $p:y2axis 1] } {
	if { !$one_axis } {
	    dlg_lines [dl_create float $x1 $x1] [dl_create float $y0 $y1] 0 $lw
	}
	dlg_markers $x1 $y2ticks htick_l \
		[expr $ticklength*$scale] -lwidth $lw
	dlg_markers $x1 $y2subticks htick_l \
		[expr $subticklength*$scale] -lwidth $lw
    }

    set ticklength [dlp_setvar $p:xticklength 16]
    set subticklength [dlp_setvar $p:xsubticklength 8]
    if { [dlp_setvar $p:xaxis 1] } {
	if { !$one_axis } {
	    dlg_lines [dl_create float $x0 $x1] [dl_create float $y0 $y0] 0 $lw
	}
	dlg_markers $xticks $y0 vtick_u \
		[expr $ticklength*$scale] -lwidth $lw
	dlg_markers $xsubticks $y0 vtick_u \
		[expr $subticklength*$scale] -lwidth $lw
    }
    set ticklength [dlp_setvar $p:x2ticklength $ticklength]
    set subticklength [dlp_setvar $p:x2subticklength $subticklength]
    if { [dlp_setvar $p:x2axis 1] } {
	if { !$one_axis } {
	    dlg_lines [dl_create float $x0 $x1] [dl_create float $y1 $y1] 0 $lw
	}
	dlg_markers $x2ticks $y1 vtick_d \
		[expr $ticklength*$scale] -lwidth $lw
	dlg_markers $x2subticks $y1 vtick_d \
		[expr $subticklength*$scale] -lwidth $lw
    }

    # If all axes were on, draw the axis lines as a group 
    #   so they will be closed
    if { $one_axis } {
	dlg_lines [dl_create float $x0 $x1 $x1 $x0 $x0] \
		[dl_create float $y0 $y0 $y1 $y1 $y0] -lwidth $lw
    }

    setclip 1

    # Label the ticks
    if {[dlp_setvar $p:xlabels 1]} {
	set font [dlp_setvar $p:xnamesfont $fontfamily]
	set size [dlp_setvar $p:xnamesfontsize 14]
	
	# Try to find a good offset for the xaxis labels independent
	# of plot size.  NOT EVEN OBVIOUS TO ME WHAT THE HECK I'M DOING 
	# HERE...
	set toff [expr pow(1.0+((1000.0-($vy1-$vy0))/1000.0),1.2)*0.025] 
	set offset [dlp_setvar $p:xloffset $toff]
	
	set dpoints [dlg_nice_dpoints $xnames]
	if {![dlp_setvar $p:xlabelswitch 0]} {
	    dlg_text $xplaces [expr $y0-($offset*($y1-$y0))] $xnames \
		    $font [expr $size*$fontscale] 0 -dpoints $dpoints
	} else {
	    dlg_text $xplaces [expr $y1+($offset*($y1-$y0))] $xnames \
		    $font [expr $size*$fontscale] 0 -dpoints $dpoints
	}
    }

    if {[dlp_setvar $p:ylabels 1]} {
	set font [dlp_setvar $p:ynamesfont $fontfamily]
	set size [dlp_setvar $p:ynamesfontsize 14]
	set offset [dlp_setvar $p:yloffset 0.015]
	set dpoints [dlg_nice_dpoints $ynames]
	if {![dlp_setvar $p:ylabelswitch 0]} {
	    dlg_text [expr $x0-($offset*($x1-$x0))] $yplaces $ynames \
		    $font [expr $size*$fontscale] 1 -dpoints $dpoints
	} else {
	    dlg_text [expr $x1+($offset*($x1-$x0))] $yplaces $ynames \
		    $font [expr $size*$fontscale] -1 -dpoints $dpoints
	}
    }


    popviewport

    # draw xtitle
    set xtitle [dlp_setvar $p:xtitle ""]
    set xtitlefontsize [dlp_setvar $p:xtitlefontsize 16]
    set spacing [dlp_setvar $p:xtitlespacing 1]
    if { $xtitle != "" } {
	if {![dlp_setvar $p:xtitleswitch 0]} {
	    pushpviewport 0 0 1 [expr $vscale/2]
	    set y [dlp_setvar $p:xtitley 0.35]
	} else {
	    pushpviewport 0 [expr 1.0-$vscale/2] 1 1
	    set y [dlp_setvar $p:xtitley 0.55]
	}
	set x [dlp_setvar $p:xtitlex 0.5]
	setwindow 0 0 1 1
	set font [dlp_setvar $p:xtitlefont $fontfamily]
	dlg_text $x $y $xtitle -font $font -spacing $spacing \
		-size [expr $xtitlefontsize*$fontscale] -just 0 -ori 0
	popviewport
    }
    
    # draw ytitle
    set ytitle [dlp_setvar $p:ytitle ""]
    set ytitlefontsize [dlp_setvar $p:ytitlefontsize 16]
    set spacing [dlp_setvar $p:ytitlespacing 1]
    if { $ytitle != "" } {
	if {![dlp_setvar $p:ytitleswitch 0]} {
	    pushpviewport 0 0 [expr $hscale/2] 1
	    set x [dlp_setvar $p:ytitlex 0.4]
	} else {
	    pushpviewport [expr 1.0-$hscale/2] 0 1 1
	    set x [dlp_setvar $p:ytitlex 0.6]
	}
	setwindow 0 0 1 1
	set y [dlp_setvar $p:ytitley 0.5]
	set font [dlp_setvar $p:ytitlefont $fontfamily]
	dlg_text $x $y $ytitle -font $font \
		-size [expr $ytitlefontsize*$fontscale] -just 0 -ori 1 \
		-spacing $spacing
	popviewport
    }

    # draw title
    set title [dlp_setvar $p:title  ""]
    set titlefontsize [dlp_setvar $p:titlefontsize 18]
    set titlejust [dlp_setvar $p:titlejust 0]
    set spacing [dlp_setvar $p:titlespacing 1.0]
    if { $title != "" } {
	set x [dlp_setvar $p:titlex 0.5]
	set y [dlp_setvar $p:titley 0.5]
	set font [dlp_setvar $p:titlefont $fontfamily]
	pushviewport
	setpviewport 0 [expr 1.0-$vscale/2] 1 1
	setwindow 0 0 1 1
	dlg_text $x $y $title -font $font -spacing $spacing\
		-size [expr $titlefontsize*$fontscale] -just $titlejust
	popviewport
    }
}

########################################################################
# PROC
#   dlp_add[XY]Data
#
# DESCRIPTION
#   Appends x and y data to a plot
#
########################################################################

proc dlp_addXData { p x } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }

    if { [dl_exists $x] } { 
	dl_append $p:xdata $x 
    } elseif { [catch {expr $x} result] } {
	error "dlp_addData: bad xdata ($x)"
    } else {
	dl_append $p:xdata [dl_create float $result]
    }
    return [expr [dl_length $p:xdata]-1]
}

proc dlp_addYData { p y } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    
    if { [dl_exists $y] } { 
	dl_append $p:ydata $y 
    } elseif { [catch {expr $y} result] } {
	error "dlp_addData: bad ydata ($y)"
    } else {
	dl_append $p:ydata [dl_create float $result]
    }
    return [expr [dl_length $p:ydata]-1]
}


########################################################################
# PROC
#   dlp_draw
#
# DESCRIPTION
#   Adds drawing command for plot p
#
########################################################################

proc dlp_draw { p func dataset args } {
    if { [info proc dlp_${func}]  == "" } {
	error "dlp_draw: draw function $func not found"
    }
    if { $func == "bars" } { 
	set defaultargs "-start \[dl_get \$thisplot:curwindow 1\]"
    } else {
	set defaultargs ""
    }

    # Otherwise, append a command for one of the dlp_drawing funcs
    if { [llength $dataset] == 1 } {
	set x 0
	set y $dataset
    } else {
	set x [lindex $dataset 0]
	set y [lindex $dataset 1]
    }
    set newargs [regsub -all %p $args {$thisplot}]
    set cmd "dlp_${func} \$thisplot \$thisplot:xdata:$x \$thisplot:ydata:$y \
	$defaultargs $newargs"
    dl_append $p:plotcmds $cmd
}

########################################################################
# PROC
#   dlp_addErrbars
#
# DESCRIPTION
#   Adds error bar commands for plot p
#
########################################################################

proc dlp_addErrbars { p dataset errdata args } {
    if { [llength $dataset] == 1 } {
	set x 0
	set y $dataset
    } else {
	set x [lindex $dataset 0]
	set y [lindex $dataset 1]
    }
    dl_append $p:errdata $errdata
    set errset [expr [dl_length $p:errdata]-1]
    set cmd "dlp_errorbars \$thisplot \$thisplot:xdata:$x \$thisplot:ydata:$y \
	\$thisplot:errdata:$errset $args"
    dl_append $p:plotcmds $cmd
}


########################################################################
# PROC
#   dlp_addErrData
#
# DESCRIPTION
#   Adds error bar data
#
########################################################################

proc dlp_addErrData { p edata } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }

    if { [dl_exists $edata] } { 
	dl_append $p:errdata $edata
    }
    return [expr [dl_length $p:errdata]-1]
}

########################################################################
# PROC
#   dlp_addErrbarCmd
#
# DESCRIPTION
#   Adds error bar command
#
########################################################################

proc dlp_addErrbarCmd { p dataset errset  args } {
    if { [llength $dataset] == 1 } {
	set x 0
	set y $dataset
    } else {
	set x [lindex $dataset 0]
	set y [lindex $dataset 1]
    }
    set cmd "dlp_errorbars \$thisplot \$thisplot:xdata:$x \$thisplot:ydata:$y \
	\$thisplot:errdata:$errset $args"
    dl_append $p:plotcmds $cmd
}


########################################################################
# PROC
#   dlp_cmd
#
# DESCRIPTION
#   Add a stand-alone command for plot p
#
########################################################################

proc dlp_cmd { p cmd } {
    set cmd [regsub -all %p $cmd {$thisplot}]
    dl_append $p:precmds $cmd
}


########################################################################
# PROC
#   dlp_wincmd
#
# DESCRIPTION
#   Add a stand-alone command for plot p, but also reset the window
#
########################################################################

proc dlp_wincmd { p win cmd } {
    if { [llength $win] != 4 } { error "invalid window specified" }
    set cmd [regsub -all %p $cmd {$thisplot}]
    set cmd "dlp_pushwindow \$thisplot; setwindow $win; $cmd; dlp_popwindow"
    dl_append $p:precmds $cmd
}

########################################################################
# PROC
#   dlp_winycmd
#
# DESCRIPTION
#   Add a stand-alone command for plot p, but also reset the window
#
########################################################################

proc dlp_winycmd { p win cmd } {
    if { [llength $win] != 2 } { error "invalid y-window specified" }
    set w "\[dl_get \$thisplot:curwindow 0\] [lindex $win 0] \
	   \[dl_get \$thisplot:curwindow 2\] [lindex $win 1]"
    set cmd [regsub -all %p $cmd {$thisplot}]
    set cmd "dlp_pushwindow \$thisplot; setwindow $w; $cmd; dlp_popwindow"
    dl_append $p:precmds $cmd
}

########################################################################
# PROC
#   dlp_winxcmd
#
# DESCRIPTION
#   Add a stand-alone command for plot p, but also reset the window
#
########################################################################

proc dlp_winxcmd { p win cmd } {
    if { [llength $win] != 2 } { error "invalid x-window specified" }
    set w "[lindex $win 0] \[dl_get \$thisplot:curwindow 1\] \
	   [lindex $win 1] \[dl_get \$thisplot:curwindow 3\]"
    set cmd [regsub -all %p $cmd {$thisplot}]
    set cmd "dlp_pushwindow \$thisplot; setwindow $w; $cmd; dlp_popwindow"
    dl_append $p:precmds $cmd
}


########################################################################
# PROC
#   dlp_postcmd
#
# DESCRIPTION
#   Add a stand-alone command to be executed after the plot axes
#
########################################################################

proc dlp_postcmd { p cmd } {
    set cmd [regsub -all %p $cmd {$thisplot}]
    set cmd "dlp_pushwindow \$thisplot; $cmd; dlp_popwindow"
    dl_append $p:postcmds $cmd
}

########################################################################
# PROC
#   dlp_winpostcmd
#
# DESCRIPTION
#   Add a stand-alone post cmd for plot p, but also reset the window
#
########################################################################

proc dlp_winpostcmd { p win cmd } {
    if { [llength $win] != 4 } { error "invalid window specified" }
    set cmd [regsub -all %p $cmd {$thisplot}]
    set cmd "dlp_pushwindow \$thisplot; setwindow $win; $cmd; dlp_popwindow"
    dl_append $p:postcmds $cmd
}

########################################################################
# PROC
#   dlp_winypostcmd
#
# DESCRIPTION
#   Add a stand-alone post cmd for plot p, but also reset the yrange
#
########################################################################

proc dlp_winypostcmd { p win cmd } {
    if { [llength $win] != 2 } { error "invalid y window specified" }
    set w "\[dl_get \$thisplot:curwindow 0\] [lindex $win 0] \
	   \[dl_get \$thisplot:curwindow 2\] [lindex $win 1]"
    set cmd [regsub -all %p $cmd {$thisplot}]
    set cmd "dlp_pushwindow \$thisplot; setwindow $w; $cmd; dlp_popwindow"
    dl_append $p:postcmds $cmd
}


########################################################################
# PROC
#   dlp_winxpostcmd
#
# DESCRIPTION
#   Add a stand-alone post cmd for plot p, but also reset the xrange
#
########################################################################

proc dlp_winxpostcmd { p win cmd } {
    if { [llength $win] != 2 } { error "invalid x window specified" }
    set w "[lindex $win 0] \[dl_get \$thisplot:curwindow 1\] \
	   [lindex $win 1] \[dl_get \$thisplot:curwindow 3\]"
    set cmd [regsub -all %p $cmd {$thisplot}]
    set cmd "dlp_pushwindow \$thisplot; setwindow $w; $cmd; dlp_popwindow"
    dl_append $p:postcmds $cmd
}

########################################################################
# PROC
#   dlp_legend
#
# DESCRIPTION
#   Add a legend to the plot
#
########################################################################

proc dlp_legend { p keylist } {
    global colors
    dlp_pushwindow $p
    setwindow 0 0 100 30
    set row 28
    foreach key $keylist {
	eval dlg_lines [dl_ilist 70 80] $row [lindex $key 1]
	eval dlg_markers 75 $row [lindex $key 0] 
	eval dlg_text 82 $row [lindex $key 2]
	incr row -1
    }
    dlp_popwindow
}

########################################################################
# PROC
#   dlp_errorbar...
#
# DESCRIPTION
#   Add error bars to a plot
#
########################################################################

proc dlp_errorbars { p x y error args } {
    set freedata 0
    set arglist $args
    set nargs [llength $arglist]
    set dir v
    set side both
    set margs ""
    set largs ""
    for { set i 0 } { $i < $nargs } { incr i } {
	set arg [lindex $arglist $i]
	switch -- $arg  {
	    -margs { incr i; set margs [lindex $arglist $i] }
	    -largs { incr i; set largs [lindex $arglist $i] }
	    -vert  { set dir v; set side both }
	    -up    { set dir v; set side up }
	    -down  { set dir v; set side down }
	    -horiz { set dir h; set side both }
	    -left  { set dir h; set side left }
	    -right { set dir h; set side right }
	    -offset { 
		if { [expr [incr i] >= $nargs] } {
		    error "dlp_errorbars: no argument to offset"
		}
		dl_local x [dl_add $x [lindex $arglist $i]]
	    }
	}
    }
    dlp_${dir}err${side} $p $x $y $error $margs $largs
    return $p:plotcmds
}

proc dlp_verrboth { p x y error { markerargs "" } { lineargs "" } } {
    dlp_pushwindow $p
    if { [catch \
	    { eval dlg_markers $x [dl_add $y $error] -marker htick -size 8s \
	       $markerargs } \
	    result] } {
	dlp_popwindow
	error $result
    }
    # No need to catch here because if the previous command worked,
    # this should too.
    eval dlg_markers $x [dl_sub $y $error] -marker htick -size 8s $markerargs
    eval dlg_disjointLines [dl_repeat $x 2] \
	    [dl_interleave [dl_add $y $error] [dl_sub $y $error]] $lineargs
    dlp_popwindow
}

proc dlp_verrup { p x y error { markerargs "" } { lineargs "" } } {
    dlp_pushwindow $p
    if { [catch \
	    { eval dlg_markers $x [dl_add $y $error] -marker htick -size 8s \
	        $markerargs } \
	    result] } {
	dlp_popwindow
	error $result
    }

    # The zero list ensures that the interleaved data is of the same datatype
    dl_local zero [dl_sub $error $error]
    eval dlg_disjointLines [dl_repeat $x 2] [dl_interleave [dl_add $y $error] \
	    [dl_add $y $zero]] $lineargs
    dlp_popwindow
}

proc dlp_verrdown { p x y error { markerargs "" } { lineargs "" } } {
    dlp_pushwindow $p
    if { [catch \
	    { eval dlg_markers $x [dl_sub $y $error] -marker htick -size 8s \
	       $markerargs } \
	    result] } {
	dlp_popwindow
	error $result
    }

    # The zero list ensures that the interleaved data is of the same datatype
    dl_local zero [dl_sub $error $error]
    eval dlg_disjointLines [dl_repeat $x 2] [dl_interleave [dl_sub $y $error] \
	    [dl_add $y $zero]] $lineargs
    dlp_popwindow
}

proc dlp_herrboth { p x y error { markerargs "" } { lineargs "" } } {
    dlp_pushwindow $p
    if { [catch \
	    { eval dlg_markers [dl_add $x $error] $y -marker vtick -size 8s \
	       $markerargs } \
	    result] } {
	dlp_popwindow
	error $result
    }
    # No need to catch here because if the previous command worked,
    # this should too.
    eval dlg_markers [dl_sub $x $error] $y -marker vtick -size 8s $markerargs
    eval dlg_disjointLines [dl_interleave [dl_add $x $error] \
	    [dl_sub $x $error]] [dl_repeat $y 2] $lineargs
    dlp_popwindow
}

proc dlp_herrright { p x y error { markerargs "" } { lineargs "" } } {
    dlp_pushwindow $p
    if { [catch \
	    { eval dlg_markers [dl_add $x $error] $y -marker vtick -size 8s \
	       $markerargs } \
	    result] } {
	dlp_popwindow
	error $result
    }
    dl_local zero [dl_sub $error $error]
    eval dlg_disjointLines [dl_interleave [dl_add $x $zero] \
	    [dl_add $x $error]] [dl_repeat $y 2] $lineargs
    dlp_popwindow
}

proc dlp_herrleft { p x y error { markerargs "" } { lineargs "" } } {
    dlp_pushwindow $p
    if { [catch \
	    { eval dlg_markers [dl_sub $x $error] $y -marker vtick -size 8s \
	       $markerargs } \
	    result] } {
	dlp_popwindow
	error $result
    }
    dl_local zero [dl_sub $error $error]
    dlg_disjointLines [dl_interleave [dl_add $x $zero] \
	    [dl_sub $x $error]] [dl_repeat $y 2] $lineargs
    dlp_popwindow
}


#######################################################################
#
# PROCS
#  dlg_commands for graph viewports
#
# NOTES
#  Each of these calls the appropriate dlg function after pushing the
# viewport.  If there's an error, it's caught and the viewport is
# restored.
#
#######################################################################

proc dlp_moveto { plot args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "eval moveto $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_markers { plot args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "eval dlg_markers $args"  result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_lines { plot args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "eval dlg_lines $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_disjointLines { plot args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "eval dlg_disjointLines $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_steps { plot args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "eval dlg_steps $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_bars { plot args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "eval dlg_bars $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_text { plot args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "dlg_text [subst $args]" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_postscript { plot args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "eval postscript $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_postscriptAt { plot x y args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    if { [catch "eval dlg_image $x $y $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_winpostscriptAt { plot win x y args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    eval setwindow $win
    if { [catch "eval dlg_image $x $y $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_winypostscriptAt { plot win x y args } {
    if {![dg_exists $plot]} { error "Plot $plot does not exist" }
    dlp_pushwindow $plot
    set w "[dl_get $plot:curwindow 0] [lindex $win 0] \
	   [dl_get $plot:curwindow 2] [lindex $win 1]"
    eval setwindow $w
    if { [catch "eval dlg_image $x $y $args" result] } {
	dlp_popwindow
	error $result
    }
    dlp_popwindow
}

proc dlp_psheight { plot win width } {
    return [expr double($width)/ [dlp_wingetaspect $plot $win]] 
}

########################################################################
# PROC
#   dlp_[gs]etpanels
#
# DESCRIPTION
#   [GS]ets the number of panels for the current plot environment
#
########################################################################

proc dlp_setpanels { nrows ncols } {
    global dlpNROWS dlpNCOLS
    set dlpNROWS $nrows
    set dlpNCOLS $ncols
    return "$nrows $ncols"
}

proc dlp_getpanels { } {
    global dlpNROWS dlpNCOLS
    return "$dlpNROWS $dlpNCOLS"
}


########################################################################
# PROC
#   dlp_pushpanel
#
# DESCRIPTION
#   Selects a subregion of the current viewport for plotting
#
########################################################################

proc dlp_pushpanel { id } {
    global dlpNROWS dlpNCOLS
    if { [expr $id >= ($dlpNROWS*$dlpNCOLS)] } {
	error "dlp_pushpanel: invalid panel selected"
    }
    set col [expr $id%$dlpNCOLS]
    set width [expr 1.0/$dlpNCOLS]
    set height [expr 1.0/$dlpNROWS]
    set row [expr $id/$dlpNCOLS]
    set x0 [expr ($col*$width)]
    set x1 [expr ($col+1)*$width]
    set y0 [expr 1.0-($row*$height)]
    set y1 [expr 1.0-(($row+1)*$height)]

    pushviewport
    setpviewport $x0 $y0 $x1 $y1
}


########################################################################
# PROC
#   dlp_poppanel
#
# DESCRIPTION
#   Pops back after plotting in a panel
#
########################################################################

proc dlp_poppanel {} {
    popviewport
}

########################################################################
# PROC
#   dlp_pushwindow 
#
# DESCRIPTION
#   Sets up the appropriate viewport / window for the current plot
# 
#
########################################################################

proc dlp_pushwindow { p } {
    if {![dg_exists $p]} { error "Plot $plot does not exist" }
    pushviewport
    if { ![dl_exists $p:curviewport] || ![dl_exists $p:curwindow] } {
	dlp_axes $p
    } else {
	eval setviewport [dl_tcllist $p:curviewport]
	eval setwindow [dl_tcllist $p:curwindow]
    }
}

proc dlp_popwindow { } {
    popviewport
}


########################################################################
#
# PROC
#   dlp_minxdata
#   dlp_maxxdata
#   dlp_minydata
#   dlp_maxydata
#
# DESCRIPTION
#   Go through the attached data and find the current mins and maxs   
#
########################################################################

proc dlp_minxdata { p } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if { ![dl_length $p:xdata] } return
    return [dl_min [dl_mins $p:xdata]]
}

proc dlp_maxxdata { p } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if { ![dl_length $p:xdata] } return
    return [dl_max [dl_maxs $p:xdata]]
}

proc dlp_minydata { p } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if { ![dl_length $p:ydata] } return
    return [dl_min [dl_mins $p:ydata]]
}

proc dlp_maxydata { p } {
    if {![dg_exists $p]} { error "Plot $p does not exist" }
    if { ![dl_length $p:ydata] } return
    return [dl_max [dl_maxs $p:ydata]]
}


########################################################################
# PROC
#   dlp_setvar
#
# DESCRIPTION
#   Check if group:varname exists and if so, return it, else return 
# default
#
########################################################################
 
proc dlp_setvar { varname default } {
    if { [dl_length $varname] == 0 } { 
	return $default 
    } else {
	return [dl_first $varname]
    }
}


########################################################################
# PROC
#   loose_label
#
# DESCRIPTION
#   Loose labeling of data range from min to max.  Taken from Graphics
# Gems I, "Nice Numbers for Graph Labels" (pp 61-63, 657-659, 
# by Paul Heckbert).
#
########################################################################

proc loose_label { min max { nticks 5 } } {
    if { $min == $max } { return $min }
    if { [expr $min > $max] } { 
	set reverse 1
	set t $min
	set min $max
	set max $t
    } else { 
	set reverse 0 
    }
    set range [dlg_nicenum [expr $max-$min] 0]
    set d [dlg_nicenum [expr $range/($nticks-1.0)] 1]
    set graphmin [expr floor($min/$d)*$d]
    set graphmax [expr ceil($max/$d)*$d]

    set nfrac [expr -floor(log10($d))]
    if { $nfrac < 0 } { set nfrac 0.0 }
    set fmt "%.[expr int($nfrac)]f"

    set upper [expr $graphmax+0.5*$d]
    dl_local labels [dl_slist]
    for { set x $graphmin } { $x < $upper } { set x [expr $x+$d] } {
	dl_append $labels [format $fmt $x]
    }
    if { $reverse } {
	dl_set $labels [dl_reverse $labels]
    }
    return [dl_tcllist $labels]
}

proc tight_label { min max { nticks 5 } } {
    if { $min == $max } { return $min }
    if { [expr $min > $max] } { 
	set reverse 1
	set t $min
	set min $max
	set max $t
    } else { 
	set reverse 0 
    }
    set range [dlg_nicenum [expr $max-$min] 0]
    set d [dlg_nicenum [expr $range/($nticks-1.0)] 1]
    set graphmin [expr floor($min/$d)*$d]
    set graphmax [expr ceil($max/$d)*$d]
    
    set nfrac [expr -floor(log10($d))]
    if { $nfrac < 0 } { set nfrac 0.0 }
    set fmt "%.[expr int($nfrac)]f"

    set upper [expr $graphmax+0.5*$d]
    dl_local labels [dl_slist]
    for { set x $graphmin } { $x < $upper } { set x [expr $x+$d] } {
	if { $x >= $min && $x <= $max } {
	    dl_append $labels [format $fmt $x]
	}
    }
    if { $reverse } {
	dl_set $labels [dl_reverse $labels]
    }
    return [dl_tcllist $labels]
}

proc nicenum { x round } {
    set exp [expr floor(log10($x))]
    set f [expr $x/pow(10,$exp)]
    if { $round } {
	if { $f < 1.5 } {
	    set nf 1.
	} elseif { $f < 3. } { 
	    set nf 2. 
	} elseif { $f < 7. } {
	    set nf 5.
	} else {
	    set nf 10.
	}
    } else {
	if { $f <= 1. } {
	    set nf 1.
	} elseif { $f <= 2. } { 
	    set nf 2.
	} elseif { $f <= 5. } {
	    set nf 5.
	} else {
	    set nf 10.
	}
    }
    return [expr $nf*pow(10.,$exp)]
}

##################################################################
# PROC
#   nice_dpoints
#
# DESCRIPTION
#   Pick a reasonable number of decimal points to print out 
# the numbers in the list provided
#
##################################################################
proc nice_dpoints { dl } {
    if { [dl_length $dl] < 2 } { 
	set dpoints 0 
    } elseif { [dl_datatype $dl] == "float" } { 
	set range [expr [dl_get $dl 1]-[dl_get $dl 0]]
	if { [expr $range < 0] } { set range [expr -1.0*$range] }
	set dpoints [expr -int(floor(log10($range)))]
	if { [expr $dpoints < 0] } { set dpoints 0 }
	if { $dpoints == 0 } {
	    dl_local ints [dl_or [dl_eq [dl_div $dl \
		    [dl_float [dl_int $dl]]] 1.0] [dl_eq $dl 0]]
	    if { [expr [dl_sum $ints]!=[dl_length $ints]] } { 
		set dpoints 1
	    }
	}
    } else {
	set dpoints 0
    }
    return $dpoints
}

#########################################################################
# PROC
#   dlp_newplotname
#
#
# DESCRIPTION
#   Find an unused plotname
#
#########################################################################

proc dlp_newplotname {} {
    for { set i 0 } { [dg_exists plot${i}] } { incr i } { }
    return plot${i}
}

#########################################################################
# PROC
#   dlp_newplot
#
#
# DESCRIPTION
#   Create a new plot
#
#########################################################################

proc dlp_newplot {} {
    return [dlp_create [dlp_newplotname]]
}


#########################################################################
# PROC
#   dlp_emptyplot
#
#
# DESCRIPTION
#   Create an empty plot, with axes and labels turned off
#
#########################################################################

proc dlp_emptyplot {} {
    set p [dlp_create [dlp_newplotname]]
    dlp_set $p hscale 0
    dlp_set $p vscale 0
    dlp_set $p xaxis  0
    dlp_set $p x2axis 0
    dlp_set $p yaxis  0
    dlp_set $p y2axis 0
    dlp_set $p xlabels 0
    dlp_set $p ylabels 0
    return $p
}

#########################################################################
# PROC
#   dlp_xyplot
#
#
# DESCRIPTION
#   Basic interface to creating an xy plot
#
#########################################################################

proc dlp_xyplot { x y { err "" } } {
    set p [dlp_create [dlp_newplotname]]
    dlp_addXData $p $x
    dlp_addYData $p $y
    dlp_draw $p lines 0

    if { $err != "" } {
	dlp_addErrbars $p 0 $err -vert
    }
    return $p
}

#########################################################################
# PROC
#   dlp_trend
#
#
# DESCRIPTION
#   Basic interface to creating a trend plot
#
#########################################################################

proc dlp_trend { d } {
    set p [dlp_create [dlp_newplotname]]
    dlp_addXData $p [dl_fromto 0 [dl_length $d]]
    dlp_addYData $p $d
    dlp_draw $p lines 0
    dlp_setxrange $p -1 [expr [dl_length $d]+1]
    return $p
}


#########################################################################
# PROC
#   dlp_scatter
#
#
# DESCRIPTION
#   Basic interface to creating a scatters plot
#
#########################################################################

proc dlp_scatter { x y } {
    set p [dlp_create [dlp_newplotname]]
    dlp_addXData $p $x
    dlp_addYData $p $y
    dlp_draw $p markers 0 -marker fcircle
    return $p
}

#########################################################################
# PROC
#   dlp_hist
#
#
# DESCRIPTION
#   Basic interface to creating a histogram
#
#########################################################################

proc dlp_hist { x { nbins 13 } { min "" } { max "" } } {
    set p [dlp_create [dlp_newplotname]]
    if { $min != "" } {
	dl_local x [dl_select $x [dl_gte $x $min]]
	set lmin $min
    } else {
	set lmin [dl_min $x]
    }
    
    if { $max != "" } {
	dl_local x [dl_select $x [dl_lte $x $max]]
	set lmax $max
    } else {
	set lmax [dl_max $x]
    }
    dl_local b [dl_bins $lmin $lmax $nbins]
    dlp_addXData $p $b
    dlp_addYData $p [dl_hist $x $lmin $lmax $nbins]
    dlp_draw $p bars 0
    return $p
}

#########################################################################
# PROC
#   dlp_barchart
#
#
# DESCRIPTION
#   Basic interface to creating a barchart
#
#########################################################################

proc dlp_barchart { x y { err "" } } {
    set p [dlp_create [dlp_newplotname]]
    dlp_addXData $p $x
    dlp_addYData $p $y
    dlp_draw $p bars 0
    if { $err != "" } {
	dlp_addErrbars $p 0 $err -up
    }
    return $p
}



#########################################################################
# PROC
#   dlp_polar
#
#
# DESCRIPTION
#   Basic interface to creating a polar plot
#
#   If add_data is true, the line will be added, o.w. call:
#
#      dlp_addPolarData
#
#   to customize lines/fills
#
#########################################################################

proc dlp_polar { angles eccs { max "" } { add_data 1 } { title ""} } {
    global pi colors
    
    # Max a new plot, with no axes, ticks, or labels
    set p [dlp_emptyplot]
    dlp_set $p hscale .05
    dlp_set $p vscale .05

    if { $max == "" } {
	set max [expr 1.15*[dl_max $eccs]]
    }

    set radii [loose_label 0 $max]
    set extent [expr 1.6*[lrange $radii e e]]

    # Adjust the range to reflect the current viewport
    dlp_setxrange $p [expr -1.*$extent] $extent
    dlp_set $p adjustxy 1

    set bgcol [dlg_rgbcolor 40 40 40]

    foreach radius $radii {
	dl_local a [dl_combine $angles [dl_first $angles]]
	set dx [dlp_addXData $p \
		    [dl_mult $radius [dl_cos [dl_mult $a [expr $pi/180.]]]]]
	set dy [dlp_addYData $p \
		    [dl_mult $radius [dl_sin [dl_mult $a [expr $pi/180.]]]]]
	dlp_cmd $p "dlp_lines \$thisplot \
	    \$thisplot:xdata:$dx \$thisplot:ydata:$dy -linecolor $bgcol"
    }
    
    # Add axis lines for each angle
    set radius [dl_last $radii]
    set nangles [dl_length $angles]
    set dx [dlp_addXData $p \
	    [dl_interleave [dl_zeros ${nangles}.] \
	    [dl_mult $radius [dl_cos [dl_mult $angles [expr $pi/180.]]]]]]
    set dy [dlp_addYData $p \
	    [dl_interleave [dl_zeros ${nangles}.] \
	    [dl_mult $radius [dl_sin [dl_mult $angles [expr $pi/180.]]]]]]
    dlp_cmd $p "dlp_disjointLines \$thisplot \
	    \$thisplot:xdata:$dx \$thisplot:ydata:$dy -linecolor $bgcol"

    # Place value axis + labels at each radius
    dl_set $p:radii $radii
    set offset [expr [dl_last $p:radii]*-1.1]
    set loffset [expr [dl_last $p:radii]*-1.05]
    set loffset2 [expr [dl_last $p:radii]*-1.02]
    dlp_set $p radiilabs [eval dl_slist $radii]
    dlp_postcmd $p "dlp_text \$thisplot $offset \$thisplot:radii \
	    \$thisplot:radiilabs -just 1"
    dlp_postcmd $p "dlp_lines \$thisplot $loffset \[dl_flist [dl_first $radii] [dl_last $radii]\]"
    set dx [dlp_addXData $p [dl_replicate "$loffset $loffset2" [dl_length $radii]]]
    set dy [dlp_addYData $p [dl_repeat $radii 2]]
    dlp_postcmd $p "dlp_disjointLines \$thisplot \
	    \$thisplot:xdata:$dx \$thisplot:ydata:$dy"

    # Add the data 
    if { $add_data } {
	dl_local c_angles [dl_combine $angles [dl_first $angles]]
	dl_local c_eccs [dl_combine $eccs [dl_first $eccs]]
	set dx [dlp_addXData $p \
		    [dl_mult $c_eccs \
			 [dl_cos [dl_mult $c_angles [expr $pi/180.]]]]]
	set dy [dlp_addYData $p \
		    [dl_mult $c_eccs \
			 [dl_sin [dl_mult $c_angles [expr $pi/180.]]]]]
	dlp_draw $p lines "$dx $dy" -fillcolor $colors(gray)
    }
    
    dlp_set $p title $title

    return $p
}

proc dlp_addPolarData { p angles eccs { options "" } } {
    global pi

    # Close data loop
    dl_local angles [dl_combine $angles [dl_first $angles]]
    dl_local eccs [dl_combine $eccs [dl_first $eccs]]

    set dx [dlp_addXData $p \
		[dl_mult $eccs [dl_cos [dl_mult $angles [expr $pi/180.]]]]]
    set dy [dlp_addYData $p \
		[dl_mult $eccs [dl_sin [dl_mult $angles [expr $pi/180.]]]]]
    eval dlp_draw $p lines \"$dx $dy\" $options
}
			
proc dlp_test {} {
    global colors

    dlp_setpanels 2 2
    set p [dlp_newplot]
    dlp_setxrange $p -4 4
    dlp_setyrange $p 0 0.3
    dlp_set $p xtitle "Z-Score"
    dlp_set $p ytitle "Frequency"
    dlp_addXData $p [dl_bins -4 4 13]
    dlp_draw $p bars 0 -fillcolor $colors(gray) -linecolor $colors(red)
    foreach i [dl_tcllist [dl_fromto 0 4]] {
        dlp_reset $p ydata
        dl_local randhist [dl_hist [dl_zrand 1000] -4 4 13]
        dlp_addYData $p [dl_div $randhist [dl_sum [dl_float $randhist]]]
        dlp_set $p title "Plot [expr $i+1]"
        dlp_subplot $p $i
    }
    flushwin
}
