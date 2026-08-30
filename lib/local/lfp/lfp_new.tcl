# AUTHORS
#   APR 2008 - LW

package provide lfp 3.0
load_Stats;

namespace eval ::lfp { } {

    proc rescale { g gain } {
	set scale [expr 10*1000000./($gain*16348.)]
	dl_set $g:eeg [dl_mult $g:eeg $scale]
    }

    proc plot { g chan sortby args } {
	# %HELP%
	# ::lfp::plot <g> <chan> <sortby> ?<args>?
	#    take lfp for channel <chan> in group <g> and plot sorted by <sortby>
	#    <sortby> can have up to two sort variables.  
	#    optional arguments <defaults>:
	#    -pretime <-50> time before align variable to get eeg data
	#    -posttime <500> time after align variable to get eeg data
	#    -align <stimon> what time to align lfp on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter a dl list for selection
	#      this will call dg_copySelected and work on the subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero relative to something other than -align list
	#    -zero_range <"-50 0"> enter a two item tcl list of the 
	#      times to use as the basis for zeroing the eeg sample.  
	#      Times are relative to -align or overwritten by -zero_align list
	# %END%

	#do we have EEG data to analyze?
	if ![dl_exists $g:eeg] {
	    error "No eeg data to analyze.  Try loading with -eeg egz."
	}
	
	#build initial array
	::helplfp::CreateInitArgsArray

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helplfp::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::lfp::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::lfp::Args(limit) 1]
	}

	#make the sortlists
	dl_local sortlists [::helplfp::MakeSortLists $g $sortby]

	#create the plot group with its data
	set pg [::helplfp::CreateLFPPlotGroup $g $chan $sortlists]

	set plots "[::helplfp::GenerateOverlayPlots $pg]"
#	closeAllTabs
	clearwin

	#determine how many panels we're going to need
	if {[llength $sortby] == 1} {
	    dlp_setpanels 1 1
	    set p [lindex $plots 0]
	    dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
	    dlp_plot $p
	} else {
	    set cur_ymax 0
	    set cur_ymin 0
	    foreach p $plots {
		if { [dl_max $p:ydata] > $cur_ymax } { set cur_ymax [dl_max $p:ydata] }
		if { [dl_min $p:ydata] < $cur_ymin } { set cur_ymin [dl_min $p:ydata] }
	    }
	    set panel_num 0
	    dlp_setpanels  [expr int(ceil([dl_length [dl_unique $sortlists:1:1]]./2.))] [expr int(ceil([dl_length [dl_unique $sortlists:1:1]]./2.))] 
	    foreach p $plots {
		#the xrange is usually pretty static whereas the yrange not so much
		dlp_setxrange $p [dl_min $p:xdata] [dl_max $p:xdata]
		dlp_setyrange $p [expr $cur_ymin - .1*abs($cur_ymin)] [expr $cur_ymax + .1*abs($cur_ymax)]
		dlp_subplot $p $panel_num
		incr panel_num
	    }
	}

	#clean up
	dg_delete $pg
	if {$::lfp::Args(limit) != "none"} {
	    dg_delete $g
	}
    }

    proc raw { g chan sortby args } {
	# %HELP%
	# ::lfp::raw <g> <chan> <sortby> ?<args>?
	#    take lfp for channel <chan> in group <g> and return raw data sorted by <sortby>
	#    <sortby> can have up to two sort variables.  
	#    optional arguments <defaults>:
	#    -pretime <-50> time before align variable to get eeg data
	#    -posttime <500> time after align variable to get eeg data
	#    -align <stimon> what time to align lfp on.
	#    -limit <none> if you wish to limit the group to a subset of trials enter a dl list for selection
	#      this will call dg_copySelected and work on the subgroup which is destroyed at the end.
	#    -zero_align <same> if you want to zero relative to something other than -align list
	#    -zero_range <"-50 0"> enter a two item tcl list of the 
	#      times to use as the basis for zeroing the eeg sample.  
	#      Times are relative to -align or -zero_align list
	# %END%

	#do we have EEG data to analyze?
	if ![dl_exists $g:eeg] {
	    error "No eeg data to analyze.  Try loading with -eeg egz."
	}
	
	#build initial array
	::helplfp::CreateInitArgsArray

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helplfp::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::lfp::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::lfp::Args(limit) 1]
	} 

	#create sortlists
	dl_local sortlists [::helplfp::MakeSortLists $g $sortby]

	set g1 [::helplfp::CreateLFPAnalGroup $g $chan $sortlists]
	
	#delete the copied list
	if {$::lfp::Args(limit) != "none"} {
	    dg_delete $g
	}

	return $g1	
    }
}
	
namespace eval ::helplfp { } {
    set cols(0) 3
    set cols(1) 5
    set cols(2) 10
    set cols(3) 14
    set cols(4) 4
    set cols(5) 2
    set cols(6) 1
    set cols(7) 12
    set cols(99) 8

    proc GetColor { } {
	incr ::helplfp::vars(colorcntr)
	if {$::helplfp::vars(colorcntr) > 7} {set ::helplfp::vars(colorcntr) 0; incr ::helplfp::vars(linecntr) 2}
	return $::helplfp::vars(colorcntr)
    }
    
    proc MakeSortLists {g sb} {
	# are we sorting by 1 or 2 lists?
	if {[llength $sb] > 2} {
	    error "::lfp:: - can't handle more than two sort variables at a time"
	}
	if {[llength $sb] == 1} {
	    set label1 [lindex $sb 0]
	    dl_local split_vals1 $g:$label1 
	    set label2 sorted_by
	    dl_local split_vals2 [dl_repeat [dl_slist "$label1"] [dl_length $g:eeg]]
	} else {
	    set label1 [lindex $sb 0]
	    dl_local split_vals1 $g:$label1
	    set label2 [lindex $sb 1]
	    dl_local split_vals2 $g:$label2
	}
	dl_return [dl_llist [dl_llist [dl_slist $label1] $split_vals1] [dl_llist [dl_slist $label2] $split_vals2]]
    }

   proc CreateLFPPlotGroup {g chan sortlists} {

	#get lfp signal first
	dl_local alleeg [::helplfp::get_lfp $g $chan]

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]	
	dl_local eeg [dl_sortedFunc $alleeg "$sb1 $sb2"]
	
	#creating out group
	set out [dg_create]
	set samp_rate [expr [lindex [split [dl_tcllist $g:eeg_info]] 2] /1000.]
	dl_set $out:xdata [dl_llist [dl_fromto $::lfp::Args(pretime) $::lfp::Args(posttime) $samp_rate]]
	dl_set $out:ydata $eeg:2
	dl_set $out:label1 $eeg:0
	dl_set $out:label2 $eeg:1
	dl_set $out:label1_counts [dl_llist $name1 $sb1]
	dl_set $out:label2_counts [dl_llist $name2 $sb2]
	dl_set $out:xtitle [dl_slist Time (ms)]
	dl_set $out:ytitle [dl_slist Amplitude (uV)]  
	return $out
    }

    proc CreateLFPAnalGroup {g chan sortlists} {

	#get lfp signal first
	dl_local alleeg [::helplfp::get_lfp $g $chan]

	#sorting
	set name1 [dl_get $sortlists:0 0]
	dl_local sb1 [dl_get $sortlists:0 1]
	set name2 [dl_get $sortlists:1 0]
	dl_local sb2 [dl_get $sortlists:1 1]	
	dl_local eeg [dl_sortByLists $alleeg $sb1 $sb2]
	
	#creating out group
	set out [dg_create]
	set samp_rate [expr [lindex [split [dl_tcllist $g:eeg_info]] 2] /1000.]
	dl_set $out:xdata [dl_llist [dl_fromto $::lfp::Args(pretime) $::lfp::Args(posttime) $samp_rate]]
	dl_set $out:ydata $eeg
	dl_set $out:label1 [dl_cross [dl_unique $sb1] [dl_unique $sb2]]:0
	dl_set $out:label2 [dl_cross [dl_unique $sb1] [dl_unique $sb2]]:1
	dl_set $out:xtitle [dl_slist Time (ms)]
	dl_set $out:ytitle [dl_slist Amplitude (uV)]  
	return $out
    }

    proc GenerateOverlayPlots {g} {
	set plots ""
	foreach main [dl_tcllist [dl_unique $g:label2]] { 
	    dl_local splits [dl_select $g:label1 [dl_eq $g:label2 $main]]
	    set p [dlp_newplot]
	    dlp_addXData $p $g:xdata
	    set ::helplfp::vars(plotcntr) 0
	    set ::helplfp::vars(colorcntr) 0
	    foreach split [dl_tcllist $splits] {
		dl_local y [dl_select $g:ydata [dl_and [dl_eq $g:label1 $split] [dl_eq $g:label2 $main]]]
		dlp_addYData $p $y
		set w $::helplfp::vars(plotcntr)
		set k [::helplfp::GetColor]
		dlp_draw $p lines $w -linecolor $::helplfp::cols($k) -lwidth 250
		set xspot .95
		set n [dl_sum [dl_and [dl_eq $g:label1_counts:1 $split] [dl_eq $g:label2_counts:1 $main]]]
		dlp_wincmd $p {0 0 1 1} "dlg_text $xspot [expr .93-${::helplfp::vars(plotcntr)}*.05] {$split ($n)} -color $::helplfp::cols($k) -just 1"
		incr ::helplfp::vars(plotcntr)
	    }
	    
	    dlp_wincmd $p {0 0 1 1} "dlg_text $xspot .97 {[dl_tcllist $g:label1_counts:0] (N)} -color $::colors(gray) -just 1"
	    dlp_set $p xtitle "[dl_tcllist $g:xtitle]"
	    dlp_set $p ytitle "[dl_tcllist $g:ytitle]"
	    
	    #main title
	    dlp_set $p title "[dl_tcllist $g:label2_counts:0] :: $main"
	    lappend plots $p
    	}
	return $plots
    }
    
    proc CreateInitArgsArray {} {
    	set ::lfp::Args(pretime) -50
	set ::lfp::Args(posttime) 500
	set ::lfp::Args(align) "stimon"
	set ::lfp::Args(zero_align) "same"
	set ::lfp::Args(zero_range) "-50 0"
	set ::lfp::Args(limit) "none"
    }
    
    proc ParseArgs {inargs} {
	set args [lindex $inargs 0]
	#set args $inargs
	foreach a $args i [dl_tcllist [dl_fromto 0 [llength $args]]] {
	    if {(![string match [string index $a 0] "-"]) || \
		     [string is integer $a]} {
		continue
	    } else {
		switch -exact -- "$a" {
		    -reset {::helplfp::CreateInitArgsArray}
		    -pretime { set ::lfp::Args(pretime) [lindex $args [expr $i + 1]]}
		    -posttime { set ::lfp::Args(posttime) [lindex $args [expr $i + 1]]}
		    -align { set ::mtsepc::Args(align) [lindex $args [expr $i + 1]]}
		    -zero_align { set ::lfp::Args(zero_align) [lindex $args [expr $i + 1]]}
		    -zero_range { set ::lfp::Args(zero_range) [lindex $args [expr $i + 1]]}
		    -limit { set ::lfp::Args(limit) [lindex $args [expr $i + 1]]}
		    default {puts "Illegal option: $a\nShould be reset, pretime, posttime, align, zero_align, zero_range, limit.\nWill plot ignoring your entry."}
		}
	    }
	}
    }
    
    #extract the desired raw LFP signal
    proc get_lfp { g chan } {
	set range "$::lfp::Args(pretime) $::lfp::Args(posttime)"
	set align $::lfp::Args(align)
	set z_r $::lfp::Args(zero_range)
	set z_2 $::lfp::Args(zero_align)
	
	#get the lfp 
	dl_local alleeg_raw [dl_unpack [dl_select $g:eeg [dl_eq $g:eeg_chans $chan]]] 
	dl_local alleeg_time [dl_unpack [dl_select $g:eeg_time [dl_eq $g:eeg_chans $chan]]]
	
	#to what are we zeroing?
	if { $z_2 != "same" } {
	    dl_local alleeg_time_zeroed [eval dl_sub $alleeg_time $g:$z_2]
	} else {
	    dl_local alleeg_time_zeroed [eval dl_sub $alleeg_time $g:$align]
	}
	set samp_rate [expr [lindex [split [dl_tcllist $g:eeg_info]] 2] /1000.]
	set samp_num [expr ([lindex $range 1] - [lindex $range 0]) / $samp_rate]
	dl_local first_samps [dl_minIndices [dl_abs [dl_sub $alleeg_time_zeroed [lindex $range 0]]]]
	dl_local add2 [dl_repeat [dl_llist [dl_fromto 0  $samp_num]] [dl_length $first_samps]]
	dl_local get_list [dl_int [dl_add $add2 $first_samps]]
	dl_local alleeg [dl_choose $alleeg_raw $get_list]

	###where zeroing takes place
	if {[llength $z_r] == 2} {
	    set samp_num_zero [expr ([lindex $z_r 1] - [lindex $z_r 0]) / $samp_rate]
	    dl_local zero_pre [dl_minIndices [dl_abs [dl_sub $alleeg_time_zeroed [lindex $z_r 0]]]]
	    dl_local add2z [dl_repeat [dl_llist [dl_fromto 0  $samp_num_zero]] [dl_length $zero_pre]]
	    dl_local get_zero [dl_int [dl_add $add2z $zero_pre]]
	    dl_local zr [dl_choose $alleeg_raw $get_zero]
	    dl_local alleeg [dl_sub $alleeg [dl_means $zr]]
	}
	dl_return $alleeg 
    }

    #code for creating combinations
    proc doublets { n } {
	# First create all possible combinations
	dl_local l0 [dl_replicate [dl_fromto 0 $n] $n] 
	dl_local l1 [dl_repeat [dl_fromto 0 $n] $n]
	
	# Now combine into doublets and sort
	dl_local all [dl_combineLists $l0 $l1]
	dl_local sorted [dl_sort $all]
	
	# Now create unique ids by multiplying and summing
	dl_local mult_sorted [dl_mult $sorted [dl_llist "1 $n"]]
	dl_local unique_ids [dl_sums $mult_sorted]
	
	# Find indices of first doublet for each unique element
	dl_local u [dl_eq [dl_rank $unique_ids] 0]
	
	# Find doublets that don't have repeats
	dl_local alldiff [dl_eq [dl_lengths [dl_unique $sorted]] 2]
	
	dl_local good [dl_and $u $alldiff]
	
	# Return selected combinations
	dl_return [dl_llist \
		       [dl_select $l0 $good] \
		       [dl_select $l1 $good]]
    }
    
    proc all_doublets { l } {
	set n [dl_length $l]
	dl_local indices [::helplfp::doublets $n]
	dl_return [dl_choose $l $indices]
    }
    

}