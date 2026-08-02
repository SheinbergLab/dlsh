namespace eval brokensys::raggedloader {
    proc loaders_init { s } {
        # builds trials happily, but one column is the wrong length --
        # loads clean, dies partway through a real session
        $s add_loader setup_trials { n } {
            if { [dg_exists stimdg] } { dg_delete stimdg }
            set g [dg_create stimdg]
            dg_rename $g stimdg
            dl_set stimdg:stimtype  [dl_fromto 0 $n]
            dl_set stimdg:remaining [dl_ones $n]
            dl_set stimdg:oops      [dl_fromto 0 [expr {$n-1}]]
            return $g
        }
    }
}
