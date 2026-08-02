#
# NAME
#   wav.tcl
#
# DESCRIPTION
#   Sound/wav support for dlsh: generate simple test/stimulus sounds as
#   dl lists, read and write RIFF wav files, and materialize a standard
#   demo sound set.
#
#   Pure Tcl + dl_* -- available everywhere dlsh is (tclsh/dlsh, the
#   dserv ess interpreter, stim2). Generated files are ordinary wavs on
#   disk, so they can be fed to anything (the dserv sound module's
#   wavLoad, stim2, external tools).
#
#   Samples are represented as FLAT FLOAT dl lists in -1.0 .. 1.0
#   (interleaved if multi-channel). Generators produce mono. There is no
#   clipping stage: keep -amp <= 1.0 (writers truncate to 16-bit).
#
#   Sizing note: read/write go through Tcl lists (dl_tcllist / binary),
#   which is instant at stimulus scale (a 1 s 48 kHz mono file is 48k
#   samples) but not intended for minutes-long recordings.
#
# EXAMPLES
#   package require wav
#   set s [wav::tone 1000 200]                  ;# 200 ms 1 kHz tone
#   wav::write /tmp/t1k.wav $s
#   wav::info /tmp/t1k.wav                      ;# rate/channels/frames/...
#   set back [wav::read /tmp/t1k.wav]           ;# float dl list
#   wav::make_demo_sounds /path/to/data         ;# standard demo set
#
# AUTHOR
#   DLS, 08/26
#

package provide wav 1.0

namespace eval wav {
    variable default_rate 48000

    # merge caller options into defaults; error on unknown keys
    proc _opts { defaults arglist } {
        foreach {k v} $arglist {
            if { ![dict exists $defaults $k] } {
                error "wav: unknown option \"$k\" (expected: [dict keys $defaults])"
            }
            dict set defaults $k $v
        }
        return $defaults
    }

    #########################################################################
    # generators (mono float dl lists, -1.0 .. 1.0)
    #########################################################################

    # raised-cosine on/off ramps applied in place; returns the shaped list
    proc envelope { s ramp_ms rate } {
        set n [dl_length $s]
        set nr [expr {int($ramp_ms * $rate / 1000.0)}]
        if { $nr <= 0 || $n < 4 } { dl_return $s }
        if { 2 * $nr > $n } { set nr [expr {$n / 2}] }

        set pi [expr {acos(-1.0)}]
        dl_local i [dl_float [dl_fromto 0 $nr]]
        dl_local up [dl_mult \
                         [dl_sub 1.0 [dl_cos [dl_mult $i [expr {$pi / $nr}]]]] \
                         0.5]
        dl_local flat [dl_float [dl_ones [expr {$n - 2 * $nr}]]]
        dl_local env [dl_combine $up $flat [dl_reverse $up]]
        dl_return [dl_mult $s $env]
    }

    # pure tone; wav::tone freq_hz dur_ms ?-rate r? ?-amp a? ?-ramp_ms m?
    proc tone { freq_hz dur_ms args } {
        variable default_rate
        set o [_opts [list -rate $default_rate -amp 0.8 -ramp_ms 10] $args]
        set rate [dict get $o -rate]
        set n [expr {int($dur_ms * $rate / 1000.0)}]
        if { $n < 1 } { error "wav::tone: zero-length tone" }

        set w [expr {2.0 * acos(-1.0) * $freq_hz / $rate}]
        dl_local s [dl_mult [dl_sin [dl_mult [dl_float [dl_fromto 0 $n]] $w]] \
                        [dict get $o -amp]]
        dl_return [envelope $s [dict get $o -ramp_ms] $rate]
    }

    # white noise burst; wav::noise dur_ms ?-rate r? ?-amp a? ?-ramp_ms m?
    proc noise { dur_ms args } {
        variable default_rate
        set o [_opts [list -rate $default_rate -amp 0.5 -ramp_ms 10] $args]
        set rate [dict get $o -rate]
        set n [expr {int($dur_ms * $rate / 1000.0)}]
        if { $n < 1 } { error "wav::noise: zero-length noise" }

        dl_local s [dl_mult [dl_sub [dl_mult [dl_urand $n] 2.0] 1.0] \
                        [dict get $o -amp]]
        dl_return [envelope $s [dict get $o -ramp_ms] $rate]
    }

    # brief broadband click; wav::click ?-rate r? ?-amp a? ?-dur_ms d?
    proc click { args } {
        variable default_rate
        set o [_opts [list -rate $default_rate -amp 0.9 -dur_ms 2] $args]
        dl_return [noise [dict get $o -dur_ms] \
                       -rate [dict get $o -rate] \
                       -amp [dict get $o -amp] -ramp_ms 0.5]
    }

    proc silence { dur_ms args } {
        variable default_rate
        set o [_opts [list -rate $default_rate] $args]
        set n [expr {int($dur_ms * [dict get $o -rate] / 1000.0)}]
        dl_return [dl_float [dl_zeros $n]]
    }

    #########################################################################
    # file i/o (RIFF wav)
    #########################################################################

    # render samples (flat float dl list, interleaved if -channels > 1) as
    # a complete 16-bit PCM wav file image, returned as a binary string.
    # Feed it to the dserv sound module's wavLoadData (no disk involved),
    # or hand it to wav::write. wav::render samples ?-rate r? ?-channels n?
    proc render { samples args } {
        variable default_rate
        set o [_opts [list -rate $default_rate -channels 1] $args]
        set rate [dict get $o -rate]
        set nchan [dict get $o -channels]

        set vals [dl_tcllist [dl_short [dl_mult $samples 32767.0]]]
        set data [binary format s* $vals]
        set nbytes [string length $data]

        set byterate [expr {$rate * $nchan * 2}]
        set align [expr {$nchan * 2}]
        set hdr [binary format a4ia4a4issiissa4i \
                     "RIFF" [expr {36 + $nbytes}] "WAVE" \
                     "fmt " 16 1 $nchan $rate $byterate $align 16 \
                     "data" $nbytes]

        return $hdr$data
    }

    # write samples as a 16-bit PCM wav file (render + save);
    # wav::write path samples ?-rate r? ?-channels n?
    proc write { path samples args } {
        set f [open $path w]
        fconfigure $f -translation binary
        puts -nonewline $f [render $samples {*}$args]
        close $f
        return $path
    }

    # parse RIFF chunks; returns dict with fmt fields + data byte range
    proc _parse { path } {
        set f [open $path r]
        fconfigure $f -translation binary
        set riff [::read $f 12]
        if { [string length $riff] < 12 ||
             ![string equal [string range $riff 0 3] "RIFF"] ||
             ![string equal [string range $riff 8 11] "WAVE"] } {
            close $f
            error "wav: \"$path\" is not a RIFF/WAVE file"
        }
        set info [dict create]
        while { 1 } {
            set chdr [::read $f 8]
            if { [string length $chdr] < 8 } { break }
            binary scan $chdr a4i tag size
            if { $tag eq "fmt " } {
                set body [::read $f $size]
                binary scan $body ssiiss fmt nchan rate byterate align bits
                dict set info format $fmt
                dict set info channels $nchan
                dict set info rate $rate
                dict set info bits $bits
            } elseif { $tag eq "data" } {
                dict set info data_offset [tell $f]
                dict set info data_bytes $size
                seek $f $size current
            } else {
                seek $f $size current
            }
            if { $size % 2 } { seek $f 1 current }
        }
        close $f
        if { ![dict exists $info rate] || ![dict exists $info data_offset] } {
            error "wav: \"$path\" is missing fmt or data chunk"
        }
        return $info
    }

    # header info without reading samples:
    # dict of rate channels bits format frames duration_ms
    proc info { path } {
        set i [_parse $path]
        set bytes_per_frame \
            [expr {[dict get $i channels] * [dict get $i bits] / 8}]
        set frames [expr {[dict get $i data_bytes] / $bytes_per_frame}]
        return [dict create \
                    rate [dict get $i rate] \
                    channels [dict get $i channels] \
                    bits [dict get $i bits] \
                    format [dict get $i format] \
                    frames $frames \
                    duration_ms [expr {$frames * 1000 / [dict get $i rate]}]]
    }

    # read 16-bit PCM (format 1) or 32-bit float (format 3) samples as a
    # flat float dl list in -1.0 .. 1.0, interleaved if multi-channel.
    # Use wav::info for rate/channels.
    proc read { path } {
        set i [_parse $path]
        set fmt [dict get $i format]
        set bits [dict get $i bits]

        set f [open $path r]
        fconfigure $f -translation binary
        seek $f [dict get $i data_offset]
        set data [::read $f [dict get $i data_bytes]]
        close $f

        if { $fmt == 1 && $bits == 16 } {
            binary scan $data s* vals
            dl_return [dl_flist {*}[lmap v $vals {expr {$v / 32768.0}}]]
        } elseif { $fmt == 3 && $bits == 32 } {
            binary scan $data r* vals
            dl_return [dl_flist {*}$vals]
        } else {
            error "wav: \"$path\": unsupported format (fmt $fmt, $bits bits);\
                   only 16-bit PCM and 32-bit float are readable"
        }
    }

    #########################################################################
    # demo sound set
    #########################################################################

    # The standard demo set: one nominal target plus distinct distractors,
    # suitable for detection-style tasks and quick audio-path checks.
    # name -> generator script (rate substituted at make time)
    variable demo_sounds {
        target_1k   { tone 1000 200 -rate $rate }
        tone_500    { tone  500 200 -rate $rate }
        tone_700    { tone  700 200 -rate $rate }
        tone_1400   { tone 1400 200 -rate $rate }
        noise_burst { noise 200 -rate $rate }
    }

    proc demo_names {} {
        variable demo_sounds
        set names {}
        foreach {name script} $demo_sounds { lappend names $name }
        return $names
    }

    # Render one demo sound by name, returning the complete wav file
    # image as a binary string -- feed it to the dserv sound module's
    # wavLoadData (or the ::ess::wav_load_data wrapper); no file is
    # involved. wav::demo_sound name ?-rate r?
    proc demo_sound { name args } {
        variable default_rate
        variable demo_sounds
        set o [_opts [list -rate $default_rate] $args]
        set rate [dict get $o -rate]
        if { ![dict exists $demo_sounds $name] } {
            error "wav::demo_sound: unknown demo sound \"$name\"\
                   (available: [demo_names])"
        }
        dl_local s [eval [subst -nocommands [dict get $demo_sounds $name]]]
        return [render $s -rate $rate]
    }

    # Materialize the demo set as real wav files in `dir` (created if
    # needed) -- e.g. as visible examples to imitate, or for tools that
    # want files. Existing files are left alone unless -force 1. Returns
    # the list of names. wav::make_demo_sounds dir ?-rate r? ?-force 0/1?
    proc make_demo_sounds { dir args } {
        variable default_rate
        set o [_opts [list -rate $default_rate -force 0] $args]
        set rate [dict get $o -rate]

        file mkdir $dir
        set names {}
        foreach name [demo_names] {
            set path [file join $dir ${name}.wav]
            if { [dict get $o -force] || ![file exists $path] } {
                set f [open $path w]
                fconfigure $f -translation binary
                puts -nonewline $f [demo_sound $name -rate $rate]
                close $f
            }
            lappend names $name
        }
        return $names
    }
}
