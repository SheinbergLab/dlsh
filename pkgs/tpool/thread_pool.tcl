# -*- mode: tcl -*-
#
# thread_pool.tcl
#   General-purpose thread pool for distributing work that
#   produces serialized dg (dynamic group) results.
#
#   Uses the tpool_map C command (from the tpool package) for thread
#   management.  Each worker gets its own Tcl interp with deterministic
#   cleanup via C++ destructors.
#
#   Contract:
#     - Caller supplies a setup script (run once per worker)
#     - Caller supplies a work script with $n, $worker_id,
#       and $args_dict in scope.  The work script must return
#       a dg serialized via dg_toString.
#     - Returns a dict: {dg <collated-dg-or-""> missing <int> errors <list>}
#
#   Options:
#     -threads        <int>    number of worker threads (default: auto)
#     -min_batch      <int>    minimum n before threading is attempted
#     -args           <dict>   passed into work script as $args_dict
#     -seed_workers   <bool>   if 1, seed dl_srand/srand per worker (default: 1)
#

package provide thread_pool 1.0

namespace eval thread_pool {

    # --------------------------------------------------------------------------
    # thread_pool::map
    #
    #   Thin wrapper around the tpool_map C command.
    #   Handles the dg collation from serialized string results.
    # --------------------------------------------------------------------------
    proc map {n setup_script work_script args} {

        # --- option defaults ---
        set num_threads  {}
        set min_batch    4
        set args_dict    {}
        set seed_workers 1

        # --- parse options ---
        foreach {opt val} $args {
            switch -- $opt {
                -threads      { set num_threads  $val }
                -min_batch    { set min_batch    $val }
                -args         { set args_dict    $val }
                -seed_workers { set seed_workers $val }
                -init_timeout - -base_timeout - -ms_per_unit {
                    # legacy timing options -- ignored (C implementation
                    # uses deterministic join, no timeouts)
                }
                default       { error "unknown option: $opt" }
            }
        }

        # --- trivial cases ---
        if {$n <= 0} {
            return [dict create dg "" missing 0 errors {}]
        }
        if {$n < $min_batch} {
            puts "thread_pool: n=$n below min_batch=$min_batch, skipping threads"
            return [dict create dg "" missing $n errors {"below min_batch"}]
        }

        # --- build tpool_map arguments ---
        set cmd [list tpool_map $n $setup_script $work_script]
        if {$num_threads ne {}} {
            lappend cmd -threads $num_threads
        }
        lappend cmd -args $args_dict
        lappend cmd -seed $seed_workers

        # --- call C command ---
        set result [{*}$cmd]

        set results [dict get $result results]
        set missing [dict get $result missing]
        set errors  [dict get $result errors]

        # --- collate dg results from per-worker list ---
        set all_dg ""

        set part_idx 0
        foreach part $results {
            if {$part eq ""} { incr part_idx; continue }
            set tmp_name "__tpool_${part_idx}_[clock microseconds]__"
            if {[catch {
                set g [_safe_dg_fromString $part $tmp_name]
                if {$all_dg eq ""} {
                    set all_dg $g
                } else {
                    dg_append $all_dg $g
                    dg_delete $g
                }
            } merge_err]} {
                puts "thread_pool: merge failed for worker $part_idx: $merge_err"
                lappend errors "merge worker $part_idx: $merge_err"
            }
            incr part_idx
        }

        set collected [expr {$n - $missing}]
        set n_threads [dict get $result n_threads]
        puts "thread_pool: $collected/$n work units collected \
            ($n_threads threads, [llength $errors] errors)"

        return [dict create dg $all_dg missing $missing errors $errors]
    }

    # --------------------------------------------------------------------------
    # Internal helpers
    # --------------------------------------------------------------------------

    proc _safe_dg_fromString {data name} {
        if {[dg_exists $name]} {
            dg_delete $name
        }
        return [dg_fromString $data $name]
    }

    namespace export map
}
