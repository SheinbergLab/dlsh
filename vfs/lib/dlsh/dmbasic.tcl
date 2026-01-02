#
# NAME
#    dmbasic.tcl
#
# DESCRIPTION
#    Basic dm_funcs that are derived directly from other primitives
#
#

package provide dlsh 1.2

#
# Calculate the determinant of a matrix by multiplying
# the elements of the diagonal of the lu decomposition 
#
proc dm_det { m } {
    dl_return [dl_prod [dm_diag [dm_ludcmp $m]:0]]
}

proc dm_covar { m } {
    set n [expr [dl_length $m]-1.0]
    dl_local mean [dl_means [dl_transpose $m]]
    dl_local centered [dl_sub $m [dl_llist $mean]]
    dl_return [dl_div [dm_mult [dm_transpose $centered] $centered] $n]
}