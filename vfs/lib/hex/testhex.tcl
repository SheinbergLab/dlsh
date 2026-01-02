package require dlsh
package require hex

clearwin
setwindow -10 -10 10 10
set s 1
dl_local cols [dlg_rgbcolors [dl_int [dl_mult 255 [dl_urand 127]]] [dl_int [dl_mult 255 [dl_urand 127]]] [dl_int [dl_mult 255 [dl_urand 127]]]]
dl_local hex_grid [::hex::cube_region [dl_ilist 0 0 0] 6]
dl_local centers [::hex::pointy_to_pixel $hex_grid $s]
dl_local hex [::hex::corner "0 0" 1.0 [dl_fromto 0 7]]
dl_local hgrid [dl_add $centers [dl_llist $hex]]
dl_local hgrid [dl_transpose $hgrid]
dlg_lines $hgrid:0 $hgrid:1 -fillcolors $cols -linecolors $cols
