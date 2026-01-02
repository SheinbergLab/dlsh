#
# NAME
#    dlmodules.tcl
#
# DESCRIPTION
#    Load various dlsh related modules
#
#

package provide dlsh 1.2

# Tk graphics
proc load_Cgwin {} {
    package require cgwin
}

proc load_Stats {} {
    package require stats
}

# Fitls module
proc load_Fit {} {
    package require fit
}

# Impro module
proc load_Impro {} {
    package require impro
}

# Analog data file module
proc load_Adf {} {
    package require adf
}

# Canvas items using dlsh commands
proc load_Dlcanvas {} {
    package require dlcanvas
}




