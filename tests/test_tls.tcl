# test_tls.tcl -- the bundled `tls` package (TclTLS + static OpenSSL).
#
# Offline by design: a self-signed certificate is generated with the
# openssl CLI, a tls::socket -server answers on loopback, and a client
# talks to it both with verification against that certificate and through
# http::geturl over https. Nothing touches the network, so it runs in CI.
#
# The tls library is only in the staged zip when the `tls` target was built.
# Without it this test SKIPs, unless DLSH_TEST_REQUIRE_TLS=1 (CI sets it),
# in which case a missing package is a failure.
#
# Run: dlsh tests/test_tls.tcl   (with DLSH_ZIP pointing at a staged zip)

package require dlsh

if {[catch {package require tls} ver]} {
    if {[info exists ::env(DLSH_TEST_REQUIRE_TLS)] && $::env(DLSH_TEST_REQUIRE_TLS)} {
        puts "FAIL: package require tls: $ver"
        exit 1
    }
    puts "SKIP: tls not in this dlsh.zip ($ver)"
    exit 0
}
package require http

set failures 0
proc check {label cond {detail ""}} {
    if {[uplevel 1 [list expr $cond]]} {
        puts "  ok   $label"
    } else {
        puts "  FAIL $label $detail"
        incr ::failures
    }
}

puts "tls $ver -- [tls::version]"
check "tls version 2.x"          {[package vsatisfies $ver 2]}
check "OpenSSL is the bundled 3.x" {[string match "OpenSSL 3.*" [tls::version]]} [tls::version]
check "TLS 1.3 available"        {"tls1.3" in [tls::protocols]} [tls::protocols]

# --- self-signed certificate -------------------------------------------------
set dir [file join [expr {[info exists ::env(TMPDIR)] ? $::env(TMPDIR) : "/tmp"}] \
             dlsh_tls_test_[pid]]
file mkdir $dir
set key  [file join $dir key.pem]
set cert [file join $dir cert.pem]
# Made with the openssl CLI, not tls::misc req: TclTLS 2.0.1's misc req never
# sets the issuer name, so its "self-signed" certificate cannot be verified.
set openssl [auto_execok openssl]
if {$openssl eq ""} {
    if {[info exists ::env(DLSH_TEST_REQUIRE_TLS)] && $::env(DLSH_TEST_REQUIRE_TLS)} {
        puts "FAIL: no openssl command to make a test certificate"
        exit 1
    }
    puts "SKIP: no openssl command to make a test certificate"
    exit 0
}
exec {*}$openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
    -keyout $key -out $cert -subj /CN=localhost \
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1" 2>@1
check "key and certificate generated" {[file size $key] > 0 && [file size $cert] > 0}

# --- server, in a child dlsh ------------------------------------------------
# A separate process, because the client calls below block during the
# handshake. It prints its port, echoes one line or answers one HTTP request
# per connection, and exits when its stdin (our pipe) closes.
set srvscript [file join $dir server.tcl]
set f [open $srvscript w]
puts $f [string map [list @CERT@ [list $cert] @KEY@ [list $key]] {
    package require tls
    proc accept {chan addr port} {
        fconfigure $chan -translation crlf -buffering line -blocking 0
        fileevent $chan readable [list serve $chan]
    }
    proc serve {chan} {
        if {[catch {gets $chan line} n]} { catch {close $chan}; return }
        if {$n < 0} { if {[eof $chan]} { close $chan }; return }
        if {[string match "GET *" $line]} {
            fconfigure $chan -blocking 1
            while {[gets $chan h] > 0} {}
            set body "hello over https"
            puts $chan "HTTP/1.1 200 OK"
            puts $chan "Content-Type: text/plain"
            puts $chan "Content-Length: [string length $body]"
            puts $chan "Connection: close"
            puts $chan ""
            fconfigure $chan -translation binary
            puts -nonewline $chan $body
            close $chan
        } else {
            puts $chan "echo:$line"
        }
    }
    set srv [tls::socket -server accept -certfile @CERT@ -keyfile @KEY@ \
                 -myaddr 127.0.0.1 0]
    puts [lindex [fconfigure $srv -sockname] 2]
    flush stdout
    fileevent stdin readable {if {[gets stdin] < 0 && [eof stdin]} {exit 0}}
    vwait forever
}]
close $f
set child [open |[list [info nameofexecutable] $srvscript 2>@stderr] r+]
set port [gets $child]
check "server listening" {[string is integer -strict $port] && $port > 0} "got '$port'"

# --- client, verifying against the self-signed cert --------------------------
set c [tls::socket -cafile $cert -servername localhost 127.0.0.1 $port]
fconfigure $c -translation crlf -buffering line
tls::handshake $c
puts $c "ping"
set reply [gets $c]
array set st [tls::status $c]
close $c
check "echo through TLS"         {$reply eq "echo:ping"} "got '$reply'"
check "peer certificate verified" {$st(subject) ne ""} [array get st]
set proto [dict get [tls::connection [set c2 [tls::socket -cafile $cert \
    -servername localhost 127.0.0.1 $port]]] protocol]
close $c2
check "negotiated TLS 1.3"       {$proto eq "TLSv1.3"} $proto

# --- verification must fail without the CA -----------------------------------
# A self-signed cert is in no system store, so a default client must refuse.
set rc [catch {
    set c3 [tls::socket -servername localhost 127.0.0.1 $port]
    tls::handshake $c3
} err]
catch {close $c3}
check "untrusted cert rejected"  {$rc != 0} "handshake succeeded: $err"

# --- http::geturl over https -------------------------------------------------
http::register https 443 [list ::tls::socket -cafile $cert]
set tok [http::geturl https://localhost:$port/ -timeout 10000]
set code [http::ncode $tok]
set body [http::data $tok]
http::cleanup $tok
check "http::geturl https status" {$code == 200} "code $code [http::error $tok]"
check "http::geturl https body"   {$body eq "hello over https"} "body '$body'"

close $child
file delete -force $dir

if {$failures} {
    puts "FAIL: $failures check(s) failed"
    exit 1
}
puts "PASS: tls"
