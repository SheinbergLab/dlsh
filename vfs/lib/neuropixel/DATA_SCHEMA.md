# Neuropixel spike-augmented dgz — data schema

Produced by the `neuropixel` Tcl package. This file documents the columns
added to a behavior `.dgz` when spikes and per-unit metadata are joined
from a Sheinberg Lab Neuropixel package SQLite file.

The augmented file is still a standard DG group (readable by `dgread` in
Python and `dg_reader.js` in the browser). All columns are trial-oriented
(outer length = number of trials), including the per-unit metadata —
see "Rectangular rule" below.

## Version

Schema version **1**. Readers should look at `spike_schema_version[0]` at
load time and branch / error cleanly if they encounter an unknown version.

## Time base (invariant)

All spike times are in **milliseconds from the start of the trial's
observation period** — the same clock as `stim_on`, `stim_off`, and
`obs_duration`. Times are never package-global. Aligning to any event
`E` in the dgz is just `t - E[i]`, where `i` is the trial index.

This convention is why long-session sync stays clean: each obs is its own
zero, so block-level drift across a long recording cannot leak into
spike/behavior alignment.

## Per-trial spike columns

| column | DG type | shape | units | meaning |
|---|---|---|---|---|
| `spike_times` | list(float) | ntrials sublists | ms | spike time within the obs period |
| `spike_unit` | list(int) | ntrials sublists | — | `unit_id` (globally unique in the package) |
| `spike_depth` | list(float) | ntrials sublists | μm | probe `y_um` of the unit's max channel |
| `spike_src_trial` | int scalar | ntrials | — | index of the trial row that holds this trial's spike sublists |

### Dedup rule

When several trials share an obs period, only the *first* trial carries
the spike sublists. The rest have zero-length sublists and their
`spike_src_trial` points back to the canonical trial. Readers **must**
dereference before reading:

```js
const src = getScalar(dg, 'spike_src_trial', i);
const times  = getSublistData(dg, 'spike_times',  src);
const units  = getSublistData(dg, 'spike_unit',   src);
const depths = getSublistData(dg, 'spike_depth',  src);
```

If trial `i` is the canonical trial, `spike_src_trial[i] == i`. If the
trial's obs has no spikes at all, `spike_src_trial[i] == i` still holds
(dereferencing yields an empty list).

Within a single canonical sublist, spikes are sorted by time ascending.
`spike_unit` / `spike_depth` are parallel to `spike_times` (same length,
same order). `spike_depth[k]` is the depth of the unit that fired spike
`k`; it is redundant with `unit_list_depth` but handy for direct
scatter-plotting.

## Per-unit metadata columns (squared)

One entry per unit that **actually fires in this block**, sorted by
`unit_id` ascending.

| column | DG type | inner type | units | source |
|---|---|---|---|---|
| `unit_list_id` | list(list(int)) | int | — | `units.unit_id` |
| `unit_list_depth` | list(list(float)) | float | μm | `probe_channels.y_um` via `units.max_on_channel_id` |
| `unit_list_x` | list(list(float)) | float | μm | `units.x_um` |
| `unit_list_amp` | list(list(float)) | float | — | `units.amplitude` |
| `unit_list_snr` | list(list(float)) | float | — | `unit_metrics` where `metric_name='snr'` |
| `unit_list_fr` | list(list(float)) | float | Hz | `unit_metrics` where `metric_name='firing_rate'` |
| `unit_list_presence` | list(list(float)) | float | 0..1 | `unit_metrics` where `metric_name='presence_ratio'` |

### Rectangular rule

These are list-of-list columns whose outer length equals `ntrials`, but
**data only lives on row 0**. Rows `1..ntrials-1` are empty sublists. This
keeps every column trial-aligned (so `countRows(dg)` stays honest) while
allowing one-value-per-unit storage.

```js
const unitIds    = getSublistData(dg, 'unit_list_id',    0);
const unitDepths = getSublistData(dg, 'unit_list_depth', 0);
// unitIds[u] — unit_id of the u-th unit; unitDepths[u] — its depth
```

Within row 0 all `unit_list_*` columns are parallel (same length, same
order). To look up metadata for a spike: find `u = indexOf(unitIds,
spike_unit[k])` once per distinct unit, cache it, then index into the
other `unit_list_*` columns by `u`.

## Missing-value sentinel

| column | value | meaning |
|---|---|---|
| `spike_missing_value[0]` | float, currently `-1.0` | "no data" for any `unit_list_*` column |

All seven `unit_list_*` metrics are non-negative by construction
(depths, x, amps, SNR, firing rate, presence ratio), so a single
negative sentinel is unambiguous. **Readers should treat `< 0` as
"missing" in these columns.**

If a future schema revision adds a signed metric (e.g.
`amplitude_median`, `silhouette`, `noise_cutoff`), this approach no
longer works — either use real IEEE NaN or emit a parallel
presence-mask column, and bump `spike_schema_version`.

## Self-describing columns

Flat per-trial columns; every row holds the same value. Readers should
read row 0.

| column | DG type | currently |
|---|---|---|
| `spike_schema_version` | int | `1` |
| `spike_time_units` | string | `"ms_from_obs_start"` |
| `spike_missing_value` | float | `-1.0` |

## Example: rebuilding a raster-ready structure in JS

```js
const src    = getScalar(dg, 'spike_src_trial', i);
const times  = getSublistData(dg, 'spike_times',  src);
const units  = getSublistData(dg, 'spike_unit',   src);

const uIds   = getSublistData(dg, 'unit_list_id',    0);
const uDepth = getSublistData(dg, 'unit_list_depth', 0);
const uSnr   = getSublistData(dg, 'unit_list_snr',   0);
const miss   = getScalar(dg, 'spike_missing_value', 0);

// Map unit_id -> index in unit_list_* row 0 (do this once per file)
const uIdx = new Map();
for (let k = 0; k < uIds.length; k++) uIdx.set(uIds[k], k);

// For each spike in this trial:
for (let k = 0; k < times.length; k++) {
    const t = times[k];
    const u = uIdx.get(units[k]);
    const depth = uDepth[u];
    const snr   = uSnr[u];
    const snrOk = snr > miss;   // i.e. not the sentinel
    // plot (t, depth) colored by snr if snrOk ...
}
```

## What is *not* in the file

Deliberately omitted for this pass (may be added in a later schema
version):

- Waveforms (`unit_waveforms.mean_waveform` BLOBs)
- The full `unit_metrics` table (30+ metrics)
- TTL events / block alignment metadata
- LFP data
- Phy curation group (currently `"unsorted"` for all units — will become
  meaningful once curation lands; could be added as `unit_list_group`)

## Producing these files

```tcl
package require neuropixel
::neuropixel::append_spikes_file \
    /path/to/package.sqlite \
    /path/to/behavior.trials.dgz \
    /path/to/out.dgz
```

Options: `-with_unit_meta 0|1`, `-with_schema 0|1`, `-block_id <int>`,
`-require_block 0|1`, `-obsid_col <name>`. See `neuropixel.tcl` for
composable primitives (`open_package`, `resolve_block_id`,
`spikes_by_obs`, `unit_metadata`, `add_spikes`, `add_unit_metadata`,
`add_schema_columns`).
