# dlsh

Tcl extension providing **dynlists** (`dl_*`) and **dyngroups** (`dg_*`): typed,
array-oriented lists implemented in C. Consumed by dserv, stim2 and the
standalone `dlsh` interpreter, all of which load `libdlsh` from `dlsh.zip` at
runtime rather than linking it.

## Writing Tcl against this library

Read `docs/dynlists.md` before writing non-trivial dynlist code. The rules
that matter most, so they are not missed:

- **Use plain `set` and `return`.** A proc can `return` a dynlist directly and
  a variable keeps it alive. `dl_local` and `dl_yield` are the older idiom;
  they still work and existing code needs no migration, but new code does not
  need them.
- **Dynlists are objects, not values.** `set b $a` aliases the same list and
  mutation shows through both. `dl_copy` gives an independent deep copy.
- **A handle is not a Tcl list.** `llength $dl` returns 1 and can strand the
  list. Use `dl_length $dl`, or `llength [dl_tcllist $dl]`.
- **Returning several lists:** `return [list $a $b]` or a dict both work,
  because those containers hold the handles. `return "$a $b"` does NOT —
  string interpolation keeps only the names.
- **Lifetime rule:** a list is freed when both its frame claim and every
  object reference are gone. So `unset x` alone does not free it; `dl_delete`
  does, immediately.

Prefer whole-column `dl_*` math over Tcl row loops — the library is
array-oriented, and the primitive you want usually already exists
(`info commands dl_*`).

## Build and test

```
cmake --build build --target dlsh -j8      # the library
ctest --test-dir build -L dlsh             # the suite
```

Tests load `libdlsh` from a zip, so testing a fresh build means staging one:
copy the built `libdlsh.{dylib,so}` into a `dlsh.zip` tree and point
`DLSH_ZIP` at it. On macOS the library must carry the same Developer ID as
the hardened `dlsh` binary or it will be refused by library validation.

**A test ending in `exit 0` skips interpreter teardown.** Exercise that path
with `dlsh -e '<script>'` and check the exit code — a whole class of bug is
invisible otherwise.

`tests/tools/` has a command-surface differential and leak soaks, plus a
README recording the traps in using them.

## Layout

- `src/tcl_dl.c` — the `dl_*` command table and implementations
- `src/dlref.c` — refcounted handles; the lifetime model lives here
- `src/lablib/` — the C list/group library (`dfu*`), no Tcl dependency
- `lib/`, `vfs/lib/` — Tcl-level library shipped inside `dlsh.zip`
- `docs/dynlists.md` — dynlist semantics
