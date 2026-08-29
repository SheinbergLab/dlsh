# dlsh

## Packages for [tcl](https://tcl.tk)

This repo includes custom packages made available to host programs including [dserv](https://github.com/SheinbergLab/dserv) and [stim2](https://github.com/SheinbergLab/stim2), but which can be loaded in any tcl interpreter.

Releases for macOS and Linux (arm64 and x86_64) are available on the [Releases page](https://github.com/SheinbergLab/dlsh/releases).

## Returning a dynlist from a proc: use `dl_yield`

Dynlists are not Tcl values — they live in a name table and are freed by a
trace on the frame that owns them. To hand one out of a proc, use `dl_yield`:

```tcl
proc scaled {xs k} {
    dl_yield [dl_mult $xs $k]
}
```

`dl_yield` returns from the enclosing proc *and* transfers the list to the
caller's frame, so it is the one thing you need. It works anywhere `return`
does — `oo::` methods, `apply` bodies, out of a loop, non-tail position — and
composes to any depth:

```tcl
proc a {} { dl_yield [dl_fromto 0 1000] }
proc b {} { dl_yield [a] }        ;# O(1): renamed into b's caller, not copied
```

### Why not `dl_return`

`dl_return` predates `dl_yield` and, despite the name, **does not return** — it
only sets the interp result and installs a delete trace one frame up. That
leaves three traps, all of which `dl_yield` closes:

| | `dl_return` | `dl_yield` |
|---|---|---|
| `… $x` followed by a bare `return` | silently returns `""` | later code is unreachable; value survives |
| handing a list up N frames | must re-wrap, deep copying at each one | renamed into the caller, O(1) |
| called on a caller's *named* list | renames it — the original is destroyed | copies it — the original is intact |

Measured marginal cost of one hand-off frame (Apple M-series, release build,
depth 8 vs depth 1, difference over 7 frames):

| elements | `return [dl_return …]` | `dl_yield` |
|---|---|---|
| 1,000 | 4.6 µs | 4.8 µs |
| 16,000 | 9.9 µs | 5.0 µs |
| 100,000 | 19.5 µs | 5.0 µs |
| 1,000,000 | 162 µs | 5.4 µs |
| 10,000,000 | 1,587 µs | 11.4 µs |

`dl_yield` is flat because it never touches the data; the drift at 10M is
allocator noise from cycling 80 MB lists, not copying.

`dl_return` remains for backward compatibility and its behavior is unchanged;
new code should use `dl_yield`. Receiving side is unchanged too: bind with
`dl_local` when you need to keep the result past the current statement.

## Standalone `dlsh` interpreter (dev/testing)

In addition to the `libdlsh` library, the repo can build a standalone `dlsh`
interpreter: a `tclsh9` with the dlsh packages linked in and the `dlsh.zip` VFS
mounted automatically, so `dl_*`/`dg_*`/`df_*` and the rest of the VFS packages
are available with no `dlsh_setup.tcl` step. Useful as a REPL and for scripted /
CI / LLM verification of dlsh + Tcl code.

It is **off by default** — the option is opt-in so normal/CI library builds are
untouched:

```sh
cmake -B build -D DLSH_BUILD_INTERP=ON      # uses full Tcl from /usr/local;
                                            # override with -D DLSH_TCL_ROOT=<prefix>
cmake --build build --target dlsh_interp    # -> build/dlsh
```

Usage:

```sh
build/dlsh                       # interactive REPL (arrow-key history, multi-line)
build/dlsh script.tcl            # run a script
build/dlsh -e 'expr 6*7'         # one-shot eval; prints result, exits 0/1
echo 'puts [dl_sum [dl_fromto 0 10]]' | build/dlsh   # piped stdin
```

The interactive REPL uses [linenoise](https://github.com/antirez/linenoise)
(vendored, BSD; POSIX only — Windows falls back to the stock `Tcl_Main` loop).

### Tests

The self-contained dlsh-layer tests run through the freshly built binary:

```sh
ctest --test-dir build           # runs tests/test_dl_*.tcl via build/dlsh
```
