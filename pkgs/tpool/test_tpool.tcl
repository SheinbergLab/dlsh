#!/usr/bin/env tclsh
#
# test_tpool.tcl
#   Standalone tests for the tpool package (tpool_map C command)
#   and the thread_pool Tcl wrapper (thread_pool::map with dg collation).
#
#   Usage:
#     dlsh test_tpool.tcl
#
#   Or from any tclsh with dlsh.zip mounted:
#     tclsh test_tpool.tcl
#

# --- dlsh bootstrap ---
if {[catch {package require dlsh}]} {
    # Try mounting dlsh.zip if not already available
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

# --- load packages under test ---
package require tpool
package require thread_pool

# --- minimal test framework ---
set ::tests_run    0
set ::tests_passed 0
set ::tests_failed 0

proc assert {desc expr} {
    incr ::tests_run
    if {[uplevel 1 [list expr $expr]]} {
        incr ::tests_passed
        puts "  PASS: $desc"
    } else {
        incr ::tests_failed
        puts "  FAIL: $desc"
    }
}

proc test {name body} {
    puts "\n--- $name ---"
    if {[catch {uplevel 1 $body} err]} {
        incr ::tests_failed
        incr ::tests_run
        puts "  ERROR: $err"
    }
}

proc summary {} {
    puts "\n========================================="
    puts "Results: $::tests_passed/$::tests_run passed"
    if {$::tests_failed > 0} {
        puts "FAILED:  $::tests_failed tests"
        exit 1
    } else {
        puts "All tests passed."
    }
    puts "========================================="
}

# =========================================================================
# Tests for tpool_map (C command)
# =========================================================================

test "tpool_map: basic return structure" {
    set result [tpool_map 4 {package require dlsh} {
        set g [dg_create]
        dl_set $g:value [dl_fromto 0 $n]
        dg_toString $g __tmp__
        dg_delete $g
        return $__tmp__
    } -threads 2]

    assert "result is dict"       {[dict exists $result results]}
    assert "has missing key"      {[dict exists $result missing]}
    assert "has errors key"       {[dict exists $result errors]}
    assert "has n_threads key"    {[dict exists $result n_threads]}
    assert "missing is 0"         {[dict get $result missing] == 0}
    assert "errors is empty"      {[llength [dict get $result errors]] == 0}
    assert "n_threads is 2"       {[dict get $result n_threads] == 2}
    assert "2 result parts"       {[llength [dict get $result results]] == 2}
}

test "tpool_map: n=0 returns empty" {
    set result [tpool_map 0 {} {} -threads 1]

    assert "missing is 0"         {[dict get $result missing] == 0}
    assert "errors is empty"      {[llength [dict get $result errors]] == 0}
    assert "n_threads is 0"       {[dict get $result n_threads] == 0}
    assert "results is empty"     {[llength [dict get $result results]] == 0}
}

test "tpool_map: args_dict is passed through" {
    set result [tpool_map 1 {package require dlsh} {
        set g [dg_create]
        set v [dict get $args_dict mykey]
        dl_set $g:value [dl_slist $v]
        dg_toString $g __tmp__
        dg_delete $g
        return $__tmp__
    } -threads 1 -args {mykey hello}]

    assert "no errors"            {[dict get $result missing] == 0}
    set data [lindex [dict get $result results] 0]
    set g [dg_fromString $data __test_args__]
    assert "value is hello"       {[dl_get $g:value 0] eq "hello"}
    dg_delete $g
}

test "tpool_map: worker_id varies across threads" {
    set result [tpool_map 4 {package require dlsh} {
        set g [dg_create]
        dl_set $g:wid [dl_ilist $worker_id]
        dg_toString $g __tmp__
        dg_delete $g
        return $__tmp__
    } -threads 4]

    assert "4 results"            {[llength [dict get $result results]] == 4}

    set wids {}
    foreach part [dict get $result results] {
        set g [dg_fromString $part __test_wid__]
        lappend wids [dl_get $g:wid 0]
        dg_delete $g
    }
    set wids [lsort -integer $wids]
    assert "worker_ids are 0..3"  {$wids eq {0 1 2 3}}
}

test "tpool_map: work partitioning sums to n" {
    set n 17
    set result [tpool_map $n {package require dlsh} {
        set g [dg_create]
        dl_set $g:count [dl_ilist $n]
        dg_toString $g __tmp__
        dg_delete $g
        return $__tmp__
    } -threads 4]

    set total 0
    foreach part [dict get $result results] {
        set g [dg_fromString $part __test_part__]
        incr total [dl_get $g:count 0]
        dg_delete $g
    }
    assert "partitions sum to $n" {$total == $n}
}

test "tpool_map: script error is captured" {
    set result [tpool_map 2 {} {
        error "intentional failure"
    } -threads 2]

    assert "missing is 2"         {[dict get $result missing] == 2}
    assert "errors not empty"     {[llength [dict get $result errors]] > 0}
}

# =========================================================================
# Tests for thread_pool::map (Tcl wrapper with dg collation)
# =========================================================================

test "thread_pool::map: dg collation" {
    set setup "package require dlsh"
    set work {
        set g [dg_create]
        dl_set $g:value [dl_fromto 0 $n]
        dg_toString $g __tmp__
        dg_delete $g
        return $__tmp__
    }

    set result [thread_pool::map 10 $setup $work \
                    -threads 2 -min_batch 1]

    set dg      [dict get $result dg]
    set missing [dict get $result missing]
    set errors  [dict get $result errors]

    assert "dg returned"          {$dg ne ""}
    assert "missing is 0"         {$missing == 0}
    assert "errors is empty"      {[llength $errors] == 0}
    assert "10 rows total"        {[dl_length $dg:value] == 10}

    dg_delete $dg
}

test "thread_pool::map: below min_batch returns early" {
    set result [thread_pool::map 2 {} {} -min_batch 10]

    assert "dg is empty"          {[dict get $result dg] eq ""}
    assert "missing is 2"         {[dict get $result missing] == 2}
}

test "thread_pool::map: n=0 returns empty" {
    set result [thread_pool::map 0 {} {}]

    assert "dg is empty"          {[dict get $result dg] eq ""}
    assert "missing is 0"         {[dict get $result missing] == 0}
}

test "thread_pool::map: multi-column dg collation" {
    set setup "package require dlsh"
    set work {
        set g [dg_create]
        dl_set $g:id    [dl_fromto 0 $n]
        dl_set $g:label [dl_repeat [dl_slist "w${worker_id}"] $n]
        dg_toString $g __tmp__
        dg_delete $g
        return $__tmp__
    }

    set result [thread_pool::map 12 $setup $work \
                    -threads 3 -min_batch 1]

    set dg [dict get $result dg]
    assert "dg returned"          {$dg ne ""}
    assert "12 rows total"        {[dl_length $dg:id] == 12}
    assert "has label column"     {[dl_exists $dg:label]}
    assert "12 labels"            {[dl_length $dg:label] == 12}

    dg_delete $dg
}

# =========================================================================
summary
