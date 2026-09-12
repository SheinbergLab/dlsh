#!/usr/bin/env dlsh
#
# test_dg_arrow.tcl
#   The Arrow exchange commands.  dg_toArrowFile must write the Arrow IPC
#   *file* format (magic + footer), which is what pandas.read_feather, R's
#   arrow::read_feather, DuckDB and pyarrow.ipc.open_file open; dg_toArrow
#   stays the in-memory stream form.  Both must round-trip through their
#   dg_from* partners, dg_fromArrowFile must accept either form, and a
#   group that is not a table must fail with a message that names the
#   offending lists.
#
#   Usage:  dlsh test_dg_arrow.tcl          (exits non-zero on any failure)

if {[catch {package require dlsh}]} {
    foreach path {/usr/local/dlsh/dlsh.zip /usr/local/lib/dlsh.zip} {
        if {[file exists $path]} {
            catch {zipfs mount $path /dlsh}
            set base [file join [zipfs root] dlsh]
            set ::auto_path [linsert $::auto_path 0 ${base}/lib]
            break
        }
    }
    package require dlsh
}

set ::fail 0
proc check {label got want} {
    if {$got eq $want} {
        puts "OK   $label"
    } else {
        puts "FAIL $label -> got {$got} want {$want}"
        incr ::fail
    }
}

set dir [file join [pwd] arrow_test_[pid]]
file mkdir $dir

# A rectangular group with every column kind: long, float, int64, double,
# string, and a ragged nested list.
set g [dg_create]
dl_set $g:id    [dl_ilist 1 2 3]
dl_set $g:rt    [dl_flist 1.5 2.5 3.5]
dl_set $g:ts    [dl_wlist 1757606400123456 -9007199254740993 42]
dl_set $g:x     [dl_dlist 3.141592653589793 1e300 -0.5]
dl_set $g:name  [dl_slist alpha beta gamma]
dl_set $g:em    [dl_llist [dl_flist 1 2 3] [dl_flist] [dl_flist 4]]

proc same_group {label a b} {
    check "$label: list names" [dl_tcllist [dg_listnames $b]] [dl_tcllist [dg_listnames $a]]
    foreach n [dl_tcllist [dg_listnames $a]] {
        check "$label: $n type"   [dl_datatype $b:$n] [dl_datatype $a:$n]
        check "$label: $n values" [dl_tcllist $b:$n]  [dl_tcllist $a:$n]
    }
}

# ---- 1. file form: magic at both ends, round trip ---------------------
set f $dir/g.arrow
dg_toArrowFile $g $f
set fh [open $f rb]; set bytes [read $fh]; close $fh
check "file starts with ARROW1 magic" [string range $bytes 0 5] "ARROW1"
check "file ends with ARROW1 magic"   [string range $bytes end-5 end] "ARROW1"
set back [dg_fromArrowFile $f roundtrip]
same_group "file round trip" $g $back
dg_delete $back

# ---- 2. stream form: unchanged, and the file reader accepts it too ------
set n [dg_toArrow $g streambytes]
check "dg_toArrow returns the byte count" [expr {$n == [string length $streambytes]}] 1
check "stream form has no file magic" [string range $streambytes 0 5] "\xff\xff\xff\xff[string range $streambytes 4 5]"
set back [dg_fromArrow $streambytes fromstream]
same_group "stream round trip" $g $back
dg_delete $back

set fs $dir/g.stream
set fh [open $fs wb]; puts -nonewline $fh $streambytes; close $fh
set back [dg_fromArrowFile $fs fromstreamfile]
check "dg_fromArrowFile reads a bare stream" [dl_tcllist $back:id] {1 2 3}
dg_delete $back

# ---- 3. a non-table fails with a message naming the lists --------------
set bad [dg_create]
dl_set $bad:trials [dl_ilist 1 2 3 4]
dl_set $bad:version [dl_ilist 7]
set rc [catch {dg_toArrowFile $bad $dir/bad.arrow} msg]
check "non-rectangular group errors" $rc 1
check "message names the lists" [expr {[string match "*version*1 element*" $msg] && [string match "*trials*4*" $msg]}] 1
check "no file left behind" [file exists $dir/bad.arrow] 0
set rc [catch {dg_toArrow $bad junk} msg]
check "dg_toArrow errors the same way" [string match "*not rectangular*" $msg] 1
dg_delete $bad

# ---- 4. garbage in is an error, not a crash ---------------------------
set fh [open $dir/junk.arrow wb]; puts -nonewline $fh "ARROW1\0\0not an arrow file at all"; close $fh
check "junk file errors" [catch {dg_fromArrowFile $dir/junk.arrow junk}] 1

dg_delete $g
file delete -force $dir

if {$::fail} {
    puts "=== $::fail FAILURE(S) ==="
    exit 1
}
puts "=== ALL PASS ==="
