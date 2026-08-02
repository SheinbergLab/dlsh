namespace eval brokensys::throws {
    proc loaders_init { s } {
        $s add_loader setup_trials { n } {
            if { [dg_exists stimdg] } { dg_delete stimdg }
            set g [dg_create stimdg]
            dg_rename $g stimdg
            dl_set stimdg:stimtype [dl_fromto 0 $n]
            dl_set stimdg:remaining [dl_ones $n]
            return $g
        }
    }
}
