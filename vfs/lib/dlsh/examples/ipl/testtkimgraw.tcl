package require dlsh
load_Ipl
package require Tkimgraw

set filename c:/stimuli/distenc/doll00.tga
set img [image create photo]
canvas .c -width 300 -height 300 -background white
.c create image 150 150 -image $img -anchor c
set iplimg [ipl readTarga $filename]
set img2 [ipl scale $iplimg - .21 .21]
ipl writeRawString $img2 curphoto
rawToPhoto image1 $curphoto
pack .c