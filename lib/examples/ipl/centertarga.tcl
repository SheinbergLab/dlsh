load_Ipl

proc rescale_targa { src dest { targ_dim 256 } } {
    # read the src image
    set in [ipl readTarga $src]

    # determine its dims
    scan [ipl info $in] "%d %d %d" w h d

    # scale if necessary using "supersampling" interpolation
    if { $w != $targ_dim && $h != $targ_dim } {
	if { $w > $h } {
	    set scale [expr (1.*$targ_dim)/$w]
	    set scaled [ipl scale $in - $scale $scale SUPER]
	} else {
	    set scale [expr (1.*$targ_dim)/$h]
	    set scaled [ipl scale $in - $scale $scale SUPER]
	}
	ipl destroy $in
	set in $scaled
    }

    # now pad to make square
    scan [ipl info $in] "%d %d %d" w h d

    # there are two ways to pad, one for portrait images, one for landscapes
    #  portrait:  just unpack pixels and pad with zeros on top and bottom
    #  landscape: do operation on transposed pixels, then transpose back
    if { $w > $h } {
	set toprows [expr ($targ_dim-$h)/2]
	set botrows [expr $targ_dim-($h+$toprows)]
	dl_local pixels [ipl toDynlist $in 0]
	dl_local top [dl_char [dl_repeat 0 [expr $toprows*$d*$w]]]
	dl_local bottom [dl_char [dl_repeat 0 [expr $botrows*$d*$w]]]
	dl_local newimg [dl_combine $top $pixels $bottom]
	set out [ipl fromDynlist $targ_dim $targ_dim $d $newimg]
    } elseif { $h > $w } {
	set leftcols [expr ($targ_dim-$w)/2]
	set rightcols [expr $targ_dim-($w+$leftcols)]
	dl_local pixels [dl_transpose [dl_reshape [ipl toDynlist $in 0] $h -]]
	dl_local pixels [dl_unpack $pixels]
	dl_local left [dl_char [dl_repeat 0 [expr $leftcols*$d*$h]]]
	dl_local right [dl_char [dl_repeat 0 [expr $rightcols*$d*$h]]]
	dl_local newimg [dl_unpack [dl_transpose \
			     [dl_reshape [dl_combine $left $pixels $right] \
				  - $targ_dim]]]
	set out [ipl fromDynlist $targ_dim $targ_dim $d $newimg]
    } else {
	ipl writeTarga $in $dest
	ipl destroy $in
	return
    }
    ipl writeTarga $out $dest
    ipl destroy $in
    ipl destroy $out
}