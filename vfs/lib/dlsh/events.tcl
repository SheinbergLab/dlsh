#
# NAME
#    dlbasic.tcl
#
# DESCRIPTION
#    Basic dl_funcs that are derived directly from other primitives
#
#

package provide dlsh 1.2

#
#  Event types stored for Experimental State System event files
#

proc init_events {} {
    global EVT
    set EVT(MAGIC) 0
    set EVT(NAME) 1
    set EVT(FILE) 2
    set EVT(USER) 3
    set EVT(TRACE) 4
    set EVT(PARAM) 5
    set EVT(FSPIKE) 16
    set EVT(HSPIKE) 17
    set EVT(ID) 18
    set EVT(BEGINOBS) 19
    set EVT(ENDOBS) 20
    set EVT(ISI) 21
    set EVT(TRIALTYPE) 22
    set EVT(OBSTYPE) 23
    set EVT(EMLOG) 24
    set EVT(FIXSPOT) 25
    set EVT(EMPARAMS) 26
    set EVT(STIMULUS) 27
    set EVT(PATTERN) 28
    set EVT(STIMTYPE) 29
    set EVT(SAMPLE) 30
    set EVT(PROBE) 31
    set EVT(CUE) 32
    set EVT(TARGET) 33
    set EVT(DISTRACTOR) 34
    set EVT(SOUND) 35
    set EVT(FIXATE) 36
    set EVT(RESP) 37
    set EVT(SACCADE) 38
    set EVT(DECIDE) 39
    set EVT(ENDTRIAL) 40
    set EVT(ABORT) 41
    set EVT(REWARD) 42
    set EVT(DELAY) 43
    set EVT(PUNISH) 44
    set EVT(PHYS) 45
    set EVT(MRI) 46
    set EVT(STIMULATOR) 47

    set EVT(FIXSPOT_OFF) 0
    set EVT(FIXSPOT_ON)  1
    set EVT(FIXSPOT_SET) 2

    set EVT(EMPARAMS_SCALE) 0
    set EVT(EMPARAMS_CIRC) 1
    set EVT(EMPARAMS_RECT) 2

    set EVT(STIMULUS_OFF) 0
    set EVT(STIMULUS_ON) 1
    set EVT(STIMULUS_SET) 2

    set EVT(PATTERN_OFF) 0
    set EVT(PATTERN_ON) 1
    set EVT(PATTERN_SET) 2

    set EVT(RESP_LEFT) 0
    set EVT(RESP_RIGHT) 1
    set EVT(RESP_BOTH) 2
    set EVT(RESP_NONE) 3
    set EVT(RESP_MULTI) 4
    set EVT(RESP_EARLY) 5

    set EVT(ENDTRIAL_INCORRECT) 0
    set EVT(ENDTRIAL_CORRECT) 1
    set EVT(ENDTRIAL_ABORT) 2

    set EVT(ABORT_EM) 0
    set EVT(ABORT_LEVER) 1
    set EVT(ABORT_NORESPONSE) 2

    set EVT(ENDOBS_WRONG) 0
    set EVT(ENDOBS_CORRECT) 1
    set EVT(ENDOBS_QUIT) 2

    set EVT(PHYS_RESP) 0
    set EVT(PHYS_SPO2) 1
    set EVT(PHYS_AWPRESSURE) 2
    set EVT(PHYS_PULSE) 3

    set EVT(MRI_TRIGGER) 0
}