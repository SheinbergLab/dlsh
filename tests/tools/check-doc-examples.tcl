# check-doc-examples.tcl -- run the command-reference examples and report.
#
# The examples in docs.db are transcripts: `% command` lines followed by the
# output that command produced. That makes them executable specifications, so
# they can be checked rather than trusted. Nothing has ever run them, and at
# least two are known wrong (a dl_return example with return-list names
# inlined into the text and its `$l` truncated to `$`).
#
# This is a REPORT, not a pass/fail gate. The point of the first pass is to
# find out how much has drifted before committing to a cleanup.
#
#   dlsh check-doc-examples.tcl <examples.tcl> ?-v?
#
# Classification per example:
#   verified      every command ran and every output matched
#   stale         ran, but at least one output differs from what is recorded
#   error         a command raised
#   graphics      dlp_/dlg_ -- drew into the gbuf; fingerprint recorded for
#                 future baselining rather than compared (no baseline exists)
#   skipped       nothing executable found
#
# Comparison notes:
#   - generated names (%list37%, >4<, &x_2&) embed allocation counters and
#     are normalized away, the same way tests/tools/dl_command_diff.tcl does
#   - whitespace is collapsed; these are hand-typed transcripts
#   - the RNG is seeded so random examples are reproducible

fconfigure stdout -buffering none
package require dlsh

set examples_file [lindex $argv 0]
set verbose [expr {[lsearch -exact $argv -v] >= 0}]
source $examples_file

catch {dl_srand 12345}

proc norm {s} {
    regsub -all {%[a-zA-Z]*[0-9]+%} $s {<tmp>} s
    regsub -all {>[0-9]+<} $s {<ret>} s
    regsub -all {&[^& ]*_[0-9]+&} $s {<loc>} s
    regsub -all {[ \t]+} $s { } s
    return [string trim $s]
}

# Split a transcript into {cmd expected} pairs. A line starting with % is a
# command; everything until the next % is its expected output.
proc parse_transcript {code} {
    set pairs {}
    set cmd ""; set out {}
    foreach line [split $code "\n"] {
        # The % must be followed by whitespace to count as a prompt: output
        # lines can start with % too, since a generated list prints as
        # %list0%, and reading those as commands invents failures.
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

proc reset_state {} {
    catch {dl_clean}
    catch {foreach g [dg_dir] { dg_delete $g }}
    catch {gbufreset}
}

set counts [dict create verified 0 stale 0 error 0 graphics 0 skipped 0]
set details {}

proc run_one {slug ns code} {
    set pairs [parse_transcript $code]
    if {![llength $pairs]} { return [list skipped {} {}] }
    set is_gfx [expr {$ns eq "dlp" || $ns eq "dlg"}]
    set bad {}; set errs {}; set ran 0
    array unset namemap
    foreach pr $pairs {
        lassign $pr cmd expected
        if {$cmd eq "" || [string match "#*" $cmd]} continue
        foreach {old new} [array get namemap] {
            set cmd [string map [list $old $new] $cmd]
        }
        incr ran
        if {[catch {uplevel #0 $cmd} got]} {
            # A transcript can legitimately record a failure -- dl_clean and
            # dl_local demonstrate what happens when a swept list is used
            # again, and the error IS the point. If the message is what was
            # recorded, that is a match, not a failure.
            if {[norm $got] eq [norm $expected] && [string trim $expected] ne ""} {
                continue
            }
            lappend errs [list $cmd $got]
            break
        }
        set et [string trim $expected]
        if {[regexp {^(%[a-zA-Z]*[0-9]+%|>[0-9]+<|\\?&[^&]*_[0-9]+\\?&)$} $et]} {
            if {[regexp {^(%[a-zA-Z]*[0-9]+%|>[0-9]+<|\\?&[^&]*_[0-9]+\\?&)$} [string trim $got]]} {
                set namemap($et) [string trim $got]
            }
            continue
        }
        if {$et eq ""} continue
        if {[norm $got] ne [norm $expected]} {
            lappend bad [list $cmd [norm $expected] [norm $got]]
        }
    }
    if {[llength $errs]}  { return [list error $errs {}] }
    if {$is_gfx}          { set fp "empty"; catch {set fp "[gbufsize] bytes"}
                            return [list graphics $fp {}] }
    if {[llength $bad]}   { return [list stale $bad {}] }
    if {$ran}             { return [list verified {} {}] }
    return [list skipped {} {}]
}

foreach ex $::DOC_EXAMPLES {
    lassign $ex slug64 ns64 code64
    # fields are base64 so the transcript text survives intact
    set slug [encoding convertfrom utf-8 [binary decode base64 $slug64]]
    set ns   [encoding convertfrom utf-8 [binary decode base64 $ns64]]
    set code [encoding convertfrom utf-8 [binary decode base64 $code64]]
    reset_state
    lassign [run_one $slug $ns $code] kind info _
    dict incr counts $kind
    if {$kind in {error stale graphics}} {
        lappend details [list $kind $slug $info]
    }
}

puts ""
puts "doc example report"
puts "=================="
set tot 0
foreach k {verified stale error graphics skipped} { incr tot [dict get $counts $k] }
foreach k {verified stale error graphics skipped} {
    puts [format "  %-9s %4d   %5.1f%%" $k [dict get $counts $k] \
              [expr {$tot ? 100.0*[dict get $counts $k]/$tot : 0}]]
}
puts [format "  %-9s %4d" total $tot]

puts ""
puts "errors (command raised):"
set n 0
foreach d $details {
    if {[lindex $d 0] ne "error"} continue
    incr n
    lassign $d _ slug errs
    lassign [lindex $errs 0] cmd msg
    puts [format "  %-26s %s" $slug [string range $cmd 0 44]]
    if {$verbose} { puts "        -> [string range $msg 0 90]" }
    if {$n >= 25 && !$verbose} { puts "  ... (use -v for all)"; break }
}

puts ""
puts "stale (output differs from what is recorded):"
set n 0
foreach d $details {
    if {[lindex $d 0] ne "stale"} continue
    incr n
    lassign $d _ slug bad
    lassign [lindex $bad 0] cmd want got
    puts [format "  %-26s %s" $slug [string range $cmd 0 44]]
    if {$verbose} {
        puts "        want: [string range $want 0 70]"
        puts "        got:  [string range $got 0 70]"
    }
    if {$n >= 25 && !$verbose} { puts "  ... (use -v for all)"; break }
}
puts ""
exit 0
