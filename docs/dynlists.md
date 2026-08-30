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

There is no single "copy" command; any operation that builds a new list gives
you an independent one. The general form works for every element type:

```tcl
set b [dl_choose $a [dl_fromto 0 [dl_length $a]]]   ;# independent copy
```

`dl_set name $a` also copies, but into a *named* list (see
[Named lists](#named-lists)) rather than a handle in a variable.

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
| an independent copy | `dl_choose $x [dl_fromto 0 [dl_length $x]]` |
| its length | `dl_length $x` |
| Tcl-side data | `dl_tcllist $x` |
| free it now | `dl_delete $x` |
| clear top-level temporaries | `dl_clean` |

| Avoid | Because |
|---|---|
| `return "$a $b"` | string interpolation drops the handles |
| `llength $x` on a handle | not a Tcl list; can strand the list |
| assuming `set b $a` copies | it aliases; build a new list to copy |
| assuming `unset x` frees | it drops one claim, not both |

---

## See also

- `dservctl docs show <command>` — per-command reference
- `tests/tools/README.md` — differential and soak harnesses for lifetime work
- `src/dlref.c` — the implementation, including why the handle lives inside
  the `DYN_LIST` rather than in a side table
