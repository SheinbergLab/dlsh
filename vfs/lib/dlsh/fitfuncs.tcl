package provide dlsh 1.2

proc dl_gaussian { x amp mean sd } {
    dl_return [dl_mult [dl_exp [dl_negate [dl_pow \
	    [dl_div [dl_sub $x $mean] $sd] 2]]] $amp]
}

proc dl_weibull { x t a b } {
    dl_return [dl_sub $t [dl_mult [dl_sub $t .5] \
	    [dl_exp [dl_negate [dl_pow [dl_div $x $a] $b]]]]]
}

proc dl_gamma { x l r } {
    dl_return [dl_mult [dl_div [dl_pow $l $r] [dl_exp [dl_lgamma $r]]] \
	    [dl_mult [dl_pow $x [dl_sub $r 1.0]] \
	    [dl_exp [dl_mult [dl_negate $l] $x]]]]
}
