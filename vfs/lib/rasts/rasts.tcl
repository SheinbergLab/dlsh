# AUTHORS
#   21.03.99 - DLS
#   FEB 2006 - REHBM
#   Jan 2007 - JMS
#   Feb 2012 - DLS
#
# DESCRIPTION
#    this package provides basic tools for ploting rasters
#    all functions are provided under the ::rasts namespace.  all helper
#       functions (called by ::rasts procs, but not directly by user)
#       are provided under the ::helprasts namespace.  this will help users
#       find appropriate procs to call from the command line.
#    at a minimum, all procs in ::rasts namespace should contain a %HELP%
#       section that can be parsed by the 'use' proc.
#
# VERSION UPDATES
#    2.1 - changed color scheme so different units rather than different channels had different colors.
#    2.2 - added placefunc to allow plots to be positioned by a user defined function
#
# TODO
#   - better error checking, for example, make sure chans are all valid before trying to plot (and giving random error message)
#   - mintrials doesn't work with by_multi since it is possible to have some plots with data and others without
#   - better looking plots.  yrange that stops at ymax, no yticks in rasters area, box off rasters?
#   - need to account for cut channel numbers in colors.  hard to do since cut chan 0 and non cut have same numbers, but from different channels


package provide rasters 2.2

load_Ipl; # For showing stimuli

# rasts is main namespace, helper functions are in ::helprasts.
namespace eval ::rasts { } {
    proc by_multi { g chans sortby align args } {
	# %HELP%
	# ::rasts::by_multi <g> <chans> <sortby> <align> ?options...?
	#    currently just a mirror of ::rasts::by.  you should be using ::rasts::by.
	#    use ::rasts::by for more details and options
	# %END%
	eval rasts::by $g \$chans \$sortby $align $args
    }

    proc by { g chans sortby align args } {
	# %HELP%
	# ::rasts::by <g> <chans> <sortby> <align> ?options...?
	#    display raster plots for group g using all channels in chans aligned to align.
	#    if chans is 'all' then use all available channels.
	#    split plots by sortby. sortby can be upto a length of two variables to
	#    sort by.
	#
	#    Options to include at end of proc; format: -option option_value
	#    Default value is shown in []
	#       -showstims : boolean, should we display images of stimuli? [1]
	#       -showtitle : boolean, should we show title for each raster plot? [1]
	#       -onerow : boolean, should we display individual plots in a single row? [0]
	#                 if we split by second variable, then each value will have a row.
	#       -placefunc : if supplied, override default placement of plots using this func [""]
	#       -stims : index of stims to limit rasters to. by default will show all stims [""]
	#       NOT VALID NOW [-mintrials : minimum number of trials needed to plot [3]]
	#       -pre : time (in positive ms) to plot before align event [150]
	#       -post : time (in positive ms) to plot after align event [350]
	#       -function : function to use for sdf estimate, 'sdf' or 'parzen' [parzen]
	#       -res : resolution in ms for sdf/parzens [2]
	#       -sd : standard deviation in ms of sdf/parzens kernal [10]
	#       -nsd : number of standard devistions for sdf/parzen kernal [2.5]
	#       -topaxis : boolean, should we display top and right axis? [1]
	#       -newwin : boolean, should we put rasters in a new window? [0]
	#       -newtab : boolean, should we put rasters in a new tab? [0]
	#       -clearwin : boolean, should we clear the display first? [1]
	#       -ymax : maximum yval (useful to normalize split plots) [estimated]
	#       -addinfo : boolean, should we add info about this call [1]
	#       -limit : boolean list of trials to use for plotting rasters.  if blank, use all data. [""]
	# %END%

	# reset default values
	helprasts::reset_defaults

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
 		    showstims -
		    showtitle -
		    onerow -
		    topaxis -
		    newwin -
		    newtab -
		    addinfo -
		    clearwin {
			# these are all booleans
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {$val != 0 && $val != 1 } {
			    error "::rasts::by - expect boolean (0 or 1) for $var"
			}
 			set ::helprasts::vars(${var}) $val
 		    }
		    limit {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {[dl_length $val] != [dl_length $g:obsid]} {
			    error "::rasts::by - length of limit list must be the same as number of obs in group $g"
			}
			set ::helprasts::vars(limit) $val
		    }
		    function {
			set v [lindex $args [incr i]]
			if {$v == "sdf" || $v == "sdfs" || $v == "dl_sdf" || $v == "dl_sdfs"} {
			    set function dl_sdfs
			} elseif { $v == "parzen" || $v == "parzens" || $v == "dl_parzen" || $v == "dl_parzens"} {
			    set function dl_parzens
			} else {
			    error "::rasts::by : value of -funciton must be 'sdf' or 'parzen'"
			}
		    }
		    placefunc {
			set v [lindex $args [incr i]]
			set ::helprasts::vars(placefunc) $v
		    }
		    stims -
		    mintrials -
		    pre -
		    post -
		    res -
		    sd -
		    nsd -
		    ymax {
			# just take next arg as value
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set ::helprasts::vars(${var}) $val
		    }
  		    default {
 			error "::rasts::by: invalid switch $a \n   must be 'showstims' 'showtitle' 'onerow' 'placefunc' 'stims' 'mintrials' 'pre' 'post' 'res' 'sd' 'topaxis' 'newwin' 'newtab' 'clearwin' 'ymax' 'limit' 'addinfo'.\n   use ::rasts::by for more details."
 		    }
 		}
 	    }
 	}

	# check limit list
	if {$::helprasts::vars(limit) == ""} {
	    set glimit [dg_copy $g]
	} else {
	    set glimit [dg_copySelected $g $::helprasts::vars(limit)]
	}

	# are we sorting by 1 or 2 lists?
	if {[llength $sortby] > 2} {
	    error "::rasts::by - can't handle more than two sort variables at a time"
	}
	if {[llength $sortby] == 1} {
	    set split_sort_label ""
	    dl_local split_sort_vals [dl_append [dl_slist] {}]
	    set plot_sort_label [lindex $sortby 0]; # zeroth sort list defines each plot
	} else {
	    set split_sort_label [lindex $sortby 1]
	    dl_local split_sort_vals $glimit:$split_sort_label
	    set plot_sort_label [lindex $sortby 0]; # zeroth sort list defines each plot
	}

	# create new window/tab if desired
	if $helprasts::vars(newwin) newWindow
	if $helprasts::vars(newtab) newTab
	

	# clear display first?
	if $helprasts::vars(clearwin) clearwin

	# check channels
	if {$chans == "all"} {
	    set chans [dl_tcllist [dl_unique [dl_unpack $glimit:spk_channels]]]
	}

	# setup data used for spliting cgwin into ports
	dl_local yports [dl_reverse [dl_series 0 1 [expr 1./[dl_length [dl_unique $split_sort_vals]]]]]
	set plots ""
	set i -1

	# runs through all the plots once, to automatically establish overall ymax, if sorting by two labels and unspecified ymax
	if {$helprasts::vars(ymax) == ""} {
		foreach temp_split_val [dl_tcllist [dl_unique $split_sort_vals]] {
		    if {$temp_split_val != ""} {
			set title "$split_sort_label = $temp_split_val"
			dl_local thisref [dl_eq $glimit:$split_sort_label $temp_split_val]
		    } else {
			set title ""
			dl_local thisref [dl_ones [dl_length $glimit:obsid]]
		    }
		    # are there any trials to plot? (i don't think this should ever happen)
		    if ![dl_sum $thisref] continue
		    # copy what we need
		    set g2 [dg_copySelected $glimit $thisref]
		    if [dl_exists $g:spike_source] {
			dl_set $g2:spike_source $g:spike_source
		    } else {
			dl_set $g2:spike_source [dl_slist unknown]
		    }
			::helprasts::plot $g2 $chans $plot_sort_label $align
			dg_delete $g2
		}
		clearwin
	}

	foreach this_split_val [dl_tcllist [dl_unique $split_sort_vals]] {
	    if {$this_split_val != ""} {
		set title "$split_sort_label = $this_split_val"
		dl_local thisref [dl_eq $glimit:$split_sort_label $this_split_val]
	    } else {
		set title ""
		dl_local thisref [dl_ones [dl_length $glimit:obsid]]
	    }

	    # are there any trials to plot? (i don't think this should ever happen)
	    if ![dl_sum $thisref] continue

	    # copy what we need
	    set g2 [dg_copySelected $glimit $thisref]
	    if [dl_exists $g:spike_source] {
		dl_set $g2:spike_source $g:spike_source
	    } else {
		dl_set $g2:spike_source [dl_slist unknown]
	    }

	    # create the right view port
	    pushpviewport 0 [dl_get $yports [incr i]] 1 [dl_get $yports [expr $i+1]]
	    ::helprasts::plot $g2 $chans $plot_sort_label $align

	    # legend for each row
	    if {$title != ""} {
		set win [getwindow]; setwindow 0 0 1 1
		dlg_text .5 .97 $title -just 0 -spacing .015 -color $::colors(white) -size 18
		eval setwindow $win
	    }
	    # pop the view port
	    poppviewport
	    # cleanup
	    dg_delete $g2
	}
	# cleanup
	dg_delete $glimit

	if $helprasts::vars(addinfo) {
	    helprasts::mark_plots "::rasts::by $g $chans $sortby $align $args" $g
	}

	return 1
    }

    proc onset { g chans sortby args } {
	# %HELP%
	# ::rasts::onset <g> <chans> <sortby> <align> ?options...?
	#    display raster plots for group <g> using channels <chans> aligned to stimon.
	#    we are really just calling ::rasts::by with the arguments supplied and aligning to stimon.
	#    use ::rasts::by for more details and options
	# %END%
	eval rasts::by $g \$chans \$sortby stimon $args
    }
     proc acquired { g chans sortby args } {
	 # %HELP%
	 # ::rasts::response <g> <chans> <sortby> <align> ?options...?
	 #    display raster plots for group <g> using channels <chans> aligned to response.
	 #    we are really just calling ::rasts::by with the arguments supplied and aligning to
	 #    response and using a pretime of 400 and a posttime of 400.	 
	 #    use ::rasts::by for more details and options
	 # %END%
	 # align to acquired and adjust the pre/post times
	 eval ::rasts::by $g \$chans \$sortby acquired $args -pre 400 -post 400;# -burst_pre 400 -burst_post 400
     }
     proc response { g chans sortby args } {
	 # %HELP%
	 # ::rasts::response <g> <chans> <sortby> <align> ?options...?
	 #    display raster plots for group <g> using channels <chans> aligned to response.
	 #    we are really just calling ::rasts::by with the arguments supplied and aligning to
	 #    response and using a pretime of 400 and a posttime of 400.	 
	 #    use ::rasts::by for more details and options
	 # %END%
	 eval ::rasts::by $g \$chans \$sortby response $args -pre 400 -post 400;# -burst_pre 650 -burst_post 0
     }
}

namespace eval ::helprasts {
#    set cols(0) [dlg_rgbcolor 102 255 102]; # green
#    set cols(1) [dlg_rgbcolor 255 255 102]; # yellow
#    set cols(2) [dlg_rgbcolor 102 204 255]; # cyan
#    set cols(3) [dlg_rgbcolor 255 102 204]; # magenta
#    set cols(4) [dlg_rgbcolor 255 51 51];   # red
#    set cols(99) [dlg_rgbcolor 108 108 108];# gray
    set cols(0) $::colors(green)
    set cols(1) $::colors(red)
    set cols(2) $::colors(cyan)
    set cols(3) $::colors(magenta)
    set cols(4) $::colors(yellow)
    set cols(5) $::colors(blue)
    set cols(6) $::colors(light_blue)
#    set cols(99) $::colors(gray)

    proc reset_defaults { } {
	# array to hold all info needed to plot rasters (changable through option tags)
	array set ::helprasts::vars ""

	set ::helprasts::vars(showstims)   1;  # show images of stimuli above rasters?
	set ::helprasts::vars(showtitle)   1; # should we show title for each raster plot?
	set ::helprasts::vars(onerow)      0;  # plot all rasters in a given sort in a single row?
	set ::helprasts::vars(placefunc)   ""; # func to use to place plots
	set ::helprasts::vars(stims)       ""; # list of indexes to use to limit rasters displayed

	set ::helprasts::vars(topaxis)     1; # should we show top and right axis?
	set ::helprasts::vars(newwin)      0; # should we display rasts in new window?
	set ::helprasts::vars(newtab)      0; # should we display rasts in new tab?
	set ::helprasts::vars(clearwin)    1; # should we clear the display first?

	set ::helprasts::vars(mintrials)   3; # minimum number of trial reps needed before we plot for a given stim
	set ::helprasts::vars(addinfo)     1; # should we add info to plot
	set ::helprasts::vars(limit)       ""; # boolean list of trials to use for plotting rasters

	set ::helprasts::vars(pre) 150
	set ::helprasts::vars(post) 350
	set ::helprasts::vars(ymax) ""
	set ::helprasts::vars(biggestymax) 0
	
	set ::helprasts::vars(find_bursts) 0
	set ::helprasts::vars(burst_pre)   0
	set ::helprasts::vars(burst_post)  450
	set ::helprasts::vars(burst_n)     3
	set ::helprasts::vars(burst_ms)    50.

	set ::helprasts::vars(function) dl_parzens; # function to use for sdf estimate
	set ::helprasts::vars(res) 2; # resolution in ms for sdf/parzens function
	set ::helprasts::vars(sd)  10; # standard deviation in ms of sdf/parzens kernal
	set ::helprasts::vars(nsd) 2.5; # number of standard devistion for sdf/parzens kernal
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

    proc add_sdfs { g chan align } {
	dl_local spikes [dl_select $g:spk_times [dl_eq $g:spk_channels $chan]]
	dl_local spikes [dl_sub $spikes $g:$align]

	# Align based on bursts...
	if { $helprasts::vars(find_bursts) } {
	    dl_local bursts [spikes::find_bursts $spikes -$helprasts::vars(burst_pre) $helprasts::vars(burst_post) $helprasts::vars(burst_n) $helprasts::vars(burst_ms)]
	    dl_local first_burst [dl_select $bursts [dl_firstPos $bursts]]
	    dl_local spikes [dl_sub $spikes $first_burst]
	    dl_set $g:abursts $bursts
	    dl_set $g:lbursts [dl_add $first_burst $g:$align]
	}
	dl_local spikes [dl_select $spikes [dl_between $spikes [expr -1*$helprasts::vars(pre)] $helprasts::vars(post)]]
	dl_set $g:lspikes_$chan $spikes
	dl_set $g:lparzens_$chan [dl_mult [eval $helprasts::vars(function) $spikes [expr -1*$helprasts::vars(pre)] $helprasts::vars(post) $helprasts::vars(sd) $helprasts::vars(nsd) $helprasts::vars(res)] 1000.]
    }

    proc plot { g chans sortby align } {
	set bgcolor 255

	# are we going to show stims? only if we can and we are asking for them
	if { $::tcl_platform(platform) != "windows" || $::helprasts::vars(showstims) == 0 } {
	    set showstims 0
	} elseif { $::helprasts::vars(showstims) == 1 &&  ( $sortby == "image" || $sortby == "stim_names" ) } {
	    set showstims 1
	} else {
	    set showstims 0
	}
	
	pushpviewport 0 0.05 1 0.95

#	dl_local channels [dl_ilist]
	dl_local all_m [dl_llist]
	dl_local all_sorteds [dl_llist]

	# if no specific stims asked for, show them all
	if { $helprasts::vars(stims) == "" } {
	    dl_local stim_idxs [dl_index [dl_unique $g:$sortby]]
	} else {
	    dl_local stim_idxs $helprasts::vars(stims)
	}
	# titles for each plot
	if { [dl_datatype $g:$sortby] == "string" } {
	    dl_local titles [dl_choose [dl_filebase [dl_unique $g:$sortby]] $stim_idxs]
	} else {
	    dl_local titles [dl_choose [dl_unique $g:$sortby] $stim_idxs]
	}
	
	foreach chan $chans {
	    add_sdfs $g $chan $align
	    dl_local sortedp [dl_sortByList $g:lparzens_$chan $g:$sortby]
	    dl_local m [dl_choose [dl_means $sortedp] $stim_idxs]
	    dl_local sorteds [dl_choose [dl_sortByList $g:lspikes_$chan $g:$sortby] $stim_idxs]
	    
#	    # Only select stimuli with at least mintrials of data
#	    dl_local enough_data [dl_gt [dl_lengths $sorteds] $helprasts::vars(mintrials)]
#	    dl_local sorteds [dl_select $sorteds $enough_data]
#	    dl_local m [dl_select $m $enough_data]

	    dl_local sorteds$chan $sorteds
	    dl_local m$chan $m
	    dl_concat $all_m $m
	    dl_concat $all_sorteds $sorteds
	}

	if ![dl_length $m] {
	    error "::helprasts::plot - no data to plot.  may want to decrease mintrials using -mintrials options"
	}

	# set up one plot and keep reseting it before we actually draw it to screen
	set p [dlp_newplot]
	dlp_set $p xplaces [dl_fromto [expr -1*$helprasts::vars(pre)] $helprasts::vars(post) [expr abs($helprasts::vars(pre))]]
	dlp_addXData $p [dl_series [expr -1*$helprasts::vars(pre)] $helprasts::vars(post) $helprasts::vars(res)]

	if { $showstims } { dlp_set $p vscale 0.43 }
	set nplots [dl_length $titles]

	# one row, box em up or user defined?
	if { $helprasts::vars(placefunc) != "" } {
	    # use whatever is the current viewport
	} elseif { $helprasts::vars(onerow) } { 
	    dlp_setpanels 1 $nplots 
	} else {
	    if { $nplots <= 4 } {
		dlp_setpanels 2 2
	    } elseif { $nplots <= 8 } {
		dlp_setpanels 2 4
	    } else {
		set nrows [expr ($nplots/8)+1]
		dlp_setpanels $nrows [expr ($nplots+$nrows-1)/$nrows]
	    }
	}

	# set yaxis max
	if {$::helprasts::vars(ymax) != ""} {
	    set yaxismax $::helprasts::vars(ymax)
		dlp_setyrange $p 0 $yaxismax
	} else {
	    set yaxismax [expr 1.8 * [dl_max $all_m]]
	    if {$yaxismax > $::helprasts::vars(biggestymax)} {
			set ::helprasts::vars(biggestymax) $yaxismax
		}
		dlp_setyrange $p 0 $::helprasts::vars(biggestymax)
	}

	# and for rasters
	set ymax [expr 3.5 * [dl_max [dl_lengths $all_sorteds]]]; # stretch for rasters
	for { set i 0 } { $i < [dl_length [set m[dl_first $chans]]] } { incr i } {
	    if { $showstims } { ipl destroyAll }
	    if { !$showstims && $helprasts::vars(showtitle) } {
		dlp_set $p title [dl_slist [dl_get $titles $i]]
	    }
	    if { $showstims } {
		set range [expr $helprasts::vars(post)+$helprasts::vars(pre)]
		set mid [expr ($range/2.)-$helprasts::vars(pre)]
		set wid [expr $range*0.35]
		set timage [dl_get $titles $i]
		set imgwidth 256
		set imgheight 256
		if { [regexp .*\.tga $timage] } {
		    load_Impro
		    dl_set $p:imgpix [img_read $timage]
		    scan [img_info $timage] "%d %d" imgwidth imgheight
		} elseif { [regexp comp_......_... $timage] } {
		    if { [package versions stimCompose] == "" } {
			lappend ::auto_path \
			    l:/projects/stimulator/classify/packages
		    }
		    package require stimCompose
		    set cmd [dl_get $g:stim_paths \
				 [dl_find $g:$sortby $timage]]
		    dl_set $p:imgpix [eval $cmd 1]
		} else {
		    set timage [file join \
				    [dl_get $g:stim_paths \
					 [dl_find $g:$sortby $timage]] \
				    "$timage.[dl_get $g:stim_extensions \
					 [dl_find $g:$sortby $timage]]"]
		    load_Impro
		    dl_set $p:imgpix [img_read $timage]
		    scan [img_info $timage] "%d %d" imgwidth imgheight
		}
		dlp_reset $p postcmds
		
		dlp_winypostcmd $p { 0 1 } \
		    "dlg_image  $mid 1.3 \[dl_llist \"$imgwidth $imgheight\" \$thisplot:imgpix\] $wid -center"
	    }
	    dlp_reset $p ydata
	    dlp_reset $p precmds
	    dlp_reset $p plotcmds

	    dlp_winycmd $p { 0 1 } {
		dlg_lines 0 "0 1" -linecolor $::colors(green) -lwidth 80
	    }

	    # add data for all channels
#	    # fills first
#	    foreach c $chans {
#		set y [dlp_addYData $p [set m${c}]:$i]
#		dlp_draw $p lines $y -fillcolor $::colors(red) -linecolor $::colors(gray)
#	    }

	    # lines and rasters
	    set markedtrials 0
	    set y 0
	    if {[llength $chans] == 1} {
#		set fill "-fillcolor $::colors(gray)"
		set fill "-fillcolor [dlg_rgbcolor 100 100 100]"
	    } else {
		set fill ""
	    }
	    foreach c $chans {
		set y [dlp_addYData $p [set m${c}]:$i]

		if {[dl_first $g:spike_source] == "online_spikes" || [dl_first $g:spike_source] == "unknown"} {
		    # online spikes are labeled by channel already
		    set cidx $c
		} else {
		    # then they were probably cut, so try to guess channel
		    # changing the commented line below to color multiple spikes on one channel differently
		    # set cidx [expr $c / 100]
		    set cidx [expr [lsearch $chans $c] % 7]
		}

		eval dlp_draw $p lines $y -linecolor $::helprasts::cols(${cidx}) -lwidth 250 $fill

		dl_set $p:xrasts$c [dl_collapse [set sorteds$c]:$i]
		dl_set $p:yrasts$c [dl_repeat [dl_fromto 0 [dl_length [set sorteds$c]:$i]] [dl_lengths [set sorteds$c]:$i]]
		dlp_winycmd $p "$ymax -1" "dlg_markers %p:xrasts$c %p:yrasts$c -marker vtick -size 1 -scale y -lwidth 40 -color $::helprasts::cols($cidx)"

		if !$markedtrials {
		    dl_set $p:rpos [dl_fromto 0 [dl_length [set sorteds$c]:$i]]
		    dlp_winycmd $p "$ymax -1" "dlg_markers -50 %p:rpos -marker htick -size 12 -scale x -lwidth 140 -color $::colors(cyan)"
		    set markedtrials 1
		}
		
		# find max raster tick
		set y [dl_tcllist [dl_max $y [expr [dl_length [set sorteds$c]:$i] + 1]]]
	    }

	    # box off rasters using max trial
	    dlp_wincmd $p "0 $ymax 1 -1" "dlg_lines [dl_flist 0 1]  [dl_flist $y $y] -linecolor $::colors(white)"
	    
	    dlp_setxrange $p [expr -1*$helprasts::vars(pre)] $helprasts::vars(post)

	    # display options
	    if { !$helprasts::vars(topaxis) } {
		dlp_set $p x2axis 0
		dlp_set $p y2axis 0
	    }

	    # use default subplot placement or override with user func
	    if { $::helprasts::vars(placefunc) == "" } {
		dlp_subplot $p $i
	    } else {
		# Window should come back as "$x0 $y0 $x1 $y1"
		set window [$::helprasts::vars(placefunc) $i $nplots $g]
		pushviewport
		eval setpviewport $window
		if { [catch { dlp_plot $p } result] } {
		    popviewport
		    error $result
		}
		popviewport
	    }
	}

	# standard legend
	set win [getwindow]
	setwindow 0 0 1 1
	
	set n [expr [llength $chans]+1]; # need space to label as chans
	dl_local x [dl_add [dl_div [dl_fromto 0 $n] 20.] [expr .5 - $n/2.*.05]]
	dl_local y [dl_repeat .01 $n]
	dl_local t [eval dl_slist channels $chans]
	dl_local c [dl_ilist $::colors(white)]
	foreach chan $chans {
	    if {[dl_first $g:spike_source] == "online_spikes" || [dl_first $g:spike_source] == "unknown"} {
		# online spikes are labeled by channel already
		set cidx $chan
	    } else {
		# then they were probably cut, so try to guess channel
		# again, changing this for version 2.1
		# set cidx [expr $chan / 100]
		set cidx [expr [lsearch $chans $chan] % 7]
	    }
	    dl_append $c $::helprasts::cols(${cidx})
	}
	dlg_text $x $y $t -just 0 -spacing .015 -colors $c -size 12.
	eval setwindow $win
	
	poppviewport
	dg_delete $p
    }

    # example of a place func that just puts plots in one row (just like onerow)
    proc place_linear { id nplots g } {
	set ncols $nplots
	if { [expr ($id >= $ncols)] } {
	    error "place_linear: invalid panel selected"
	}
	set col [expr $id%$ncols]
	set width [expr 1.0/$ncols]
	set height 1.0
	set row [expr $id/$ncols]
	set x0 [expr ($col*$width)]
	set x1 [expr ($col+1)*$width]
	set y0 [expr 1.0-($row*$height)]
	set y1 [expr 1.0-(($row+1)*$height)]

	return "$x0 $y0 $x1 $y1"
    }

    proc place_octagon { id nplots g } {
	set ncols $nplots
	if { [expr ($id >= $ncols)] } {
	    error "place_linear: invalid panel selected"
	}
	set col [expr $id%$ncols]

	set width .2;
	set ofs .4;

	if {$id>3} { 
	    set ofs [expr $ofs+.08]
	}

	set ofy .45;
	set height .25

	set d .35
	set i $id

	set h [expr $d*(sin($::pi/8 + $i*($::pi/4)))]; #Formula for Octagon - 1st Coordinate
	set w [expr $d*(cos($::pi/8 + $i*($::pi/4)))]; #Formula for Octagon - 2nd Coordinate

	set x0 [expr $h+$ofs];
	set x1 [expr $h+$width+$ofs]
	set y0 [expr $w+$ofy]
	set y1 [expr ($w+$height)+$ofy]

	return "$x0 $y0 $x1 $y1"
    }




}