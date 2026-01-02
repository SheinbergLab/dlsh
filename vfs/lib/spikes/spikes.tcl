# AUTHORS
#   APR 2008 - LW

package provide spikes 1.0

namespace eval ::spikes { } {

    proc raw { g chans sortby args } {
	# %HELP%
	# ::spikes::raw <g> <chans> <sortby> ?<args>?
	#
	# This function's chief purpose is to allow you to recover raw spike density
	# functions, spike times and spike counts sorted by variables of your choosing (up to 2)
	#
	#    Take spikes for channels <chans> in group <g> and return raw data sorted by <sortby>
	#    <sortby> can have up to two sort variables.  
	#
	#    optional arguments <defaults>:
	#    -pretime <-50> time before align variable to get
	#    -posttime <250> time after align variable to get
	#    -align <stimon> what time to align spikes on.
	#    -function <parzenz> function to use for sdf estimate, 'sdf' or 'parzens' 
	#    -res <2> resolution in ms for sdf/parzens 
	#    -sd <10> standard deviation in ms of sdf/parzens kernel 
	#    -nsd <2.5> number of standard devistions for sdf/parzen kernel 
	#    -limit <none> if you wish to limit the group to a subset of trials enter a dl list for selection
	# %END%

	#do we have spike data to analyze?
	if ![dl_exists $g:spk_times] {
	    error "No spike data to analyze"
	}
	
	#build initial array
	::helpspikes::CreateInitArgsArray

	#modify this array according to user set options
	if { [llength $args] } {
	    ::helpspikes::ParseArgs [list $args]
	}

	#are we limiting our analysis to a particular subset of trials?
	if {$::spikes::Args(limit) != "none"} {
	    set g [dg_copySelected $g $::spikes::Args(limit) 1]
	} 

	#check channels
	if {$chans == "all"} {
	    set chans [dl_tcllist [dl_unique [dl_unpack $g:spk_channels]]]
	}

	#create sortlists
	dl_local sortlists [::helpspikes::MakeSortLists $g $sortby]

	set g1 [::helpspikes::CreateSpikeAnalGroup $g $chans $sortlists]
	
	#delete the copied list
	if {$::spikes::Args(limit) != "none"} {
	    dg_delete $g
	}

	return $g1	
    }
}
	
namespace eval ::helpspikes { } {
     
    proc MakeSortLists {g sb} {
	# are we sorting by 1 or 2 lists?
	if {[llength $sb] > 2} {
	    error "::spikes:: - can't handle more than two sort variables at a time"
	}
	if {[llength $sb] == 1} {
	    set label1 [lindex $sb 0]
	    dl_local split_vals1 $g:$label1 
	    set label2 sorted_by
	    dl_local split_vals2 [dl_repeat [dl_slist "$label1"] [dl_length $g:spk_times]]
	} else {
	    set label1 [lindex $sb 0]
	    dl_local split_vals1 $g:$label1
	    set label2 [lindex $sb 1]
	    dl_local split_vals2 $g:$label2
	}
	dl_return [dl_llist [dl_llist [dl_slist $label1] $split_vals1] [dl_llist [dl_slist $label2] $split_vals2]]
    }

    proc CreateSpikeAnalGroup {g chans sortlists} {
	
	set align $::spikes::Args(align)
	
	dl_local label1 [dl_llist]
	dl_local label2 [dl_llist]
	dl_local allchans_sorted_smooth_mean [dl_llist]
	dl_local allchans_sorted_smooth [dl_llist]
	dl_local allchans_sorted_smooth_counts [dl_llist]
	dl_local allchans_sorted_times [dl_llist]
	dl_local allchans_sorted_counts_mean [dl_llist]
	dl_local allchans_sorted_counts [dl_llist]
	dl_local channel_ids [dl_ilist]
	
	foreach chan $chans {

	    dl_local aligned_spikes           [dl_sub [::helpspikes::spike_extract $g $chan] $g:$align]
	    dl_local smooth_spikes            [::helpspikes::smooth_spikes $aligned_spikes]
	    dl_local restricted_spikes        [dl_select $aligned_spikes [dl_betweenEq $aligned_spikes $::spikes::Args(pretime) \
								       $::spikes::Args(posttime)]]
	    dl_local restricted_spike_counts  [dl_counts $restricted_spikes $::spikes::Args(pretime) $::spikes::Args(posttime)]

	    #sorting
	    set name1 [dl_get $sortlists:0 0]
	    dl_local sb1 [dl_get $sortlists:0 1]
	    set name2 [dl_get $sortlists:1 0]
	    dl_local sb2 [dl_get $sortlists:1 1]

	    dl_local sorted_smooth_mean            [dl_sortedFunc $smooth_spikes "$sb1 $sb2"]
	    dl_local sorted_smooth                 [dl_sortByLists $smooth_spikes $sb1 $sb2] 
	    dl_local sorted_times                  [dl_sortByLists $restricted_spikes $sb1 $sb2]
	    dl_local sorted_counts_mean            [dl_sortedFunc $restricted_spike_counts "$sb1 $sb2"]
	    dl_local sorted_counts                 [dl_sortByLists $restricted_spike_counts $sb1 $sb2]

	    dl_append $label1 $sorted_smooth_mean:0
	    dl_append $label2 $sorted_smooth_mean:1

	    dl_append $allchans_sorted_smooth_mean       $sorted_smooth_mean:2
	    dl_append $allchans_sorted_smooth            $sorted_smooth
	    dl_append $allchans_sorted_smooth_counts     [dl_mult $sorted_smooth [expr $::spikes::Args(res)/1000.]]
	    dl_append $allchans_sorted_times             $sorted_times
	    dl_append $allchans_sorted_counts_mean       $sorted_counts_mean:2
	    dl_append $allchans_sorted_counts            $sorted_counts

	    dl_append $channel_ids $chan
	}
	
	#creating out group
	set out [dg_create]
	dl_set $out:time                    [dl_repeat [dl_llist [dl_series $::spikes::Args(pretime) \
								      $::spikes::Args(posttime) $::spikes::Args(res)]] [llength $chans]]
	dl_set $out:channel_ids              $channel_ids
	dl_set $out:raw_spktimes             $allchans_sorted_times
	dl_set $out:sdf_mean                 $allchans_sorted_smooth_mean
	dl_set $out:sdf_ind                  $allchans_sorted_smooth
	dl_set $out:sdf_counts               $allchans_sorted_smooth_counts
	dl_set $out:spkcounts_mean           $allchans_sorted_counts_mean
	dl_set $out:spkcounts_ind            $allchans_sorted_counts
	dl_set $out:label1                   $label1
	dl_set $out:label2                   $label2
	return $out
    }
    
    proc CreateInitArgsArray {} {
    	set ::spikes::Args(pretime) -50
	set ::spikes::Args(posttime) 2000
	set ::spikes::Args(align) "stimon"
	set ::spikes::Args(function) "parzens"
	set ::spikes::Args(res) 2
	set ::spikes::Args(sd) 10
	set ::spikes::Args(nsd) 2.5
	set ::spikes::Args(limit) "none"
    }
    
    proc ParseArgs {inargs} {
	set args [lindex $inargs 0]
	foreach a $args i [dl_tcllist [dl_fromto 0 [llength $args]]] {
	    if {(![string match [string index $a 0] "-"]) || \
		     [string is integer $a]} {
		continue
	    } else {
		switch -exact -- "$a" {
		    -reset {::spikes::CreateInitArgsArray}
		    -pretime { set ::spikes::Args(pretime) [lindex $args [expr $i + 1]]}
		    -posttime { set ::spikes::Args(posttime) [lindex $args [expr $i + 1]]}
		    -align { set ::mtsepc::Args(align) [lindex $args [expr $i + 1]]}
		    -function { set ::spikes::Args(function) [lindex $args [expr $i + 1]]}
		    -res { set ::spikes::Args(res) [lindex $args [expr $i + 1]]}
		    -sd { set ::spikes::Args(sd) [lindex $args [expr $i + 1]]}
		    -nsd { set ::spikes::Args(nsd) [lindex $args [expr $i + 1]]}
		    -limit { set ::spikes::Args(limit) [lindex $args [expr $i + 1]]}
		    default {puts "Illegal option: $a\nShould be reset, pretime, posttime, align, function, res, sd, nsd, limit.\nWill work ignoring your entry."}
		}
	    }
	}
    }

    proc spike_extract { g channel } {
	dl_local spk_selector [dl_eq $g:spk_channels $channel]
	dl_local all_spk_times [dl_select $g:spk_times $spk_selector]
	dl_return $all_spk_times
    }

    proc smooth_spikes { s } {
	dl_local smoothed [dl_mult [eval dl_$::spikes::Args(function) $s $::spikes::Args(pretime) \
			       $::spikes::Args(posttime) $::spikes::Args(sd) $::spikes::Args(nsd) $::spikes::Args(res)] 1000.]
	dl_return $smoothed
    }
}