set dlshlib [file join /usr/local dlsh dlsh.zip]
if [file exists $dlshlib] {
    set base [file join [zipfs root] dlsh]
   zipfs mount $dlshlib $base
   set auto_path [linsert $auto_path [set auto_path 0] $base/lib]
}

package require dlsh
package require dbscan

#    double data[4][4] = {
#        {0, 2, 6, 10},
#        {2, 0, 5, 9},
#        {6, 5, 0, 4},
#        {10, 9, 4, 0}


# create a dummy distance matrix
dl_local x [dl_repeat [dl_flist 0 1 2] 3]
dl_local y [dl_repeat [dl_flist 1 0 1] 3]
dl_local z [dl_repeat [dl_flist 2 1 0] 3]
dl_local d [dl_combine \
		[dl_replicate [dl_llist $x] 3] \
		[dl_replicate [dl_llist $y] 3] \
		[dl_replicate [dl_llist $z] 3]]
dl_local d [dl_add [dl_restructure [dl_mult 0.1 [dl_zrand 81]] $d] $d]

puts [dl_tcllist [dbscan::cluster $d 0.5]]
