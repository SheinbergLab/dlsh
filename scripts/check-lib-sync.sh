#!/usr/bin/env bash
# check-lib-sync.sh -- lib/ and vfs/lib/ are hand-kept duplicates; verify they
# still agree.
#
# The Tcl library exists twice: lib/ is the working tree, vfs/lib/ is what the
# build packages into dlsh.zip and therefore what actually runs. Nothing
# copies one to the other, so editing only one leaves the shipped copy stale
# -- and because `use <proc>` reads the SHIPPED tree, a doc or code fix can
# appear to have no effect at all.
#
# That has bitten more than once: robust.tcl and compose.tcl had drifted, with
# vfs/ carrying dl_yield and `package require impro` while lib/ still had
# dl_return and load_Impro.
#
# Pairs checked:
#   lib/<f>.tcl          <->  vfs/lib/dlsh/<f>.tcl
#   lib/local/<pkg>/     <->  vfs/lib/<pkg>/        (packages present in both)
#
# Editor and OS droppings are ignored, as are the platform library dirs which
# exist only under vfs/.
#
# Usage: scripts/check-lib-sync.sh [--fix-from-vfs]
#   --fix-from-vfs   copy vfs/ over lib/ for differing files (vfs/ is what
#                    ships, so it is normally the one that has been maintained)

set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

FIX=0
[ "${1:-}" = "--fix-from-vfs" ] && FIX=1

IGNORE=(-x '.DS_Store' -x '*~' -x '#*#' -x '.gitkeep' -x 'Darwin' -x 'Linux'
        -x 'tclIndex' -x 'local')

problems=0
report() { echo "  $*"; problems=$((problems + 1)); }

# --- top-level Tcl files -------------------------------------------------
for f in lib/*.tcl lib/dlshrc; do
    [ -f "$f" ] || continue
    v="vfs/lib/dlsh/$(basename "$f")"
    if [ ! -f "$v" ]; then
        report "only in lib/: $f  (not shipped)"
        continue
    fi
    if ! diff -q "$f" "$v" >/dev/null 2>&1; then
        if [ "$FIX" = 1 ]; then
            cp "$v" "$f"; echo "  fixed from vfs: $f"
        else
            report "differs: $f  vs  $v"
        fi
    fi
done

# --- shipped local packages ----------------------------------------------
for d in lib/local/*/; do
    pkg="$(basename "$d")"
    v="vfs/lib/$pkg"
    [ -d "$v" ] || continue          # not shipped; dev-only package
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        if [ "$FIX" = 1 ] && [[ "$line" == Files*differ ]]; then
            a=$(echo "$line" | awk '{print $2}')
            b=$(echo "$line" | awk '{print $4}')
            cp "$b" "$a"; echo "  fixed from vfs: $a"
        else
            report "$line"
        fi
    done < <(diff -rq "${IGNORE[@]}" "$d" "$v" 2>/dev/null)
done

if [ "$problems" -gt 0 ]; then
    echo ""
    echo "lib/ and vfs/lib/ are out of sync ($problems difference(s))."
    echo "vfs/lib/ is what ships and what 'use' reads, so it is usually the"
    echo "one to keep: scripts/check-lib-sync.sh --fix-from-vfs"
    exit 1
fi

echo "lib/ and vfs/lib/ are in sync"
exit 0
