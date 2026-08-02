namespace eval brokensys::throws {
    proc protocol_init { s } {
        $s set_protocol [namespace tail [namespace current]]
        # stand-in for "a dependency this host does not have"
        error "cannot reach shape database on /shared/qpcs (no such host)"
    }
}
