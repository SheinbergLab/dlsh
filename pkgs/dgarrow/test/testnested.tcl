load /Users/sheinb/src/dlsh/build/libdlsh.dylib
load ./build/libdgarrow.dylib
set g [dg_create]; dl_set $g:id [dl_fromto 0 10]; dl_set $g:colors [dl_replicate [dl_llist [dl_flist 0 .4 .1]] 10];  dg_toArrow $g x
dg_fromArrow $x y
puts [dg_tclListnames y]
puts [dl_tcllist y:id]
puts [dl_tcllist y:colors]
