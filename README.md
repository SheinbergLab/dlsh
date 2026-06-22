# dlsh

## Packages for [tcl](https://tcl.tk)

This repo includes custom packages made available to host programs including [dserv](https://github.com/SheinbergLab/dserv) and [stim2](https://github.com/SheinbergLab/stim2), but which can be loaded in any tcl interpreter.

Releases for macOS and Linux (arm64 and x86_64) are available on the [Releases page](https://github.com/SheinbergLab/dlsh/releases).

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
