package provide spec 0.9

namespace eval ::helpspec {
    ####Window Functions####
    #All windows should follow naming convention <name>Window
    #Add whatever anyone wants to code here
    proc hanningWindow {len} {
	dl_return [dl_mult 0.5 [dl_sub 1 [dl_cos [dl_div [dl_mult 2. $::pi [dl_series 1 $len]] [expr $len + 1]]]]]
    }
}