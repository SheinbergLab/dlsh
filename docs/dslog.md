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
