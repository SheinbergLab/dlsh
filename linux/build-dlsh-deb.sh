#!/usr/bin/env bash
#
# build-dlsh-deb.sh -- build a Debian .deb installing the standalone dlsh
# interpreter, mirroring the macOS .pkg layout:
#
#     /usr/local/bin/dlsh         the interpreter
#     /usr/local/dlsh/dlsh.zip    the dlsh PACKAGE (libdlsh + Tcl helpers)
#
# Linux needs no code signing, so the simple "append" (single-file) build is
# used: the Tcl/Tk runtime is baked into the binary, so only two files install.
# /usr/local matches the project's existing packaging convention
# (CPACK_PACKAGING_INSTALL_PREFIX) and where dlsh.zip is discovered.
#
# Usage:
#   cmake -B build -D DLSH_BUILD_INTERP=ON && cmake --build build --target dlsh_interp
#   linux/build-dlsh-deb.sh build/dlsh <dlsh.zip> <version>
set -euo pipefail

DLSH_BIN=${1:?usage: build-dlsh-deb.sh <dlsh-binary> <dlsh.zip> <version>}
DLSH_ZIP=${2:?missing dlsh.zip path}
VERSION=${3:?missing version}
PKG=dlsh-interp
ARCH=$(dpkg --print-architecture)   # amd64 / arm64

[ -f "$DLSH_BIN" ] || { echo "no such binary: $DLSH_BIN" >&2; exit 1; }
[ -f "$DLSH_ZIP" ] || { echo "no such zip: $DLSH_ZIP" >&2; exit 1; }

ROOT=$(mktemp -d)/pkg
mkdir -p "$ROOT/DEBIAN" "$ROOT/usr/local/bin" "$ROOT/usr/local/dlsh"
install -m 0755 "$DLSH_BIN" "$ROOT/usr/local/bin/dlsh"
install -m 0644 "$DLSH_ZIP" "$ROOT/usr/local/dlsh/dlsh.zip"

# Depends: what the (append-mode) binary links dynamically -- Tk's X11 stack +
# zlib -- plus libpq5 for the libdlsh that loads from the zip at runtime.
cat > "$ROOT/DEBIAN/control" <<CTRL
Package: $PKG
Version: $VERSION
Architecture: $ARCH
Maintainer: SheinbergLab
Section: science
Priority: optional
Depends: libc6, libx11-6, libxft2, libxss1, libxext6, libxrender1, libfontconfig1, zlib1g, libpq5
Description: Standalone dlsh interpreter (dynamic lists + Tcl/Tk)
 A self-contained Tcl/Tk interpreter with the dlsh dynamic-list, data-group,
 and cgraph commands. Installs the interpreter plus the dlsh package
 (/usr/local/dlsh/dlsh.zip). Run \`dlsh\`, \`dlsh -e <script>\`, or \`dlsh --gui\`.
CTRL

OUT="${PKG}_${VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$ROOT" "$OUT"
rm -rf "$(dirname "$ROOT")"
echo "built: $OUT"
echo "installs: /usr/local/bin/dlsh, /usr/local/dlsh/dlsh.zip"
