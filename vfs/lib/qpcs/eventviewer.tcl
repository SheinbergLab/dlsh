package require dlsh
package require qpcs

set server [lindex $::argv 0]

::qpcs::buildQNXListener $server
::eventviewer::buildEventViewer $server .
