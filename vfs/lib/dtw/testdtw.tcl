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

      
