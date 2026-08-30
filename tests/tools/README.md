# Diagnostic harnesses

Not part of `ctest`. These are for answering "did this change behavior?" and
"does this leak?" when working on dynlist lifetimes, and they exist because
the refcounted-handle work needed them. Each has already found real bugs.

## dl_command_diff.tcl -- behavioral differential

Exercises every `dl_*`/`dm_*`/`dlp_*` command and proc against a set of canned
argument patterns and writes a normalized transcript. Run it against two
builds of `libdlsh` and diff the transcripts: any behavioral change shows up
as a diff line, and an empty diff is strong evidence a change is invisible.

    # build A
    dlsh tests/tools/dl_command_diff.tcl /tmp/a.txt
    # ...swap in the other libdlsh...
    dlsh tests/tools/dl_command_diff.tcl /tmp/b.txt
    diff /tmp/a.txt /tmp/b.txt

Roughly 400 commands x 11 patterns ~ 4100 invocations. Most of them error
(wrong arity for that pattern), which is fine -- an error message is just as
comparable as a result, and changes to them matter too.

It found two latent bugs on master: `dl_getPercentile` and `dlp_setvar` both
do `return [dl_first $x]`, which on a list-of-lists returns a sublist temp
that dies with the proc frame, so they handed back a dangling name.

### Gotcha: do not build fixtures inside a proc

The fixture builder returns *the script that makes the list*, not the list,
and the caller evaluates it in the global frame. That is deliberate. Writing
the obvious thing instead --

    proc fixture {} { return [dl_ilist 1 2 3] }    ;# WRONG

-- means every fixture is dead on arrival on any build without refcounted
handles, and the whole transcript reads `dynlist ... not found`. The first
version of this harness did exactly that and produced a 4000-line diff that
was entirely the harness's fault.

## dl_soak.tcl -- leak soak

    dlsh tests/tools/dl_soak.tcl <mode> ?blocks? ?threads? ?iters?

Growth is reported **per block**, never as a total: the first block always
includes allocator warm-up, so an absolute threshold reads that as a leak.
What matters is whether later blocks flatten.

| mode | what it does |
|---|---|
| `single` | single-threaded churn over every lifetime path |
| `pooled` | long-lived worker interpreters, reused across blocks -- **the pattern dserv actually runs** |
| `fresh` | a new interpreter per block, created and destroyed |
| `bare` | threads that do nothing; control for thread create/join cost |
| `load` | threads that load dlsh but do no list work; control for per-interp package state |

Use `pooled` unless you have a reason not to. `fresh`, `bare` and `load`
exist to attribute growth when `pooled` is not flat.

### Gotcha: work belongs inside a proc

By default the work body runs in a proc, because that is how real dlsh code
is written and because temps are reclaimed at frame exit. Pass `-global` to
run it at the top level instead, which accumulates lists without bound --
that is by design, not a leak, and is what `dl_clean` is for. A soak that
churns at global scope will show tens of thousands of retained lists and tell
you nothing.

### Gotcha: `tpool` hangs in the standalone `dlsh` interpreter

`tpool::create` succeeds and `tpool::wait` never returns. This is not a dlsh
problem: a trivial worker with no dlsh loaded hangs identically, and so does
an older `libdlsh`. `thread::create` works fine, which is what these
harnesses use. dserv is unaffected -- it links its own Tcl.
