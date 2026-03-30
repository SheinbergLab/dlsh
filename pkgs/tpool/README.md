# tpool — Thread Pool for Parallel Tcl Workloads

A standalone Tcl C extension that provides parallel script evaluation
via a lightweight thread pool.  Each worker thread gets its own
`Tcl_Interp` with access to the dlsh package VFS.

Originally extracted from [dserv](https://github.com/SheinbergLab/dserv)'s
built-in `TpoolMap` command.

## Packages

| Package | Type | Description |
|---------|------|-------------|
| `tpool` | C extension | Provides the `tpool_map` command |
| `thread_pool` | Tcl wrapper | dg-aware wrapper around `tpool_map` with result collation |

## Quick Start

```tcl
package require thread_pool

# Generate 20 work units across 4 threads
set result [thread_pool::map 20 \
    {package require dlsh; package require mypackage} \
    {
        set g [my_generate $n $args_dict]
        dg_toString $g __tmp__
        dg_delete $g
        return $__tmp__
    } \
    -threads 4 \
    -args {param1 value1 param2 value2}]

set dg      [dict get $result dg]      ;# collated dg
set missing [dict get $result missing] ;# count of failed units
set errors  [dict get $result errors]  ;# list of error messages
```

## tpool_map (C command)

```
tpool_map n setup_script work_script ?-threads N? ?-args dict? ?-seed 0|1?
```

### Arguments

| Arg | Description |
|-----|-------------|
| `n` | Total number of work units to distribute |
| `setup_script` | Evaluated once per worker before work begins (e.g., `package require` calls) |
| `work_script` | The work to perform; has `$n`, `$worker_id`, and `$args_dict` in scope |

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `-threads` | auto (ncpu - 1) | Number of worker threads |
| `-args` | `""` | Dict passed to workers as `$args_dict` |
| `-seed` | `1` | Seed each worker's RNG via `dl_srand` |

### Worker Variables

Each worker script has these variables pre-set:

- `$n` — number of work units assigned to this worker
- `$worker_id` — 0-based thread index
- `$args_dict` — the value passed via `-args`

### Return Value

A Tcl dict with keys:

- `results` — list of per-worker result strings
- `missing` — number of work units that failed
- `errors` — list of error messages
- `n_threads` — number of threads used

## thread_pool::map (Tcl wrapper)

```
thread_pool::map n setup_script work_script ?-threads N? ?-args dict? ?-min_batch N?
```

Calls `tpool_map`, then collates per-worker `dg_toString` results into
a single dg via `dg_fromString` + `dg_append`.

Returns a dict: `{dg <handle-or-""> missing <int> errors <list>}`

## Architecture

- Built as a stubs-based Tcl extension (same pattern as the Tcl Thread package)
- Each worker creates a new `Tcl_Interp`, calls `Tcl_InitStubs`, then
  bootstraps `auto_path` from the process-global zipfs VFS
- No dependency on dserv, TclServer, or any application framework
- Uses `std::thread` (C++11) for portability

## Testing

```sh
dlsh test_tpool.tcl
```

Tests exercise both the C command (`tpool_map`) and the Tcl wrapper
(`thread_pool::map`), covering work partitioning, args passthrough,
dg collation, error handling, and edge cases.
