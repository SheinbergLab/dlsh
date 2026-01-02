#
# NAME
#    dlcompat.tcl
#
# DESCRIPTION
#    Aliases of functions for maintaining compatibility
#
#

package provide dlsh 1.2

# dl_multRows / dl_divRows were once their own functions.  Now they
# are just one form of dl_mult / dl_div

proc dl_multRows { args } {
    dl_return [dl_mult $args]
}

proc dl_divRows { args } {
    dl_return [dl_div $args]
}


proc dl_equal { a b } {
    dl_return [dl_eq $a $b]
}

