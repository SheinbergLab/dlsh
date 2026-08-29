# Fixture system with UNPAIRED states: `limbo` has a transition but no
# action, `deadend` has an action but no transition.  Before the load-time
# validate_state_methods check, both loaded fine and then threw a swallowed
# TCL LOOKUP METHOD mid-update at runtime, silently parking the machine in
# the state (the remap 2026-08-29 bug).  The harness asserts this system is
# now REFUSED at load, with both states named.
package require ess

namespace eval parkstate {
    proc create {} {
        set sys [::ess::create_system [namespace tail [namespace current]]]
        $sys add_param interblock_time 100 time int
        $sys add_variable n_obs 0
        $sys add_variable obs_count 0
        $sys add_variable cur_id 0

        $sys set_start start
        $sys add_state start { return limbo }

        # transition-only: no limbo_a method ever gets defined
        $sys add_transition limbo { return deadend }

        # action-only: no deadend_t method ever gets defined
        $sys add_action deadend {}

        $sys add_state stop {} {}
        $sys set_end stop
        return $sys
    }
}
