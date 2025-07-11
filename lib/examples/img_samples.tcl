load_Impro

proc gaussian {} {
    set img [img_create]
    img_mkgauss $img
    img_show $img
    img_delete $img
}

proc randcircs { { n 50 } { val 255 } { h 256 } { w 256 } } {
    dl_local xs [dl_add [dl_int [dl_mult [dl_urand $n] [expr $w-30]]] 15]
    dl_local ys [dl_add [dl_int [dl_mult [dl_urand $n] [expr $h-30]]] 15]
    dl_local img [img_create]
    img_circles $img $xs $ys 5 -$val $val
    dl_return $img
}


