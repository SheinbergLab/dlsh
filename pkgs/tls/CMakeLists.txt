cmake_minimum_required(VERSION 3.20)
project(tcltls C)

# =====================================================================
# `package require tls` -- TclTLS 2.x, with OpenSSL built from a pinned
# source tarball and linked in statically. Nothing at runtime depends on a
# system or Homebrew OpenSSL. See docs/tls.md for how to bump either pin.
#
# Pins: bump the version AND its SHA-256 together.
#   TclTLS  : https://core.tcl-lang.org/tcltls/wiki/Download
#   OpenSSL : https://github.com/openssl/openssl/releases  (stay on an LTS
#             branch -- 3.5.x is supported to 2030-04)
# The committed vfs/lib/tls/pkgIndex.tcl names TCLTLS_VERSION too; the check
# below fails the configure if the two drift apart.
# =====================================================================
set(TCLTLS_VERSION  2.0.1)
set(TCLTLS_URL      https://core.tcl-lang.org/tcltls/uv/tcltls${TCLTLS_VERSION}.tar.gz)
set(TCLTLS_SHA256   afffeb5de1978f47745db4804c1dfdcd6605ceac32e324fe8bbe49843de0baae)

set(OPENSSL_VERSION 3.5.9)
set(OPENSSL_URL     https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz)
set(OPENSSL_SHA256  603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a)

# Where the static OpenSSL looks for its default CA bundle / hashed CA dir
# (OPENSSLDIR/cert.pem, OPENSSLDIR/certs) when a script gives no -cafile or
# -cadir. /etc/ssl holds the system roots on both macOS (cert.pem) and
# Debian/Ubuntu/Raspberry Pi OS (certs/), so https verification works out of
# the box. SSL_CERT_FILE / SSL_CERT_DIR still override it at runtime.
set(TLS_OPENSSLDIR "/etc/ssl" CACHE STRING "OPENSSLDIR compiled into the bundled OpenSSL")
# =====================================================================

if(WIN32)
    # OpenSSL on Windows needs perl + nasm + nmake; no dlsh CI job builds
    # Windows today. Add a build here if that changes.
    message(STATUS "tls: skipped on Windows (no bundled OpenSSL build)")
    return()
endif()

# The committed pkgIndex.tcl hard-codes the version; catch a half-done bump.
file(READ ${CMAKE_SOURCE_DIR}/vfs/lib/tls/pkgIndex.tcl _tls_pkgindex)
string(FIND "${_tls_pkgindex}" "package ifneeded tls ${TCLTLS_VERSION} " _tls_pkgidx_ok)
if(_tls_pkgidx_ok EQUAL -1)
    message(FATAL_ERROR "vfs/lib/tls/pkgIndex.tcl does not provide tls ${TCLTLS_VERSION} "
        "-- update it to match TCLTLS_VERSION in pkgs/tls/CMakeLists.txt")
endif()

include(FetchContent)
include(ExternalProject)
include(ProcessorCount)

# ---------------------------------------------------------------------
# TclTLS source (TEA package, no CMakeLists -> MakeAvailable just unpacks).
# ---------------------------------------------------------------------
FetchContent_Declare(tcltls_src
    URL      ${TCLTLS_URL}
    URL_HASH SHA256=${TCLTLS_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR do-not-configure
)
FetchContent_MakeAvailable(tcltls_src)
set(TLS_SRC ${tcltls_src_SOURCE_DIR})
if(NOT EXISTS ${TLS_SRC}/generic/tls.c)
    message(FATAL_ERROR "tls: TclTLS source not populated: ${TLS_SRC}")
endif()

# ---------------------------------------------------------------------
# OpenSSL, static, from the pinned tarball.
#   no-shared  : only libssl.a / libcrypto.a
#   no-module  : the legacy provider is compiled into libcrypto rather than
#                shipped as a loadable ossl-modules/legacy.so
#   no-apps/no-docs/no-tests : we only need the libraries
# ---------------------------------------------------------------------
if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" OR
       (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64"))
        set(_ossl_target darwin64-x86_64-cc)
    else()
        set(_ossl_target darwin64-arm64-cc)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_ossl_target linux-aarch64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
        set(_ossl_target linux-x86_64)
    else()
        set(_ossl_target linux-generic64)
    endif()
else()
    message(FATAL_ERROR "tls: no OpenSSL Configure target for ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(_ossl_cflags -fPIC)
if(APPLE)
    # Keep the deployment target in step with the rest of dlsh.zip (CI exports
    # MACOSX_DEPLOYMENT_TARGET=14.0; the Xcode signing build sets the variable).
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND _ossl_cflags -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
    elseif(DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
        list(APPEND _ossl_cflags -mmacosx-version-min=$ENV{MACOSX_DEPLOYMENT_TARGET})
    endif()
endif()

ProcessorCount(_ncpu)
if(_ncpu EQUAL 0)
    set(_ncpu 2)
endif()

set(OPENSSL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/openssl-install)
set(OPENSSL_SSL_LIB    ${OPENSSL_PREFIX}/lib/libssl.a)
set(OPENSSL_CRYPTO_LIB ${OPENSSL_PREFIX}/lib/libcrypto.a)
file(MAKE_DIRECTORY ${OPENSSL_PREFIX}/include)

# CC is passed on the Configure line (not inherited from the environment) so an
# Xcode script phase or a stray CC/CFLAGS in the caller's shell cannot leak in.
ExternalProject_Add(tls_openssl
    URL               ${OPENSSL_URL}
    URL_HASH          SHA256=${OPENSSL_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/openssl
    BUILD_IN_SOURCE   TRUE
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env --unset=CFLAGS --unset=LDFLAGS --unset=CPPFLAGS
                      perl ./Configure ${_ossl_target}
                          no-shared no-module no-apps no-docs no-tests
                          --prefix=${OPENSSL_PREFIX}
                          --openssldir=${TLS_OPENSSLDIR}
                          --libdir=lib
                          CC=${CMAKE_C_COMPILER}
                          ${_ossl_cflags}
    BUILD_COMMAND     make -j${_ncpu} build_libs
    INSTALL_COMMAND   make install_dev
    BUILD_BYPRODUCTS  ${OPENSSL_SSL_LIB} ${OPENSSL_CRYPTO_LIB}
    LOG_CONFIGURE     TRUE
    LOG_BUILD         TRUE
    LOG_INSTALL       TRUE
    LOG_OUTPUT_ON_FAILURE TRUE
)

# ---------------------------------------------------------------------
# Generated headers the TEA Makefile would normally make.
# ---------------------------------------------------------------------
set(TLS_GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${TLS_GEN})

# tlsUuid.h: build-info identifier, from the tarball's manifest.uuid.
set(_tls_uuid unknown)
if(EXISTS ${TLS_SRC}/manifest.uuid)
    file(STRINGS ${TLS_SRC}/manifest.uuid _tls_uuid LIMIT_COUNT 1)
endif()
file(CONFIGURE OUTPUT ${TLS_GEN}/tlsUuid.h
    CONTENT "#define TLS_VERSION_UUID \\\n@_tls_uuid@\n" @ONLY)

# tls.tcl.h: library/tls.tcl as the body of a char-array initializer
# (tls.c does `tlsTclInitScript[] = { #include "tls.tcl.h" };` and evals it
# in Tls_Init, so the package needs no separate tls.tcl at runtime).
add_custom_command(
    OUTPUT  ${TLS_GEN}/tls.tcl.h
    COMMAND ${CMAKE_COMMAND} -DINPUT=${TLS_SRC}/library/tls.tcl
            -DOUTPUT=${TLS_GEN}/tls.tcl.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/embed_init_script.cmake
    DEPENDS ${TLS_SRC}/library/tls.tcl ${CMAKE_CURRENT_SOURCE_DIR}/embed_init_script.cmake
    COMMENT "Embedding tls.tcl -> tls.tcl.h"
    VERBATIM
)

# ---------------------------------------------------------------------
# The tls module. Target and vfs/lib folder are both named `tls`, which is
# what cmake/stage_test_zip.cmake keys on.
# ---------------------------------------------------------------------
add_library(tls SHARED
    ${TLS_GEN}/tls.tcl.h
    ${TLS_SRC}/generic/tls.c
    ${TLS_SRC}/generic/tlsBIO.c
    ${TLS_SRC}/generic/tlsIO.c
    ${TLS_SRC}/generic/tlsX509.c
)
add_dependencies(tls tls_openssl)

# What TclTLS's configure defines by default (SSLv3 off, hardening on).
target_compile_definitions(tls PRIVATE
    USE_TCL_STUBS
    BUILD_tls
    PACKAGE_NAME="tls"
    PACKAGE_VERSION="${TCLTLS_VERSION}"
    NO_SSL3=1
    _FORTIFY_SOURCE=2
    TCL_CFG_OPTIMIZED=1          # only feeds tls::build-info, as TEA does
)
target_compile_options(tls PRIVATE
    -fstack-protector-all -fno-strict-overflow
    -fvisibility=hidden          # only Tls_* (DLLEXPORT) leave the module
    -Wno-deprecated-declarations
)
target_include_directories(tls PRIVATE
    ${TLS_GEN}
    ${TLS_SRC}/generic
    ${OPENSSL_PREFIX}/include
)

if(tcl_SOURCE_DIR)
    target_include_directories(tls PRIVATE
        ${tcl_SOURCE_DIR}/generic
        ${tcl_SOURCE_DIR}/libtommath
        ${tcl_SOURCE_DIR}/unix
    )
endif()

target_link_libraries(tls PRIVATE
    tclstub
    ${OPENSSL_SSL_LIB}
    ${OPENSSL_CRYPTO_LIB}
)

if(APPLE)
    # Export only the Tcl entry points; two-level namespace already keeps the
    # static OpenSSL from colliding with any other libssl in the process.
    set_target_properties(tls PROPERTIES LINK_FLAGS "-dynamiclib")
    target_link_options(tls PRIVATE "LINKER:-exported_symbol,_Tls_*")
else()
    # The host may already have a different libssl.so loaded (dserv links the
    # system OpenSSL for its https server). Make every OpenSSL symbol in this
    # module local so tls.so binds to its own copy, never to the host's.
    target_link_options(tls PRIVATE "LINKER:--exclude-libs,ALL")
    target_link_libraries(tls PRIVATE pthread ${CMAKE_DL_LIBS})
endif()

# Filename follows TEA's Tcl 9 convention, as named in vfs/lib/tls/pkgIndex.tcl:
#   libtcl9tls<version>.{so,dylib}
set_target_properties(tls PROPERTIES
    OUTPUT_NAME "tcl9tls${TCLTLS_VERSION}"
    PREFIX "lib"
    POSITION_INDEPENDENT_CODE ON
)
