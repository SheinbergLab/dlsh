set dlshlib [file join /usr/local/dlsh dlsh.zip]
set base [file join [zipfs root] dlsh]
if { ![file exists $base] && [file exists $dlshlib] } {
    zipfs mount $dlshlib $base
}
set ::auto_path [linsert $::auto_path [set auto_path 0] $base/lib]

package require dlsh
package require dtw

dl_local a [dl_llist [dl_flist 0 1 2 3 4] [dl_flist 2 2 2 2 2]]
dl_local b [dl_llist [dl_flist 0 1 2 3 4] [dl_flist 0 0 0 0 0]]

# single path comparison
puts [dl_tcllist [dtwDistanceOnly $a $b]]

# single path vs multi path comparison
puts [dl_tcllist [dtwDistanceOnly $a [dl_replicate [dl_llist $b] 10]]]

# multi path vs single path comparison
puts [dl_tcllist [dtwDistanceOnly [dl_replicate [dl_llist $a] 10] $b]]

# matrix of distances multi path vs single path comparison
puts [dl_tcllist [dtwDistanceOnly [dl_llist $a $b $a $b]]]

# bad arguments must raise a Tcl error, not crash the interpreter
proc expect_error { label script } {
    if { ![catch { uplevel 1 $script } msg] } {
	puts "FAIL: $label: expected an error, got: $msg"
	exit 1
    }
    puts "ok: $label: $msg"
}

# a flat list is not a list of paths
expect_error "flat list" { dtwDistanceOnly [dl_flist 1 2 3] }

# a list of two float lists is one path, so as a *list* of paths its
# elements are floats, not paths
expect_error "float sublists" \
    { dtwDistanceOnly [dl_llist [dl_flist 1 2] [dl_flist 3 4]] }
expect_error "int sublists" \
    { dtwDistanceOnly [dl_llist [dl_ilist 1 2] [dl_ilist 3 4]] }

# float sublists again, this time in the two argument form
expect_error "float sublist, two args" \
    { dtwDistanceOnly [dl_llist [dl_flist 1 2 3]] $a }
expect_error "float sublists, two args" \
    { dtwDistanceOnly [dl_llist [dl_flist 1 2] [dl_flist 3 4] [dl_flist 5 6]] $a }

# a path with the wrong number of sublists
expect_error "one sublist path" \
    { dtwDistanceOnly [dl_llist [dl_llist [dl_flist 1 2 3]]] }
expect_error "three sublist path" \
    { dtwDistanceOnly [dl_llist [dl_llist [dl_flist 1 2] [dl_flist 3 4] \
				     [dl_flist 5 6]]] $a }

# path sublists of unequal length
expect_error "ragged path" \
    { dtwDistanceOnly [dl_llist [dl_llist [dl_flist 1 2 3] [dl_flist 1 2]]] }

# non float path sublists
expect_error "int path" \
    { dtwDistanceOnly [dl_llist [dl_llist [dl_ilist 1 2] [dl_ilist 3 4]]] }

# empty list of paths
expect_error "empty list" { dtwDistanceOnly [dl_llist] }

# two lists of paths is not a supported combination
expect_error "two path lists" \
    { dtwDistanceOnly [dl_llist $a $b] [dl_llist $a $b] }

puts "all dtw argument checks passed"

      
