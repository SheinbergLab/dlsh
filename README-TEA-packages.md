# Building TEA Packages in dlsh

This guide covers how to add or update Tcl Extension Architecture (TEA) packages (like tdom, sqlite, thread) that are built in CI and bundled into the dlsh zipfs.

## Package Structure

TEA packages follow this structure in `vfs/lib/`:

```
vfs/lib/<package>/
├── Darwin/
│   └── arm64/
│       └── libtcl9<package><version>.dylib
├── Linux/
│   ├── aarch64/
│   │   └── libtcl9<package><version>.so
│   └── x86_64/
│       └── libtcl9<package><version>.so
├── Windows NT/
│   └── <if supported>
└── pkgIndex.tcl
```

**Important**: Only `pkgIndex.tcl` is committed to the repo. The binaries are built by CI.

## Adding a New TEA Package

### Step 1: Add version variables to release.yml

```yaml
env:
  # TEA package versions - update these when upgrading
  TCL_VERSION: "9.0.1"
  TCL_TAG: "core-9-0-1"
  TDOM_VERSION: "0.9.6"
  NEW_PACKAGE_VERSION: "1.2.3"  # <-- add new package
```

### Step 2: Add build steps to each platform job

Add after the Tcl build step in all three jobs (`build-linux-x86_64`, `build-linux-aarch64`, `build-macos-arm64`):

```yaml
      - name: Build <package>
        run: |
          curl -L -o package-src.tgz <download-url>
          tar xzf package-src.tgz
          cd <extracted-dir>/        # use glob like package-*/ if name varies
          ./configure --with-tcl=${{ github.workspace }}/tcl-install/lib
          make -j$(nproc)            # use $(sysctl -n hw.ncpu) on macOS
```

### Step 3: Add to the assemble step

In each platform's "Assemble TEA packages into vfs" step:

```yaml
      - name: Assemble TEA packages into vfs
        run: |
          # existing packages...
          
          # <package>
          mkdir -p install/lib/<package>/Linux/x86_64  # adjust per platform
          cp <extracted-dir>/*.so install/lib/<package>/Linux/x86_64/
```

Platform-specific paths:
- Linux x86_64: `install/lib/<package>/Linux/x86_64/` with `*.so`
- Linux aarch64: `install/lib/<package>/Linux/aarch64/` with `*.so`
- macOS arm64: `install/lib/<package>/Darwin/arm64/` with `*.dylib`

### Step 4: Create pkgIndex.tcl

Create `vfs/lib/<package>/pkgIndex.tcl`:

```tcl
#
# Tcl package index file for <package>
# NOTE: Version must match <PACKAGE>_VERSION in .github/workflows/release.yml
#
if { $::tcl_platform(os) == "Windows NT" } {
    package ifneeded <package> <version> \
        [list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) tcl9<package><ver>[info sharedlibextension]]]
} else {
    package ifneeded <package> <version> \
        [list load [file join $dir $::tcl_platform(os) $::tcl_platform(machine) libtcl9<package><version>[info sharedlibextension]] <PackageName>]
}
```

### Step 5: Commit and test

```bash
git add vfs/lib/<package>/pkgIndex.tcl
git add .github/workflows/release.yml
git commit -m "Add <package> <version> to CI builds"
git push
git tag <next-version>
git push --tags
```

## Updating a Package Version

When upgrading a package (e.g., tdom 0.9.6 → 0.9.7):

1. **Update `release.yml`**: Change the version in the `env:` section
2. **Update `pkgIndex.tcl`**: Change the version number and library filename
3. **Test locally first** (optional): Build on your Mac to verify directory structure

```bash
# On Mac, build and check the output
curl -L -o tdom-src.tgz http://tdom.org/downloads/tdom-0.9.7-src.tgz
tar xzf tdom-src.tgz
cd tdom-*/
./configure --with-tcl=/path/to/tcl/lib
make
ls -la *.dylib  # verify the filename matches pkgIndex.tcl
```

## Troubleshooting

### "No such file or directory" during build

The tarball probably extracts to a different directory name than expected. Use a glob:
```yaml
cd package-*/
```

Or check what it extracts to:
```bash
tar tzf package-src.tgz | head -5
```

### Package not loading at runtime

Check that `pkgIndex.tcl` paths match actual binary names:
- Verify `$::tcl_platform(os)` returns `Darwin` or `Linux`
- Verify `$::tcl_platform(machine)` returns `arm64`, `aarch64`, or `x86_64`
- Check library naming (with/without `lib` prefix, version in filename)

Test interactively:
```tcl
puts $::tcl_platform(os)      ;# Darwin, Linux
puts $::tcl_platform(machine) ;# arm64, aarch64, x86_64
package require <package>
```

### glibc version errors on Linux

This happens when building on a newer system than the target. Solution: build natively on the CI runner for that platform (ubuntu-22.04 for x86_64, ubuntu-22.04-arm for aarch64).

### configure can't find Tcl

Make sure `--with-tcl=` points to the directory containing `tclConfig.sh`:
```yaml
--with-tcl=${{ github.workspace }}/tcl-install/lib
```

## CI Build Environments

| Platform | Runner | CPU count |
|----------|--------|-----------|
| Linux x86_64 | ubuntu-22.04 | `$(nproc)` |
| Linux aarch64 | ubuntu-22.04-arm | `$(nproc)` |
| macOS arm64 | macos-14 | `$(sysctl -n hw.ncpu)` |

## Package Sources

Common TEA package download locations:

- **tdom**: `http://tdom.org/downloads/tdom-<version>-src.tgz`
- **thread**: `https://core.tcl-lang.org/thread/tarball/thread-<version>.tar.gz?uuid=thread-<tag>`
- **sqlite**: `https://sqlite.org/<year>/sqlite-autoconf-<version>.tar.gz`
- **tcllib**: `https://core.tcl-lang.org/tcllib/tarball/tcllib-<version>.tar.gz?uuid=tcllib-<tag>`

## Notes

- Pure Tcl packages (no binaries) can simply be dropped in `vfs/lib/` without platform subdirectories
- The Tcl build is cached per-platform to speed up subsequent runs
- Binaries are NOT committed to the repo - CI builds them fresh each release
