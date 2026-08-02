namespace eval brokensys::throws {
    variable variants {
        basic {
            description "fixture variant"
            loader_proc setup_trials
            loader_options { n { { four 4 } } }
        }
    }
}
