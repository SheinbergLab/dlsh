package provide dlsh 1.2

proc dl_lsfit { x y } {
    if { [dl_length $x] != [dl_length $y] } {
	error "dl_lsfit: x and y vectors must have same length"
    }
    set mx [dl_mean $x]
    set my [dl_mean $y]
    dl_local xx [dl_sub $x $mx]
    dl_local yy [dl_sub $y $my]
    set sumxy [dl_sum [dl_mult $xx $yy]]
    set ssx [dl_sum [dl_mult $xx $xx]]
    set ssy [dl_sum [dl_mult $yy $yy]]

    if { [expr $ssx != 0.0] && [expr $ssy != 0.0] } {
	set m [expr double($sumxy)/double($ssx)]
	set r [expr double($sumxy)/sqrt($ssx*$ssy)]
    } else {
	set m 0.0;
	set r 1.0;
    }
    set b [expr double($my)-double($mx*$m)]
    set r [expr $r*$r]
    return "$m $b $r"
}
