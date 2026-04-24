#
# neuropixel.tcl — tools for Neuropixel "package" SQLite databases
#
# The Sheinberg Lab packaging pipeline produces a per-session SQLite file
# (a "package") containing sorted spike data from one or more recording
# blocks, together with probe geometry, unit metrics, and block/obs-period
# alignment tables. Behavioral data lives alongside in per-block .dgz files.
#
# This package bridges the two: it reads spike and unit data from a
# package DB and appends per-trial spike columns plus per-unit metadata to
# a behavior dgz, producing a single trial-oriented dgz suitable for
# viewing/analysis tools (e.g. dg_viewer).
#
# ---------------------------------------------------------------------------
# COLUMNS ADDED TO THE DGZ
# ---------------------------------------------------------------------------
# Per-trial spike columns (list-of-list, one sublist per trial):
#
#   spike_times      (float, ms)  spike time within the obs period — same
#                                 clock as stim_on / stim_off; zero is the
#                                 start of the trial's obs period
#   spike_unit       (int)        unit_id
#   spike_depth      (float, um)  probe y_um of unit's max channel
#
# And a scalar int per-trial pointer column:
#
#   spike_src_trial  (int)        index of the trial row that actually
#                                 holds this trial's spike sublists
#
# Per-unit metadata columns ("squared" list-of-list, ntrials rows long but
# data lives only in row 0; rows 1..N-1 are empty sublists):
#
#   unit_list_id        (int)           unit_id (parallel to all others)
#   unit_list_depth     (float, um)     y_um
#   unit_list_x         (float, um)     x_um
#   unit_list_amp       (float)         units.amplitude
#   unit_list_snr       (float)         unit_metrics.snr
#   unit_list_fr        (float, Hz)     unit_metrics.firing_rate
#   unit_list_presence  (float, 0..1)   unit_metrics.presence_ratio
#
# Only units that actually fire in this block appear. Order: unit_id asc.
#
# Self-describing metadata (flat per-trial columns; all rows hold the same
# value, readers should read row 0):
#
#   spike_schema_version   (int)     current: 1
#   spike_time_units       (string)  "ms_from_obs_start"
#   spike_missing_value    (float)   -1.0   (negative sentinel for missing)
#
# ---------------------------------------------------------------------------
# CONVENTIONS
# ---------------------------------------------------------------------------
# * All spike times are in ms relative to the START of each trial's obs
#   period — the same timebase as stim_on/stim_off. Never package-global
#   time. This is invariant across the pipeline and is the primary reason
#   long-session sync stays clean.
#
# * Trials that share an obs period: only the first trial carries the
#   spike sublists (spike_src_trial[i] == i). Others have empty sublists
#   and spike_src_trial[i] points back to the canonical trial. Readers
#   MUST dereference via spike_src_trial.
#
# * Missing-value sentinel is -1.0. All currently-emitted unit_list_*
#   columns are non-negative by construction (depths, x, amps, snr, fr,
#   presence) so a single sentinel is unambiguous. If a future revision
#   adds a signed metric (e.g. silhouette, amplitude_median), revisit
#   with NaN support or a parallel presence mask, and bump
#   spike_schema_version.
#
# Dependencies: dlsh (dg_*, dl_*) and sqlite3 — both provided by dlsh.zip.
#

package provide neuropixel 1.0
package require sqlite3

namespace eval ::neuropixel {
    variable SCHEMA_VERSION 1
    variable DEFAULT_MISSING -1.0
    variable DEFAULT_TIME_UNITS "ms_from_obs_start"

    namespace export \
        open_package resolve_block_id package_label \
        list_blocks match_trials_dir \
        unit_depths unit_metadata firing_unit_ids spikes_by_obs \
        add_spikes add_unit_metadata add_schema_columns \
        append_spikes_file append_spikes_dir
}

# ---------------------------------------------------------------------------
# open_package db_path ?-handle name?
# ---------------------------------------------------------------------------
proc ::neuropixel::open_package {db_path args} {
    set handle ""
    foreach {opt val} $args {
        switch -- $opt {
            -handle { set handle $val }
            default { error "open_package: unknown option $opt" }
        }
    }
    if {$handle eq ""} {
        set handle ::neuropixel::db[incr ::neuropixel::_db_counter]
    }
    sqlite3 $handle $db_path -readonly 1
    return $handle
}

# ---------------------------------------------------------------------------
# package_label db
#
# Return the package_label string (e.g. "bank_a"). Useful as a suffix for
# disambiguating output filenames when multiple packages (different channel
# subsets) cover the same behavior session. Returns "" if no label is set.
# ---------------------------------------------------------------------------
proc ::neuropixel::package_label {db} {
    set label [$db onecolumn {SELECT package_label FROM packages LIMIT 1}]
    if {$label eq ""} { return "" }
    return $label
}

# ---------------------------------------------------------------------------
# resolve_block_id db path_or_stem
# ---------------------------------------------------------------------------
proc ::neuropixel::resolve_block_id {db path_or_stem} {
    set stem [file tail $path_or_stem]
    foreach suffix {.trials.dgz .dgz .obs.dgz} {
        if {[string match *$suffix $stem]} {
            set stem [string range $stem 0 end-[string length $suffix]]
            break
        }
    }
    set bid [$db onecolumn {
        SELECT block_id FROM dgz_sources WHERE session_stem = :stem
    }]
    return $bid
}

# ---------------------------------------------------------------------------
# list_blocks db
#
# Return a list of dicts describing every block with an associated dgz
# source in the package. Each dict has:
#
#   block_id       integer
#   block_name     full block name (usually includes date/time suffix)
#   session_stem   the dgz session stem (matches behavior .dgz filename)
#   dgz_path       path stored at packaging time (may be foreign, e.g.
#                  Windows paths); do NOT assume it resolves locally
#
# Ordered by block_index.
# ---------------------------------------------------------------------------
proc ::neuropixel::list_blocks {db} {
    set out [list]
    $db eval {
        SELECT s.block_id       AS block_id,
               b.block_name     AS block_name,
               s.session_stem   AS session_stem,
               s.dgz_path       AS dgz_path
        FROM dgz_sources s
        JOIN blocks b ON b.block_id = s.block_id
        ORDER BY b.block_index
    } row {
        lappend out [dict create \
            block_id     $row(block_id)     \
            block_name   $row(block_name)   \
            session_stem $row(session_stem) \
            dgz_path     $row(dgz_path)]
    }
    return $out
}

# ---------------------------------------------------------------------------
# match_trials_dir db trials_dir ?-pattern glob?
#
# Scan a local directory for .dgz / .trials.dgz files whose stems match a
# session_stem in the package. Returns a list of dicts:
#
#   block_id
#   session_stem
#   path            local filesystem path to the matched dgz
#
# Files that don't match any session_stem in the DB are silently skipped.
# ---------------------------------------------------------------------------
proc ::neuropixel::match_trials_dir {db trials_dir args} {
    set pattern "*.dgz"
    foreach {opt val} $args {
        switch -- $opt {
            -pattern { set pattern $val }
            default  { error "match_trials_dir: unknown option $opt" }
        }
    }

    # Build session_stem -> block_id lookup
    set stem2bid [dict create]
    foreach blk [list_blocks $db] {
        dict set stem2bid [dict get $blk session_stem] [dict get $blk block_id]
    }

    set matches [list]
    foreach path [lsort [glob -nocomplain -directory $trials_dir $pattern]] {
        set stem [file tail $path]
        foreach suffix {.trials.dgz .dgz .obs.dgz} {
            if {[string match *$suffix $stem]} {
                set stem [string range $stem 0 end-[string length $suffix]]
                break
            }
        }
        if {[dict exists $stem2bid $stem]} {
            lappend matches [dict create \
                block_id     [dict get $stem2bid $stem] \
                session_stem $stem \
                path         $path]
        }
    }
    return $matches
}

# ---------------------------------------------------------------------------
# unit_depths db ?-default val?
# Quick path for just unit_id -> y_um. See unit_metadata for the full set.
# ---------------------------------------------------------------------------
proc ::neuropixel::unit_depths {db args} {
    set default_depth $::neuropixel::DEFAULT_MISSING
    foreach {opt val} $args {
        switch -- $opt {
            -default { set default_depth $val }
            default  { error "unit_depths: unknown option $opt" }
        }
    }
    set result [dict create]
    $db eval {
        SELECT u.unit_id AS uid,
               pc.y_um   AS y_um
        FROM units u
        LEFT JOIN probe_channels pc
          ON pc.package_id = u.package_id
         AND pc.channel_id = CAST(REPLACE(u.max_on_channel_id, 'AP', '') AS INTEGER)
    } row {
        set d [expr {$row(y_um) eq "" ? $default_depth : $row(y_um)}]
        dict set result $row(uid) $d
    }
    return $result
}

# ---------------------------------------------------------------------------
# spikes_by_obs db block_id ?-depths dict?
#
# Returns dict obs_period -> [list times_list units_list depths_list].
# Inner lists are parallel (same length) and time-sorted.
# ---------------------------------------------------------------------------
proc ::neuropixel::spikes_by_obs {db block_id args} {
    set depths ""
    foreach {opt val} $args {
        switch -- $opt {
            -depths { set depths $val }
            default { error "spikes_by_obs: unknown option $opt" }
        }
    }
    if {$depths eq ""} {
        set depths [unit_depths $db]
    }

    array set T {}
    array set U {}
    array set D {}
    $db eval {
        SELECT unit_id          AS uid,
               block_obs_period AS op,
               obs_period_ms    AS ms
        FROM spike_raster_ms
        WHERE block_id = :block_id
        ORDER BY block_obs_period, obs_period_ms
    } {
        lappend T($op) $ms
        lappend U($op) $uid
        if {[dict exists $depths $uid]} {
            lappend D($op) [dict get $depths $uid]
        } else {
            lappend D($op) $::neuropixel::DEFAULT_MISSING
        }
    }

    set out [dict create]
    foreach op [array names T] {
        dict set out $op [list $T($op) $U($op) $D($op)]
    }
    return $out
}

# ---------------------------------------------------------------------------
# firing_unit_ids spikes_by_obs
#
# Given the dict from spikes_by_obs, return the sorted (ascending) list of
# distinct unit_ids that appear in at least one spike. This is the
# population that will populate unit_list_*.
# ---------------------------------------------------------------------------
proc ::neuropixel::firing_unit_ids {spikes_by_obs} {
    array set seen {}
    dict for {op triple} $spikes_by_obs {
        foreach uid [lindex $triple 1] {
            set seen($uid) 1
        }
    }
    return [lsort -integer [array names seen]]
}

# ---------------------------------------------------------------------------
# unit_metadata db unit_ids ?-missing val?
#
# Given an open DB and a list of unit_ids, return a dict mapping
# unit_id -> dict with keys {depth x amp snr fr presence}. Missing values
# are replaced with -missing (default -1.0).
# ---------------------------------------------------------------------------
proc ::neuropixel::unit_metadata {db unit_ids args} {
    set missing $::neuropixel::DEFAULT_MISSING
    foreach {opt val} $args {
        switch -- $opt {
            -missing { set missing $val }
            default  { error "unit_metadata: unknown option $opt" }
        }
    }
    set result [dict create]
    if {[llength $unit_ids] == 0} { return $result }

    # unit_ids are integers we already got from the DB; safe to inline.
    set in_clause [join $unit_ids ,]

    # LEFT JOIN two pivoted subqueries (units→probe_channels for depth/x,
    # then unit_metrics pivot for snr/fr/presence). GROUP BY unit_id to
    # fold the per-metric rows into one row per unit.
    set sql "
        SELECT u.unit_id AS uid,
               u.amplitude AS amp,
               u.x_um AS xum,
               pc.y_um AS yum,
               MAX(CASE WHEN m.metric_name='snr'            THEN m.metric_value END) AS snr,
               MAX(CASE WHEN m.metric_name='firing_rate'    THEN m.metric_value END) AS fr,
               MAX(CASE WHEN m.metric_name='presence_ratio' THEN m.metric_value END) AS presence
        FROM units u
        LEFT JOIN probe_channels pc
          ON pc.package_id = u.package_id
         AND pc.channel_id = CAST(REPLACE(u.max_on_channel_id, 'AP', '') AS INTEGER)
        LEFT JOIN unit_metrics m ON m.unit_id = u.unit_id
        WHERE u.unit_id IN ($in_clause)
        GROUP BY u.unit_id
    "
    $db eval $sql row {
        dict set result $row(uid) [dict create \
            depth    [expr {$row(yum)      eq "" ? $missing : $row(yum)}]      \
            x        [expr {$row(xum)      eq "" ? $missing : $row(xum)}]      \
            amp      [expr {$row(amp)      eq "" ? $missing : $row(amp)}]      \
            snr      [expr {$row(snr)      eq "" ? $missing : $row(snr)}]      \
            fr       [expr {$row(fr)       eq "" ? $missing : $row(fr)}]       \
            presence [expr {$row(presence) eq "" ? $missing : $row(presence)}]]
    }
    return $result
}

# ---------------------------------------------------------------------------
# add_spikes g spikes_by_obs ?-obsid_col obsid?
#
# Append spike_times / spike_unit / spike_depth / spike_src_trial columns
# with per-obs dedup. Returns the number of trials that actually carry
# spike sublists.
# ---------------------------------------------------------------------------
proc ::neuropixel::add_spikes {g spikes_by_obs args} {
    set obsid_col obsid
    foreach {opt val} $args {
        switch -- $opt {
            -obsid_col { set obsid_col $val }
            default    { error "add_spikes: unknown option $opt" }
        }
    }
    if {![dl_exists $g:$obsid_col]} {
        error "add_spikes: no column '$obsid_col' in $g"
    }
    set ntrials [dl_length $g:$obsid_col]

    set times_col  [dl_llist]
    set units_col  [dl_llist]
    set depths_col [dl_llist]
    set src_col    [dl_ilist]

    array set first_trial {}
    set n_src 0

    for {set i 0} {$i < $ntrials} {incr i} {
        set op [dl_get $g:$obsid_col $i]
        set have [dict exists $spikes_by_obs $op]

        if {$have && ![info exists first_trial($op)]} {
            set first_trial($op) $i
            lassign [dict get $spikes_by_obs $op] t_list u_list d_list
            dl_append $times_col  [dl_flist {*}$t_list]
            dl_append $units_col  [dl_ilist {*}$u_list]
            dl_append $depths_col [dl_flist {*}$d_list]
            dl_append $src_col    $i
            incr n_src
        } else {
            dl_append $times_col  [dl_flist]
            dl_append $units_col  [dl_ilist]
            dl_append $depths_col [dl_flist]
            set pointer [expr {[info exists first_trial($op)] ? $first_trial($op) : $i}]
            dl_append $src_col $pointer
        }
    }

    dl_set $g:spike_times     $times_col
    dl_set $g:spike_unit      $units_col
    dl_set $g:spike_depth     $depths_col
    dl_set $g:spike_src_trial $src_col
    return $n_src
}

# ---------------------------------------------------------------------------
# add_unit_metadata g unit_ids metadata ?-obsid_col col?
#
# Append per-unit metadata as "squared" list-of-list columns: outer length
# ntrials, with data only on row 0 and empty sublists on rows 1..N-1.
# ---------------------------------------------------------------------------
proc ::neuropixel::add_unit_metadata {g unit_ids metadata args} {
    set obsid_col obsid
    foreach {opt val} $args {
        switch -- $opt {
            -obsid_col { set obsid_col $val }
            default    { error "add_unit_metadata: unknown option $opt" }
        }
    }
    if {![dl_exists $g:$obsid_col]} {
        error "add_unit_metadata: no column '$obsid_col' in $g"
    }
    set ntrials [dl_length $g:$obsid_col]

    # Build row-0 sublists.
    set id_row    [list]
    set depth_row [list]
    set x_row     [list]
    set amp_row   [list]
    set snr_row   [list]
    set fr_row    [list]
    set pres_row  [list]
    foreach uid $unit_ids {
        set m [dict get $metadata $uid]
        lappend id_row    $uid
        lappend depth_row [dict get $m depth]
        lappend x_row     [dict get $m x]
        lappend amp_row   [dict get $m amp]
        lappend snr_row   [dict get $m snr]
        lappend fr_row    [dict get $m fr]
        lappend pres_row  [dict get $m presence]
    }

    # Wrap each row-0 sublist in an ntrials-long outer list, empty elsewhere.
    foreach {col_name inner_ctor row} [list \
        unit_list_id         dl_ilist $id_row    \
        unit_list_depth      dl_flist $depth_row \
        unit_list_x          dl_flist $x_row     \
        unit_list_amp        dl_flist $amp_row   \
        unit_list_snr        dl_flist $snr_row   \
        unit_list_fr         dl_flist $fr_row    \
        unit_list_presence   dl_flist $pres_row  \
    ] {
        set outer [dl_llist]
        dl_append $outer [$inner_ctor {*}$row]
        for {set i 1} {$i < $ntrials} {incr i} {
            dl_append $outer [$inner_ctor]
        }
        dl_set $g:$col_name $outer
    }

    return [llength $unit_ids]
}

# ---------------------------------------------------------------------------
# add_schema_columns g ?-version v? ?-time_units s? ?-missing v? ?-obsid_col c?
#
# Append flat per-trial scalar columns that describe how to read the
# spike/unit data. Every trial row gets the same value, so readers can
# simply take row 0.
# ---------------------------------------------------------------------------
proc ::neuropixel::add_schema_columns {g args} {
    set version    $::neuropixel::SCHEMA_VERSION
    set time_units $::neuropixel::DEFAULT_TIME_UNITS
    set missing    $::neuropixel::DEFAULT_MISSING
    set obsid_col  obsid
    foreach {opt val} $args {
        switch -- $opt {
            -version    { set version $val }
            -time_units { set time_units $val }
            -missing    { set missing $val }
            -obsid_col  { set obsid_col $val }
            default     { error "add_schema_columns: unknown option $opt" }
        }
    }
    if {![dl_exists $g:$obsid_col]} {
        error "add_schema_columns: no column '$obsid_col' in $g"
    }
    set ntrials [dl_length $g:$obsid_col]

    set v_col [dl_ilist]
    set t_col [dl_slist]
    set m_col [dl_flist]
    for {set i 0} {$i < $ntrials} {incr i} {
        dl_append $v_col $version
        dl_append $t_col $time_units
        dl_append $m_col $missing
    }
    dl_set $g:spike_schema_version $v_col
    dl_set $g:spike_time_units     $t_col
    dl_set $g:spike_missing_value  $m_col
}

# ---------------------------------------------------------------------------
# append_spikes_file db_path in_dgz out_dgz ?options?
#
# All-in-one: open the package DB, read $in_dgz, add spike + unit-metadata
# + schema columns, write $out_dgz. Returns a dict with:
#
#   block_id    matched block_id (empty if unmatched and -require_block 0)
#   ntrials     number of trial rows
#   n_src       number of trials that actually carry spike sublists
#   nunits      number of units with spikes in this block
#   n_spikes    total number of spikes attached
#
# Options:
#   -block_id <int>          override the dgz_sources lookup
#   -require_block 0|1       error if no matching block (default 1)
#   -obsid_col <name>        override obsid column name (default "obsid")
#   -with_unit_meta 0|1      include unit_list_* columns (default 1)
#   -with_schema    0|1      include spike_schema_* columns (default 1)
# ---------------------------------------------------------------------------
proc ::neuropixel::append_spikes_file {db_path in_dgz out_dgz args} {
    set block_id        ""
    set require_block   1
    set obsid_col       obsid
    set with_unit_meta  1
    set with_schema     1
    foreach {opt val} $args {
        switch -- $opt {
            -block_id        { set block_id $val }
            -require_block   { set require_block $val }
            -obsid_col       { set obsid_col $val }
            -with_unit_meta  { set with_unit_meta $val }
            -with_schema     { set with_schema $val }
            default          { error "append_spikes_file: unknown option $opt" }
        }
    }

    set db [open_package $db_path]
    set unit_ids [list]
    set meta [dict create]
    try {
        if {$block_id eq ""} {
            set block_id [resolve_block_id $db $in_dgz]
        }
        if {$block_id eq ""} {
            if {$require_block} {
                error "no matching block for [file tail $in_dgz] in $db_path"
            }
            set sbo [dict create]
        } else {
            set sbo [spikes_by_obs $db $block_id]
            if {$with_unit_meta} {
                set unit_ids [firing_unit_ids $sbo]
                set meta [unit_metadata $db $unit_ids]
            }
        }
    } finally {
        $db close
    }

    set g [dg_read $in_dgz]
    set n_src    [add_spikes $g $sbo -obsid_col $obsid_col]
    if {$with_unit_meta && [llength $unit_ids]} {
        add_unit_metadata $g $unit_ids $meta -obsid_col $obsid_col
    }
    if {$with_schema} {
        add_schema_columns $g -obsid_col $obsid_col
    }
    set ntrials  [dl_length $g:$obsid_col]
    set n_spikes 0
    dict for {op triple} $sbo {
        incr n_spikes [llength [lindex $triple 0]]
    }
    dg_write $g $out_dgz
    dg_delete $g

    return [dict create \
        block_id $block_id \
        ntrials  $ntrials  \
        n_src    $n_src    \
        nunits   [llength $unit_ids] \
        n_spikes $n_spikes]
}

# ---------------------------------------------------------------------------
# append_spikes_dir db_path trials_dir out_dir ?options?
#
# Batch variant of append_spikes_file: match every dgz in $trials_dir
# against the package's session_stems and write an augmented dgz for each
# match into $out_dir. Unmatched files are skipped.
#
# Output filename:
#   "<session_stem>.<suffix>.spikes.dgz" if a non-empty suffix is used
#   "<session_stem>.spikes.dgz"          otherwise
#
# Suffix resolution:
#   -suffix <string>   explicit; "" forces bare names
#   (not supplied)     auto — use packages.package_label from the DB if
#                      non-empty (e.g. "bank_a"), else bare.
#
# Other options are forwarded per-file to append_spikes_file (e.g.
# -with_unit_meta, -with_schema, -obsid_col). -block_id / -require_block
# are not accepted here since matching is automatic.
#
# Returns a list of per-file info dicts, each extending the
# append_spikes_file result with:
#
#   session_stem   matched stem
#   in_path        input dgz path
#   out_path       written dgz path
#   suffix         effective suffix applied ("" if none)
# ---------------------------------------------------------------------------
proc ::neuropixel::append_spikes_dir {db_path trials_dir out_dir args} {
    # Split -suffix out of args; pass the rest through to append_spikes_file.
    set suffix_given 0
    set suffix ""
    set forwarded [list]
    foreach {opt val} $args {
        switch -- $opt {
            -block_id - -require_block {
                error "append_spikes_dir: $opt is not applicable in batch mode"
            }
            -suffix {
                set suffix $val
                set suffix_given 1
            }
            default {
                lappend forwarded $opt $val
            }
        }
    }

    file mkdir $out_dir

    set db [open_package $db_path]
    set matches [match_trials_dir $db $trials_dir]
    if {!$suffix_given} {
        set suffix [package_label $db]
    }
    $db close

    set results [list]
    foreach m $matches {
        set in_path  [dict get $m path]
        set stem     [dict get $m session_stem]
        set out_name [expr {$suffix eq "" \
            ? "${stem}.spikes.dgz" \
            : "${stem}.${suffix}.spikes.dgz"}]
        set out_path [file join $out_dir $out_name]
        set info [append_spikes_file $db_path $in_path $out_path \
                    -block_id [dict get $m block_id] {*}$forwarded]
        dict set info session_stem $stem
        dict set info in_path      $in_path
        dict set info out_path     $out_path
        dict set info suffix       $suffix
        lappend results $info
    }
    return $results
}
