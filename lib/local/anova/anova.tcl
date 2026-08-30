package provide anova 0.1
package require matlabUtils

namespace eval ::anova {
    proc ::anova::demo { } {
		puts "The following are the available demos:\n   repeatDemo\n\nTo get more specifics on each type: use ::anova::demoName\n\nWhere you substitute the demoName with one of the options above.\n\n"
    }
    
    proc ::anova::init { } {
	puts "Namespace for anova package is ::anova";update
	puts "Checking for open Matlab";update
	if ![::mlu::checkMLOpen] {
	    puts "Matlab is not open, Please wait while I try to start Matlab";update
	    if {[catch ::mlu::openMatlab cout]} {
		puts "Sorry, can't open Matlab. You will have to trouble shoot.";update
		return 0
	    } else {
		puts "Matlab open"
	    }
	}
	#check for necessary m files
	puts "Checking for necessary m files";update
	set rmcnt 0
	dl_local fns [eval dl_slist [split [ls l:/projects/analysis/packages/matlabUtils/*.m]]]
	set rmnames [list RMAOV1.m RMAOV2.m RMAOV32.m RMAOV33.m]
	foreach an $rmnames {
	    incr rmcnt [dl_sum [dl_stringmatch $fns $an]]
	}
	if {$rmcnt != [llength $rmnames]} {
	    puts "You do not have all necessary m files";update
	    return 0;
	} else {
	    puts "Necessary m files present";update
	}
	puts "For a quick demo run ::anova::repeatDemo";update
	return 1;
    }
    
    proc repeat {g names} {
	#%HELP%
	# use ::anova::repeat
	#   purpose: This function is to give you access to some matlab
	#            m files for repeated measures anova. The m files
	#            were NOT written by me. I just ammended the output
	#            options a little. This function expects the name of a
	#            group and a tcl list of names for the anova. The first
	#            list is the dependent variable. The last list name is the
	#            subjects factor (e.g. dataset id or filename). The list 
	#            in betweens are those that you are examing as factors.
	#            example: ::anova::repeat mydatagrp "weight gender diet monkey"
	#            Note that it returns three values for each analysis
	#            And that it computes all interactions, so you  might
	#            have to play a while to figure out which elements of the
	#            output variables are most relevant for your application.
	#   input: 
	#     g: groupname
	#     names: a tcllist of names from g for the analysis
	#   return: f value p-value df
	#%END%
	
	#Shouldn't really need this line, but just here as a failsafe
	if {![::mlu::checkMLOpen]} {::mlu::openMatlab}
	
	m_eval {addpath('l:/projects/analysis/packages/matlabUtils/');}
	set atype [expr [llength $names] - 2]
	if {$atype > 3} {puts "Can only do up to 3 way anova, reduce number\n of factors please."; update; return 0}
	if {$atype < 1} {puts "You forgot to enter list of names.";update}
	
	if {$atype == 3} {
	    set anova_name RMAOV33
	} else {
	    set anova_name RMAOV${atype}
	}

	set vl [list]
	for {set i 0} {$i < [llength $names]} {incr i} {
	    if {$i == 0} {
		m_put $g:[lindex $names 0] v0
		lappend vl v0'
		
	    } else {
		dl_local temp [dl_add [dl_recode $g:[lindex $names $i]] 1]
		m_put $temp v$i
		lappend vl v${i}'
	    }
	}
	#puts "out = ${anova_name}(\[ $vl \]);"
	m_eval "out = ${anova_name}(\[ $vl \]);"
	set o [dl_tcllist [m_get out]]
	return $o
	    
	
	
    }

    proc repeatDemo { } {
	#this is just a little function which loads some data
	#and runs a junk analysis so that you can see how the whole
	#thing works.
	puts {set g [load_data m_nfCon_011706* m_nfCon_011806*]};update
	set g [load_data m_nfCon_011706* m_nfCon_011806*]
	puts {::anova::repeat $g "nsaccades order stim_contrasts class fileid" };update
	set o [::anova::repeat $g "nsaccades order stim_contrasts class fileid"]
	dl_local outdat [dl_reshape [eval dl_flist [join $o]] - 3]
	set sg [dg_create]
	dl_set $sg:fstat [dl_collapse [dl_choose $outdat [dl_llist [dl_ilist 0]]]]
	dl_set $sg:pval [dl_collapse [dl_choose $outdat [dl_llist [dl_ilist 1]]]]
	dl_set $sg:df [dl_collapse [dl_choose $outdat [dl_llist [dl_ilist 2]]]]
	dg_view $sg
	puts "group $sg is the stat group you are seeing"
	puts "For this demo the first row is the effect of order\n second row is the stim_contrasts\n third row class\n fourth row order x contrast\n fifth row order x class\n sixth row contrast x class\n seventh row order x contrast x class"
	
    }

} 


::anova::init
