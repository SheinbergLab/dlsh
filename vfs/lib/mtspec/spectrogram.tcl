package require dlsh
package require mtspec

proc spectrogram_test {} {
    dlp_setpanels 3 1
    set i -1; set length 2; set k 50; set f0 1; 
    ::helpmtspec::CreateInitArgsArray "continuous"
    set Fs $::mtspec::Args(samp_freq)
    set ::mtspec::Args(pretime) 0
    set ::mtspec::Args(posttime) [expr 1000*$length]
    set ::mtspec::Args(fpass) "0 250";
    
    # Create the chirp
    dl_local x [dl_fromto 0 $length [expr 1/$Fs.]]
    dl_local fs [dl_add $f0 [dl_mult [expr $k/2.] $x]]
    set n [dl_length $x]
    dl_local y [dl_cos [dl_mult $x $fs [expr 2*$::pi]]]
    set p [dlp_newplot]
    set xdata [dlp_addXData $p $x]
    set ydata [dlp_addYData $p $y]
    dlp_setxrange $p 0 $length
    dlp_setyrange $p -2 2
    dlp_draw $p lines "$xdata $ydata" -lwidth 50
    dlp_set $p xtitle "time (seconds)"
    dlp_set $p ytitle "amplitude (a.u.)"
    dlp_set $p title "Chirp with a $k Hz/s rate"
    dlp_subplot $p [incr i]
    
    set step 10; set window 500;
    dl_local Stf [helpmtspec::mtspecgram [dl_llist $y] $step $window "continuous"]
    dl_local S [dl_reverse [dl_mult [dl_log10 $Stf:0] 10]]
    dl_local freqs $Stf:1
    dl_local times $Stf:2
    
    set plotmin [dl_min [dl_mins $S]]
    set realmax [dl_max [dl_maxs $S]]
    set plotmax [dl_max [dl_maxs [dl_sub $S $plotmin]]]
    
    set ::mtspec::Args(colormap) "jet"
    set p [helpmtspec::GenerateSpecgramPlot $S $freqs $times $plotmin $plotmax $realmax]
    dlp_set $p title "Spectogram with stepsize=$step ms, winsize=$window ms, nw=$::mtspec::Args(nw), ntapers=$::mtspec::Args(ntapers), Jet Colormap"
    dlp_subplot $p [incr i]
    
    set ::mtspec::Args(colormap) "hot"
    set p [helpmtspec::GenerateSpecgramPlot $S $freqs $times $plotmin $plotmax $realmax]
    dlp_set $p title "Spectogram with stepsize=$step ms, winsize=$window ms, nw=$::mtspec::Args(nw), ntapers=$::mtspec::Args(ntapers), Hot Colormap"
    dlp_subplot $p [incr i]
}

spectrogram_test