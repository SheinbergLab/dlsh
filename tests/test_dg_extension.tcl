#!/usr/bin/env dlsh
#
# test_dg_extension.tcl
#   The skippable extension envelope, dg tag 250 (DG_EXT_TAG): a reader that
#   does not know an extension id must step over the record at every level
#   (top, group, list) and hand back the rest of the file intact, through
#   both the file parser (.dg) and the buffer parser (.dgz), and a corrupt
#   envelope must be an error rather than a crash.
#
#   The file is assembled here byte by byte rather than written by dg_write,
#   so the test is also a record of the wire layout it depends on.
#
#   Usage:  dlsh test_dg_extension.tcl      (exits non-zero on any failure)

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

# ---- wire-format helpers (little-endian, as this host writes) ---------
proc u8 {v}       { return [binary format c $v] }
proc i32 {v}      { return [binary format i $v] }
proc f32 {v}      { return [binary format r $v] }
proc f64 {v}      { return [binary format q $v] }
proc cstr {tag s} { return "[u8 $tag][i32 [expr {[string length $s]+1}]]$s\0" }
proc ext {id payload} {
    # DG_EXT_TAG, int ext_id, int length, payload
    return "[u8 250][i32 $id][i32 [string length $payload]]$payload"
}
set END [u8 255]

# A list record as dgRecordDynList lays it out: NAME, INCREMENT, FLAGS,
# DATA marker, then the typed data tag.  `extra` is spliced in where the
# caller says: before the data (pre) or after it (post).
proc list_record {name datatag n data {pre ""} {post ""}} {
    set r [u8 2]                        ;# DG_DYNLIST_TAG
    append r [cstr 0 $name]             ;# DL_NAME_TAG
    append r [u8 1][i32 10]             ;# DL_INCREMENT_TAG
    append r [u8 10][i32 0]             ;# DL_FLAGS_TAG
    append r $pre
    append r [u8 2]                     ;# DL_DATA_TAG (marker, no payload)
    append r [u8 $datatag][i32 $n]$data
    append r $post
    append r $::END
    return $r
}

proc build_file {top group list_pre list_post} {
    set f [binary format c4 {0x21 0x12 0x36 0x63}]   ;# magic
    append f [u8 0][f32 1.0]                         ;# DG_VERSION_TAG 1.0
    append f $top
    append f [u8 1]                                  ;# DG_BEGIN_TAG
    append f [cstr 0 "extgroup"]                     ;# DG_NAME_TAG
    append f [u8 1][i32 2]                           ;# DG_NLISTS_TAG
    append f $group
    append f [list_record a 6 3 "[i32 1][i32 2][i32 3]" $list_pre ""]
    append f [list_record b 12 2 "[f64 1.5][f64 2.5]" "" $list_post]
    append f $::END                                  ;# end group
    append f $::END                                  ;# end top level
    return $f
}

set dir [file join [pwd] ext_test_[pid]]
file mkdir $dir
proc writefile {path bytes {gzip 0}} {
    set fh [open $path wb]
    puts -nonewline $fh [expr {$gzip ? [zlib gzip $bytes] : $bytes}]
    close $fh
}

# ---- 1. envelopes at every level, both parsers ------------------------
set bytes [build_file \
    [ext 7 "top-level payload"] \
    [ext 8 ""] \
    [ext 9 "xyz"] \
    [ext 1 "!"]]

writefile $dir/ext.dg  $bytes
writefile $dir/ext.dgz $bytes 1

foreach {label path} [list "file parser (.dg)" $dir/ext.dg \
                           "buffer parser (.dgz)" $dir/ext.dgz] {
    set g [dg_read $path]
    check "$label: list names"   [dl_tcllist [dg_listnames $g]] {a b}
    check "$label: a intact"     [dl_tcllist $g:a] {1 2 3}
    check "$label: a type"       [dl_datatype $g:a] long
    check "$label: b intact"     [dl_tcllist $g:b] {1.5 2.5}
    check "$label: b type"       [dl_datatype $g:b] double
    dg_delete $g
}

# ---- 2. no envelopes at all still reads (the helpers are sound) --------
writefile $dir/plain.dg [build_file "" "" "" ""]
set g [dg_read $dir/plain.dg]
check "plain file reads" [dl_tcllist $g:a] {1 2 3}
dg_delete $g

# ---- 3. a large payload is stepped over, not parsed --------------------
set big [string repeat "\xff" 100000]
writefile $dir/big.dg  [build_file [ext 42 $big] "" "" ""]
writefile $dir/big.dgz [build_file [ext 42 $big] "" "" ""] 1
foreach path [list $dir/big.dg $dir/big.dgz] {
    set g [dg_read $path]
    check "100 KB envelope skipped ([file extension $path])" [dl_tcllist $g:b] {1.5 2.5}
    dg_delete $g
}

# ---- 4. corrupt envelopes are errors, not crashes ----------------------
# length larger than the rest of the file
set bad_len "[u8 250][i32 5][i32 100000000]abc"
writefile $dir/badlen.dg  [build_file "" $bad_len "" ""]
writefile $dir/badlen.dgz [build_file "" $bad_len "" ""] 1
check "oversize length (.dg) errors"  [catch {dg_read $dir/badlen.dg}]  1
check "oversize length (.dgz) errors" [catch {dg_read $dir/badlen.dgz}] 1

# negative length
set neg_len "[u8 250][i32 5][i32 -1]"
writefile $dir/neglen.dg  [build_file "" "" $neg_len ""]
writefile $dir/neglen.dgz [build_file "" "" $neg_len ""] 1
check "negative length (.dg) errors"  [catch {dg_read $dir/neglen.dg}]  1
check "negative length (.dgz) errors" [catch {dg_read $dir/neglen.dgz}] 1

# header cut off by end of file
set f [build_file "" "" "" ""]
set truncated [string range $f 0 end-2]
append truncated [u8 250][i32 5]
writefile $dir/trunc.dg  $truncated
writefile $dir/trunc.dgz $truncated 1
check "truncated header (.dg) errors"  [catch {dg_read $dir/trunc.dg}]  1
check "truncated header (.dgz) errors" [catch {dg_read $dir/trunc.dgz}] 1

# ---- 5. a still-unknown bare tag still aborts (the envelope is opt-in) --
set bare_unknown [u8 99]
writefile $dir/bare.dg [build_file "" "" $bare_unknown ""]
check "bare unknown tag still errors" [catch {dg_read $dir/bare.dg}] 1

file delete -force $dir

if {$::fail} {
    puts "=== $::fail FAILURE(S) ==="
    exit 1
}
puts "=== ALL PASS ==="
