#
# NAME
#    dlinput.tcl
#
# DESCRIPTION
#    Routines for inputting data from external files
#
#

package provide dlsh 1.2

proc dg_dumpAsCSV { g { f "" } } {
    # %HELP%
    # NAME
    #  dg_dumpAsCSV
    #
    # DESCRIPTION
    #  Dump a dynamic group as a comma separated value list (for, e.g., Excel)
    # %END%

    if { $f == "" } { set f $g.csv }
    if { ![dg_exists $g] } { error "dg_dumpAsCSV: group $g not found" }
    set file [open $f w]
    dg_dump $g $file 44
    close $file
    return $f
}
proc dg_asciiWrite { g {f ""}  } {
    # %HELP%
    # NAME
    #  dg_asciiWrite
    #
    # DESCRIPTION
    #  Writes a tab separated ascii file.  If no output file is supplied
    #  the file "groupname".dat is created.
    # %END%

    if { $f == "" } { set f $g.dat }
    if { ![dg_exists $g] } { error "dg_asciiWrite: group $g not found" }
    set file [open $f w]
    dg_dump $g $file
    close $file
    return $f
}

proc dg_asciiRead { file group args } {
    # Create the group and add appropriate lists
    if { ![dg_exists $group] } { dg_create $group }

    set rewind 1
    set f [open $file]
    if { $args == "" } {
	gets $f headerline
	if { [regexp ^# $headerline] } {
	    regsub {^# *} $headerline {} header
	    set args [split $header]
	}
	set rewind 0
    }

    set splitchar " "
    # Parse the args
    #   Choices are:     name
    #                    name:datatype
    #                    name:datatype:col
    set id 0
    foreach arg $args {
	set spec [split $arg :]
	switch [llength $spec] {
	    3 {	set col($id) [lindex $spec 2]; set name($id) [lindex $spec 0]; \
		    set datatype($id) [lindex $spec 1] }
	    2 { set name($id) [lindex $spec 0]; \
		    set s [lindex $spec 1]; \
		    if { [catch { expr int($s) } result ] } { \
		    set datatype($id) $s; set col($id) $id } \
		    { set col($id) $s; set datatype($id) float } }
	    1 { set col($id) $id; set name($id) [lindex $spec 0]; \
		    set datatype($id) float } 
	    default { error "dl_asciiRead: bad arg spec" }
	}
	incr id
    }

    set maxcol 0
    for { set i 0 } { $i < $id } { incr i } {
	if { ![dl_exists $group:$name($i)] } {
	    dg_addNewList $group $name($i) $datatype($i)
	} elseif { [dl_datatype $group:$name($i)] != $datatype($i) } {
	    error "datatype mismatch between list \"$name($i)\" and \
		    list \"$group:$name($i)\""
	}
	if { $col($i) > $maxcol } { set maxcol $col($i) }
    }

    if { $rewind } { seek $f 0 start }
    set linenum -1
    foreach line [split [read $f] \n] {
	incr linenum
	set ncols [llength $line]
	if { [expr $ncols == 0] } continue
	if { [expr $ncols <= $maxcol] } {
	    close $f
	    dg_delete $group
	    error "too few columns in data file: line $linenum"
	}
	for { set i 0 } { $i < $id } { incr i } {
	    catch { dl_append $group:$name($i) [lindex $line $col($i)] } 
	}
    }
    close $f
    return $group
}