# Updating Pre-built Packages in dlsh

This guide covers how to update Tcl packages with pre-built binaries (like tdom, sqlite, thread, yajltcl, etc.) in the `vfs/lib/` directory.

## Package Structure

Pre-built packages follow this structure:

```
vfs/lib/<package>/
├── Darwin/
│   └── arm64/
│       └── libtcl9<package><version>.dylib
├── Linux/
│   ├── aarch64/
│       └── libtcl9<package><version>.so
│   └── x86_64/
│       └── libtcl9<package><version>.so
├── Windows NT/
│   └── <if supported>
└── pkgIndex.tcl
```

## Build Environments

| Platform | Build Environment |
|----------|-------------------|
| macOS arm64 | Native Mac (M1/M2/M3) |
| Linux aarch64 | Native ARM64 VM or Raspberry Pi |
| Linux x86_64 | Cross-compile from ARM64 VM, or native x86_64 machine |

## Building for Each Platform

### Prerequisites

You need a built Tcl installation for each target. The `tclConfig.sh` file is required.

### macOS arm64 (native build)

```bash
cd ~/src/<package>/unix   # or macosx if present
make distclean 2>/dev/null || true
./configure --with-tcl=/path/to/tcl/lib --prefix=/tmp/<package>-darwin
make -j$(sysctl -n hw.ncpu)
```

### Linux aarch64 (native build)

```bash
cd ~/src/<package>/unix
make distclean 2>/dev/null || true
./configure --with-tcl=/path/to/tcl/lib --prefix=/tmp/<package>-aarch64
make -j$(nproc)
```

### Linux x86_64 (cross-compile from ARM64)

#### Step 1: Install cross-compiler

```bash
sudo apt install -y gcc-x86-64-linux-gnu g++-x86-64-linux-gnu
```

#### Step 2: Build Tcl for x86_64 (needed for tclConfig.sh)

```bash
cd ~/src/tcl<version>/unix
make distclean 2>/dev/null || true

# Symlink native tclsh BEFORE make (critical for thread package, etc.)
ln -sf /usr/local/bin/tclsh9.0 tclsh

CC=x86_64-linux-gnu-gcc \
./configure --host=x86_64-linux-gnu --prefix=/tmp/tcl-x86_64

make -j$(nproc)
make install
```

#### Step 3: Cross-compile the package

```bash
cd ~/src/<package>/unix
make distclean 2>/dev/null || true

CC=x86_64-linux-gnu-gcc \
./configure --host=x86_64-linux-gnu \
    --with-tcl=/tmp/tcl-x86_64/lib \
    --prefix=/tmp/<package>-x86_64

make -j$(nproc)
```

#### Verify the binary architecture

```bash
file /tmp/<package>-x86_64/lib/*.so
# Should show: ELF 64-bit LSB shared object, x86-64, ...
```

## Assembling the Package

1. Create the directory structure:

```bash
mkdir -p vfs/lib/<package>/Darwin/arm64
mkdir -p vfs/lib/<package>/Linux/aarch64
mkdir -p vfs/lib/<package>/Linux/x86_64
```

2. Copy binaries from each build:

```bash
cp /tmp/<package>-darwin/lib/*.dylib vfs/lib/<package>/Darwin/arm64/
cp /tmp/<package>-aarch64/lib/*.so vfs/lib/<package>/Linux/aarch64/
cp /tmp/<package>-x86_64/lib/*.so vfs/lib/<package>/Linux/x86_64/
```

3. Create or update `pkgIndex.tcl`:

```tcl
# Example pkgIndex.tcl for multi-platform package
if { $::tcl_platform(os) == "Windows NT" } {
    package ifneeded <package> <version> \
        [list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) tcl9<package><ver>[info sharedlibextension]]]
} else {
    package ifneeded <package> <version> \
        [list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtcl9<package><version>[info sharedlibextension]] <PackageName>]
}
```

## Adding to Repository

Since `.gitignore` typically excludes binaries, use `-f` to force add:

```bash
git add -f vfs/lib/<package>/
git commit -m "Add/Update <package> <version> with pre-built binaries"
git push
```

## Creating a Release

Push a tag to trigger the GitHub Actions release workflow:

```bash
git tag <version>
git push --tags
```

The workflow will:
1. Build C/C++ packages via CMake for each platform
2. Merge `vfs/lib/` contents (including pre-built packages)
3. Create `dlsh-<version>.zip` with everything combined

## Troubleshooting

### "cannot execute binary file: Exec format error"

The build is trying to run a cross-compiled binary. Use the symlink trick:

```bash
ln -sf /usr/local/bin/tclsh9.0 tclsh   # before running make
```

### Package not loading at runtime

Check that `pkgIndex.tcl` paths match actual binary names:
- Verify `$::tcl_platform(os)` and `$::tcl_platform(machine)` values
- Check library naming (with/without `lib` prefix, version in filename)

### Missing tclConfig.sh

You need a Tcl installation for the target platform. Build Tcl first, then use `--with-tcl=/path/to/lib` pointing to the directory containing `tclConfig.sh`.

## Notes

- Pure Tcl packages (no binaries) can simply be dropped in `vfs/lib/` without platform subdirectories
- The GitHub CI builds C/C++ packages defined in CMakeLists.txt automatically
- Pre-built packages in `vfs/lib/` are for dependencies not in our CMake build system
