# tls: TLS sockets and https

`package require tls` loads [TclTLS](https://core.tcl-lang.org/tcltls) 2.0.1
from `dlsh.zip`, so the standalone `dlsh`, stim2 and dserv can all use it.
The OpenSSL it uses is built from source and linked in statically. Nothing
depends on a system or Homebrew OpenSSL, and `tls::version` names the
bundled copy:

```
% package require tls
2.0.1
% tls::version
OpenSSL 3.5.9 29 Sep 2026
```

## Using it

An https client:

```tcl
package require tls
package require http
http::register https 443 ::tls::socket
set tok [http::geturl https://api.github.com/repos/tcltk/tcl/releases?per_page=1]
http::ncode $tok        ;# 200
```

A server on the lab-CA certificate:

```tcl
tls::socket -server accept -certfile server.crt -keyfile server.key 8140
```

Some defaults worth knowing:

- **Clients verify the server's certificate by default** (TclTLS 2.x
  defaults `-require 1` for clients). With no `-cafile`/`-cadir`, the trust
  store is the system one under `/etc/ssl`: `/etc/ssl/cert.pem` on macOS and
  `/etc/ssl/certs` on Debian, Ubuntu and Raspberry Pi OS. To trust a
  private CA such as dserv's lab CA, pass `-cafile` naming its certificate.
  `SSL_CERT_FILE` and `SSL_CERT_DIR` override the default location for the
  whole process.
- **SNI is automatic.** The host name you connect to is sent as the server
  name, and it is also the name the certificate is checked against. When
  you connect by IP address, pass `-servername`.
- **`tls::misc req` makes certificates that won't verify.** In TclTLS
  2.0.1 it never sets the issuer, so a client rejects them even with
  `-cafile` pointing at the certificate itself. Make test certificates with
  `openssl req -x509 ...` instead, as `tests/test_tls.tcl` does.
- **dserv's main interpreter has no event loop**, so `http::geturl` (which
  uses `after`) fails there with or without tls. Blocking `tls::socket` I/O
  works in dserv, and so does dserv's own `https_get`.

## What is bundled

| | Version | Source |
|---|---|---|
| TclTLS | 2.0.1 | `https://core.tcl-lang.org/tcltls/uv/tcltls2.0.1.tar.gz` |
| OpenSSL | 3.5.9 (LTS, supported to April 2030) | `https://github.com/openssl/openssl/releases` |

The build lives in `pkgs/tls/CMakeLists.txt`. It fetches both tarballs and
checks their pinned SHA-256s, the same way `pkgs/sqlite` and `pkgs/thread`
pin their sources. It then:

- builds OpenSSL as static libraries with `no-shared no-module no-apps
  no-docs no-tests` and `--openssldir=/etc/ssl`. With `no-module`, the
  legacy provider is compiled into libcrypto, so there is no
  `ossl-modules/*.so` to ship;
- compiles TclTLS's four C files against the Tcl stubs. `library/tls.tcl`
  is embedded in the library, as TclTLS's own build does;
- produces `lib/tls/<os>/<arch>/libtcl9tls2.0.1.{dylib,so}`, which
  `vfs/lib/tls/pkgIndex.tcl` loads.

Only the `Tls_*` entry points are exported. On Linux, every OpenSSL symbol
in the module is local (`--exclude-libs,ALL`). That matters because the host
process may already have a different libssl loaded: dserv links the system
OpenSSL for its own https server. Without this, `tls.so` would bind to the
host's copy. On macOS the two-level namespace gives the same isolation.

On macOS, the release build (Xcode generator) signs `libtcl9tls` with the
Developer ID, like every other package. A hardened `dlsh` or stim2 refuses
to load it otherwise. `release.yml` fails if it ends up unsigned, or if any
platform's `tls` library is missing from `dlsh.zip`.

Platforms: macOS arm64, Linux x86_64 and Linux aarch64, which are the ones
CI builds. Windows is skipped. Building OpenSSL there needs perl, nasm and
nmake, and no dlsh CI job builds Windows today.

## Updating the pinned versions

Both pins are at the top of `pkgs/tls/CMakeLists.txt`.

**OpenSSL** (security releases come every few months):

1. Pick the newest release on the current LTS branch (3.5.x) from
   <https://github.com/openssl/openssl/releases>. Move to a newer LTS
   branch only on purpose. TclTLS 2.0.1 supports OpenSSL 1.1.1 through 4.0.
2. Set `OPENSSL_VERSION`, and set `OPENSSL_SHA256` to the value in the
   release's `openssl-X.Y.Z.tar.gz.sha256` file.
3. Build and test (below). `tls::version` should report the new version.

**TclTLS**:

1. Find the release on <https://core.tcl-lang.org/tcltls/wiki/Download>.
   Mind the tarball name: 2.0.1 is `tcltls2.0.1.tar.gz` and 2.0 was
   `tcltls-2.0-src.tar.gz`, so check the link rather than guessing it.
2. Set `TCLTLS_VERSION`, `TCLTLS_URL` and `TCLTLS_SHA256`
   (`shasum -a 256 <tarball>`).
3. Change the two version strings in `vfs/lib/tls/pkgIndex.tcl`. The
   configure step fails if the pkgIndex and `TCLTLS_VERSION` disagree.
4. If the release adds C files, add them to `add_library(tls ...)`. Check
   the new `configure.ac` (`TEA_ADD_SOURCES`) and the defines in
   `acinclude.m4`.

## Building and testing

```
cmake --build build --target tls -j8        # OpenSSL + the module, ~1 min
DLSH_TEST_REQUIRE_TLS=1 ctest --test-dir build -L dlsh -R 'stage_test_zip|test_tls'
```

`tests/test_tls.tcl` stays offline. It makes a self-signed certificate,
starts a `tls::socket -server` in a child `dlsh`, and checks three things:
a verified TLS 1.3 echo, that a client without the CA is rejected, and an
`http::geturl` over https. If the tls library isn't in the staged zip, the
test SKIPs. CI sets `DLSH_TEST_REQUIRE_TLS=1` so that a missing library
fails the test instead.

To try the module in the *installed*, hardened `dlsh` on macOS, sign it
first, then stage it into a zip:

```
codesign --force --options runtime --timestamp \
  --sign "Developer ID Application" build/pkgs/tls/libtcl9tls2.0.1.dylib
```
