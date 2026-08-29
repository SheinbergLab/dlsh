namespace eval parkstate::works {
    proc protocol_init { s } {
        $s set_protocol [namespace tail [namespace current]]
        $s set_protocol_init_callback { ::ess::init }
        $s set_protocol_deinit_callback {}
        $s set_reset_callback { dl_set stimdg:remaining [dl_ones [dl_length stimdg:stimtype]] }
        $s set_start_callback {}
        $s set_quit_callback {}
        $s set_end_callback {}
        $s add_method n_obs {} { return [dl_length stimdg:stimtype] }
        $s add_method nexttrial {} {}
        $s add_method finished {} { return 1 }
        return
    }
}
