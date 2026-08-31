# score-doc-variants.tcl -- run candidate readings and count matching lines.
#
# check-doc-examples.tcl answers "does this example reproduce its transcript",
# which is the right question for a report but too coarse to choose between
# readings of a damaged one. Several of these examples record output from an
# older dlsh -- dl_local echoing a plain `a` rather than a handle -- so no
# reading of them can ever fully verify, and a pass/fail answer cannot tell
# `$a` from `$b`.
#
# This scores instead: for each variant, how many recorded output lines does
# it actually reproduce? The reading that matches most is the one the author
# wrote, regardless of unrelated drift elsewhere in the transcript.
#
#   dlsh score-doc-variants.tcl <variants.tcl>
#
# prints:  <slug#idx> <matched> <compared> <errors>

package require dlsh
set variants_file [lindex $argv 0]
source $variants_file
catch {dl_srand 12345}

proc canon {m args} { return [format %.6g $m] }

# Floats are compared by value, not spelling. These transcripts were recorded
# when dlsh printed 0.000000 where it now prints 0.0, and that difference is
# in every numeric example -- it would otherwise drown out the thing being
# measured here, which is which variable a `$` referred to.
proc norm {s} {
    regsub -all {%[a-zA-Z]*[0-9]+%} $s {<tmp>} s
    regsub -all {>[0-9]+<} $s {<ret>} s
    regsub -all {&[^& ]*_[0-9]+&} $s {<loc>} s
    regsub -all -command -- {-?[0-9]+\.[0-9]+(?:[eE][-+]?[0-9]+)?} $s canon s
    regsub -all {[ \t]+} $s { } s
    return [string trim $s]
}

proc parse_transcript {code} {
    set pairs {}
    set cmd ""; set out {}
    foreach line [split $code "\n"] {
        if {[regexp {^[ \t]*%[ \t]+(\S.*)$} $line -> rest]} {
            if {$cmd ne ""} { lappend pairs [list $cmd [join $out "\n"]] }
            set cmd [string trim $rest]
            set out {}
        } elseif {$cmd ne ""} {
            lappend out $line
        }
    }
    if {$cmd ne ""} { lappend pairs [list $cmd [join $out "\n"]] }
    return $pairs
}

foreach ex $::DOC_EXAMPLES {
    lassign $ex slug64 ns64 code64
    set slug [encoding convertfrom utf-8 [binary decode base64 $slug64]]
    set code [encoding convertfrom utf-8 [binary decode base64 $code64]]

    catch {dl_clean}
    catch {foreach g [dg_dir] { dg_delete $g }}
    catch {gbufreset}

    set matched 0; set compared 0; set errs 0
    # An error stops the run -- later state depends on earlier commands -- but
    # what matched before it still counts, so a reading that gets further is
    # ranked above one that fails immediately.
    foreach pr [parse_transcript $code] {
        lassign $pr cmd expected
        if {$cmd eq "" || [string match "#*" $cmd]} continue
        if {[catch {uplevel #0 $cmd} got]} { incr errs; break }
        set et [string trim $expected]
        if {$et eq ""} continue
        # a dl_local echo is a generated name, not content -- and some of
        # them carry a stray backslash from the import, so allow for it
        if {[regexp {^(%[a-zA-Z]*[0-9]+%|>[0-9]+<|\\?&[^&]*_[0-9]+\\?&)$} $et]} continue
        incr compared
        if {[norm $got] eq [norm $expected]} { incr matched }
    }
    puts "$slug $matched $compared $errs"
}
exit 0
