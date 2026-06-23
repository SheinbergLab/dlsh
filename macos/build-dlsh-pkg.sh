#!/usr/bin/env bash
#
# build-dlsh-pkg.sh -- build a macOS installer .pkg for the standalone dlsh
# interpreter, mirroring the by-hand install layout:
#
#     /usr/local/bin/dlsh         the interpreter (single-file / append build)
#     /usr/local/dlsh/dlsh.zip    the dlsh PACKAGE (libdlsh + Tcl helpers)
#
# This is the "append" (single-file) form: the Tcl/Tk runtime is inside the
# binary, so only two files install. That binary is NOT codesignable (trailing
# data past __LINKEDIT); for a notarizable installer build dlsh in sidecar mode
# (-D DLSH_APPEND_ZIP=OFF) and also stage dlsh-runtime.zip, then sign + notarize.
#
# Usage:
#   macos/build-dlsh-pkg.sh <dlsh-binary> <dlsh.zip> [version] [pkg-sign-identity]
#
# Example:
#   cmake -B build -D DLSH_BUILD_INTERP=ON && cmake --build build --target dlsh_interp
#   macos/build-dlsh-pkg.sh build/dlsh /usr/local/dlsh/dlsh.zip 0.1.0
set -euo pipefail

DLSH_BIN=${1:?usage: build-dlsh-pkg.sh <dlsh-binary> <dlsh.zip> [version] [sign-id]}
DLSH_ZIP=${2:?missing dlsh.zip path}
VERSION=${3:-0.0.0}
SIGN_ID=${4:-}
IDENTIFIER=org.sheinberglab.dlsh

[ -f "$DLSH_BIN" ] || { echo "no such binary: $DLSH_BIN" >&2; exit 1; }
[ -f "$DLSH_ZIP" ] || { echo "no such zip: $DLSH_ZIP" >&2; exit 1; }

STAGE=$(mktemp -d)
ROOT="$STAGE/root"
mkdir -p "$ROOT/usr/local/bin" "$ROOT/usr/local/dlsh"
# COPYFILE_DISABLE: don't let macOS write ._ AppleDouble xattr forks into the payload.
COPYFILE_DISABLE=1 install -m 0755 "$DLSH_BIN" "$ROOT/usr/local/bin/dlsh"
COPYFILE_DISABLE=1 install -m 0644 "$DLSH_ZIP" "$ROOT/usr/local/dlsh/dlsh.zip"
xattr -cr "$ROOT" 2>/dev/null || true

OUT="dlsh-${VERSION}-$(uname -m).pkg"
pkgbuild --root "$ROOT" \
         --identifier "$IDENTIFIER" \
         --version "$VERSION" \
         --install-location / \
         "$STAGE/component.pkg"

# Wrap in a distribution (product) pkg so it presents a normal installer UI.
productbuild --package "$STAGE/component.pkg" "$OUT"

if [ -n "$SIGN_ID" ]; then
    productsign --sign "$SIGN_ID" "$OUT" "${OUT%.pkg}-signed.pkg"
    echo "signed: ${OUT%.pkg}-signed.pkg"
fi

rm -rf "$STAGE"
echo "built: $OUT"
echo "installs: /usr/local/bin/dlsh, /usr/local/dlsh/dlsh.zip"
