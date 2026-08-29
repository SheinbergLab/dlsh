# Minimal fixture system for ess_test's ESS harness. Two protocols:
#   works  -- loads normally
#   throws -- protocol_init raises, the way a missing external dependency does
package require ess

namespace eval brokensys {
    proc create {} {
        set sys [::ess::create_system [namespace tail [namespace current]]]
        $sys add_param interblock_time 100 time int
        $sys add_variable n_obs 0
        $sys add_variable obs_count 0
        $sys add_variable cur_id 0

        $sys set_start start
        $sys add_state start { return stop }
        # both args: a one-arg add_state defines only the action, and the
        # load-time validate_state_methods check refuses unpaired states
        $sys add_state stop {} {}
        $sys set_end stop
        return $sys
    }
}
