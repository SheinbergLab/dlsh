#
# Tools for getting grasp object info from the grasp database (graspdb)
#
# The grasp server (running on QNX) reads the UDP stream from
# the arduino board and places each data sample into the dataserver (ess_ds).
# The samples are stored in a data point called "grasp:data".  These are
# stored in log files and converted alongside normal data.
#
#  vals:0 -> left channel
#  vals:1*256+vals:2 -> left timestamp
#  vals:3-vals:14 -> values for left 12 channels
#  vals:15 -> right channel
#  vals:16*256+vals:17 -> right timestamp
#  vals:18-vals:30 -> values for right 12 channels
#
#
# To be automatically processed during load_data, we define the proc:
#   grasp::postfilecmd
# below
#

package require sqlite3
package require dlsh
package provide grasp 1.0

namespace eval grasp {

    if [file exists c:/stimuli/grasp/objects.db] {
	set dbfile c:/stimuli/grasp/objects.db
    } elseif [file exists l:/stimuli/grasp/objects.db] {
	set dbfile l:/stimuli/grasp/objects.db
    } elseif [file exists /Volumes/Labfiles/stimuli/grasp/objects.db] {
	set dbfile /Volumes/Labfiles/stimuli/grasp/objects.db
    } else {
	set dbfile /shared/lab/stimuli/grasp/objects.db
    }
    catch { grasp_dbcmd close }
    sqlite3 grasp_dbcmd $dbfile
    
    proc get_object_info { dbcmd obj_ids } {
	set dg [dg_create]
	dl_set $dg:ids $obj_ids
	dl_set $dg:mins [dl_llist]
	dl_set $dg:maxs [dl_llist]
	dl_set $dg:chans [dl_llist]
	dl_set $dg:maps [dl_llist]

	foreach o [dl_tcllist $obj_ids] {
	    set maxs \
		[$dbcmd eval "SELECT maxVal FROM calTable$o WHERE calNum = 1"]
	    set mins \
		[$dbcmd eval "SELECT minVal FROM calTable$o WHERE calNum = 1"]
	    set chans \
		[$dbcmd eval "SELECT channel FROM calTable$o WHERE calNum = 1"]
	    set maps [$dbcmd eval "SELECT pad FROM calTable$o WHERE calNum = 1"]
	    dl_append $dg:mins [eval dl_ilist $mins]
	    dl_append $dg:maxs [eval dl_ilist $maxs]
	    dl_append $dg:chans [eval dl_ilist $chans]
	    dl_append $dg:maps [eval dl_ilist $maps]
	}
	return $dg
    }

    #reads shape information from database and puts it into useful array shape
    proc get_object_coords { dbcmd obj_ids } {
	set dg [dg_create]
	dl_set $dg:ids $obj_ids
	dl_set $dg:coords [dl_llist]
	
	foreach o [dl_tcllist $obj_ids] {
	    # read x, y, and pad numbers from database
	    set x [$dbcmd eval "SELECT x FROM shapeTable${o}"]
	    set y [$dbcmd eval "SELECT y FROM shapeTable${o}"]
	    set pad [$dbcmd eval "SELECT pad FROM shapeTable${o}"]

	    #convert points into list
	    dl_local xvals [eval dl_flist $x]
	    dl_local yvals [eval dl_flist $y]
	    
	    #reshape the array for convenience with create line
	    dl_local xy [dl_transpose [dl_llist $xvals $yvals]]
	    dl_local sorted_by_pad [dl_deepUnpack [dl_sortByList $xy $pad]]
	
	    dl_append $dg:coords $sorted_by_pad
	}
	return $dg
    }

    #reads default orientation from database and puts it into useful array shape
    proc get_default_orientation { dbcmd obj_ids } {
	set dg [dg_create]
	dl_set $dg:ids $obj_ids
	dl_set $dg:default_orientation [dl_flist]
	
	foreach o [dl_tcllist $obj_ids] {
	    set default_orientation [$dbcmd eval "SELECT DefaultOrientation FROM locationTable WHERE ID = ${o}"] 
	    dl_local ori [eval dl_flist $default_orientation]
	    dl_append $dg:default_orientation $ori
	}
	return $dg
    }


    
    proc postfilecmd { datadg dg fileid } {

	# Add new lists if they don't exist yet
	if { ![dl_exists $dg:grasp_coords_sample] } {
	    foreach c { grasp_coords_sample grasp_coords_left grasp_coords_right
		grasp_touch_left_times grasp_touch_left_vals 
		grasp_touch_right_times grasp_touch_right_vals
		grasp_pcts_times grasp_pcts_left_times grasp_pcts_right_times
		grasp_pcts_left grasp_pcts_right
		grasp_vals_left grasp_vals_right } {
		dl_set $dg:$c [dl_llist]
	    }
	}
	    
	set dbcmd grasp_dbcmd

	# First select out obs periods in the loaded datafiles
	dl_local thisfile [dl_eq $dg:fileid $fileid]
	dl_local obsids [dl_select $dg:obsid $thisfile]
	
	dl_local varnames [dl_choose $datadg:<grasp>varname $obsids]
	dl_local times [dl_choose $datadg:<grasp>timestamp $obsids]
	dl_local vals [dl_choose $datadg:<grasp>vals $obsids]

	# Now get selecters for both raw data rows and touch data
	dl_local beginobs_t [dl_regmatch $varnames ess:obs:begin]
	dl_local rawdata_t [dl_regmatch $varnames grasp:data]
	dl_local l_touchdata_t [dl_regmatch $varnames grasp:touched:0]
	dl_local r_touchdata_t [dl_regmatch $varnames grasp:touched:1]

	# Use selectors to get actual data vals
	dl_local obsstarts [dl_select $times $beginobs_t]
	dl_local rawdata [dl_select $vals $rawdata_t]
	dl_local rawtimes [dl_sub [dl_select $times $rawdata_t] $obsstarts]
	dl_local l_touchdata [dl_select $vals $l_touchdata_t]
	dl_local l_touchtimes [dl_sub [dl_select $times $l_touchdata_t] \
				   $obsstarts]
	dl_local r_touchdata [dl_select $vals $r_touchdata_t]
	dl_local r_touchtimes [dl_sub [dl_select $times $r_touchdata_t] \
				   $obsstarts]

	# Get info about the objects used on left and right
	dl_local choice1_id [dl_select $dg:choice1_id $thisfile]
	dl_local choice2_id [dl_select $dg:choice2_id $thisfile]
	dl_local u1 [dl_unique $choice1_id]
	dl_local u2 [dl_unique $choice2_id]
	dl_local recoded_left_id [dl_recode $choice1_id]
	dl_local recoded_right_id [dl_recode $choice2_id]
	set l_objinfo [grasp::get_object_info $dbcmd $u1]
	set r_objinfo [grasp::get_object_info $dbcmd $u2]
	set l_objcoords [grasp::get_object_coords $dbcmd $u1]
	set r_objcoords [grasp::get_object_coords $dbcmd $u2]

	# And the sample coords
	dl_local sample_id [dl_select $dg:sample_id $thisfile]
	dl_local samp [dl_unique $sample_id]
	dl_local recoded_sample_id [dl_recode $sample_id]
	set s_objcoords [grasp::get_object_coords $dbcmd $samp]

	# Store coords for sample and left/right objects
	dl_concat $dg:grasp_coords_sample \
	    [dl_choose $s_objcoords:coords $recoded_sample_id]
	dl_concat $dg:grasp_coords_left \
	    [dl_choose $l_objcoords:coords $recoded_left_id]
	dl_concat $dg:grasp_coords_right \
	    [dl_choose $r_objcoords:coords $recoded_right_id]

	# Extract pad mappings from l/r objinfo data
	dl_local chans [dl_sub $l_objinfo:chans 1]
	dl_local pads [dl_sortIndices [dl_sub $l_objinfo:maps 1]]
	dl_local l_map [dl_choose [dl_choose $pads $chans] $recoded_left_id]
	dl_local l_map [dl_pack $l_map]

	dl_local chans [dl_sub $r_objinfo:chans 1]
	dl_local pads [dl_sortIndices [dl_sub $r_objinfo:maps 1]]
	dl_local r_map [dl_choose [dl_choose $pads $chans] $recoded_right_id]
	dl_local r_map [dl_pack $r_map]

	# Now get touch data, applying map
	dl_concat $dg:grasp_touch_left_times [dl_int [dl_mult $l_touchtimes 1000]]
	dl_concat $dg:grasp_touch_left_vals  [dl_choose $l_touchdata $l_map]
	dl_concat $dg:grasp_touch_right_times [dl_int [dl_mult $r_touchtimes 1000]]
	dl_concat $dg:grasp_touch_right_vals [dl_choose $r_touchdata $l_map]

	# Now get range and max info for each pad for each object
	dl_local ranges [dl_sub $l_objinfo:maxs $l_objinfo:mins]
	dl_local l_ranges [dl_choose $ranges $recoded_left_id]
	dl_local l_maxs [dl_choose $l_objinfo:maxs $recoded_left_id]

	dl_local ranges [dl_sub $r_objinfo:maxs $r_objinfo:mins]
	dl_local r_ranges [dl_choose $ranges $recoded_right_id]
	dl_local r_maxs [dl_choose $r_objinfo:maxs $recoded_right_id]
	
	# Extract times from raw data for left and right channels	
	dl_local l_high [dl_pack [dl_llist 1]]
	dl_local l_low [dl_pack [dl_llist 2]]
	dl_local left_times \
	    [dl_add [dl_uint [dl_choose $rawdata $l_low]] \
		 [dl_mult 256 [dl_uint [dl_choose $rawdata $l_high]]]]
	dl_local left_times [dl_unpack $left_times]
	
	dl_local r_high [dl_pack [dl_llist 16]]
	dl_local r_low [dl_pack [dl_llist 17]]
	dl_local right_times \
	    [dl_add [dl_uint [dl_choose $rawdata $r_low]] \
		 [dl_mult 256 [dl_uint [dl_choose $rawdata $r_high]]]]
	dl_local right_times [dl_unpack $right_times]

	# Eliminate first sample if not in obs period
	set rate2 50;		# 2xsample rate - first sample should be less than
	dl_local in_obsp_left \
	    [dl_not [dl_and [dl_firstPos $left_times] [dl_gt $left_times $rate2]]]
	dl_local in_obsp_right \
	    [dl_not [dl_and [dl_firstPos $left_times] [dl_gt $left_times $rate2]]]

	dl_local left_times [dl_select $left_times $in_obsp_left]
	dl_local right_times [dl_select $right_times $in_obsp_right]

	dl_local rawtimes [dl_select $rawtimes $in_obsp_left]
	
	# Extract vals from raw data for left and right channels
	dl_local l [dl_pack [dl_llist [dl_fromto 3 15]]]
	dl_local left_vals [dl_ufloat [dl_choose $rawdata $l]]
	dl_local left_vals [dl_select $left_vals $in_obsp_left]
	
	dl_local r [dl_pack [dl_llist [dl_fromto 18 30]]]
	dl_local right_vals [dl_ufloat [dl_choose $rawdata $r]]
	dl_local right_vals [dl_select $right_vals $in_obsp_right]

	# Turn vals to pcts
	dl_local l_prop [dl_div [dl_sub [dl_pack $l_maxs] $left_vals] \
			     [dl_pack $l_ranges]]
	dl_local l_pcts [dl_int [dl_mult $l_prop 100]]

	dl_local r_prop [dl_div [dl_sub [dl_pack $r_maxs] $right_vals] \
			     [dl_pack $r_ranges]]
	dl_local r_pcts [dl_int [dl_mult $r_prop 100]]
	
	# Finally remap to correct pad order
	dl_local l_pcts [dl_choose $l_pcts $l_map]
	dl_local r_pcts [dl_choose $r_pcts $r_map]
	
	dl_concat $dg:grasp_pcts_times [dl_int [dl_mult $rawtimes 1000]]
	dl_concat $dg:grasp_pcts_left_times $left_times
	dl_concat $dg:grasp_pcts_left $l_pcts
	dl_concat $dg:grasp_pcts_right_times $right_times
	dl_concat $dg:grasp_pcts_right $r_pcts

	dl_concat $dg:grasp_vals_left [dl_choose $left_vals $l_map]
	dl_concat $dg:grasp_vals_right [dl_choose $right_vals $r_map]

	dg_delete $l_objinfo
	dg_delete $r_objinfo

	return $dg
    }
    
    
    
    proc draw_blobs { shape_id_l shape_id_r } {

	#open database
	set dbfile l:/stimuli/grasp/objects.db
	sqlite3 dbcmd $dbfile

	#get the points for specified object
	if {$shape_id_l} {set coords_l [get_shape_coords dbcmd $shape_id_l 250 250]}
	if {$shape_id_r} {set coords_r [get_shape_coords dbcmd $shape_id_r 250 250]}
	
	#close database
	dbcmd close

	if { [winfo exists .cl] } {
	    .cl delete all
	    .cr delete all
	} else {	    
	    #start a canvas and put it in a grid
	    canvas .cl -width 250 -height 250 -background black
	    canvas .cr -width 250 -height 250 -background black
	    grid .cl .cr
	}
	
	#for each pad, draw it and give it a mouse binding to change color when you mouse over
	if {$shape_id_l} {
	    set pad_id 0
	    foreach padlist $coords_l {
		incr pad_id
		.cl create line $padlist -tag padback${pad_id} -fill black -width 20
	    }
	    
	    set pad_id 0
	    foreach padlist $coords_l {
		incr pad_id
		.cl create line $padlist -tag pad${pad_id} -fill grey -width 3
		
		.cl bind padback${pad_id} <Enter> ".cl itemconfigure pad${pad_id} -fill green"
		.cl bind padback${pad_id} <Leave> ".cl itemconfigure pad${pad_id} -fill grey"
	    }
	}
	#for each pad, draw it and give it a mouse binding to change color when you mouse over
	if {$shape_id_r} {
	    set pad_id 0
	    foreach padlist $coords_r {
		incr pad_id
		.cr create line $padlist -tag padback${pad_id} -fill black -width 20
	    }
	    
	    set pad_id 0
	    foreach padlist $coords_r {
		incr pad_id
		.cr create line $padlist -tag pad${pad_id} -fill grey -width 3
		
		.cr bind padback${pad_id} <Enter> ".cr itemconfigure pad${pad_id} -fill green"
		.cr bind padback${pad_id} <Leave> ".cr itemconfigure pad${pad_id} -fill grey"
	    }
	}
    }
}






