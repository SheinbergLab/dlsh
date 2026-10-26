# dslog: reading dserv data files

The `dslog` package (`package require dslog`) reads the `.ess` files that
dserv's logger writes. A file is a 16-byte header followed by records, each
carrying a datapoint name, a timestamp in epoch microseconds, a dserv type
code, and a payload. This page covers the whole-file readers and the three
per-datapoint table readers; `dslog::open` / `next` / `put` / `close` are
the record-at-a-time layer underneath them, for the cases these do not fit.

## The two whole-file readers

`dslog::read path` gives a flat group named after the path, one row per
record, with `varname`, `timestamp` (seconds since the file's first
record; `float`, or `double` under `dslog::wideTypes 1`) and `vals` (a
list column, one sublist per record). Event records are named
`evt:TYPE:SUBTYPE`, and a synthetic `logger:open` row carrying the file
version sits first. `dslog::readESS path` is the obs-oriented analysis
group. Both narrow doubles to float and drop int64 payloads unless
`dslog::wideTypes` is on; the reasons are in
[dynlists.md](dynlists.md#element-types-and-the-two-wide-ones).

`dslog::read` takes two options, which can be combined:

| Option | Effect |
|---|---|
| `-match pattern` (repeatable) | keep only records whose name matches one of the patterns (Tcl `string match` rules, `evt:*` names and the `logger:open` row included). The file is still scanned end to end, and `timestamp` stays relative to the file's first record whatever it is called; only the decoding of the other records is skipped. |
| `-absolute` | `timestamp` becomes an `int64` column of epoch microseconds, the record timestamps exactly as logged. This column does not depend on `wideTypes`. The `logger:open` row takes the first record's timestamp. |

With no option the output is exactly what it was before the options
existed.

```tcl
set g [dslog::read $path -match stim/frame -match stim/cmd -absolute]
```

## Per-datapoint tables: the wide-type assumption

`dslog::unpack` and `dslog::jsonTable` turn every record of one datapoint
into one row of a rectangular group, with a typed column per field. They
exist for the records that used to need a Tcl loop over `dslog::next`: the
per-swap `stim/frame` array (seven int64 at up to 120 rows a second), the
`stim/cmd` JSON objects, and the one-per-file `stim/script`, `stim/bind`
and `stim/lib/<name>` JSON records.

**Both always create `int64` and `double` columns, whatever
`dslog::wideTypes` says.** Every record they are for carries microsecond
timestamps or 64-bit counters, and a float32 or a 32-bit long would lose
them. A group they return can therefore only be written to or read by
tools that know the wide element types (dlsh from 2026, dgread 1.2.1+ and
the readers listed in dynlists.md).

Both groups start with the same two columns:

| Column | Type | Meaning |
|---|---|---|
| `timestamp` | int64 | the record's timestamp, epoch microseconds, as logged |
| `seconds` | double | the same, as seconds since the file's first record (any datapoint), the anchor `dslog::read` uses |

The group is named after the datapoint (`stim/frame`, `stim/cmd`), so,
like `dslog::read`, reading it again while the first group still exists
fails with "already exists": `dg_delete` or `dg_rename` the old one first.
A file with no record of that name is an error, not an empty table,
because the field columns could not be typed.

## dslog::unpack path name {field ...}

For a datapoint whose payload is a fixed-width array: one column per
element, named by the field list, typed by the payload:

| Payload type | Column type |
|---|---|
| DSERV_INT64 | int64 |
| DSERV_INT | int64 (32-bit ints are widened) |
| DSERV_DOUBLE | double |
| DSERV_FLOAT | float |
| DSERV_SHORT | short |

Every record of the name must have exactly as many elements as there are
field names; one that does not is an error naming the record (its index
among records of that name and its index in the file) and both counts:

    stim/frame record 1 (file record 17) has 5 values, expected 7

A record whose type differs from the first record's, or a payload that is
not one of the five array types (a string, JSON, an event), is an error
in the same form.

```tcl
set fr [dslog::unpack $path stim/frame \
            {frame swapcount flip_us ticks_us missed cmds_applied ds_dispatched}]
dg_tclListnames $fr
;# timestamp seconds frame swapcount flip_us ticks_us missed cmds_applied ds_dispatched
dl_datatype $fr:flip_us      ;# int64
dl_diff $fr:flip_us          ;# the swap intervals, in us
```

Gives one row per display swap with seven int64 columns.

## dslog::jsonTable path name

For a datapoint of DSERV_JSON type, or a DSERV_STRING whose text is a JSON
object: each record is parsed (with jansson), and every top-level scalar
key becomes a column, in the order keys are first seen across the whole
file.

| JSON value | Column type | Stored as |
|---|---|---|
| integer | int64 | the value |
| real | double | the value |
| string | string | the value |
| `true` / `false` | int64 | 1 / 0 |
| `null` | (the column's type) | the missing marker |
| object, array | not a column | skipped |

**Missing markers.** A key that a record lacks, or holds `null` in, gets
the marker of its column's type:

| Column type | Marker |
|---|---|
| int64 | `INT64_MIN` (-9223372036854775808) |
| double | NaN |
| string | `""` |

A key that is `null` in every record becomes an int64 column of markers.
Keys that hold integers in some records and reals in others are promoted
to double, with every integer widened and every marker kept as NaN. A key
that is a string in one record and a number in another is an error naming
the record and the key; so is a record that does not parse, or parses to
something other than an object:

    stim/cmd record 7 (file record 913): string or '}' expected near 'not' (byte 1)

Keys called `timestamp` or `seconds` would add a second column of that
name; none of the lab's records uses either.

```tcl
set c [dslog::jsonTable $path stim/cmd]
dg_tclListnames $c
;# timestamp seconds seq cmd t_send t_ret dq done swap flip status hash len
```

For `stim/cmd` the `cmd` and `hash`/`len` columns are complementary: a
command longer than 4096 characters is logged as its sha256 `hash` and
byte `len` with no `cmd`, so those rows hold `""` in `cmd` and the others
hold `""` in `hash` and `INT64_MIN` in `len`. `dq`, `done`, `swap` and
`flip` are `null` when the reply carried no ACK header (`status` `nohdr`,
`io` or `timeout`), so they hold NaN / `INT64_MIN` there, and `dq` and
`done` are double columns. To join a hash-only row to the script it sent:

```tcl
set s [dslog::jsonTable $path stim/script]
set i [dl_find $c:hash [dl_get $s:hash 0]]
```

## Worked example: a frame and command report for one session

Everything below is whole-column `dl_*` math on the two tables; no loop
touches a record.

```tcl
package require dslog
set f /path/to/session.ess
set MISSING -9223372036854775808

set fr  [dslog::unpack $f stim/frame \
             {frame swapcount flip_us ticks_us missed cmds_applied ds_dispatched}]
set cmd [dslog::jsonTable $f stim/cmd]

# indices where a 0/1 mask is set (dl_findIndices returns one index, not all)
proc where {mask} { dl_select [dl_fromto 0 [dl_length $mask]] $mask }

# --- swap timing ---------------------------------------------------------
set ivl [dl_div [dl_double [dl_diff $fr:flip_us]] 1000.0]   ;# ms between hardware flips
puts "frames: [dl_length $fr:frame] over [format %.1f \
          [expr {[dl_last $fr:seconds] - [dl_first $fr:seconds]}]] s"
puts "flip interval ms: median [dl_median $ivl]  max [dl_max $ivl]"
puts "intervals > 20 ms: [dl_sum [dl_gt $ivl 20]]   stim2's missed count: [dl_sum $fr:missed]"
dl_foreach i [where [dl_gt $ivl 20]] {
    puts [format "  %7.2f s  %7.1f ms gap after frame %d" \
              [dl_get $fr:seconds $i] [dl_get $ivl $i] [dl_get $fr:frame $i]]
}
# cmds_applied and ds_dispatched are running totals: per-swap counts are their diffs
set per_swap [dl_diff $fr:cmds_applied]
puts "commands applied per swap: max [dl_max $per_swap], swaps with any: [dl_sum [dl_gt $per_swap 0]]"

# --- commands ------------------------------------------------------------
set st [dl_unique $cmd:status]
puts "commands: [dl_length $cmd:seq]  status [dl_tcllist $st] -> [dl_tcllist [dl_countOccurences $cmd:status $st]]"
dl_foreach i [where [dl_noteq $cmd:status ok]] {
    puts [format "  seq %-4d %-6s %s" [dl_get $cmd:seq $i] [dl_get $cmd:status $i] \
              [string range [dl_get $cmd:cmd $i] 0 60]]
}
# dq and done are ms on stim2's own tick clock (the clock of ticks_us), so
# only their difference means anything next to the wall-clock fields
set acked [dg_select $cmd [dl_noteq $cmd:swap $MISSING]]   ;# rows whose reply carried an ACK header
puts "queued -> done ms: median [dl_median [dl_sub $acked:done $acked:dq]]"

# --- join: which frame did each command land on? ---------------------------
# swapcount is contiguous in the frame log, so the row is plain arithmetic;
# dl_choose wants long indices, hence the dl_int
set row  [dl_sub $acked:swap [dl_first $fr:swapcount]]
set ok   [dl_and [dl_gte $row 0] [dl_lt $row [dl_length $fr:frame]]]
set j    [dg_select $acked $ok]
set frow [dl_int [dl_select $row $ok]]
set flip [dl_choose $fr:flip_us $frow]
set lat  [dl_div [dl_double [dl_sub $flip $j:t_send]] 1000.0]   ;# send -> hardware flip, ms
puts "send -> flip ms: median [dl_median $lat]  max [dl_max $lat]"
set has_flip [dl_noteq $j:flip $MISSING]
puts "ACK flip equals the frame log's flip_us: [dl_sum [dl_eq [dl_select $j:flip $has_flip] \
          [dl_select $flip $has_flip]]] of [dl_sum $has_flip]"
puts "slowest five:"
dl_foreach i [dl_choose [dl_sortIndices [dl_negate $lat]] [dl_fromto 0 5]] {
    puts [format "  %6.2f ms  seq %-4d swap %d  %s" [dl_get $lat $i] [dl_get $j:seq $i] \
              [dl_get $j:swap $i] [string range [dl_get $j:cmd $i] 0 55]]
}
```

On the 113 s human_runner_door session of 2026-10-25 this reports 11,426
swaps at a median 8.333 ms flip interval (120 Hz) with stim2's `missed`
at 0; 28 gaps over 20 ms, which come as a regular 308 ms + 825 ms pair
once per trial (the display idling between trials, not stalls); 113
commands, 110 `ok`, 2 `io`, 1 `error`; queued-to-done 5.1 ms median
inside stim2; send-to-flip 6.0 ms median and 12.5 ms max, the slowest
being `!preview_on`, `!scroll_on` and `!feedback`; and in all 59 commands
whose ACK carried a flip time it equalled the frame log's `flip_us`
exactly, so the two logs are on one clock.

Three facts about the records that are easy to get wrong:

- `cmds_applied` and `ds_dispatched` in `stim/frame` are running totals,
  not per-swap counts; `dl_diff` them.
- `dq` and `done` in `stim/cmd` are milliseconds on stim2's tick clock,
  the same clock as `ticks_us`, not offsets from `t_send`.
- `swap` in `stim/cmd` is the `swapcount` of the frame it landed on, and
  `flip` (when not null) is that frame's `flip_us`.

## Timing

On a 1.7 MB file with 15k records (11,426 `stim/frame`, 113 `stim/cmd`),
on an Apple Silicon Mac:

| | ms |
|---|---|
| `dslog::read` (wideTypes 1) | 6.3 |
| `dslog::read -match stim/frame` | 4.3 |
| `dslog::unpack … stim/frame` | 4.2 |
| `dslog::jsonTable … stim/cmd` | 3.8 |
| Tcl loop over `dslog::next` + `binary scan` for `stim/frame` | 58 |

`tests/test_dslog_tables.tcl` prints these for the real file when it is
present and asserts everything else against a fixture it writes itself
(`tests/dslog_fixture.tcl`).
