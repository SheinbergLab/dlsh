# AUTHORS
#   DLS, REHBM
#
# DESCRIPTION
#    this package provides basic tools for graphing datasets
#    all functions are provided under the ::gr namespace.  all helper
#       functions (called by ::gr procs, but not directly by user)
#       are provided under the ::helpgr namespace.  this will help users
#       find appropriate procs to call from the command line.
#    all procs should contain a %HELP% section that can be parsed by the
#       'use' proc.
#
# TODO
#   Ability to get back the group created as easy data source - or to dg_view it for debugging
#
#   Histogram plots.
#          better nbins guessing. Arbitraryily goes with 20 bins as default
#
#   barchart - should probably use -offset and -interbar to create bars at correct spot.  this will make veridical_x a lot easier because xs can just be $g:sort0
#
#   -veridical_x doesn't really work properly
#
#   New plot type, Density Plot? Plot each datapoint with some x-jitter, but still grouped by tags.
#      This is essentially boxwhisker with -function median -linemin 50 -linemax 50 -boxmin 50 -boxmax 50 -plot_outliers 1

package provide graphing 2.0

namespace eval ::gr {
    proc demo { } {
	# %HELP%
	# ::gr::demo
	#    Run a demonstration of the graphing procedures
	#    offered in the graphing package.
	# %END%	

	# Demo best viewed at max cgraph size for readability.
	wm state . zoomed
	focus .console
	focus .console.tab1

	puts "*****Graphing Package Demo*****"
	puts " This demo will open up newTabs and create a series of plots.\n For this demo, each plot's title is the command used to create the plot."

	puts " set g \[load_data s_dfR-016_051206*\]\n"
	set g [load_data -verbose 0 -adfspkdir none -em 0 s_dfR-016_051206*]
	
	set TABIDX 0

	puts " Tab [incr TABIDX] - Basic Histrograms"
	newTab "$TABIDX. Basic Histograms"
	dlp_setpanels 2 2
	set i -1
	foreach cmd {
	    {::gr::histogram $g:rts}
	    {::gr::histogram $g:rts -max 1000}
	    {::gr::histogram $g:rts -max 1000 -proportion 1 -ymax .2}
	    {::gr::histogram $g:rts -max 1000 -nbins 10}
	} {
	    set p [eval $cmd -draw 0]
	    dlp_set $p title "$cmd"
	    dlp_subplot $p [incr i]
	    dg_delete $p
	}

	puts " Tab [incr TABIDX] - Sorted Histrograms"
	newTab "$TABIDX. Sorted Histograms"
	dlp_setpanels 2 2
	set i -1
	foreach cmd {
	    {::gr::histogram $g:rts $g:targ_ecc -nbins 5 -max 1000 -split interleave}
	    {::gr::histogram $g:rts $g:side -max 1000 -split split}
	    {::gr::histogram $g:rts $g:targ_ecc -binwidth 50 -max 1000 -split stack}
	} {
	    set p [eval $cmd -draw 0]
	    dlp_set $p title "$cmd"
	    dlp_subplot $p [incr i]
	    dg_delete $p
	}

	puts " Tab [incr TABIDX] - Cumulative Distibutions"
	newTab "$TABIDX. Cumulative Dists"
	dlp_setpanels 2 2
	set i -1
	    #{::gr::cumdist [dl_zrand 100] [dl_replicate "0 1" 50]}
	    #{::gr::cumdist [dl_zrand 100] [dl_replicate "0 1" 50] -xmin -1 -xmax 1}
	foreach cmd {
	    {::gr::cumdist $g:rts $g:targ_ecc}
	    {::gr::cumdist [dl_combine [dl_zrand 200] [dl_urand 200]] [dl_repeat [dl_slist gaussian uniform] 200]}
	    {::gr::cumdist $g:rts $g:targ_ecc -min 500 -max 1000}
	    {::gr::cumdist $g:rts $g:targ_ecc -xmin 500 -xmax 1000}
	} {
	    set p [eval $cmd -draw 0]
	    dlp_set $p title "$cmd"
	    dlp_subplot $p [incr i]
	    dg_delete $p
	}

	puts " Tab [incr TABIDX] - Three Types of Plots"
	newTab "$TABIDX. Three Types"
	dlp_setpanels 2 2
	set i -1
	foreach cmd {
	    {::gr::barchart $g:rts "$g:targ_ecc $g:side" -showcount 1}
	    {::gr::boxwhisker $g:rts "$g:targ_ecc $g:side" -function median}
	    {::gr::lineplot $g:rts "$g:targ_ecc $g:side" -errfunc 95ci -stagger 1}
	} {
	    set p [eval $cmd -draw 0]
	    dlp_set $p title "$cmd"
	    dlp_subplot $p [incr i]
	    dg_delete $p
	}

	puts " Tab [incr TABIDX] - Some Shared Options"
	newTab "$TABIDX. Shared Options"
	dlp_setpanels 2 2
	set i -1
	# {::gr::barchart $g:rts "$g:targ_ecc $g:side" -limit [dl_oneof $g:targ_ecc "2. 4. 8."] -veridical_x 1}
	foreach cmd {
	    {::gr::barchart %g:rts "%g:side %g:targ_ecc" -group $g}
	    {::gr::barchart $g:rts "$g:side $g:targ_ecc" -limit [dl_oneof $g:targ_ecc "2. 4. 8."]}
	    {::gr::barchart $g:rts "$g:side $g:targ_ecc" -palette {"0 153 0" "153 0 153"}}

	} {
	    set p [eval $cmd -draw 0]
	    dlp_set $p title "$cmd"
	    dlp_subplot $p [incr i]
	    dg_delete $p
	}

	puts " Tab [incr TABIDX] - Three-Way Sort"
	newTab "$TABIDX. Three-Way Sort"
	set cmd {::gr::barchart $g:rts "$g:targ_ecc $g:targ $g:description"}
	puts "  Command: $cmd"
	pushpviewport 0 0 1 .95
	set plots [eval $cmd -clearwin 0]
	poppviewport
	set win [getwindow]
	setwindow 0 0 1 1
	dlg_text .5 .97 $cmd -size 12
	eval setwindow $win
	eval dg_delete $plots
	
	puts " Tab [incr TABIDX] - Other Interesting Plot"
	newTab "$TABIDX. Other Plots"
	dlp_setpanels 2 3
	set i 0
	foreach cmd {
	    {::gr::boxwhisker $g:rts $g:targ_ecc -function median -linemin 50 -linemax 50 -boxmin 50 -boxmax 50 -plot_outliers 1}
	} {
	    set p [eval $cmd -draw 0]
	    dlp_set $p title "$cmd"
	    dlp_subplot $p [incr i]
	    dg_delete $p
	}
	
	puts " Tab [incr TABIDX] - Scatter Plot"
	puts "   Using fake spike rate data..."
	dl_set $g:fake_spkrate [dl_abs [dl_add [dl_div $g:rts 10.] [dl_mult 50 [dl_zrand [dl_length $g:rts]]]]]
	newTab "$TABIDX. Scatter Plot"
	dlp_setpanels 2 2
	set i -1
	foreach cmd {
	    {::gr::xyscatter $g:rts $g:fake_spkrate}
	    {::gr::xyscatter $g:rts $g:fake_spkrate -line lsfit}
	    {::gr::xyscatter $g:rts $g:fake_spkrate -limit [dl_lt $g:rts 2000] -line lsfit}
	    {::gr::xyscatter $g:rts $g:fake_spkrate -limit [dl_lt $g:rts 2000] -line x=y -symmetric 1}
	} {
	    set p [eval $cmd -draw 0]
	    dlp_set $p title "$cmd"
	    dlp_subplot $p [incr i]
	    dg_delete $p
	}

	puts "\n For more details on each procedure try the 'use' proc, e.g.:\n   use ::gr::boxwhisker"

	# clean up
	    #dg_delete $g
	
	return "Enjoy..."
    }

    proc cumdist { vals {tags ""} args } {
	# %HELP%
	# ::gr::cumdist data(0) vals ?tags? ?options...?
	#    Creates a cumulative distribution plot of vals.
	#    If tags are supplied, then the cumulative distributions will be split by tags.
	#    If more than one tags is supplied, they are plotted in seperate
	#    plots. If tags is "", then all the data is averaged together.
	#
	#    Options to include at end of proc; format: -option option_value
	#    Default value is shown in []
	#       -min : min value of data (vals) used. This datapoint is forced to be the 0th percentile. ['xdata minimum]']
	#       -max : max value of data (vals) used. This datapoint is forced to be the 100th percentile. ['xdata maximum]'
	#       -xmin : min value for drawing xaxis ['guessed']
	#       -xmin : max value for drawing xaxis ['guessed']
	#       -style : 'direct' or 'step', defining how the line is drawn ['step']
	#       -group : supply a group that will be used to locate any lists in vals or
	#                tags that start with '%g'
	#       -newwin : boolean, should we put in a newWindow? [0]
	#       -newtab : boolean, should we put in a newTab? [0]
	#       -limit : boolean list to use as a pre-selection for all vals and tags.  if blank, use all. [""]
	#       -draw : draw plot to cgwin? set to 0 if you just want the plot(s) list back for future plotting.
	#               if zero, clearwin will also be set to 0 [1]
	#       -clearwin : should we clear the plotting win? [1]
	#       -palette : 'color', 'grayscale', or two lists of RGB values for min and max color range (i.e. -palette {"0 153 0" "153 0 153"}) [color]
	# %END%

	# set DEFAULTS
	set max "";
	set min "";
	set newwin 0; # plot in new window?
	set newtab 0; # plot in new tab?
	set limit ""; # limit plot to certain trials?
	set draw 1; # by default, we will draw plot to screen
	set clearwin 1; # by default, clear the window
	set ::helpgr::palette color; # should we draw in color or grayscale (grayscale better for many boxes)
	set xmin ""; # min value for drawing xaxis
	set xmax ""; # max value for drawing xaxis
	set style "step"; # defines how the line is drawn

	# account for the possibility that someone wanted to use switches without using tags
	if {[string index $tags 0] == "-"} {
	    set args "$tags $args"
	    set tags ""
	}

	# parse switches
	for {set i 0} {$i < [llength $args]} {incr i} {
	    set a [lindex $args $i]

	    if {[string range $a 0 0] != "-"} {
		break
	    } elseif {[string range $a 0 1] == "--"} {
		break
	    } elseif {[string range $a 0 0] == "-"} {
		set have_data 1

		# below we define any tags that will be used
		switch -exact [string range $a 1 end] {
		    xmin -
		    xmax -
		    min -
		    max {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set $var $val
		    }
		    clearwin -
		    draw -
		    newwin -
		    newtab {
			# these are all booleans
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {$val != 0 && $val != 1 } {
			    error "::graphing::cumdist - expect boolean (0 or 1) for $var"
			}
 			set $var $val

			# if we are not drawing, then don't clear the window
			if {$var == "draw" && !$val} { set clearwin 0 }
		    }
		    group {
			set g [lindex $args [incr i]]
			set vals [regsub -all -- "%g" "$vals" "$g"]
			set tags [regsub -all -- "%g" "$tags" "$g"]
		    }
		    limit {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set limit $val
		    }
		    palette {
			set v [lindex $args [incr i]]
#			if {$v != "color" && $v != "grayscale" } {
#			    error "::gr::boxwhisker : value of -palette must be 'color' or 'grayscale'"
#			}
			set ::helpgr::palette $v
		    }
		    style {
			set v [lindex $args [incr i]]
			if {$v != "step" && $v != "direct"} {
			    error "::gr::cumdist : value of -style must be 'step' or 'direct'"
			}
			set style $v
		    }
		    default {
			error "::gr::cumdist: invalid switch $a \n   must be 'min' 'max' 'xmin' 'xmax' 'group' 'newwin' 'newtab' 'limit' 'draw' 'clearwin' 'palette'.\n   use ::gr::cumdist for more details."
		    }
		}
	    }
	}

	# use all the data if there are no sort tags
	if {$tags == ""} {
	    set tags "all_data"
	    dl_set all_data [dl_ones [dl_length $vals]]
	}

	# in case we limit trials, we want to know what the original vals and tags were called for x and y titles
	set val_title $vals
	set tag_title $tags

	# limit trials if we want (now that -group has been accounted for)
	if {$limit != ""} {
	    if {[dl_length $limit] != [dl_length $vals]} {
		error "::gr::boxwhisker - length of limit list must be the same as length of vals"
	    }
	    dl_local vals [dl_select $vals $limit]
	    dl_local t [dl_llist]
	    set tnames ""
	    set i -1
	    foreach tag $tags {
		dl_append $t [dl_select $tag $limit]
		lappend tnames $t:[incr i]
	    }
	    set tags $tnames
	}

	# make sure there is data to plot
	if ![dl_length $vals] {
	    error "::gr::cumdist - no data to plot"
	}

	# check tags
	if ![llength $tags] {
	    error "::gr::cumdist - cannot sort by an empty variable"
	} elseif {[llength $tags] > 3} {
	    error "::gr::cumdist - cannot sort by more than 2 variables"
	}

	# general info
	set g [dg_create]
	dl_set $g:vals [eval dl_slist $val_title]
	dl_set $g:tags [eval dl_slist $tag_title]

	# get all information we could use on this data
	dl_local info [eval dl_uniqueCross $tags]
	for {set i 0} {$i < [dl_length $info]} {incr i} {
	    dl_set $g:sort$i $info:$i
	}
	dl_set $g:data [eval dl_sortByLists $vals $tags]

	# data must be sorted (low to high) for cumulative distribution plot.
	dl_set $g:data [dl_sort $g:data]

	# make sure there is data to plot
	if ![dl_length $g:data] {
	    error "::gr::cumdist - no data to plot"
	}

	# plotting options
	if {$min == ""} {
	    # use all the data we have (pad to avoid rounding errors for floats)
	    set min [expr [dl_min $g:data] - .000001]
	}
	if {$max == ""} {
	    # use all the data we have (pad to avoid rounding errors for floats)
	    set max [expr [dl_max $g:data] + .000001]
	}
	foreach dl {min max} {
	    dl_set $g:$dl [set $dl]
	}

	# select data range
	dl_set $g:data_used [dl_select $g:data [dl_betweenEq $g:data $min $max]]

	# cumulative distribution on data
	dl_set $g:proportion [dl_div [dl_add [dl_index $g:data_used] 1] [dl_float [dl_lengths $g:data_used]]]

	# newwindow?
	if $newwin newWindow
	if $newtab newTab
	if $clearwin clearwin

	# for xaxis
	set range [expr $max - $min]
	set pad [expr $range * .1]; # pad axis with 10% of total range
	if {$xmin == ""} {
	    dl_set $g:xmin [expr $min - $pad]
	} else {
	    dl_set $g:xmin $xmin
	}
	if {$xmax == ""} {
	    dl_set $g:xmax [expr $max + $pad]
	} else {
	    dl_set $g:xmax $xmax
	}
	# y axis is always proportion
	dl_set $g:ymin -.05
	dl_set $g:ymax 1.05

	dl_set $g:style [dl_slist $style]

	set plots [::helpgr::plot_cumdist $g $draw]
	if $draw {
	    ::helpgr::mark_plots "::gr::cumdist $vals $tags $args"
	}

	#cleanup
	dg_delete $g; # could keep around and return as easy data source
#	return "$g - $plots"
	return $plots

    }

    proc histogram { vals {tags ""} args } {
	# %HELP%
	# ::gr::histogram data(0) vals ?tags? ?options...?
	#    Creates a histogram plot of vals.
	#    If tags are supplied, then the histogram will be split by tags.
	#    If more than one tags is supplied, they are plotted in seperate
	#    plots. If tags is "", then all the data is averaged together.
	#
	#    Options to include at end of proc; format: -option option_value
	#    Default value is shown in []
	#       -binwidth : (approximate) width of bins in xaxis units (same as vals) ['guessed']
	#       -nbins : number of bins to use.  takes presidence over binwidth ['20']
	#       -min : min value of x-axis and lowest bin ['guessed']
	#       -max : max value of x-axis and highest bin ['guessed']
	#       -split : how to split up within-group sort.  'interleave', 'stack' or 'split' (for one or two withing-group sort values) ['interleave']
	#       -group : supply a group that will be used to locate any lists in vals or
	#                tags that start with '%g'
	#       -proportion : convert y axis to proportion of data? [0]
	#       -newwin : boolean, should we put in a newWindow? [0]
	#       -newtab : boolean, should we put in a newTab? [0]
	#       -limit : boolean list to use as a pre-selection for all vals and tags.  if blank, use all. [""]
	#       -draw : draw plot to cgwin? set to 0 if you just want the plot(s) list back for future plotting. if zero, clearwin will also be set to 0 [1]
	#       -clearwin : should we clear the plotting win? [1]
	#       -palette : 'color', 'grayscale', or two lists of RGB values for min and max color range (i.e. -palette {"0 153 0" "153 0 153"}) [color]
	#       -ymin : minimum of yaxis [estimated from data]
	#       -ymax : maximum of yaxis [estimated from data]
	# %END%

	# set DEFAULTS
	set binwidth "";
	set nbins "";
	set max "";
	set min "";
	set split "interleave"; # default because it can handle any number of sort vals
	set proportion 0;
	set newwin 0; # plot in new window?
	set newtab 0; # plot in new tab?
	set limit ""; # limit plot to certain trials?
	set draw 1; # by default, we will draw plot to screen
	set clearwin 1; # by default, clear the window
	set ::helpgr::palette color; # should we draw in color or grayscale (grayscale better for many boxes)
	set ymin -9999; # null value used by match_yaxes_replot
	set ymax -9999;

	# account for the possibility that someone wanted to use switches without using tags
	if {[string index $tags 0] == "-"} {
	    set args "$tags $args"
	    set tags ""
	}

	# parse switches
# 	set have_data 0
# 	dl_local data [dl_llist]; # to collect all data sets
# 	dl_local dnames [dl_slist]; # for titles
	for {set i 0} {$i < [llength $args]} {incr i} {
	    set a [lindex $args $i]
#	    # check to see if this is a dataset
#	    if {[string range $a 0 0] != "-" && !$have_data } {
#		dl_append $data $a
#		dl_append $dnames $a
#	    }

	    if {[string range $a 0 0] != "-"} {
		break
	    } elseif {[string range $a 0 1] == "--"} {
		break
	    } elseif {[string range $a 0 0] == "-"} {
		set have_data 1

		# below we define any tags that will be used
		switch -exact [string range $a 1 end] {
		    binwidth -
		    nbins -
		    ymin -
		    ymax -
		    min -
		    max {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set $var $val
		    }
		    split {
			set v [lindex $args [incr i]]
			if {$v != "split" && $v != "interleave" && $v != "stack"} {
			    error "::gr::histogram : value of -split must be 'split', 'stack' or 'interleave'"
			}
			set split $v
		    }
		    proportion -
		    clearwin -
		    draw -
		    veridical_x -
		    newwin -
		    newtab {
			# these are all booleans
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {$val != 0 && $val != 1 } {
			    error "::graphing::histogram - expect boolean (0 or 1) for $var"
			}
 			set $var $val

			# if we are not drawing, then don't clear the window
			if {$var == "draw" && !$val} { set clearwin 0 }
		    }
		    group {
			set g [lindex $args [incr i]]
			set vals [regsub -all -- "%g" "$vals" "$g"]
			set tags [regsub -all -- "%g" "$tags" "$g"]
		    }
		    limit {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set limit $val
		    }
		    palette {
			set v [lindex $args [incr i]]
#			if {$v != "color" && $v != "grayscale" } {
#			    error "::gr::boxwhisker : value of -palette must be 'color' or 'grayscale'"
#			}
			set ::helpgr::palette $v
		    }
		    default {
			error "::gr::histogram: invalid switch $a \n   must be 'nbins' 'min' 'max' 'split' 'group' 'newwin' 'newtab' 'limit' 'draw' 'clearwin' 'palette'.\n   use ::gr::histogram for more details."
		    }
		}
	    }
	}
	# make sure $vals is not a list of strings
	if {[dl_datatype $vals] == "string"} {
	    error "::gr::histogram - invalid datatype for vals ([dl_datatype $vals])"
	}

	# use all the data if there are no sort tags
	if {$tags == ""} {
	    set tags "all_data"
	    dl_set all_data [dl_ones [dl_length $vals]]
	}

	# in case we limit trials, we want to know what the original vals and tags were called for x and y titles
	set val_title $vals
	set tag_title $tags

	# limit trials if we want (now that -group has been accounted for)
	if {$limit != ""} {
	    if {[dl_length $limit] != [dl_length $vals]} {
		error "::gr::boxwhisker - length of limit list must be the same as length of vals"
	    }
	    dl_local vals [dl_select $vals $limit]
	    dl_local t [dl_llist]
	    set tnames ""
	    set i -1
	    foreach tag $tags {
		dl_append $t [dl_select $tag $limit]
		lappend tnames $t:[incr i]
	    }
	    set tags $tnames
	}

	# make sure there is data to plot
	if ![dl_length $vals] {
	    error "::gr::histogram - no data to plot"
	}

	# check tags
	if ![llength $tags] {
	    error "::gr::histogram - cannot sort by an empty variable"
	} elseif {[llength $tags] > 2} {
	    error "::gr::histogram - cannot sort by more than 2 variables"
	}

	# general info
	set g [dg_create]
	dl_set $g:vals [eval dl_slist $val_title]
	dl_set $g:tags [eval dl_slist $tag_title]

	# get all information we could use on this data
	dl_local info [eval dl_uniqueCross $tags]
	for {set i 0} {$i < [dl_length $info]} {incr i} {
	    dl_set $g:sort$i $info:$i
	}
	dl_set $g:data [eval dl_sortByLists $vals $tags]

	# make sure there is data to plot
	if ![dl_length $g:data] {
	    error "::gr::histogram - no data to plot"
	}

	# plotting options
	if {$min == ""} {
	    # use all the data we have
	    set min [dl_min $g:data]

	}
	if {$max == ""} {
	    # use all the data we have
	    set max [dl_max $g:data]

	}

	if {$nbins == ""} {
	    if {$binwidth == ""} {
#		# try to get 10 per bin?
#		set nbins [expr int([dl_mean [dl_lengths $g:data]]) /  10]; # can this be calculated better?
		set nbins 20; # this is very arbitrary.  not sure what is best here
	    } else {
		# set by binwidth (at least approximately)
		set range [expr int($max - $min)]; # range MUST be integer for % to work
		set i 0
		set step .5
 		while {[expr $range % $binwidth] != 0} {
 		    if {[incr i] > 100} {error "infinate loop?"}
 		    # if we can't fit bins evenly lets increase range by 1
 		    set min [expr $min-$step]
 		    set max [expr $max+$step]
 		    set range [expr int($max - $min)]; # range MUST be integer for '%' operator in while expression to work
#puts "$min $max      $range      $binwidth      [expr int($range) % $binwidth]"
 		}
		set nbins [expr $range / $binwidth]
	    }
	}

	# need to pad min and max so that extreme values get used since dl_bins and dl_hist seem to use dl_between not dl_betweenEq
	# best to pad by one bin width (split across both sides)
	set bin [expr ($max - $min) / (1.0 * $nbins)]
	set min [expr $min - ($bin/2.)]
	set max [expr $max + ($bin/2.)]

	foreach dl {min max nbins} {
	    dl_set $g:$dl [set $dl]
	}
	dl_set $g:split [dl_slist $split]
	dl_set $g:proportion [dl_ilist $proportion]

	# figure out bins and hists
	dl_set $g:bins [dl_bins $min $max $nbins]
	dl_set $g:hist [dl_hists $g:data $min $max $nbins]

	# convert to proportion?
	# NOT SURE IF THIS SHOULD ONLY ACCOUNT FOR DATA BETWEEN MIN AND MAX, OR NORMALIZE BY ALL DATA
	if $proportion {
	    dl_set $g:hist [dl_div $g:hist [dl_length $vals].]
	}
	
	# newwindow?
	if $newwin newWindow
	if $newtab newTab
	if $clearwin clearwin

	# for yaxis
	dl_set $g:ymin $ymin
	dl_set $g:ymax $ymax

	set plots [::helpgr::plot_histogram $g $draw]
	if $draw {
	    ::helpgr::mark_plots "::gr::histogram $vals $tags $args"
	}

	#cleanup
	dg_delete $g; # could keep around and return as easy data source
#	return "$g - $plots"
	return $plots

    }

    proc boxwhisker { vals tags args} {
	# %HELP%
	# ::gr::boxwhisker vals tags ?options...?
	#    Creates a box and whisker plot of data in $vals sorted by values in $tags.
	#    The structure of $vals and $tags is similar to what you would use for
	#    dl_sortedFunc. You are limited to 0-3 sort lists.  If tags is "", then all
	#    the data is averaged together.
	#
	#    Options to include at end of proc; format: -option option_value
	#    Default value is shown in []
	#       -function : 'means' or 'medians' [medians]
	#       -linemax : percentile of data marked by maximum of line [90]
	#       -linemin : percentile of data marked by minimum of line [10]
	#       -boxmin : percentile of data marked by maximum of box [75]
	#       -boxmix : percentile of data marked by minimum of box [25]
	#       -veridical_x : '1' to plot x-axis with a true dimension [0]
	#       -plot_outliers : '1' to plot outliers (data above/below line min/max) [0]
	#       -group : supply a group that will be used to locate any lists in vals or
	#                tags that start with '%g'
	#       -newwin : boolean, should we put in a newWindow? [0]
	#       -newtab : boolean, should we put in a newTab? [0]
	#       -limit : boolean list to use as a pre-selection for all vals and tags.  if blank, use all. [""]
	#       -draw : draw plot to cgwin? set to 0 if you just want the plot(s) list back for future plotting. if zero, clearwin will also be set to 0 [1]
	#       -clearwin : should we clearw plotting win? [1]
	#       -palette : 'color', 'grayscale', or two lists of RGB values for min and max color range (i.e. -palette {"0 153 0" "153 0 153"}) [color]
	#       -showcount : boolean, should we label boxes with number of datapoint? [0]
	#       -ymin : minimum of yaxis [calculated from data]
	#       -ymax : maximum of yaxis [calculated from data]
	#
	#  EXAMPLE
	#    dl_local v [dl_combine [dl_fromto 1 11] [dl_fromto 10 30 2] [dl_fromto 10 110 10]]
	#    dl_local t [dl_repeat "0 5 20" 10]
	#    ::gr::boxwhisker $v $t
	#    ::gr::boxwhisker $v $t -boxmin 20 -boxmax 80 -plot_outliers 1 -veridical_x 1]
	# %END%

	# set DEFAULTS
	set function dl_medians;
	set linemin 10; # min proportion of data marked by line
	set boxmin 25; # min proportion of data marked by box
	set boxmax 75; # max proportion of data marked by box
	set linemax 90; # max proportion of data marked by line
	set veridical_x 0; # plot x-axis with true dimension?
	set plot_outliers 0; # plot outliers?
	set newwin 0; # plot in new window?
	set newtab 0; # plot in new tab?
	set limit ""; # limit plot to certain trials?
	set draw 1; # should we draw plot to screen
	set clearwin 1; # should we clearw plotting win?
	set ::helpgr::palette color; # should we draw in color or grayscale (grayscale better for many boxes)
	set showcount 0; # don't show datapoint count
	set ymin -9999; # null value used by match_yaxes_replot
	set ymax -9999;

	# parse switches
	for {set i 0} {$i < [llength $args]} {incr i} {
	    set a [lindex $args $i]
	    if {[string range $a 0 0] != "-"} {
		break
	    } elseif {[string range $a 0 1] == "--"} {
		break
	    } else {
		# below we define any tags that will be used
		switch -exact [string range $a 1 end] {
		    function {
			set v [lindex $args [incr i]]
			if {$v == "means" || $v == "mean" || $v == "dl_mean" || $v == "dl_means"} {
			    set function dl_means
			} elseif { $v == "medians" || $v == "median" || $v == "dl_median" || $v == "dl_medians"} {
			    set function dl_medians
			} else {
			    error "::gr::boxwhisker : value of -funciton must be 'means' or 'medians'"
			}
		    }
		    ymin -
		    ymax {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set $var $val
		    }
		    palette {
			set v [lindex $args [incr i]]
#			if {$v != "color" && $v != "grayscale"} {
#			    error "::gr::boxwhisker : value of -palette must be 'color' or 'grayscale'"
#			}
			set ::helpgr::palette $v
		    }
		    linemin {
			set linemin [lindex $args [incr i]]
		    }
		    linemax {
			set linemax [lindex $args [incr i]]
		    }
		    boxmin {
			set boxmin [lindex $args [incr i]]
		    }
		    boxmax {
			set boxmax [lindex $args [incr i]]
		    }
		    draw -
		    clearwin -
		    veridical_x -
		    plot_outliers -
		    showcount -
		    newwin -
		    newtab {
			# these are all booleans
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {$val != 0 && $val != 1 } {
			    error "::graphing::boxwhisker - expect boolean (0 or 1) for $var"
			}
 			set $var $val

			# if we are not drawing, then don't clear the window
			if {$var == "draw" && !$val} { set clearwin 0 }
 		    }
		    group {
			set g [lindex $args [incr i]]
			set vals [regsub -all -- "%g" "$vals" "$g"]
			set tags [regsub -all -- "%g" "$tags" "$g"]
		    }
		    limit {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set limit $val
		    }
		    default {
			error "::gr::boxwhisker: invalid switch $a \n   must be 'function' 'linemin' 'linemax' 'boxmin' boaxmax' 'veridical_x' 'plot_outliers' 'group' 'newwin' 'newtab' 'limit' 'clearwin' 'draw' 'palette' 'showcount'.\n   use ::gr::boxwhisker for more details."
		    }
		}
	    }
	}

	# use all the data if there are no sort tags
	if {$tags == ""} {
	    set tags "all_data"
	    dl_set all_data [dl_ones [dl_length $vals]]
	}

	# in case we limit trials, we want to know what the original vals and tags were called for x and y titles
	set val_title $vals
	set tag_title $tags

	# limit trials if we want (now that -group has been accounted for)
	if {$limit != ""} {
	    if {[dl_length $limit] != [dl_length $vals]} {
		error "::gr::boxwhisker - length of limit list must be the same as length of vals"
	    }
	    dl_local vals [dl_select $vals $limit]
	    dl_local t [dl_llist]
	    set tnames ""
	    set i -1
	    foreach tag $tags {
		dl_append $t [dl_select $tag $limit]
		lappend tnames $t:[incr i]
	    }
	    set tags $tnames
	}

	# make sure there is data to plot
	if ![dl_length $vals] {
	    error "::gr::boxwhisker - no data to plot"
	}

	# check tags
	if ![llength $tags] {
	    error "::gr::boxwhisker - cannot sort by an empty variable"
	} elseif {[llength $tags] > 3} {
	    error "::gr::boxwhisker - cannot sort by more than 3 variables"
	}

	# general info
	set g [dg_create]
	dl_set $g:vals [eval dl_slist $val_title]
	dl_set $g:tags [eval dl_slist $tag_title]

	# get all information we could use on this data
	dl_local info   [dl_sortedFunc $vals $tags $function]
	for {set i 0} {$i < [expr [dl_length $info]-1]} {incr i} {
	    dl_set $g:sort$i $info:$i
	}
	dl_set $g:data  [dl_last $info]
	dl_set $g:count [dl_last [dl_sortedFunc $vals $tags dl_lengths]]
	dl_local sorted_vals [eval dl_sortByLists $vals $tags]

	# the following will fail if there are lists in sorted_vals that are length zero.  ie. we have a cross of tags that contains no data
	# for now we will fill with a 0
 	dl_local sorted_vals [dl_replace $sorted_vals [dl_not [dl_lengths $sorted_vals]] [dl_llist [dl_create [dl_datatype [dl_unpack $sorted_vals]] 0]]]

	dl_set $g:loline [dl_getPercentiles $sorted_vals $linemin]
	dl_set $g:hiline [dl_getPercentiles $sorted_vals $linemax]
        dl_set $g:lobox  [dl_getPercentiles $sorted_vals $boxmin]
	dl_set $g:hibox  [dl_getPercentiles $sorted_vals $boxmax]
	dl_set $g:outliers [dl_select $sorted_vals [dl_not [dl_betweenEq $sorted_vals $g:loline $g:hiline]]]

	# plotting options
	dl_set $g:function [dl_slist $function]
	dl_set $g:linemin $linemin
	dl_set $g:boxmin $boxmin
	dl_set $g:boxmax $boxmax
	dl_set $g:linemax $linemax
	dl_set $g:veridical_x $veridical_x
	dl_set $g:plot_outliers $plot_outliers
	dl_set $g:showcount $showcount

	# newwindow? clearwin?
	if $newwin newWindow
	if $newtab newTab
	if $clearwin clearwin

	# for yaxis
	dl_set $g:ymin $ymin
	dl_set $g:ymax $ymax

	# sort the data so it is in a good order for next functions
 	switch [llength $tags] {
 	    1 {
		dg_sort $g sort0
 	    }
 	    2 {
		dg_sort $g sort0 sort1
  	    }
 	    3 {
		dg_sort $g sort0 sort1 sort2
  	    }
 	}
	set plots [::helpgr::plot_boxwhisker $g $draw]
	if $draw {
	    ::helpgr::mark_plots "::gr::boxwhisker $vals $tags $args"
	}

	#cleanup
	dg_delete $g; # could keep around and return as easy data source
#	return "$g - $plots"
	return $plots
    }

    proc barchart { vals tags args} {
	# %HELP%
	# ::gr::barchart vals tags ?options...?
	#    Creates a barchart of data in $vals sorted by values in $tags.
	#    The structure of $vals and $tags is similar to what you would use for
	#    dl_sortedFunc. You are limited to 0-3 sort lists. If tags is "", then all
	#    the data is averaged together.
	#
	#    Options to include at end of proc; format: -option option_value
	#    Default value is shown in []
	#       -function : 'means' or 'medians' [means]
	#       -errfunc  : 'stdevs' 'sems' '95cis' or 'none'   [sems]
	#       -veridical_x : '1' to plot x-axis with a true dimension [0]
	#       -group : supply a group that will be used to locate any lists in vals or
	#                tags that start with '%g'
	#       -newwin : boolean, should we put in a newWindow? [0]
	#       -newtab : boolean, should we put in a newTab? [0]
	#       -limit : boolean list to use as a pre-selection for all vals and tags.  if blank, use all. [""]
	#       -draw : boolean, draw plot to cgwin? set to 0 if you just want the plot(s) list back for future plotting. if zero, clearwin will also be set to 0 [1]
	#       -clearwin : boolean, should we clear the window? [1]
	#       -palette : 'color', 'grayscale', or two lists of RGB values for min and max color range (i.e. -palette {"0 153 0" "153 0 153"}) [color]
	#       -showcount : boolean, should we label boxes with number of datapoint? [0]
	#       -ymin : minimum of yaxis [calculated from data]
	#       -ymax : maximum of yaxis [calculated from data]
	#
	#  EXAMPLE
	#    dl_local v [dl_combine [dl_fromto 1 11] [dl_fromto 10 30 2] [dl_fromto 10 110 10]]
	#    dl_local t [dl_repeat "0 5 20" 10]
	#    ::gr::barchart $v $t
	# %END%

	# set DEFAULTS
	set function dl_means;
	set errfunc dl_sems; # error bars for SEMs by default
	set veridical_x 0; # plot x-axis with true dimension?
	set newwin 0; # plot in new window?
	set newtab 0; # plot in new tab?
	set limit ""; # limit plot to certain trials?
	set draw 1; # by default, we will draw plot to screen
	set clearwin 1; # by default, clear the window
	set ::helpgr::palette color; # should we draw in color or grayscale (grayscale better for many boxes)
	set showcount 0; # don't show datapoint count
	set ymin -9999; # null value used by match_yaxes_replot
	set ymax -9999;

	# parse switches
	for {set i 0} {$i < [llength $args]} {incr i} {
	    set a [lindex $args $i]
	    if {[string range $a 0 0] != "-"} {
		break
	    } elseif {[string range $a 0 1] == "--"} {
		break
	    } else {
		# below we define any tags that will be used
		switch -exact [string range $a 1 end] {
		    function {
			set v [lindex $args [incr i]]
			if {$v == "means" || $v == "mean" || $v == "dl_mean" || $v == "dl_means"} {
			    set function dl_means
			} elseif { $v == "medians" || $v == "median" || $v == "dl_median" || $v == "dl_medians" } {
			    set function dl_medians
			} else {
			    error "::gr::barchart : value of -funciton must be 'means' or 'medians'"
			}
		    }
		    errfunc {
			set v [lindex $args [incr i]]
			if {$v == "stdev" || $v == "stdevs" || $v == "dl_std" || $v == "dl_stds"} {
			    set errfunc dl_stds
			} elseif {$v == "sem" || $v == "sems" || $v == "dl_sem" || $v == "dl_sems"} {
			    set errfunc dl_sems
			} elseif { $v == "95%ci" || $v == "95%cis" || $v == "95ci" || $v == "95cis" || $v == "dl_95ci" || $v == "dl_95cis" } {
			    set errfunc dl_95cis
			} elseif { $v == "none" } {
			    set errfunc none
			} else {
			    error "::gr::barchart : value of -errfunc must be 'stdev', 'sem' or '95ci'"
			}
		    }
		    palette {
			set v [lindex $args [incr i]]
#			if {$v != "color" && $v != "grayscale" } {
#			    error "::gr::boxwhisker : value of -palette must be 'color' or 'grayscale'"
#			}
			set ::helpgr::palette $v
		    }
		    ymin -
		    ymax {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set $var $val
		    }
		    clearwin -
		    draw -
		    veridical_x -
		    showcount -
		    newwin -
		    newtab {
			# these are all booleans
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {$val != 0 && $val != 1 } {
			    error "::graphing::barchart - expect boolean (0 or 1) for $var"
			}
 			set $var $val

			# if we are not drawing, then don't clear the window
			if {$var == "draw" && !$val} { set clearwin 0 }
		    }
		    group {
			set g [lindex $args [incr i]]
			set vals [regsub -all -- "%g" "$vals" "$g"]
			set tags [regsub -all -- "%g" "$tags" "$g"]
		    }
		    limit {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set limit $val
		    }
		    default {
			error "::gr::barchart: invalid switch $a \n   must be 'function' 'errfunc' 'veridical_x' 'group' 'newwin' 'newtab' 'limit' 'draw' 'clearwin' 'palette' 'showcount'.\n   use ::gr::barchart for more details."
		    }
		}
	    }
	}

	# use all the data if there are no sort tags
	if {$tags == ""} {
	    set tags "all_data"
	    dl_set all_data [dl_ones [dl_length $vals]]
	}

	# in case we limit trials, we want to know what the original vals and tags were called for x and y titles
	set val_title $vals
	set tag_title $tags

	# limit trials if we want (now that -group has been accounted for)
	if {$limit != ""} {
	    if {[dl_length $limit] != [dl_length $vals]} {
		error "::gr::barchart - length of limit list must be the same as length of vals"
	    }
	    dl_local vals [dl_select $vals $limit]
	    dl_local t [dl_llist]
	    set tnames ""
	    set i -1
	    foreach tag $tags {
		dl_append $t [dl_select $tag $limit]
		lappend tnames $t:[incr i]
	    }
	    set tags $tnames
	}

	# make sure there is data to plot
	if ![dl_length $vals] {
	    error "::gr::barchart - no data to plot"
	}

	# check tags
	if ![llength $tags] {
	    error "::gr::barchart - cannot sort by an empty variable"
	} elseif {[llength $tags] > 3} {
	    error "::gr::barchart - cannot sort by more than 3 variables"
	}

	# general info
	set g [dg_create]
	dl_set $g:vals [eval dl_slist $val_title]
	dl_set $g:tags [eval dl_slist $tag_title]

	# get all information we could use on this data
	dl_local info   [dl_sortedFunc $vals $tags $function]
	for {set i 0} {$i < [expr [dl_length $info]-1]} {incr i} {
	    dl_set $g:sort$i $info:$i
	}
	dl_set $g:data  [dl_last $info]
	dl_set $g:count [dl_last [dl_sortedFunc $vals $tags dl_lengths]]
	if {$errfunc != "none"} {
	    dl_set $g:err    [dl_last [dl_sortedFunc $vals $tags $errfunc]]
	}

	# plotting options
	dl_set $g:function [dl_slist $function]
	dl_set $g:errfunc  [dl_slist $errfunc]
	dl_set $g:veridical_x $veridical_x
	dl_set $g:showcount $showcount
	
	# newwindow?
	if $newwin newWindow
	if $newtab newTab
	if $clearwin clearwin

	# for yaxis
	dl_set $g:ymin $ymin
	dl_set $g:ymax $ymax

	# sort the data so it is in a good order for next functions
 	switch [llength $tags] {
 	    1 {
		dg_sort $g sort0
 	    }
 	    2 {
		dg_sort $g sort0 sort1
  	    }
 	    3 {
		dg_sort $g sort0 sort1 sort2
  	    }
 	}

	set plots [::helpgr::plot_barchart $g $draw]
	if $draw {
	    ::helpgr::mark_plots "::gr::barchart $vals $tags $args"
	}
	
	#cleanup
	dg_delete $g; # could keep around and return as easy data source
#	return "$g - $plots"
	return $plots
    }

    proc lineplot { vals tags args} {
	# %HELP%
	# ::gr::lineplot vals tags ?options...?
	#    Creates a lineplot of data in $vals sorted by values in $tags.
	#    The structure of $vals and $tags is similar to what you would use for
	#    dl_sortedFunc. You are limited to 0-3 sort lists. If tags is "", then all
	#    the data is averaged together.
	#
	#    Options to include at end of proc; format: -option option_value
	#    Default value is shown in []
	#       -function : 'means' or 'medians' [means]
	#       -errfunc  : 'stdevs' 'sems' '95cis' or 'none'   [sems]
	#       -veridical_x : '1' to plot x-axis with a true dimension [0]
	#       -group : supply a group that will be used to locate any lists in vals or
	#                tags that start with '%g'
	#       -newwin : boolean, should we put in a newWindow? [0]
	#       -newtab : boolean, should we put in a newTab? [0]
	#       -limit : boolean list to use as a pre-selection for all vals and tags.  if blank, use all. [""]
	#       -skip_zero_ns : don't plot values without valid datapoints? [0]
	#       -draw : boolean, draw plot to cgwin? set to 0 if you just want the plot(s) list back for future plotting. if zero, clearwin will also be set to 0 [1]
	#       -clearwin : boolean, should we clear the window? [1]
	#       -palette : 'color', 'grayscale', or two lists of RGB values for min and max color range (i.e. -palette {"0 153 0" "153 0 153"}) [color]
	#       -showcount : boolean, should we label boxes with number of datapoint? [0]
	#       -stagger : boolean, jitter x-axis placement of each line group when sorted with two variables? [0]
	#       -ymin : minimum of yaxis [calculated from data]
	#       -ymax : maximum of yaxis [calculated from data]
	#
	#  EXAMPLE
	#    dl_local v [dl_combine [dl_fromto 1 11] [dl_fromto 10 30 2] [dl_fromto 10 110 10]]
	#    dl_local t [dl_repeat "0 5 20" 10]
	#    ::gr::lineplot $v $t
	# %END%

	# set DEFAULTS
	set function dl_means;
	set errfunc dl_sems; # error bars for SEMs by default
	set veridical_x 0; # plot x-axis with true dimension?
	set newwin 0; # plot in new window?
	set newtab 0; # plot in new tab?
	set limit ""; # limit plot to certain trials?
	set draw 1; # by default, we will draw plot to screen
	set clearwin 1; # by default, clear the window
	set ::helpgr::palette color; # should we draw in color or grayscale (grayscale better for many boxes)
	set showcount 0; # don't show datapoint count
	set stagger 0; # don't stagger data on x-axis
	set ymin -9999; # null value used by match_yaxes_replot
	set ymax -9999;
	set skip_zero_ns 0; # don't plot points with no data

	# parse switches
	for {set i 0} {$i < [llength $args]} {incr i} {
	    set a [lindex $args $i]
	    if {[string range $a 0 0] != "-"} {
		break
	    } elseif {[string range $a 0 1] == "--"} {
		break
	    } else {
		# below we define any tags that will be used
		switch -exact [string range $a 1 end] {
		    function {
			set v [lindex $args [incr i]]
			if {$v == "means" || $v == "mean" || $v == "dl_mean" || $v == "dl_means"} {
			    set function dl_means
			} elseif { $v == "medians" || $v == "median" || $v == "dl_median" || $v == "dl_medians" } {
			    set function dl_medians
			} else {
			    error "::gr::lineplot : value of -funciton must be 'means' or 'medians'"
			}
		    }
		    errfunc {
			set v [lindex $args [incr i]]
			if {$v == "stdev" || $v == "stdevs" || $v == "dl_std" || $v == "dl_stds"} {
			    set errfunc dl_stds
			} elseif {$v == "sem" || $v == "sems" || $v == "dl_sem" || $v == "dl_sems"} {
			    set errfunc dl_sems
			} elseif { $v == "95%ci" || $v == "95%cis" || $v == "95ci" || $v == "95cis" || $v == "dl_95ci" || $v == "dl_95cis" } {
			    set errfunc dl_95cis
			} elseif { $v == "none" } {
			    set errfunc none
			} else {
			    error "::gr::lineplot : value of -errfunc must be 'stdev', 'sem' or '95ci'"
			}
		    }
		    palette {
			set v [lindex $args [incr i]]
#			if {$v != "color" && $v != "grayscale" } {
#			    error "::gr::boxwhisker : value of -palette must be 'color' or 'grayscale'"
#			}
			set ::helpgr::palette $v
		    }
		    ymin -
		    ymax {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set $var $val
		    }
		    skip_zero_ns -
		    clearwin -
		    draw -
		    veridical_x -
		    showcount -
		    stagger -
		    newwin -
		    newtab {
			# these are all booleans
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {$val != 0 && $val != 1 } {
			    error "::graphing::lineplot - expect boolean (0 or 1) for $var"
			}
 			set $var $val

			# if we are not drawing, then don't clear the window
			if {$var == "draw" && !$val} { set clearwin 0 }
		    }
		    group {
			set g [lindex $args [incr i]]
			set vals [regsub -all -- "%g" "$vals" "$g"]
			set tags [regsub -all -- "%g" "$tags" "$g"]
		    }
		    limit {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set limit $val
		    }
		    default {
			error "::gr::lineplot: invalid switch $a \n   must be 'function' 'errfunc' 'veridical_x' 'group' 'newwin' 'newtab' 'limit' 'skip_zero_ns' 'draw' 'clearwin' 'palette' 'showcount' 'stagger'.\n   use ::gr::lineplot for more details."
		    }
		}
	    }
	}

	# use all the data if there are no sort tags
	if {$tags == ""} {
	    set tags "all_data"
	    dl_set all_data [dl_ones [dl_length $vals]]
	}

	# in case we limit trials, we want to know what the original vals and tags were called for x and y titles
	set val_title $vals
	set tag_title $tags

	# limit trials if we want (now that -group has been accounted for)
	if {$limit != ""} {
	    if {[dl_length $limit] != [dl_length $vals]} {
		error "::gr::lineplot - length of limit list must be the same as length of vals"
	    }
	    dl_local vals [dl_select $vals $limit]
	    dl_local t [dl_llist]
	    set tnames ""
	    set i -1
	    foreach tag $tags {
		dl_append $t [dl_select $tag $limit]
		lappend tnames $t:[incr i]
	    }
	    set tags $tnames
	}

	# make sure there is data to plot
	if ![dl_length $vals] {
	    error "::gr::lineplot - no data to plot"
	}

	# check tags
	if ![llength $tags] {
	    error "::gr::lineplot - cannot sort by an empty variable"
	} elseif {[llength $tags] > 3} {
	    error "::gr::lineplot - cannot sort by more than 3 variables"
	}

	# general info
	set g [dg_create]
	dl_set $g:vals [eval dl_slist $val_title]
	dl_set $g:tags [eval dl_slist $tag_title]

	# get all information we could use on this data
	dl_local info   [dl_sortedFunc $vals $tags $function]
	for {set i 0} {$i < [expr [dl_length $info]-1]} {incr i} {
	    dl_set $g:sort$i $info:$i
	}
	dl_set $g:data  [dl_last $info]
	dl_set $g:count [dl_last [dl_sortedFunc $vals $tags dl_lengths]]
	if {$errfunc != "none"} {
	    dl_set $g:err    [dl_last [dl_sortedFunc $vals $tags $errfunc]]
	}

	# plotting options
	dl_set $g:function [dl_slist $function]
	dl_set $g:errfunc  [dl_slist $errfunc]
	dl_set $g:veridical_x $veridical_x
	dl_set $g:showcount $showcount
	dl_set $g:stagger $stagger
	
	# newwindow?
	if $newwin newWindow
	if $newtab newTab
	if $clearwin clearwin

	# for yaxis
	dl_set $g:ymin $ymin
	dl_set $g:ymax $ymax

	# sort the data so it is in a good order for next functions
 	switch [llength $tags] {
 	    1 {
		dg_sort $g sort0
 	    }
 	    2 {
		dg_sort $g sort0 sort1
  	    }
 	    3 {
		dg_sort $g sort0 sort1 sort2
  	    }
 	}

	# here we can eliminate data that has n=0 entries
	if $skip_zero_ns {
	    dg_select $g [dl_noteq $g:count 0]
	}

	set plots [::helpgr::plot_lineplot $g $draw]
	if $draw {
	    ::helpgr::mark_plots "::gr::lineplot $vals $tags $args"
	}

	#cleanup

	dg_delete $g; # could keep around and return as easy data source
	#return "$g - $plots"
	return $plots
    }

    proc xyscatter { xdata ydata args} {
	# %HELP%
	# ::gr::xyscatter xdata ydata ?options...?
	#    Creates a xyscatter of $xdata  and $ydata. $xdata and $ydata must be the
	#    the same length.
	#
	#    Options to include at end of proc; format: -option option_value
	#    Default value is shown in []
	#       -group : supply a group that will be used to locate xdata and ydata dls
	#                that start with '%g'
	#       -newwin : boolean, should we put in a newWindow? [0]
	#       -newtab : boolean, should we put in a newTab? [0]
	#       -limit : boolean list to use as a pre-selection for all xdata and ydata.  if blank, use all. [""]
	#       -draw : boolean, draw plot to cgwin? set to 0 if you just want the plot(s) list back for future plotting. if zero, clearwin will also be set to 0 [1]
	#       -clearwin : boolean, should we clear the window? [1]
	#       -ymin : minimum of yaxis [calculated from data]
	#       -ymax : maximum of yaxis [calculated from data]
	#       -xmin : minimum of xaxis [calculated from data]
	#       -xmax : maximum of xaxis [calculated from data]
	#       -min : minimum of xaxis AND yaxis. supersedes ymin and xmin. [calculated from data]
	#       -max : maximum of yaxis AND yaxis. supersedes xmax and ymax. [calculated from data]
	#       -symmetric : boolean, should the x and y axes show the same range? This supersedes ymin/ymax/xmin/xmax (but will use the largest of those values) [0]
	#       -line : draw a dashed line under data. 'x=y', 'x=-y', 'lsfit' (using dl_lsfit), or 'none' [none] 
	#
	#  EXAMPLE
	#    dl_local x [dl_zrand 20]
	#    dl_local y [dl_zrand 20]
	#    ::gr::xyscatter $x $y
	# %END%

	# set DEFAULTS
	set newwin 0; # plot in new window?
	set newtab 0; # plot in new tab?
	set limit ""; # limit plot to certain trials?
	set draw 1; # by default, we will draw plot to screen
	set clearwin 1; # by default, clear the window
	set ymin -9999; # null value used by match_yaxes_replot
	set ymax -9999;
	set xmin -9999;
	set xmax -9999;
	set min -9999;
	set max -9999;
	set ymin -9999;
	set ymax -9999;
	set symmetric 0; # symmetric x and y ranges?
	set line "none"; # draw a dashed line?

	# parse switches
	for {set i 0} {$i < [llength $args]} {incr i} {
	    set a [lindex $args $i]
	    if {[string range $a 0 0] != "-"} {
		break
	    } elseif {[string range $a 0 1] == "--"} {
		break
	    } else {
		# below we define any tags that will be used
		switch -exact [string range $a 1 end] {
		    line {
			set v [lindex $args [incr i]]
			if {$v == "x=y" || $v == "x=-y" || $v == "lsfit" || $v == "none"} {
			    set line $v
			} else {
			    error "::gr::xyscatter : value of -line must be 'x=y', 'x=-y', 'lsfit', or 'none'"
			}
		    }
		    ymin -
		    ymax -
		    xmin -
		    xmax -
		    min -
		    max {
			# these are scalars
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set $var $val
		    }
		    clearwin -
		    draw -
		    newwin -
		    newtab -
		    symmetric {
			# these are all booleans
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			if {$val != 0 && $val != 1 } {
			    error "::graphing::xyscatter - expect boolean (0 or 1) for $var"
			}
 			set $var $val

			# if we are not drawing, then don't clear the window
			if {$var == "draw" && !$val} { set clearwin 0 }
		    }
		    group {
			set g [lindex $args [incr i]]
			set xdata [regsub -all -- "%g" "$xdata" "$g"]
			set ydata [regsub -all -- "%g" "$ydata" "$g"]
		    }
		    limit {
			set var [string range [lindex $args $i] 1 end]
			set val [lindex $args [incr i]]
			set limit $val
		    }
		    default {
			error "::gr::xyscatter: invalid switch $a \n   must be 'group' 'newwin' 'newtab' 'limit' 'draw' 'clearwin' 'ymin' 'ymax' 'xmin' 'xmax' 'min' 'max' 'symmetric' 'line'.\n   use ::gr::xyscatter for more details."
		    }
		}
	    }
	}

	# make sure x and y data are not strings
	if {[dl_datatype $xdata] == "string"} {
	    error "::gr::xyscatter - invalid datatype for xdata ([dl_datatype $xdata])"
	}
	if {[dl_datatype $ydata] == "string"} {
	    error "::gr::xyscatter - invalid datatype for ydata ([dl_datatype $ydata])"
	}

	# make sure xdata and ydata are same length
	if {[dl_length $xdata] != [dl_length $ydata]} {
	    error "::gr::xyscatter - length of xdata and ydata must be the same"
	}

	# in case we limit trials, we want to know what the original vals and tags were called for x and y titles
	set x_title $xdata
	set y_title $ydata

	# limit trials if we want (now that -group has been accounted for)
	if {$limit != ""} {
	    if {[dl_length $limit] != [dl_length $xdata]} {
		error "::gr::xyscatter - length of limit list must be the same as length of xdata and ydata"
	    }
	    dl_local xdata [dl_select $xdata $limit]
	    dl_local ydata [dl_select $ydata $limit]
	}

	# make sure there is data to plot
	if ![dl_length $xdata] {
	    error "::gr::xyscatter - no data to plot"
	}

	# general info
	set g [dg_create]
	dl_set $g:xtitle [eval dl_slist $x_title]
	dl_set $g:ytitle [eval dl_slist $y_title]
	dl_set $g:xdata $xdata
	dl_set $g:ydata $ydata
	dl_set $g:count [dl_length $xdata]

	# newwindow?
	if $newwin newWindow
	if $newtab newTab
	if $clearwin clearwin

	# axes
	set pad .05; # proportion of range used to pad axes when guessing
	if {$min != -9999} {
	    # use for both axes
	    set this_ymin $min
	    set this_xmin $min
	} else {
	    if {$ymin == -9999} {
		# guess ymin
		set range [expr [dl_max $ydata]-[dl_min $ydata]]
		set this_ymin [expr [dl_min $ydata] - $range*$pad]
	    } else {
		# user supplied
		set this_ymin $ymin
	    }
	    if {$xmin == -9999} {
		# guess xmin
	      dl_set x $xdata
		set range [expr [dl_max $xdata]-[dl_min $xdata]]
		set this_xmin [expr [dl_min $xdata] - $range*$pad]
	    } else {
		# user supplied
		set this_xmin $xmin
	    }
	}
	if {$max != -9999} {
	    # use for both axes
	    set this_ymax $max
	    set this_xmax $max
	} else {
	    if {$ymax == -9999} {
		# guess ymax
		set range [expr [dl_max $ydata]-[dl_min $ydata]]
		set this_ymax [expr [dl_max $ydata] + $range*$pad]
	    } else {
		# user supplied
		set this_ymax $ymax
	    }
	    if {$xmax == -9999} {
		# guess xmax
		set range [expr [dl_max $xdata]-[dl_min $xdata]]
		set this_xmax [expr [dl_max $xdata] + $range*$pad]
	    } else {
		# user supplied
		set this_xmax $xmax
	    }
	}
	if $symmetric {
	    # then make symmetric
	    set sym_min [dl_min "$this_ymin $this_xmin"]
	    set sym_max [dl_max "$this_ymax $this_xmax"]
	    set this_ymin $sym_min
	    set this_ymax $sym_max
	    set this_xmin $sym_min
	    set this_xmax $sym_max
	}
	# now we have the real min/max values
	dl_set $g:ymin $this_ymin
	dl_set $g:ymax $this_ymax
	dl_set $g:xmin $this_xmin
	dl_set $g:xmax $this_xmax

	dl_set $g:line [dl_slist $line]
	
	set plots [::helpgr::plot_xyscatter $g $draw]
	if $draw {
	    ::helpgr::mark_plots "::gr::xyscatter $xdata $ydata $args"
	}

	#cleanup
	dg_delete $g; # could keep around and return as easy data source
	#	return "$g - $plots"
	return $plots
    }
}



namespace eval ::helpgr {
    variable wincounter 0; # to keep track of windows

    proc match_yaxes_replot { plots draw g } {
	set ymin [dl_first $g:ymin]
	set ymax [dl_first $g:ymax]
	if {$ymin == -9999} {
	    set ymin 0
	    foreach p $plots {
		set ymin [dl_tcllist [dl_min $ymin [dl_min $p:ydata]]]
	    }
	    # now we have global ymin across all plots
	    # don't pad ymin, since it is likely to be zero (will still normalize all to lowest value)
	}
	if {$ymax == -9999} {
	    set ymax 0
	    foreach p $plots {
		set ymax [dl_tcllist [dl_max $ymax [dl_max $p:ydata]]]
	    }
	    # now we have global ymax across all plots
	    # padding - add 10% to full range
	    set pad [expr ($ymax-$ymin)*.1]
	    set ymax [expr $ymax+$pad]
	}

	foreach p $plots {
	    dlp_setyrange $p $ymin $ymax
	}
	# plot it if we want to
	if $draw {
	    dlp_setpanels [llength $plots] 1
	    set i -1
	    foreach p $plots {
		dlp_subplot $p [incr i]
	    }
	}
    }

    proc get_colors { ncolors } {
	if {$::helpgr::palette == "grayscale"} {
	    # for grayscale boxcolors
	    set cmin 50
	    set cmax 225
	    set crange [expr $cmax-$cmin]
	    if {$ncolors != 1} {
		dl_local grays [dl_int [dl_series $cmin $cmax [expr $crange / [expr $ncolors-1]]]]
		dl_local c [dlg_rgbcolors $grays $grays $grays]
	    } else {
		dl_local c [dlg_rgbcolor 100 100 100]
	    }
	} elseif {$::helpgr::palette == "color"} {
	    # for red-to-blue boxcolors
	    dl_local cmin "255 0 0"
	    dl_local cmax "0 0 255"
	    dl_local crange [dl_sub $cmax $cmin]
	    if {$ncolors != 1} {
		dl_local rgb [dl_int [dl_series $cmin $cmax [dl_div $crange [expr $ncolors-1]]]]
		dl_local rgb [dl_select $rgb [dl_lt [dl_index $rgb] $ncolors]]
		dl_local c [dlg_rgbcolors $rgb:0 $rgb:1 $rgb:2]
	    } else {
		dl_local c [dlg_rgbcolor 255 0 0]
	    }
	} else {
	    # user supplied palette
	    if {[llength $::helpgr::palette] != 2 || [llength [lindex $::helpgr::palette 0]] != 3 || [llength [lindex $::helpgr::palette 1]] != 3 } {
		error "::helpgr::get_colors - invalid color palette supplied\n     try something like -palette {\"0 153 0\" \"153 0 153\"}"
	    }

	    dl_local cmin [lindex $::helpgr::palette 0]
	    dl_local cmax [lindex $::helpgr::palette 1]

	    dl_local crange [dl_sub $cmax $cmin]
	    if {$ncolors != 1} {
		dl_local rgb [dl_int [dl_series $cmin $cmax [dl_div $crange [expr $ncolors-1]]]]
		dl_local rgb [dl_select $rgb [dl_lt [dl_index $rgb] $ncolors]]
		dl_local c [dlg_rgbcolors $rgb:0 $rgb:1 $rgb:2]
	    } else {
		dl_local c [dlg_rgbcolor 255 0 0]
	    }
	    
	}
	dl_return $c
    }

    proc mark_plots { text } {
	# %HELP%
	# ::helpgr::mark_plots text 
	#    adds some text info to plots.  always adds date.  
	# %END%
	set win [getwindow]
	setwindow 0 0 1 1
	set date [clock format [clock seconds] -format %m/%d/%y]
	dlg_text .95 .5 "$date\n$text" -ori 1 -just 0 -spacing .015 -color 156204255
	eval setwindow $win
    }


    proc plot_cumdist { g draw } {
	if [dl_exists $g:sort1] {
#	    set n [dl_length [dl_unique $g:sort1]]
#	    dl_local yports [dl_reverse [dl_series 0 1 [expr 1./$n]]]

#	    set i -1
	    set plots ""
	    foreach v [dl_tcllist [dl_unique $g:sort1]] {
#		set g2 [dg_copySelected $g [dl_eq $g:sort1 $v]]
#		foreach dl "vals tags min max nbins bins" {
#		    dl_set $g2:$dl $g:$dl
#		}
		set g2 [dg_copySelected $g [dl_eq $g:sort1 [dl_create [dl_datatype $g:sort1] $v]] 1]

#		pushpviewport 0 [dl_get $yports [incr i]] 1 [dl_get $yports [expr $i+1]]
		lappend plots [set p [create_cumdist_plot $g2 "[dl_get $g2:tags 1] = $v"]]
#		if $draw { dlp_plot $p }
#		poppviewport
		dg_delete $g2
	    }
	} else {
	    lappend plots [set p [create_cumdist_plot $g]]
#	    if $draw { dlp_plot $p }
	}

	# equalize yaxes
	match_yaxes_replot $plots $draw $g
 
	return $plots
    }
    
    proc create_cumdist_plot { g { title ""} } {
	set p [dlp_newplot]

	dl_local data $g:data_used
	dl_local prop $g:proportion

	switch -exact -- [dl_first $g:style] {
	    direct {
		# prepend the nodata 0th percentile point
		dl_local data [dl_combineLists [dl_select $data [dl_firstPos $data]] $data]
		dl_local prop [dl_combineLists [dl_repeat 0. [dl_length $prop]] $prop]
	    }
	    step {
		# prepend the nodata 0th percentile point
		dl_local data [dl_repeatElements $data 2]
		dl_local data [dl_select $data [dl_not [dl_firstPos $data]]]
		dl_local prop [dl_repeatElements $prop 2]
		dl_local prop [dl_select $prop [dl_not [dl_lastPos $data]]]
	    }
	}

	# colors
	dl_local linecolors [get_colors [dl_length $g:sort0]]

	# make a cumulative distribution plot
	set n [dl_length $data]
	for {set i 0} {$i < $n} {incr i} {
	    set x [dlp_addXData $p $data:$i]
	    set y [dlp_addYData $p $prop:$i]
	    dlp_draw $p lines "$x $y" -linecolor [dl_get $linecolors $i] -lwidth 200
	}

	# xlabels
	dlp_setxrange $p [dl_first $g:xmin] [dl_first $g:xmax]

	# axis labels
	dlp_set $p xtitle \"[dl_get $g:vals 0]\"
	dlp_set $p ytitle Proportion
	dlp_set $p title $title

	# color legend for second sort variable
	set n [expr 1 + [dl_length [dl_unique $g:sort0]]]
	dlp_set $p xleg [dl_repeat .98 $n]
	dlp_set $p yleg [dl_series .95 [expr .95-.05*[expr $n-1]] -.05]
	dlp_set $p tleg [dl_prepend [eval dl_slist [dl_tcllist [dl_unique $g:sort0]]] [dl_get $g:tags 0]]
	dlp_set $p cleg [dl_prepend [dl_select $linecolors [dl_eq [dl_rank $linecolors] 0]] $::colors(white)]

	dlp_wincmd $p {0 0 1 1} "dlg_text %p:xleg %p:yleg %p:tleg -colors %p:cleg -size 12 -just 1"
	return $p
    }


    proc plot_histogram { g draw } {
	if [dl_exists $g:sort1] {
#	    set n [dl_length [dl_unique $g:sort1]]
#	    dl_local yports [dl_reverse [dl_series 0 1 [expr 1./$n]]]

#	    set i -1
	    set plots ""
	    foreach v [dl_tcllist [dl_unique $g:sort1]] {
#		set g2 [dg_copySelected $g [dl_eq $g:sort1 $v]]
#		foreach dl "vals tags min max nbins bins" {
#		    dl_set $g2:$dl $g:$dl
#		}
		set g2 [dg_copySelected $g [dl_eq $g:sort1 [dl_create [dl_datatype $g:sort1] $v]] 1]

#		pushpviewport 0 [dl_get $yports [incr i]] 1 [dl_get $yports [expr $i+1]]
		lappend plots [set p [create_histogram_plot $g2 "[dl_get $g2:tags 1] = $v"]]
#		if $draw { dlp_plot $p }
#		poppviewport
		dg_delete $g2
	    }
	} else {
	    lappend plots [set p [create_histogram_plot $g]]
#	    if $draw { dlp_plot $p }
	}

	# equalize yaxes
	match_yaxes_replot $plots $draw $g
 
	return $plots
    }
    proc create_histogram_plot { g { title ""} } {
	set p [dlp_newplot]

	set nbins [dl_first $g:nbins]
	set min  [dl_first $g:min]
	set max [dl_first $g:max]

	# set up appropriate x
	dl_local xs [dl_bins $min $max $nbins]
	if {[dl_length $xs] == 1} {
	    set bin 1
	} else {
	    set bin [expr [dl_first [dl_diff $xs]] * .9]; #thin out for looks
	}

	# setup offsets and widths here
	set n [dl_length $g:data]
	set start [expr (-($n-1)/2.) * ($bin/$n.)]
	
	# colors
	dl_local barcolors [get_colors [dl_length $g:sort0]]

	# make a histogram
	dl_local totals [dl_sumList $g:hist]
	dl_local used [dl_mult 0 $totals]
	set x [dlp_addXData $p $xs]
	for {set i 0} {$i < $n} {incr i} {
	    switch [dl_first $g:split] {
		interleave {
		    #set y [dlp_addYData $p [dl_hist $g:data:$i $min $max $nbins]]
		    set y [dlp_addYData $p $g:hist:$i]
		    #dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors $i] -linecolor [dl_get $barcolors $i] -width [expr 1./$n] -offset [expr $start + [expr $i*$bin/$n]] -interbar $bin -start 0
		    dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors $i] -width [expr 1./$n] -offset [expr $start + [expr $i*$bin/$n]] -interbar $bin -start 0
		}
		stack {
		    dl_local this_y [dl_sub $totals $used]
		    dl_local used [dl_add $used $g:hist:$i]
		    set y [dlp_addYData $p $this_y]
		    #dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors $i] -linecolor [dl_get $barcolors $i] -start 0
		    dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors $i] -start 0
		}
		split {
		    switch $i {
			0 {
#			    set y [dlp_addYData $p [dl_hist $g:data:$i $min $max $nbins]]
			    set y [dlp_addYData $p $g:hist:$i]
			    #dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors $i] -linecolor [dl_get $barcolors $i] -start 0
			    dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors $i] -start 0
			}
			1 {
#			    set y [dlp_addYData $p [dl_negate [dl_hist $g:data:$i $min $max $nbins]]]
			    set y [dlp_addYData $p [dl_negate $g:hist:$i]]
			    #dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors $i] -linecolor [dl_get $barcolors $i] -start 0
			    dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors $i] -start 0
			}
			2 {
			    error "::gr::histogram - too many sort values to plot by 'split'"
			}
		    }
		}
		default {
		    error "::helpgr::create_histogram_plot - invalid split type [dl_first $g:split]. shouldn't have gotten this far.  need better checking in ::gr::histogram"
		}
	    }
	}

	# xlabels
	dlp_setxrange $p [expr $min-$bin] [expr $max+$bin]; # padded by 1 bin in each direction for "good looks"
#	dlp_set $p xticks $g:bins

	# axis labels
	dlp_set $p xtitle \"[dl_get $g:vals 0]\"
	if [dl_first $g:proportion] {
	    dlp_set $p ytitle Proportion
	} else {
	    dlp_set $p ytitle Count
	}
	dlp_set $p title $title

	# color legend for second sort variable
	set n [expr 1 + [dl_length [dl_unique $g:sort0]]]
	dlp_set $p xleg [dl_repeat .98 $n]
	dlp_set $p yleg [dl_series .95 [expr .95-.05*[expr $n-1]] -.05]
	dlp_set $p tleg [dl_prepend [eval dl_slist [dl_tcllist [dl_unique $g:sort0]]] [dl_get $g:tags 0]]
	dlp_set $p cleg [dl_prepend [dl_select $barcolors [dl_eq [dl_rank $barcolors] 0]] $::colors(white)]

	dlp_wincmd $p {0 0 1 1} "dlg_text %p:xleg %p:yleg %p:tleg -colors %p:cleg -size 12 -just 1"
	return $p
    }


    proc plot_boxwhisker { g draw } {
	if [dl_exists $g:sort2] {
#	    set n [dl_length [dl_unique $g:sort2]]
#	    dl_local yports [dl_reverse [dl_series 0 1 [expr 1./$n]]]

#	    set i -1
	    set plots ""
	    foreach v [dl_tcllist [dl_unique $g:sort2]] {
#		set g2 [dg_copySelected $g [dl_eq $g:sort2 $v]]
#		foreach dl "vals tags function linemin boxmin boxmax linemax veridical_x plot_outliers" {
#		    dl_set $g2:$dl $g:$dl
#		}
		set g2 [dg_copySelected $g [dl_eq $g:sort2 [dl_create [dl_datatype $g:sort2] $v]] 1]

#		pushpviewport 0 [dl_get $yports [incr i]] 1 [dl_get $yports [expr $i+1]]
		lappend plots [set p [create_boxwhisker_plot $g2 "[dl_get $g2:tags 2] = $v"]]
#		if $draw { dlp_plot $p }
#		poppviewport
		dg_delete $g2
	    }
	} else {
	    lappend plots [set p [create_boxwhisker_plot $g]]
#	    if $draw { dlp_plot $p }
	}

	# equalize yaxes
	match_yaxes_replot $plots $draw $g
 
	return $plots
    }
    proc create_boxwhisker_plot { g {title ""}} {
	set p [dlp_newplot]

	# now that we have extraced actual tags...
	dl_local xnames   [dl_unique $g:sort0]

	# set up appropriate x
	# each slot is 1 wide, just keep going up as we need
	set w .5
	dl_local xs [dl_add [dl_index $g:sort0] [dl_mult 2 [dl_recode $g:sort0]]]
	if {[dl_first $g:veridical_x] && ( [dl_datatype $g:sort0] == "float" || [dl_datatype $g:sort0] == "long" ) } {
	    # adjust xs by x-axis sort values
#	    set mindist [dl_min [dl_diff $g:sort0]]; #min distance in real coordinates between neighboring points.  need to scale this to 1.
#	    dl_local xs [dl_mult $xs $g:sort0 [expr 1./$mindist]]
	    dl_local xs [dl_add $xs $g:sort0]; # this doesn't work for values less than 1 and above doesn't work for values over 1
	}

	# adjust xplaces and colors depending on how many sort variables we are plotting
	if [dl_exists $g:sort1] {
	    set firstxplace [dl_get [dl_unique $g:sort1] [expr int([dl_median [dl_index [dl_unique $g:sort1]]])]]
	    dl_local xplaces [dl_select $xs [dl_eq $g:sort1 [eval dl_create [dl_datatype $g:sort1] $firstxplace]]]

	    if {![expr [dl_length [dl_unique $g:sort1]]%2]} {
		dl_local xplaces [dl_add $xplaces .5]; # shift to middle of bars
	    }
	    dl_local c [get_colors [dl_length [dl_unique $g:sort1]]]
	    dl_local boxcolors [dl_choose $c [dl_recode $g:sort1]]
	} else {
	    dl_local xplaces $xs
	    dl_local c [get_colors 1]
	    dl_local boxcolors [dl_repeat $c [dl_length $g:sort0]]
	}

	dl_local xlo [dl_sub $xs $w]
	dl_local xhi [dl_add $xs $w]

	# range ticks (min to max)
	set x [dlp_addXData $p [dl_repeat $xs 2]]
	set y [dlp_addYData $p [dl_interleave $g:loline $g:hiline]]
	dlp_draw $p markers "$x $y" -marker htick

	set x [dlp_addXData $p [dl_repeat $xs 4]]
	set y [dlp_addYData $p [dl_unpack [dl_combineLists $g:loline $g:lobox $g:hibox $g:hiline]]]
	dlp_draw $p disjointLines "$x $y"

	# mval - mean is after box so line shows
	set mx [dlp_addXData $p [dl_interleave $xlo $xhi]]
	set my [dlp_addYData $p [dl_repeat $g:data 2]]
	# don't draw yet
	
	# box (g:lobox to g:hibox)
	set i -1
	foreach t [dl_tcllist $xnames] xlo [dl_tcllist $xlo] xhi [dl_tcllist $xhi] lopc [dl_tcllist $g:lobox] hipc [dl_tcllist $g:hibox] {
	    set x [dlp_addXData $p "$xlo $xlo $xhi $xhi $xlo"]
	    set y [dlp_addYData $p "$lopc $hipc $hipc $lopc $lopc"]
	    dlp_draw $p lines "$x $y" -fillcolor [dl_get $boxcolors [incr i]]
	    #dlp_draw $p lines "$x $y" -fillcolor [dl_get $c [incr i]]
	}

	# now draw mean so it is over the box
	dlp_draw $p disjointLines "$mx $my" -lwidth 250

	# outliers
	if [dl_first $g:plot_outliers] {
	    set ns [dl_lengths $g:outliers]
#	    set x [dlp_addXData $p [dl_repeat $xs $ns]]
	    dl_local jitter [dl_mult .5 [dl_sub [dl_urand [dl_sum $ns]] .5]]
	    set x [dlp_addXData $p [dl_add [dl_repeat $xs $ns] $jitter]]
	    set y [dlp_addYData $p [dl_unpack $g:outliers]]
	    dlp_set $p outlier_colors [dl_repeat $boxcolors [dl_lengths $g:outliers]]
	    dlp_draw $p markers "$x $y" -marker circle -colors %p:outlier_colors
	}

	if [dl_first $g:showcount] {
	    # we've plotted the boxes, not mark each bar with N samples in that bar's mean/median at base of plot under boxs
	    dlp_set $p xN $xs
	    dlp_set $p yN [dl_repeat .01 [dl_length $xs]]
	    dlp_set $p tN $g:count
	    dlp_winypostcmd $p {0 1} "dlg_text %p:xN %p:yN %p:tN -ori 1 -just -1"
	}

	# xlabels
	set xmin [expr [dl_min $xs]-2]
	dlp_setxrange $p $xmin [expr [dl_max $xs]+2]
	dlp_set $p xticks $xmin
	dlp_set $p xsubticks $xmin
	dlp_set $p xplaces $xplaces
	dlp_set $p xnames $xnames

	# axis labels
	dlp_set $p xtitle \"[dl_get $g:tags 0]\"
	dlp_set $p ytitle \"[dl_get $g:vals 0]\"
	dlp_set $p title $title

	# labels (will match up with farthest right box-and-whisker)
	dlp_set $p xlab [dl_repeat [expr [dl_max $xs]+1] 5]
	dlp_set $p ylab [dl_combine [dl_last $g:data] [dl_float [dl_last $g:loline]] [dl_float [dl_last $g:lobox]] [dl_float [dl_last $g:hibox]] [dl_float [dl_last $g:hiline]]]
	dlp_set $p tlab [dl_slist [string range [dl_first $g:function] 3 end] [dl_first $g:linemin]% [dl_first $g:boxmin]% [dl_first $g:boxmax]% [dl_first $g:linemax]%]
	dlp_cmd $p "dlg_text %p:xlab %p:ylab %p:tlab -just -1"
	
	# color labels for second sort variable
	if [dl_exists $g:sort1] {
	    #set n [expr 1 + [dl_length [dl_unique $boxcolors]]]
	    set n [expr 1 + [dl_length $c]]
	    dlp_set $p xleg [dl_repeat .98 $n]
	    dlp_set $p yleg [dl_series .95 [expr .95-.05*[expr $n-1]] -.05]
	    dlp_set $p tleg [dl_prepend [eval dl_slist [dl_tcllist [dl_unique $g:sort1]]] [dl_get $g:tags 1]]
	    #dlp_set $p cleg [dl_prepend [dl_select $boxcolors [dl_eq [dl_rank $boxcolors] 0]] $::colors(white)]
	    dlp_set $p cleg [dl_prepend $c $::colors(white)]

	    dlp_wincmd $p {0 0 1 1} "dlg_text %p:xleg %p:yleg %p:tleg -colors %p:cleg -size 12 -just 1"
	}

	return $p
    }


    proc plot_barchart { g draw } {
	if [dl_exists $g:sort2] {
	    set n [dl_length [dl_unique $g:sort2]]
	    dl_local yports [dl_reverse [dl_series 0 1 [expr 1./$n]]]

	    set i -1
	    set plots ""
	    foreach v [dl_tcllist [dl_unique $g:sort2]] {
#		set g2 [dg_copySelected $g [dl_eq $g:sort2 $v]]
#		foreach dl "vals tags function errfunc veridical_x showcount" {
#		    dl_set $g2:$dl $g:$dl
#		}
		set g2 [dg_copySelected $g [dl_eq $g:sort2 [dl_create [dl_datatype $g:sort2] $v]] 1]

#		pushpviewport 0 [dl_get $yports [incr i]] 1 [dl_get $yports [expr $i+1]]
		lappend plots [set p [create_barchart_plot $g2 "[dl_get $g2:tags 2] = $v"]]
#		if $draw { dlp_plot $p }
#		poppviewport
		dg_delete $g2
	    }
	} else {
	    lappend plots [set p [create_barchart_plot $g]]
#	    if $draw { dlp_plot $p }
	}

 	# equalize yaxes
	match_yaxes_replot $plots $draw $g
 	
	return $plots
    }
    proc create_barchart_plot { g {title ""}} {
	set p [dlp_newplot]

	# now that we have extraced actual tags...
	dl_local xnames   [dl_unique $g:sort0]

	# set up appropriate x
	# each slot is 1 wide, just keep going up as we need
	dl_local xs [dl_add [dl_index $g:sort0] [dl_mult 2 [dl_recode $g:sort0]]]
	if {[dl_first $g:veridical_x] && ( [dl_datatype $g:sort0] == "float" || [dl_datatype $g:sort0] == "long" ) } {
	    # adjust xs by x-axis sort values
	    dl_local xs [dl_add $xs $g:sort0]
	}

	# adjust xplaces and colors depending on how many sort variables we are plotting
	if [dl_exists $g:sort1] {
	    set firstxplace [dl_get [dl_unique $g:sort1] [expr int([dl_median [dl_index [dl_unique $g:sort1]]])]]
	    dl_local xplaces [dl_select $xs [dl_eq $g:sort1 [eval dl_create [dl_datatype $g:sort1] $firstxplace]]]
#	    dl_local xplaces [dl_select $xs [dl_eq $g:sort1 [dl_get [dl_unique $g:sort1] [expr int([dl_median [dl_index [dl_unique $g:sort1]]])]]]]
	    if {![expr [dl_length [dl_unique $g:sort1]]%2]} {
		dl_local xplaces [dl_add $xplaces .5]; # shift to middle of bars
	    }
	    dl_local c [get_colors [dl_length [dl_unique $g:sort1]]]
	    dl_local barcolors [dl_choose $c [dl_recode $g:sort1]]
	} else {
	    dl_local xplaces $xs
	    dl_local c [get_colors 1]
	    dl_local barcolors [dl_repeat $c [dl_length $g:sort0]]
	}

	# make a barchart (the way we do this depends on if we have one or two data sets)
	set i -1
	if [dl_exists $g:sort1] {
	    foreach v [dl_tcllist [dl_unique $g:sort1]] {
		dl_local ref [dl_eq $g:sort1 [dl_create [dl_datatype $g:sort1] $v]]
		set x [dlp_addXData $p [dl_select $xs $ref]]
		set y [dlp_addYData $p [dl_select $g:data $ref]]
		if {[dl_first $g:errfunc] != "none"} {
		    set e [dlp_addErrData $p [dl_select $g:err $ref]]
		    dlp_addErrbarCmd $p "$x $y" $e
		}
		#dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors [incr i]] -linecolor $::colors(white) -width 1 -interbar 1
		dlp_draw $p bars "$x $y" -fillcolor [dl_get $c [incr i]] -linecolor $::colors(white) -width 1 -interbar 1
		
#		# for a putative ::gr::lines function
#		dlp_draw $p markers "$x $y" -color [dl_get $barcolors [incr i]]
#		dlp_draw $p lines "$x $y" -linecolor [dl_get $barcolors $i]
	    }
	} else {
	    set x [dlp_addXData $p $xs]
	    set y [dlp_addYData $p $g:data]
	    if {[dl_first $g:errfunc] != "none"} {
		set e [dlp_addErrData $p $g:err]
		dlp_addErrbarCmd $p "$x $y" $e
	    }
	    #dlp_draw $p bars "$x $y" -fillcolor [dl_get $barcolors [incr i]] -linecolor $::colors(white) -width 1 -interbar 1
	    dlp_draw $p bars "$x $y" -fillcolor [dl_get $c [incr i]] -linecolor $::colors(white) -width 1 -interbar 1
	}
	
	if [dl_first $g:showcount] {
	    # we've plotted the bars, not mark each bar with N samples in that bar's mean/median at base of bar
	    dlp_set $p xN $xs
	    dlp_set $p yN [dl_repeat .01 [dl_length $xs]]
	    dlp_set $p tN $g:count
	    dlp_winypostcmd $p {0 1} "dlg_text %p:xN %p:yN %p:tN -ori 1 -just -1"
	}

	# xlabels
	set xmin [expr [dl_min $xs]-2]
	dlp_setxrange $p $xmin [expr [dl_max $xs]+2]
	dlp_set $p xticks $xmin
	dlp_set $p xsubticks $xmin
	dlp_set $p xplaces $xplaces
	dlp_set $p xnames $xnames

	# axis labels
	dlp_set $p xtitle \"[dl_get $g:tags 0]\"
	dlp_set $p ytitle \"[dl_get $g:vals 0]\"
	dlp_set $p title $title

	# labels (will match up with farthest right box-and-whisker)
	dlp_set $p xlab [expr [dl_max $xs]+1]
	dlp_set $p ylab [dl_last $g:data]
	dlp_set $p tlab [dl_slist [string range [dl_first $g:function] 3 end]]
	if {[dl_first $g:errfunc] != "none"} {
	    dl_append $p:xlab [dl_first $p:xlab]
	    dl_append $p:ylab [expr [dl_last $g:err] + [dl_last $g:data]]
	    dl_append $p:tlab [string range [dl_first $g:errfunc] 3 end]
	}
	dlp_cmd $p "dlg_text %p:xlab %p:ylab %p:tlab -just -1"

	# color legend for second sort variable
	if [dl_exists $g:sort1] {
	    #set n [expr 1 + [dl_length [dl_unique $barcolors]]]
	    set n [expr 1 + [dl_length $c]]
	    dlp_set $p xleg [dl_repeat .98 $n]
	    dlp_set $p yleg [dl_series .95 [expr .95-.05*[expr $n-1]] -.05]
	    dlp_set $p tleg [dl_prepend [eval dl_slist [dl_tcllist [dl_unique $g:sort1]]] [dl_get $g:tags 1]]
	    #dlp_set $p cleg [dl_prepend [dl_select $barcolors [dl_eq [dl_rank $barcolors] 0]] $::colors(white)]
	    dlp_set $p cleg [dl_prepend $c $::colors(white)]

	    dlp_wincmd $p {0 0 1 1} "dlg_text %p:xleg %p:yleg %p:tleg -colors %p:cleg -size 12 -just 1"
	}

	return $p
    }


    proc plot_lineplot { g draw } {
	if [dl_exists $g:sort2] {
	    set n [dl_length [dl_unique $g:sort2]]
	    dl_local yports [dl_reverse [dl_series 0 1 [expr 1./$n]]]

	    set i -1
	    set plots ""
	    foreach v [dl_tcllist [dl_unique $g:sort2]] {
		set g2 [dg_copySelected $g [dl_eq $g:sort2 [dl_create [dl_datatype $g:sort2] $v]] 1]
		lappend plots [set p [create_lineplot_plot $g2 "[dl_get $g2:tags 2] = $v"]]
		dg_delete $g2
	    }
	} else {
	    lappend plots [set p [create_lineplot_plot $g]]
	}
 	# equalize yaxes
	match_yaxes_replot $plots $draw $g
	return $plots
    }
    proc create_lineplot_plot { g {title ""}} {
	set p [dlp_newplot]

	# now that we have extraced actual tags...
	dl_local xnames   [dl_unique $g:sort0]


	# set up appropriate x
	dl_local xs [dl_recode $g:sort0]

	if {[dl_first $g:veridical_x] && ( [dl_datatype $g:sort0] == "float" || [dl_datatype $g:sort0] == "long" ) } {
	    # then use the values given
	    dl_local xs $g:sort0
	}

	# now we have a list of the actual final xs, save for xplaces withing plot dg
	dl_local xplaces_final [dl_unique $xs]

	# the following would be if we want to jitter the data at points so they don't overlap
	if {[dl_exists $g:sort1] && [dl_first $g:stagger]} {
	    set shift .05
	    dl_local xs [dl_add $xs [dl_mult $shift [dl_recode $g:sort1]]]
	} else {
	    set shift 0
	}

	# adjust xplaces and colors depending on how many sort variables we are plotting
	if [dl_exists $g:sort1] {
	    set firstxplace [dl_get [dl_unique $g:sort1] [expr int([dl_median [dl_index [dl_unique $g:sort1]]])]]
	    dl_local xplaces [dl_select $xs [dl_eq $g:sort1 [eval dl_create [dl_datatype $g:sort1] $firstxplace]]]

#	    dl_local xplaces [dl_select $xs [dl_eq $g:sort1 [dl_get [dl_unique $g:sort1] [expr int([dl_median [dl_index [dl_unique $g:sort1]]])]]]]
	    if {![expr [dl_length [dl_unique $g:sort1]]%2]} {
		dl_local xplaces [dl_add $xplaces $shift]; # shift to middle of bars
	    }
	    dl_local c [get_colors [dl_length [dl_unique $g:sort1]]]
	    dl_local linecolors [dl_choose $c [dl_recode $g:sort1]]
	} else {
	    dl_local xplaces $xs
	    dl_local c [get_colors 1]
	    dl_local linecolors [dl_repeat $c [dl_length $g:sort0]]
	}

	# make a lineplot (the way we do this depends on if we have one or two data sets)
	set i -1
	if [dl_exists $g:sort1] {
	    foreach v [dl_tcllist [dl_unique $g:sort1]] {
		dl_local ref [dl_eq $g:sort1 [dl_create [dl_datatype $g:sort1] $v]]
		set x [dlp_addXData $p [dl_select $xs $ref]]
		set y [dlp_addYData $p [dl_select $g:data $ref]]
		if {[dl_first $g:errfunc] != "none"} {
		    set e [dlp_addErrData $p [dl_select $g:err $ref]]
		    dlp_addErrbarCmd $p "$x $y" $e
		}

		#dlp_draw $p markers "$x $y" -color [dl_get $linecolors [incr i]] -size 5 -marker fcircle
		#dlp_draw $p lines "$x $y" -linecolor [dl_get $linecolors $i] -lwidth 200
		dlp_draw $p markers "$x $y" -color [dl_get $c [incr i]] -size 5 -marker fcircle
		dlp_draw $p lines "$x $y" -linecolor [dl_get $c $i] -lwidth 200

	    }
	} else {
	    set x [dlp_addXData $p $xs]
	    set y [dlp_addYData $p $g:data]
	    if {[dl_first $g:errfunc] != "none"} {
		set e [dlp_addErrData $p $g:err]
		dlp_addErrbarCmd $p "$x $y" $e
	    }

	    #dlp_draw $p markers "$x $y" -color [dl_get $linecolors [incr i]] -size 5 -marker fcircle
	    #dlp_draw $p lines "$x $y" -linecolor [dl_get $linecolors $i] -lwidth 200
	    dlp_draw $p markers "$x $y" -color [dl_get $c [incr i]] -size 5 -marker fcircle
	    dlp_draw $p lines "$x $y" -linecolor [dl_get $c $i] -lwidth 200
	}

	###########################################################################################
	# below is for plotting y-value in center of mark instead of count
	if 0 {
	    # we've plotted the lines, mark each marker with N samples in the center of that marker
	    dlp_set $p xN $xs
	    dlp_set $p yN $g:data
	    dlp_set $p tN $g:data
	    dlp_set $p cN $linecolors
	    dlp_postcmd $p "dlg_text %p:xN %p:yN %p:tN -just 0"
	}
	###########################################################################################
	if [dl_first $g:showcount] {
	    # we've plotted the lines, mark each marker with N samples in the center of that marker
	    dlp_set $p xN $xs
	    dlp_set $p yN $g:data
	    dlp_set $p tN $g:count
	    dlp_set $p cN $linecolors
	    dlp_postcmd $p "dlg_text %p:xN %p:yN %p:tN -just 0"
	}

	# xlabels
	set xmin [expr [dl_min $xs]-2]
	dlp_setxrange $p $xmin [expr [dl_max $xs]+2]
	dlp_set $p xticks $xmin
	#dlp_set $p xsubticks $xplaces
	#dlp_set $p xplaces $xplaces
	dlp_set $p xsubticks $xplaces_final
	dlp_set $p xplaces $xplaces_final
	dlp_set $p xnames $xnames

	# axis label
	dlp_set $p xtitle \"[dl_get $g:tags 0]\"
	dlp_set $p ytitle \"[dl_get $g:vals 0]\"
	dlp_set $p title $title

	# labels (will match up with farthest right box-and-whisker)
	dlp_set $p xlab [expr [dl_max $xs]+1]
	dlp_set $p ylab [dl_last $g:data]
	dlp_set $p tlab [dl_slist [string range [dl_first $g:function] 3 end]]
	if {[dl_first $g:errfunc] != "none"} {
	    dl_append $p:xlab [dl_first $p:xlab]
	    dl_append $p:ylab [expr [dl_last $g:err] + [dl_last $g:data]]
	    dl_append $p:tlab [string range [dl_first $g:errfunc] 3 end]
	}
	dlp_cmd $p "dlg_text %p:xlab %p:ylab %p:tlab -just -1"

	# color legend for second sort variable
	if [dl_exists $g:sort1] {
	    #set n [expr 1 + [dl_length [dl_unique $linecolors]]]
	    set n [expr 1 + [dl_length $c]]
	    dlp_set $p xleg [dl_repeat .98 $n]
	    dlp_set $p yleg [dl_series .95 [expr .95-.05*[expr $n-1]] -.05]
	    dlp_set $p tleg [dl_prepend [eval dl_slist [dl_tcllist [dl_unique $g:sort1]]] [dl_get $g:tags 1]]
	    #dlp_set $p cleg [dl_prepend [dl_select $linecolors [dl_eq [dl_rank $linecolors] 0]] $::colors(white)]
	    dlp_set $p cleg [dl_prepend $c $::colors(white)]
	    dlp_wincmd $p {0 0 1 1} "dlg_text %p:xleg %p:yleg %p:tleg -colors %p:cleg -size 12 -just 1"
	}

	return $p
    }

    proc plot_xyscatter { g draw } {
	set p [create_xyscatter_plot $g]
	# plot it if we want to
	if $draw {
	    dlp_plot $p
	}
	return $p
    }

  
    proc create_xyscatter_plot { g } {
	set p [dlp_newplot]

	# line (do first so it is behind data
	if {[dl_first $g:line] !="none"} {
	    set line_type [dl_first $g:line]
	    dl_local linex "[dl_first $g:xmin] [dl_first $g:xmax]"
	    set xL [dlp_addXData $p $linex]
	    switch -exact -- $line_type {
		lsfit {
		    set ls_line [dl_lsfit $g:xdata $g:ydata]
		    set slope [lindex $ls_line 0]
		    set offset [lindex $ls_line 1]
		    set r2 [lindex $ls_line 2]
		}
		x=y {
		    set slope 1
		    set offset 0
		}
		x=-y {
		    set slope -1
		    set offset 0
		}
	    }
	    set yL [dlp_addYData $p [dl_add [dl_mult $linex $slope] $offset]]
	    dlp_draw $p lines "$xL $yL" -lstyle 3 -linecolor $::colors(gray)
	    dlp_winpostcmd $p {0 0 1 1} "dlg_text .80 .95 $line_type -just -1 -color $::colors(gray)"
	    dlp_winpostcmd $p {0 0 1 1} "dlg_text .80 .90 slope=[format %.2f $slope] -just -1 -color $::colors(gray)"
	    dlp_winpostcmd $p {0 0 1 1} "dlg_text .80 .85 offset=[format %.2f $offset] -just -1 -color $::colors(gray)"
	    if {$line_type == "lsfit"} {
		dlp_winpostcmd $p {0 0 1 1} "dlg_text .80 .80 r^2=[format %.2f $r2] -just -1 -color $::colors(gray)"
	    }
	    
	}

	# xyscatter data
	set x [dlp_addXData $p $g:xdata]
	set y [dlp_addYData $p $g:ydata]
	dlp_draw $p markers "$x $y" -marker fcircle -size 5

	# xlabels
	dlp_setxrange $p [dl_first $g:xmin] [dl_first $g:xmax]
	dlp_setyrange $p [dl_first $g:ymin] [dl_first $g:ymax]

	# axis labels
	dlp_set $p xtitle \"[dl_get $g:xtitle 0]\"
	dlp_set $p ytitle \"[dl_get $g:ytitle 0]\"

	return $p
    }
}