
package require qpcs

set serversock -1
set status -1 

while { 1 } {
	# create a socket if there is none
	if { $serversock == -1 } {
		puts stdout "Establishing a new connection"	
		if {[catch {set serversock [socket clifford.neuro.brown.edu 2560]}]} { return -1 }
		fconfigure $serversock -buffering line
	}

	puts stdout "Please enter the tcl command: "
	set command [gets stdin]
	if { $command == "q" } { return 0 }
	# send/receive the data stream
	puts $serversock $command
	catch {gets $serversock status}
	puts stdout $status
}
 
return 0
 


