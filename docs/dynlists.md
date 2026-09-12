# Dynlists: values, handles, and lifetimes

How dynamic lists behave in Tcl — what a `dl_*` command actually hands you,
how long it lives, and the few places that still surprise people.

If you are writing new code, the short version is: **use `set` and `return`
like ordinary Tcl.** The rest of this explains why that works and where the
edges are.

---

## What a dl_* command returns

A dynlist lives in C, not in Tcl. A `dl_*` command hands back a *handle* to
it, which prints as a name:

```tcl
% dl_fromto 0 5
%list12%
```

You pass that handle to other `dl_*` commands and they operate on the list
behind it. Two things follow, and most confusion comes from missing one:

**Dynlists are objects, not values.** Two variables holding the same handle
name the *same list*, and mutation shows through both:

```tcl
set a [dl_ilist 1 2 3]
set b $a
dl_append $b 4
dl_tcllist $a          ;# => 1 2 3 4   -- $a changed too
```

Use `dl_copy` when you want an independent snapshot:

```tcl
set b [dl_copy $a]
dl_append $b 4
dl_tcllist $a          ;# => 1 2 3   -- unchanged
```

It is a deep copy: the sublists of a list-of-lists are copied too, so the
result shares no storage with the original. `dl_set name $a` also copies, but
into a *named* list (see [Named lists](#named-lists)) rather than a handle in
a variable.

A dynlist is closer to a numpy array or a Tk widget name than to a Tcl list.

**The handle is not a Tcl list.** `llength $dl` returns 1 — the name is one
word. Use `dl_length $dl`, or convert first with `dl_tcllist $dl` when you
genuinely want Tcl-side data. See [Things that surprise
people](#things-that-surprise-people).

---

## Lifetimes

A list stays alive while **either** of two claims exists, and is freed when
both are gone.

**1. The frame claim.** Whichever frame created the list holds one. It is
released when that frame exits — or, at the top level, never, which is what
`dl_clean` is for.

**2. Object references.** Every Tcl value holding the handle counts: a
variable you `set`, an element of a list, a value in a dict, a returned
result.

That is the whole model. Everything below is a consequence.

One reference Tcl holds on your behalf is worth knowing about: when a
command fails, Tcl records that command and its argument objects in the
`-errorstack` return option (`info errorstack`), and keeps them until the
next error replaces them. A dynlist passed to a command that errored
therefore outlives its frame until another error occurs, or until
`dl_clean`. It is bounded to one command's arguments and is only visible
as an off-by-one in a `dl_dir` census taken right after a caught error.

The important property is that it is **additive**: a reference can only
*extend* a lifetime, never shorten one. Nothing that worked before behaves
differently now.

### Returning lists from procs

A proc can return a dynlist the ordinary way:

```tcl
proc column { n } {
    return [dl_fromto 0 $n]
}

set col [column 100]         ;# stays alive: your variable holds a reference
```

The frame claim dies when `column` returns; the reference in `col` keeps the
list. Escaping into a global, a dict, or an accumulator works for the same
reason:

```tcl
foreach n {10 20 30} { lappend cols [column $n] }
```

### Returning more than one

What matters is whether the returned value still *holds the handles*:

```tcl
return [list $a $b]         ;# works -- a Tcl list holds both objects
dict set d a $a; return $d  ;# works -- so does a dict
return [dl_llist $a $b]     ;# works -- and gives you a real dynlist column
return "$a $b"              ;# BROKEN -- see below
```

`"$a $b"` builds a *string*. String interpolation keeps only the printed
names and throws the handles away, and a bare name is not a claim on
anything. This is the one form that does not work, and it is worth
remembering as a rule: **containers that hold Tcl objects preserve
references; building a string does not.**

---

## dl_local and dl_yield

Older code binds and returns lists with these:

```tcl
proc column { n } {
    dl_local out [dl_fromto 0 $n]
    dl_yield $out
}
dl_local col [column 100]
```

They exist because a plain `return` used to hand back a name whose list had
already been freed. `dl_local` re-parents the frame claim onto *your*
variable; `dl_yield` hands it up one frame.

**They still work and are still correct.** There is a great deal of code
using them and none of it needs changing. New code does not need them —
`set` and `return` do the same job with no dlsh-specific vocabulary — but
mixing the two styles in one script is fine:

```tcl
dl_local x [column 100]     ;# fine
set y     [column 100]      ;# also fine, independent list
```

One difference worth knowing: `dl_yield` takes exactly one list, which is why
older code bundles multiple results into a `dl_llist`. A plain `return` has
no such limit — see [Returning more than one](#returning-more-than-one).

---

## Named lists

`dl_set` binds a list to a *name* rather than to a Tcl variable:

```tcl
dl_set mycol [dl_fromto 0 10]
dl_length mycol            ;# note: no $ -- the name IS the handle
```

Named lists have no frame claim, so they are not reclaimed when a proc
returns; they live until `dl_delete`. That is what you want for something
long-lived like a dyngroup column, and not what you want for a scratch value
— use `set` for those.

## Generated names are not identities

`%listN%`, `>N<` and `groupN` are generated from counters. They are unique
within an interpreter and mean nothing outside it, so do not store one as
text and do not send one anywhere expecting it to still refer to the same
thing. Keep the handle a command gives you.

Two consequences worth knowing.

**A generated name is never handed out twice.** The counters do not restart —
not on `dl_clean`, not on `dg_clean`. A name whose list has been freed simply
stops resolving:

```tcl
set n %list0%               ;# don't do this, but if you did
dl_clean
dl_length $n                ;# error, not somebody else's list
```

Before this, the counter restarted and `%list0%` came round again, so that
last line quietly returned the length of an unrelated list.

**A group arrives under a local name.** `dg_toString` records whatever the
group was called where it was made. `dg_fromString` treats the two kinds of
name differently:

```tcl
set g [dg_fromString $blob]      ;# a groupN blob -> renamed locally; keep $g
dg_fromString $blob              ;# a "stimdg" blob -> still stimdg
dg_fromString $blob mycopy       ;# an explicit name always wins
```

A `groupN` came from the sending interpreter's counter and identifies nothing
here — two unrelated groups can easily both be `group7` — so it is renamed on
arrival and you use the returned name. A name a person chose is an identity:
code refers to `stimdg` by name, and restoring over it to replace the
contents is the point, so it survives the trip. That also means restoring a
named group repeatedly replaces it in place rather than accumulating copies.

## Things that surprise people

**`unset` does not free the list.**

```tcl
set x [dl_fromto 0 10]
unset x                     ;# drops YOUR reference
```

The frame claim outlives it, so the list is still there until the proc
returns (or until `dl_clean` at the top level). `dl_delete $x` frees
immediately and overrides any outstanding reference — after it, other
handles to that list report `dynlist ... not found` rather than reading
freed memory.

**Top-level temps accumulate.** Inside a proc, temporaries are reclaimed
when it returns. At the global scope there is no frame exit, so they live
until `dl_clean`. Long-running loops belong inside a proc — which is what
analysis and loader code does anyway.

**Do not apply Tcl list or string commands to a handle.** `llength $dl`,
`lindex $dl 0`, `string length $dl` all treat the handle as text. Besides
being meaningless, this is the one operation that can still strand a list:
it converts the handle to another Tcl type, dropping the reference. Use the
`dl_*` command, or convert explicitly:

```tcl
dl_length $dl                ;# yes
llength [dl_tcllist $dl]     ;# yes -- llength sees a real Tcl list
llength $dl                  ;# no  -- returns 1, and can strand the list
```

`dl_tcllist` is safe because it returns an ordinary Tcl list; the commands
you run on *that* never touch the handle.

---

## Quick reference

| You want | Write |
|---|---|
| keep a list in a variable | `set x [dl_...]` |
| return a list from a proc | `return $x` |
| return several | `return [list $a $b]` or a dict or `dl_llist` |
| an independent copy | `dl_copy $x` |
| its length | `dl_length $x` |
| Tcl-side data | `dl_tcllist $x` |
| free it now | `dl_delete $x` |
| clear top-level temporaries | `dl_clean` |

| Avoid | Because |
|---|---|
| `return "$a $b"` | string interpolation drops the handles |
| `llength $x` on a handle | not a Tcl list; can strand the list |
| assuming `set b $a` copies | it aliases; use `dl_copy` |
| assuming `unset x` frees | it drops one claim, not both |

---

## Element types, and the two wide ones

A dynlist holds one element type: `char`, `short`, `long` (32-bit; `int`
is a synonym), `float` (32-bit), `string`, or `list`. Since 2026 there are
two 8-byte types as well:

| Type | Create | Convert to | Notes |
|---|---|---|---|
| `int64` (`wide`) | `dl_wlist 5000000000` or `dl_create int64 ...` | `dl_int64 $x` | exact 64-bit integers; a microsecond epoch timestamp fits |
| `double` | `dl_dlist 0.1` or `dl_create double ...` | `dl_double $x` | full 64-bit precision, where `float` keeps ~7 digits |

`long` is still 32 bits and `float` still 32; nothing about existing lists
changed.

**What works on wide lists:** the storage layer (create, append, get/put,
`dl_tcllist`, `dl_foreach`, copy, select, permute, concat, conversions,
`dg_write`/`dg_read`, JSON, msgpack, Arrow), and the analysis core:
arithmetic (`dl_add` .. `dl_fmod`), comparisons and their `Index` forms,
`dl_not`, `dl_where`, the elementwise math functions, `dl_negate`,
`dl_diff`, `dl_gradient`, `dl_cumsum`, sorting and `dl_sortIndices`,
`dl_unique`, `dl_rank`, `dl_recode`, `dl_find`, and the reductions
(`dl_sum`, `dl_prod`, `dl_mean`, `dl_std`, `dl_var`, `dl_min`, `dl_max`,
`dl_any`, `dl_all`, their `s` and `Index`/`Positions` forms).

**Promotion.** A result is as wide as its widest operand: char < short <
long < int64 < float < double, except that int64 combined with float gives
double, because a float cannot hold an int64. int64 arithmetic wraps on
overflow exactly as long does; `dl_sum` and `dl_prod` of an int64 list
fall back to double rather than wrap. Comparisons between integer types are
exact in 64 bits.

**Literals follow the operand.** In a command that has a wide list among
its arguments, a literal such as `0.1` or `5000000001` is parsed as a
double or an int64, so `dl_eq $doubles 0.1` is true where a float32 `0.1`
never could be. Without a wide list present, literals stay float and long
exactly as before.

Also covered: histograms and counting (`dl_hist`, `dl_count`, ...), the
find family (`dl_findIndices` hashes int64 keys exactly), positions and
shape (`dl_subshift`, `dl_cut`, `dl_pack`, `dl_reshape`, splice, ...),
`dl_replace`, the category sorts, and the `dl_b*`/`dl_h*` reducers, which
answer in double for wide leaves. The plotting commands (`dlg_lines`,
`dlg_markers`, ...) draw a wide list as a float copy: a double time axis
plots exactly as its float conversion would, and the list itself is not
touched.

**What still errors** rather than answering: the commands that take no
wide list by design (string and path ops, generators, `dl_srand`), the
float32 spike-density kernels (`dl_sdf`, `dl_parzen`), and `dl_fill`
refuse a wide list with

    dl_fill: int64/double lists are not supported by this command yet
    (convert with dl_int or dl_float)

That refusal is deliberate: those commands dispatch on element type with no
default arm, and before the guard a wide list came back unsorted from
`dl_sort` and with `0.0` from `dl_mean`. `dlWideOkCommands` in
`src/tcl_dl.c` is the list of verified commands; the wide-type kernels they
share live in `src/dlwide.c`.

**Files that contain a wide column can only be opened by readers that know
tags 11 and 12**: dlsh from this version on, dgread 1.2.1+ for Python (int64
and float64 arrays), R 1.1.1+ (both arrive as doubles; int64 is exact to
2^53), the matching MATLAB MEX (both as doubles), and dserv's updated web
viewers. Older readers report the file as corrupt or a newer format. For
that reason `dslog::read` and `dslog::readESS` still narrow doubles to float
and drop int64 values by default; turn on `dslog::wideTypes 1` once every
consumer of a rig's files has been updated. With it on, the per-record time
columns `<dst>NAME` and `<blobt>NAME` also become `double`: still
milliseconds from the same anchors, but with the microsecond fraction kept
instead of truncated.

---

## See also

- `dservctl docs show <command>` — per-command reference
- `tests/tools/README.md` — differential and soak harnesses for lifetime work
- `src/dlref.c` — the implementation, including why the handle lives inside
  the `DYN_LIST` rather than in a side table
