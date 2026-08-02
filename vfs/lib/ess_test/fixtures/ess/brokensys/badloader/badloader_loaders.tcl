namespace eval brokensys::badloader {
    proc loaders_init { s } {
        # throws from INSIDE the loader -- i.e. after variant_init has already
        # set ess/status to "loading". This is the path that used to wedge.
        $s add_loader setup_trials { n } {
            error "db file not found"
        }
    }
}
