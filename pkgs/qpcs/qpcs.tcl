package provide qpcs 3.40

package require dlsh

namespace eval qpcs {

    variable qnxport 2560
    variable dsport  4620
    
    set data(0) null
    unset data(0)
    set callbacks(0) null
    unset callbacks(0)
    set evtcallbacks(0) null
    unset evtcallbacks(0)
    set ess_ds_data(main) -1
    set obs_starttime -1
    
    # This function itantiates a server on a system provided
    # port.  The acceptEvent command defines the actions taken
    # when data is received.
    
    proc buildServer { } {
	set ::qpcs::data(main) [socket -server ::qpcs::acceptEvent 0]            
    }
    
    # This function takes the name of the host of the QNX process
    # with which to register to receive information.  The name of 
    # the host is then stored for future use in an array. 
    proc registerWithQNX {servername} {
	if {[llength [array names ::qpcs::data main]] == 0} {return -1}
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line
	puts $tempsock "%reg [lindex $socklist 0] [lindex \
		[fconfigure $::qpcs::data(main) -sockname] 2]"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    # This function sends the input text message to the specified7
    # QNX server, or to all on which have been registered if none is
    # specified.
    
    proc sendToQNX {servername args} {
	if { $servername == "" } { return -1 }
	global serverref
	global servsock
	set status -1
	set extra -1
	flush stdout
	# apparently this is the first time we connect to that server
	if { $serverref != $servername } {
		set serverref $servername
		# close an already existing socket (if any) 
		catch {close $servsock} 
		if {[catch {set tempsock [socket $servername $::qpcs::qnxport]}]} {return -1}
		# non-blocking socket 
		fconfigure $tempsock -blocking 1 -buffering line
		# store the open socket
		set servsock $tempsock
		puts $tempsock $args
		catch {gets $tempsock status}
		#gets $tempsock status
	} else {
		set tempsock $servsock
		if {[catch {puts $tempsock $args}]} {
			if {[catch {set tempsock [socket $servername $::qpcs::qnxport]}]} {return -1}
			# non-blocking socket 
			fconfigure $tempsock -blocking 1 -buffering line
			# store the open socket
			set servsock $tempsock
			# last attempt to send data
			puts $tempsock $args
		}	
		catch {gets $tempsock status}
		#gets $tempsock status
	}
	# Loop to clear out the socket buffer
	fconfigure $tempsock -blocking 0 -buffering line 
	while { 1 } {
		#if { [eof $tempsock] } { 
		#	puts stdout "eof on socket"
		#	break
		#}
		catch {gets $tempsock extra }
		if { $extra == "" } {
			break
		} else {
		    set status [string cat $status $extra]
		}
	}
	fconfigure $tempsock -blocking 1 -buffering line
	return $status
    }
    
    # This function informs a QNX server process of the events that this
    # process wishes to listen to.
    proc listenTo { servername args } {
	set match eventlog/events
	set every 1
	
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line

	puts $tempsock "%match [lindex $socklist 0] [lindex \
	         	[fconfigure $::qpcs::data(main) -sockname] 2] \
			$match $every"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    # This function informs a QNX server process of the events that this
    # process wishes to ignore.
    proc ignore {servername args} {
	set match eventlog/events
	
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line
	puts $tempsock "%unmatch [lindex $socklist 0] [lindex \
	         	[fconfigure $::qpcs::data(main) -sockname] 2] \
			$match"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    # This function is part of the server building process and should
    # not need to be called by the user.
    proc acceptEvent {sock addr port} {
	set ::qpcs::data($sock) $sock
	#	fconfigure $sock -translation binary -buffering none
	fconfigure $sock -buffering line
	fileevent $sock readable [list ::qpcs::processEvent $sock]
    }
    
    # This function is part of the server building process and should
    # not need to be called by the user.
    proc processEvent {sock} {
	if {[eof $sock]} {
	    close $sock
	    unset ::qpcs::data($sock)
	    return
	} else {
	    set evt_str [gets $sock]

	    # puts $evt_str
	    
	    set evt_info [split [lindex $evt_str 0] :]

	    set tag ev
	    set etype [lindex $evt_info 1]
	    set esubtype [lindex $evt_info 2]

	    if { $etype == $::EV(BEGINOBS) ||
		 $::qpcs::obs_starttime == -1 ||
		 $::qpcs::obs_starttime == "" } {
		set ::qpcs::obs_starttime [lindex $evt_str 2]
	    }
	    
	    set g [dg_create]
	    dl_set $g:type $etype
	    dl_set $g:subtype $esubtype
	    dl_set $g:time \
		[expr {int([lindex $evt_str 2]-$::qpcs::obs_starttime)/1000}]
	    if { [lindex $evt_str 1] == 1 } {
		dl_set $g:params [eval dl_slist [lindex $evt_str 4]]
	    } else {
		dl_set $g:params [dsData $evt_str]
	    }
	    
	    if {$tag == "sd"} {
		foreach callback [array names ::qpcs::callbacks sd,*] {
		    eval $::qpcs::callbacks($callback) sd $g
		}
		dg_delete $g
	    }
	    if {$tag == "dg"} {
		foreach callback [array names ::qpcs::callbacks dg,*] {
		    eval $::qpcs::callbacks($callback) dg $g
		}
		dg_delete $g
	    }
	    if {$tag == "ev"} {
		dispatchEventCallback "ev" $g
		foreach callback [array names ::qpcs::callbacks ev,*] {
		    eval $::qpcs::callbacks($callback) ev $g
		}
		dg_delete $g
	    }
	    if {$tag == "sp"} {

		return 0
		
		if { $length == 0 } {
		    if { [dg_exists spikedg] } { 
			foreach callback [array names ::qpcs::callbacks sp,*] {
			    eval $::qpcs::callbacks($callback) sp spikedg
			}
			dg_reset spikedg
		    } else {
			return
		    }
		} else {
		    set dgstring [read $sock $length]
		    dg_fromString $dgstring receiveddg
		    if { ![dg_exists spikedg] } { 
			dg_rename receiveddg spikedg 
		    } else {
			dg_append spikedg receiveddg
			dg_delete receiveddg
		    }
		}
	    }
	}
    }

    ####################################
    ### Data Server (ess_ds) support ###
    ####################################

    proc dsVersion { servername } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	puts $tempsock "%version"
	catch {gets $tempsock status}
	close $tempsock
	return $status
    }

    proc dsFileRead { servername filename } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	puts $tempsock "%fileExists $filename"
	catch {gets $tempsock exists}

	if { !$exists } {
	    close $tempsock
	    error "unable to read file \"$filename\""
	}
	
	puts $tempsock "%fileRead $filename"
	catch {gets $tempsock b64encoded}

	close $tempsock
	return $b64encoded
    }

    proc dsFileCopy { servername sourcefile destfile } {
	set str [dsFileRead $servername $sourcefile]
	dl_local b [dl_clist]
	dl_fromString64 $str $b
	dl_write $b $destfile
    }
    

    proc dsTable { servername } {
      set table ""
      if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
        fconfigure $tempsock -buffering line

        # Version 1.0 doesn't support
        puts $tempsock %version
        catch { gets $tempsock version }
        if { [llength $version] == 2 } {
            set version [lindex $version 1]
	}
	if { [expr {$version >= 2.0}] } {
	    puts $tempsock %gettable
	    catch { gets $tempsock table }
	}
	close $tempsock
	return $table
    }

    proc dsKeys { servername } {
	set keys ""
	
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	# Version 1.0 requires looping through, 2.0 gets in one call
	puts $tempsock %version
	catch { gets $tempsock version }
	if { [llength $version] == 2 } {
	    set version [lindex $version 1]
	}
	if { [expr {$version >= 2.0}] } {
	    puts $tempsock %getkeys
	    catch {gets $tempsock keys}
	} else {
	    puts $tempsock %firstkey
	    catch {gets $tempsock key}
	    if { $key != "" } {
		lappend keys $key
	    }
	    while { 1 } {
		puts $tempsock %nextkey
		catch {gets $tempsock key}
		if { $key != "" } {
		    lappend keys $key
		} else {
		    break
		}
	    }
	}
	close $tempsock
	return $keys
    }

    proc dsSendInfo { servername } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
        fconfigure $tempsock -buffering line
	puts $tempsock %sendinfo
        catch {gets $tempsock sinfo}
	close $tempsock
	return $sinfo
    }

    proc dsClear { servername args } {
	if { $args == "" } return
	
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	foreach v $args {
	    puts $tempsock "%clear $v"
	    catch {gets $tempsock status}
	}
	close $tempsock
	return
    }

    proc dsSet { servername var val } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	puts $tempsock "%set $var=$val"
	catch {gets $tempsock status}
	close $tempsock
	return $status
    }

    proc dsSetData { servername var dl } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line
	set len [dl_toString64 $dl v64]
	set d [dict create char 0 float 2 short 4 long 5]
	set type [dict get $d [dl_datatype $dl]]

	set dsz [dict create char 1 float 4 short 2 long 4]
	set eltsize [dict get $dsz [dl_datatype $dl]]
	set len [expr {$eltsize*[dl_length $dl]}]
	
	set cmd [format "%%setdata %s %d 0 %d {%s}" $var $type $len $v64]
	puts $tempsock $cmd
	catch {gets $tempsock status}
	close $tempsock
	return $status
    }

    proc dsPut { servername varname value } { 
	if {[catch {set s [socket $servername 4620]}]} {return -1}
	fconfigure $s -buffering line

	set varlen [expr {[string len $varname]+2}]
	set datalen [expr {[string len $value]+2}]
	set datatype 1
	puts $s "@set $varlen $datatype $datalen"
	gets $s
	puts $s $varname
	gets $s
	puts $s $value
	set status [gets $s]
	close $s
	return $status
    }

    proc dsGrab { servername varname } { 
	if {[catch {set s [socket $servername 4620]}]} {return -1}
	fconfigure $s -buffering line

	set varlen [expr {[string len $varname]+2}]
	puts $s "@get $varlen"
	gets $s
	puts $s $varname
	set size [gets $s]
	if { $size == 0 } {
	    set retval 0
	} else {
	    puts $s 1
	    set retval [gets $s]
	}
	close $s
	return $retval
    }
    
    proc dsPutData { servername varname dl } { 
	set d [dict create char 0 float 2 short 4 long 5]
	set dtype [dl_datatype $dl]
	set type [dict get $d $dtype]
	set d [dict create char 1 float 4 short 2 long 4]
	set eltsize [dict get $d $dtype]
	set len [dl_toString64 $dl _v64]
	set datalen [expr {$eltsize*[dl_length $dl]}]

	if {[catch {set s [socket $servername 4620]}]} {return -1}
	fconfigure $s -buffering line

	set varlen [expr {[string len $varname]+2}]
	puts $s "@set $varlen $type $datalen"
	gets $s
	puts $s $varname
	gets $s
	puts $s $_v64
	set status [gets $s]
	unset _v64
	close $s
	return $status
    }

    proc dsPutDG { servername varname dg } {
	set dgsize [dg_toString64 $dg _dgbuffer]

	if {[catch {set s [socket $servername 4620]}]} {return -1}
	fconfigure $s -buffering line

	set DSERV_DG 6
	set varlen [expr {[string len $varname]+2}]
	puts $s "@set $varlen $DSERV_DG $dgsize"
	gets $s
	puts $s $varname
	gets $s
	puts $s $_dgbuffer
	set status [gets $s]
	unset _dgbuffer
	close $s
	return $status
    }
    
    proc dsGetDG { servername varname } {

	if {[catch {set s [socket $servername 4620]}]} {return -1}
	fconfigure $s -buffering line

	set varlen [expr {[string len $varname]+2}]
	puts $s "@get $varlen"
	gets $s
	puts $s $varname
	set size [gets $s]
	if { $size == 0 } {
	    set g {}
	} else {
	    puts $s 1
	    set dg_datapoint [gets $s]
	    set g [dg_tmpname]
	    dg_fromString64 [lindex $dg_datapoint 4] $g
	}
	close $s
	return $g
    }

    proc dsDGDir { servername } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	puts $tempsock "%dgdir"
	catch {gets $tempsock status}

	close $tempsock
	return $status
    }

    proc dgDir { server } { dsDGDir $server }
    proc dgPut { server varname dg } { dsPutDG $server $varname $dg }
    proc dgGet { server varname } { dsGetDG $server $varname }
    
    
    proc dsGet { servername var } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	puts $tempsock "%get $var"
	catch {gets $tempsock status}

	close $tempsock
	return $status
    }

    proc dsGetSize { servername var } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	puts $tempsock "%getsize $var"
	catch {gets $tempsock status}

	close $tempsock
	return $status
    }


    proc dsGetData { servername var } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line
	puts $tempsock "%get $var"
	catch {gets $tempsock data}
	close $tempsock
	dl_return [dsData [lrange $data 1 end]]
    }

    
    proc dsTouch { servername var } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	puts $tempsock "%touch $var"
	catch {gets $tempsock status}

	close $tempsock
	return $status
    }


    proc dsData { data } {
	set type [lindex $data 1]

	# ESS_DS_STRING
	if { $type == 1 } { return [lindex $data 4] }

	# Other types are base64 encoded
	set d [dict create 0 char 1 string 2 float 4 short 5 long]
	set datatype [dict get $d $type]
	dl_local dl [dl_create $datatype]
	dl_fromString64 [lindex $data 4] $dl
	
	set d [dict create char 1 float 4 short 2 long 4]
	set eltsize [dict get $d $datatype]
	if { [dl_length $dl] != [expr [lindex $data 3]/$eltsize] } {
	    error "invalid data received"
	}
	dl_return $dl
    }

    proc dsTimestamp { data } {
	return [lindex $data 2]
    }
    
    proc dsSocketOpen { servername } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line
	return $tempsock
    }
    
    proc dsSocketClose { sock } {
	close $sock
    }

    proc dsSocketSet { sock var val } {
	puts $sock "%set $var=$val"
	catch {gets $sock status}
	return $status
    }

    proc dsSocketSetData { sock var dl } {
	set len [dl_toString64 $dl v64]
	set d [dict create char 0 float 2 short 4 long 5]
	set type [dict get $d [dl_datatype $dl]]
	set dsz [dict create char 1 float 4 short 2 long 4]
	set eltsize [dict get $dsz [dl_datatype $dl]]
	set len [expr {$eltsize*[dl_length $dl]}]
	set cmd [format "%%setdata %s %d 0 %d {%s}" $var $type $len $v64]
	puts $sock $cmd
	catch {gets $sock status}
	return $status
    }
   
    proc dsSocketGet { sock var } {
	puts $sock "%get $var"
	catch {gets $sock status}
	return $status
    }

    proc dsSocketTouch { sock var } {
	puts $sock "%touch $var"
	catch {gets $sock status}
	return $status
    }

    proc dsSocketGetData { sock var } {
	puts $sock "%getdata $var"
	catch {gets $sock data}
	return $data
    }

    proc dsAddMatch { servername match { every 1 } } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line

	puts $tempsock "%match [lindex $socklist 0] [lindex \
	         	[fconfigure $::qpcs::ess_ds_data(main) -sockname] 2] \
			$match $every"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    proc dsRemoveMatch { servername match } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line
	
	puts $tempsock "%unmatch [lindex $socklist 0] [lindex \
	         	[fconfigure $::qpcs::ess_ds_data(main) -sockname] 2] \
			$match"	
	catch {gets $tempsock status}

	close $tempsock	
	return $status
    }

    proc dsGetMatches { servername } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line
	
	puts $tempsock "%getmatch [lindex $socklist 0] [lindex \
	         	[fconfigure $::qpcs::ess_ds_data(main) -sockname] 2]"
	catch {gets $tempsock status}

	close $tempsock	
	return $status
    }
    
    proc dsRegister { servername { flags 0 } } {
	set ::qpcs::ess_ds_data(main) [socket -server ::qpcs::dsAccept 0]
	if {[llength [array names ::qpcs::ess_ds_data main]] == 0} {return -1}
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line
	puts $tempsock "%reg [lindex $socklist 0] [lindex \
		[fconfigure $::qpcs::ess_ds_data(main) -sockname] 2] $flags"
	catch {gets $tempsock status}

	close $tempsock	
	return $status
    }

    proc dsMirror { servername server port } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line
	puts $tempsock "%reg $server $port 1"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    proc dsMirrorAddMatch { servername mirror_ip mirror_port match { every 1 } } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line

	puts $tempsock "%match $mirror_ip $mirror_port \
			$match $every"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    proc dsMirrorRemoveMatch { servername mirror_ip mirror_port match { every 1 } } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	set socklist [fconfigure $tempsock -sockname]
	fconfigure $tempsock -buffering line

	puts $tempsock "%unmatch $mirror_ip $mirror_port \
			$match"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }


    proc dsUnregister {} {
	foreach s [array names ::qpcs::ess_ds_data] {
	    if { $::qpcs::ess_ds_data($s) != -1 } {
		close $::qpcs::ess_ds_data($s)
		unset ::qpcs::ess_ds_data($s)
	    }
	}
	set ::qpcs::ess_ds_data(main) -1
    }

    proc dsAddCallback { name } {
	set ::qpcs::callbacks(ds,$name) $name
    }

    proc dsRemoveCallback { name } {
	catch {unset ::qpcs::callbacks(ds,$name)} 
    }
    
    
    # This function is part of the ess_ds_server building process and should
    # not need to be called by the user.
    proc dsAccept {sock addr port} {
	set ::qpcs::ess_ds_data($sock) $sock
	fconfigure $sock -buffering line
	fileevent $sock readable [list ::qpcs::dsProcessEvent $sock]
    }
    
    # This function is part of the server building process and should
    # not need to be called by the user.
    proc dsProcessEvent {sock} {
	if {[eof $sock]} {
	    close $sock
	    unset ::qpcs::ess_ds_data($sock)
	    return
	} else {
	    set ess_ds_str [gets $sock]
	    foreach callback [array names ::qpcs::callbacks ds,*] {
		eval $::qpcs::callbacks($callback) ds $ess_ds_str
	    }
	}
    }

    proc dsLogOpen { servername filename { flags "" } } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line
	if { $flags != "" } {
	    puts $tempsock "%logopen $filename $flags"
	} else {
	    puts $tempsock "%logopen $filename"
	}
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    proc dsLogClose { servername filename } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line
	puts $tempsock "%logclose $filename"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    proc dsLogStart { servername filename } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line
	puts $tempsock "%logstart $filename"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    proc dsLogPause { servername filename } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line
	puts $tempsock "%logpause $filename"
	catch {gets $tempsock status}
	close $tempsock	
	return $status
    }

    proc dsLogAddMatch { servername filename match
			 { every 1 } { obs 0 } { buffer 0 } } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line

	puts $tempsock "%logmatch $filename $match $every $obs $buffer"
	catch {gets $tempsock status}

	close $tempsock	
	return $status
    }

    proc dsLogRemoveMatch { servername filename match } {
	if {[catch {set tempsock [socket $servername 4620]}]} {return -1}
	fconfigure $tempsock -buffering line
	
	puts $tempsock "%logunmatch $filename $match"
	catch {gets $tempsock status}

	close $tempsock	
	return $status
    }
    
    proc dsLogFileOpen { server filename { matches "" } } {
	# The third argument says only log between obs on/off
	if { [dsLogOpen $server $filename 1] != 1 } {
	    return 0
	}
	# Always include obs period info
	dsLogAddMatch $server $filename ess:obs:*
	foreach m $matches {
	    dsLogAddMatch $server $filename $m
	}
	return 1
    }

    proc dsLogFileClose { server filename } {
	dsLogClose $server $filename
    }
    
    proc dsStimRegister { server { port 4620 } } {
	set s [socket $server $port]
	fconfigure $s -buffering line
	set our_ip [lindex [fconfigure $s -sockname] 0]

	puts $s "%reg $our_ip 4611"
	catch {gets $s status}

	close $s
	return $status
    }

    proc dsStimUnregister { server { port 4620 } } {
	set s [socket $server $port]
	fconfigure $s -buffering line
	set our_ip [lindex [fconfigure $s -sockname] 0]
	puts $s "%unreg $our_ip 4611"
	catch {gets $s status}
	close $s
	return $status
    }
    
    # Tell server to update dsVals("pattern") upon change
    proc dsStimAddMatch { server pattern { every 1 } { port 4620 } } {
	set s [socket $server $port]
	fconfigure $s -buffering line
	set our_ip [lindex [fconfigure $s -sockname] 0]
	puts $s "%match $our_ip 4611 $pattern $every"
	catch {gets $s status}
	close $s
	return $status
    }

    # Tell server we are no longer in getting updates for "pattern"
    proc dsStimRemoveMatch { server pattern { port 4620 } } {
	set s [socket $server $port]
	fconfigure $s -buffering line
	set our_ip [lindex [fconfigure $s -sockname] 0]
	puts $s "%unmatch $our_ip 4611 $pattern"
	catch {gets $s status}
	close $s
	return $status
    }

    # Tell server we are no longer in getting updates for "pattern"
    proc dsStimGetMatches { server { port 4620 } } {
	set s [socket $server $port]
	fconfigure $s -buffering line
	set our_ip [lindex [fconfigure $s -sockname] 0]
	puts $s "%getmatch $our_ip 4611"
	catch {gets $s status}
	close $s
	return $status
    }

    #### End Data Server funcs ###
    
    # This function matches specific callbacks to specific event types
    proc dispatchEventCallback { tag dg } {
	set type [dl_tcllist $dg:type]
	set callbacks [array names ::qpcs::evtcallbacks $type]
	set number [llength $callbacks]
	if {$number == 0} {return}
	set callbacks $::qpcs::evtcallbacks($type)
	for {set i 0} {$i < [llength $callbacks]} {incr i} { 
	    eval [lindex $callbacks $i] $tag $dg
	}
    }
    
    # This function adds an event specific callback
    proc addEventCallback { name event } {
	set callbacks [array names ::qpcs::evtcallbacks $event]
	if {[llength $callbacks] == 0} {
	    set ::qpcs::evtcallbacks($event) [list $name]
	} else {
	    set ::qpcs::evtcallbacks($event) [concat [list $name] \
		    $::qpcs::evtcallbacks($event) ]
	}
    }

    # This function returns whether or not an event process is currently
    # running on the input server
    proc systemExists { server } {
	set response [::qpcs::user_queryState $server]
	if {[lindex $response 0] == 1} {return 0}
	if {[lindex $response 1] == 0} {return 1}
	if {[lindex $response 1] == 1} {return 1}
	return 0
    }

    # This function adds a callback to the list with the specified
    # tag (that is which type of received data will trigger the call).
    # If no tag is specified, all types of received data will trigger
    # the callback.
    proc addCallback { name { tag all } } {
	if [string match $tag all] {
	    set ::qpcs::callbacks(ev,$name) $name
	    set ::qpcs::callbacks(sd,$name) $name
	    set ::qpcs::callbacks(dg,$name) $name
	    set ::qpcs::callbacks(sp,$name) $name
	} else {
	    set ::qpcs::callbacks($tag,$name) $name
	}
    }

    # This function can remove a callback from the callback list
    proc removeCallback { name { tag all } } {
	if [string match $tag all] {
	    catch {unset ::qpcs::callbacks(ev,$name)}
	    catch {unset ::qpcs::callbacks(sd,$name)}
	    catch {unset ::qpcs::callbacks(dg,$name)}
	    catch {unset ::qpcs::callbacks(sp,$name)}
	} else {
	    catch {unset ::qpcs::callbacks($tag,$name)} 
	}
    }

    # This function removes the given event specific callback from the 
    # event callback list
    proc removeEventCallback {name event} {
	set callbacks [array names ::qpcs::evtcallbacks $event]
	if {[llength $callbacks] == 0} { return }
	set callbacks $::qpcs::evtcallbacks($event)
	for {set i 0} {$i < [llength $callbacks]} {incr i} {
	    if {[string match [lindex $callbacks $i] $name]} {
		set ::qpcs::evtcallbacks($event) [lreplace $callbacks $i $i] 
		return
	    }
	}
    }
    
    # This function celars the callback list
    proc clearCallbacks { } {
	foreach index [array names ::qpcs::callbacks] {
	    unset ::qpcs::callbacks($index);
	}
    }

    # This function closes all sockets associated with the server.
    proc disconnectFromServer { } {	
	foreach sock [array names ::qpcs::data] {
	    close $::qpcs::data($sock)
	    unset ::qpcs::data($sock)
	}
    }
    
    # The following functions are merely wrappers for simple calls
    
    proc user_setname { server arg1 } {
	::qpcs::sendToQNX $server USER_SETNAME $arg1
    }
    proc user_start { server } {
	::qpcs::sendToQNX $server USER_START 
    }
    proc user_stop { server } {
	::qpcs::sendToQNX $server USER_STOP 
    }
    proc user_reset { server } {
	::qpcs::sendToQNX $server USER_RESET 
    }
    proc user_setSystem { server arg1 } {
	::qpcs::sendToQNX $server USER_SET_SYSTEM $arg1
    }
    proc user_setTrace { server arg1 } {
	::qpcs::sendToQNX $server USER_SET_TRACE $arg1
    }
    proc user_setEyes { server arg1 arg2 arg3 arg4 } {
	::qpcs::sendToQNX $server USER_SET_EYES $arg1 $arg2 $arg2 $arg4
    }
    proc user_setLevers { server arg1 arg2 } {
	::qpcs::sendToQNX $server USER_SET_LEVERS $arg1 $arg2
    }
    proc user_setParam { server arg1 arg2 } {
	::qpcs::sendToQNX $server USER_SET_PARAMS $arg1 $arg2
    }
    proc user_queryState { server } {
	::qpcs::sendToQNX $server USER_QUERY_STATE 
    }
    proc user_queryDetails { server } {
	::qpcs::sendToQNX $server USER_QUERY_DETAILS 
    }
    proc user_queryName { server } {
	::qpcs::sendToQNX $server USER_QUERY_NAME 
    }
    proc user_queryNameIndex { server arg1 } {
	::qpcs::sendToQNX $server USER_QUERY_NAME_INDEX $arg1
    }
    proc user_queryDatafile { server } {
	::qpcs::sendToQNX $server USER_QUERY_DATAFILE 
    }
    proc user_queryRemote { server } {
	::qpcs::sendToQNX $server USER_QUERY_REMOTE 
    }
    proc user_queryExecname { server } {
	::qpcs::sendToQNX $server USER_QUERY_EXECNAME 
    }
    proc user_queryParam { server arg1 arg2 } {
	::qpcs::sendToQNX $server USER_QUERY_PARAM $arg1 $arg2
    }
    proc user_checkDiskspace { server } {
	::qpcs::sendToQNX $server USER_CHECK_DISKSPACE
    }
    proc user_fileio { server arg1 arg2 arg3 arg4 } {
	::qpcs::sendToQNX $server USER_FILEIO $arg1 $arg2 $arg3 $arg4
    }
    proc user_kill { server } {
	::qpcs::sendToQNX $server USER_KILL 
    }
    proc user_spawn { server node execname {remote NULL}  } {
	set essstatus [::qpcs::user_queryState $server]
	if { [lindex $essstatus 1] != 255 } { return "0 0" }
	if {[string match $remote NULL]} { 
	    ::qpcs::sendToQNX $server USER_SPAWN $node $execname 
	} else {
	    ::qpcs::sendToQNX $server USER_SPAWN $node $execname $remote
	}
    }

    proc getSubject { server } {
	set response [::qpcs::user_queryParam $server -1 3]
	return [lindex $response 2]
    }

    proc setSubject { server subject } {
	::qpcs::user_setParam $server "ESS Subject" $subject
    }

    proc emInfo { server } {
	::qpcs::sendToQNX $server EM_INFO
    }
    proc emPosition { server } {
	::qpcs::sendToQNX $server EM_POS
    }
    proc emPositionDeg { server } {
	::qpcs::sendToQNX $server EM_POS_DEG
    }
    proc emSetCalib { server args } {
	eval ::qpcs::sendToQNX $server EM_SET_CALIB $args
    }
    proc emGetCalib { server } {
	::qpcs::sendToQNX $server EM_GET_CALIB
    }
    proc juice { server time } {
	::qpcs::sendToQNX $server JUICE $time
    }
    proc piowrite { server amask adata \
			bmask bdata cmask cdata { pioname /dev/pioint } } {
	::qpcs::sendToQNX $server piowrite \
	    $amask $adata $bmask $bdata $cmask $cdata $pioname
    }

    
    # These functions can be used to set up the most common listeners
    
    # This one clears the callback list, builds a server and registers
    # (to listen to all events) with a QNX proccess on the input host
    proc buildQNXListener { host } {
	clearCallbacks
	buildServer
	registerWithQNX $host
    }

    # This one clears the callback list, builds a server and registers
    # (to listen to all events) with a Stim proccess on the input host
    proc buildStimListener { host } {
	clearCallbacks
	buildServer
	registerWithStim $host
    }

    # This one clears the callback list, builds a server and registers
    # (to listen to all events) with a QNX proccess on the first input
    # host, and a Stim process on the second
    proc buildDualListener { host1 host2 } {
	clearCallbacks
	buildServer
	registerWithQNX $host1
	listenTo $host1
	registerWithStim $host2
    }

    # Below are references for the enumerations of the events    
    set array evttypes
    
    # user defined events that have been encorporated into state systems
    foreach type { \
		       {0  E_MAGIC      "Magic Number"} \
		       {1  E_NAME       "Event Name"} \
		       {2  E_FILE       "File I/0"} \
		       {3  E_USER       "User Interaction"} \
		       {4  E_TRACE      "State System Trace"} \
		       {5  E_PARAM      "Parameter Set"} \
		       {16 E_FSPIKE     "Time Stamped Spike"} \
		       {17 E_HSPIKE     "DIS-1 Hardware Spike"} \
		       {18 E_ID         "Name"} \
		       {19 E_BEGINOBS   "Begin Obs Period"} \
		       {20 E_ENDOBS     "End Obs Period"} \
		       {21 E_ISI        "ISI"} \
		       {22 E_TRIALTYPE  "Trial Type"} \
		       {23 E_OBSTYPE    "Obs Period Type"} \
		       {24 E_EMLOG      "EM Log"} \
		       {25 E_FIXSPOT    "Fixspot"} \
		       {26 E_EMPARAMS   "EM Params"} \
		       {27 E_STIMULUS   "Stimulus"} \
		       {28 E_PATTERN    "Pattern"} \
		       {29 E_STIMTYPE   "Stimulus Type"} \
		       {30 E_SAMPLE     "Sample"} \
		       {31 E_PROBE      "Probe"} \
		       {32 E_CUE        "Cue"} \
		       {33 E_TARGET     "Target"} \
		       {34 E_DISTRACTOR "Distractor"} \
		       {35 E_SOUND      "Sound Effect"} \
		       {36 E_FIXATE     "Fixation"} \
		       {37 E_RESP       "Response"} \
		       {38 E_SACCADE    "Saccade"} \
		       {39 E_DECIDE     "Decide"} \
		       {40 E_ENDTRIAL   "EOT"} \
		       {41 E_ABORT      "Abort"} \
		       {42 E_REWARD     "Reward"} \
		       {43 E_DELAY      "Delay"} \
		       {44 E_PUNISH     "Punish"} \
		       {45 E_PHYS       "Physio Params"} \
		       {46 E_MRI        "Mri"} \
		       {47 E_STIMULATOR "Stimulator Signal"} \
		   } {
	set num [lindex $type 0]
	set e_name [lindex $type 1]
	set name [lindex $type 2]
	set evttypes($num,enum) $name
	set evttypes($num,name) $name
	set evttypes($e_name,num) $num
	set evttypes($e_name,name) $name
    }


    for {set i 48} { $i < 128 } {incr i} {
	set evttypes($i,enum) E_SYSTEM_EVENT
	set evttypes($i,name) "System Event"
	set evttypes(E_SYSTEM_EVENT,num) $i
	set evttypes(E_SYSTEM_EVENT,name) "System Event"
    }


    # user defined events that have been encorporated into state systems
    foreach type { \
		       {128 E_TARGNAME      "Target Name"} \
		       {129 E_SCENENAME     "Scene Name"} \
		       {130 E_SACCADE_INFO  "Saccade Info"} \
		       {131 E_STIM_TRIGGER  "Stim Trigger"} \
		       {132 E_MOVIENAME     "Movie Name"} \
		       {133 E_STIMULATION   "Stimulation"} \
		       {134 E_SECOND_CHANCE "Second Chance"} \
		       {135 E_SECOND_RESP   "Second Response"} \
		       {136 E_SWAPBUFFER    "Swap Buffer"} \
		       {137 E_STIM_DATA     "Stim Data"} \
		   } {
	set num [lindex $type 0]
	set e_name [lindex $type 1]
	set name [lindex $type 2]
	set evttypes($num,enum) $name
	set evttypes($num,name) $name
	set evttypes($e_name,num) $num
	set evttypes($e_name,name) $name
    }

    for {set i 138} { $i < 256 } {incr i} {
	set evttypes($i,enum) E_USER_EVENT
	set evttypes($i,name) "User Event"
	set evttypes(E_USER_EVENT,num) $i
	set evttypes(E_USER_EVENT,name) "User Event"
    }

    set array evtsubtypes
    set evtsubtypes(3,0) E_USER_START
    set evtsubtypes(E_USER,0) E_USER_START
    set evtsubtypes(3,1) E_USER_QUIT
    set evtsubtypes(E_USER,1) E_USER_QUIT
    set evtsubtypes(3,2) E_USER_RESET
    set evtsubtypes(E_USER,2) E_USER_RESET
    set evtsubtypes(3,3) E_USER_SYSTEM
    set evtsubtypes(E_USER,3) E_USER_SYSTEM
    set evtsubtypes(4,0) E_TRACE_ACT
    set evtsubtypes(E_TRACE,0) E_TRACE_ACT
    set evtsubtypes(4,1) E_TRACE_TRANS
    set evtsubtypes(E_TRACE,1) E_TRACE_TRANS
    set evtsubtypes(4,2) E_TRACE_WAKE
    set evtsubtypes(E_TRACE,2) E_TRACE_WAKE
    set evtsubtypes(4,3) E_TRACE_DEBUG
    set evtsubtypes(E_TRACE,3) E_TRACE_DEBUG
    set evtsubtypes(5,0) E_PARAM_NAME
    set evtsubtypes(E_PARAM,0) E_PARAM_NAME
    set evtsubtypes(5,1) E_PARAM_VAL
    set evtsubtypes(E_PARAM,1) E_PARAM_VAL
    set evtsubtypes(18,0) E_ID_ESS
    set evtsubtypes(E_ID,0) E_ID_ESS
    set evtsubtypes(18,1) E_ID_SUBJECT
    set evtsubtypes(E_ID,1) E_ID_SUBJECT
    set evtsubtypes(20,0) E_ENDOBS_WRONG
    set evtsubtypes(E_ENDOBS,0) E_ENDOBS_WRONG
    set evtsubtypes(20,1) E_ENDOBS_CORRECT
    set evtsubtypes(E_ENDOBS,1) E_ENDOBS_CORRECT
    set evtsubtypes(20,2) E_ENDOBS_QUIT
    set evtsubtypes(E_ENDOBS,2) E_ENDOBS_QUIT
    set evtsubtypes(24,0) E_EMLOG_STOP
    set evtsubtypes(E_EMLOG,0) E_EMLOG_STOP
    set evtsubtypes(24,1) E_EMLOG_START
    set evtsubtypes(E_EMLOG,1) E_EMLOG_START
    set evtsubtypes(24,2) E_EMLOG_RATE
    set evtsubtypes(E_EMLOG,2) E_EMLOG_RATE
    set evtsubtypes(25,0) E_FIXSPOT_OFF
    set evtsubtypes(E_FIXSPOT,0) E_FIXSPOT_OFF
    set evtsubtypes(25,1) E_FIXSPOT_ON
    set evtsubtypes(E_FIXSPOT,1) E_FIXSPOT_ON
    set evtsubtypes(25,2) E_FIXSPOT_SET
    set evtsubtypes(E_FIXSPOT,2) E_FIXSPOT_SET
    set evtsubtypes(26,0) E_EMPARAMS_SCALE
    set evtsubtypes(E_EMPARAMS,0) E_EMPARAMS_SCALE
    set evtsubtypes(26,1) E_EMPARAMS_CIRC
    set evtsubtypes(E_EMPARAMS,1) E_EMPARAMS_CIRC
    set evtsubtypes(26,2) E_EMPARAMS_RECT
    set evtsubtypes(E_EMPARAMS,2) E_EMPARAMS_RECT
    set evtsubtypes(27,0) E_STIMULUS_OFF
    set evtsubtypes(E_STIMULUS,0) E_STIMULUS_OFF
    set evtsubtypes(27,1) E_STIMULUS_ON
    set evtsubtypes(E_STIMULUS,1) E_STIMULUS_ON
    set evtsubtypes(27,2) E_STIMULUS_SET
    set evtsubtypes(E_STIMULUS,2) E_STIMULUS_SET
    set evtsubtypes(28,0) E_PATTERN_OFF
    set evtsubtypes(E_PATTERN,0) E_PATTERN_OFF
    set evtsubtypes(28,1) E_PATTERN_ON
    set evtsubtypes(E_PATTERN,1) E_PATTERN_ON
    set evtsubtypes(28,2) E_PATTERN_SET
    set evtsubtypes(E_PATTERN,2) E_PATTERN_SET
    set evtsubtypes(30,0) E_SAMPLE_OFF
    set evtsubtypes(E_SAMPLE,0) E_SAMPLE_OFF
    set evtsubtypes(30,1) E_SAMPLE_ON
    set evtsubtypes(E_SAMPLE,1) E_SAMPLE_ON
    set evtsubtypes(30,2) E_SAMPLE_SET
    set evtsubtypes(E_SAMPLE,2) E_SAMPLE_SET
    set evtsubtypes(31,0) E_PROBE_OFF
    set evtsubtypes(E_PROBE,0) E_PROBE_OFF
    set evtsubtypes(31,1) E_PROBE_ON
    set evtsubtypes(E_PROBE,1) E_PROBE_ON
    set evtsubtypes(31,2) E_PROBE_SET
    set evtsubtypes(E_PROBE,2) E_PROBE_SET
    set evtsubtypes(32,0) E_CUE_OFF
    set evtsubtypes(E_CUE,0) E_CUE_OFF
    set evtsubtypes(32,1) E_CUE_ON
    set evtsubtypes(E_CUE,1) E_CUE_ON
    set evtsubtypes(32,2) E_CUE_SET
    set evtsubtypes(E_CUE,2) E_CUE_SET
    set evtsubtypes(33,0) E_TARGET_OFF
    set evtsubtypes(E_TARGET,0) E_TARGET_OFF
    set evtsubtypes(33,1) E_TARGET_ON
    set evtsubtypes(E_TARGET,1) E_TARGET_ON
    set evtsubtypes(34,0) E_DISTRACTOR_OFF
    set evtsubtypes(E_DISTRACTOR,0) E_DISTRACTOR_OFF
    set evtsubtypes(34,1) E_DISTRACTOR_ON
    set evtsubtypes(E_DISTRACTOR,1) E_DISTRACTOR_ON
    set evtsubtypes(36,0) E_FIXATE_OUT
    set evtsubtypes(E_FIXATE,0) E_FIXATE_OUT
    set evtsubtypes(36,1) E_FIXATE_IN
    set evtsubtypes(E_FIXATE,1) E_FIXATE_IN
    set evtsubtypes(37,0) E_RESP_LEFT
    set evtsubtypes(E_RESP,0) E_RESP_LEFT
    set evtsubtypes(37,1) E_RESP_RIGHT
    set evtsubtypes(E_RESP,1) E_RESP_RIGHT
    set evtsubtypes(37,2) E_RESP_BOTH
    set evtsubtypes(E_RESP,2) E_RESP_BOTH
    set evtsubtypes(37,3) E_RESP_NONE
    set evtsubtypes(E_RESP,3) E_RESP_NONE
    set evtsubtypes(37,4) E_RESP_MULTI
    set evtsubtypes(E_RESP,4) E_RESP_MULTI
    set evtsubtypes(37,5) E_RESP_EARLY
    set evtsubtypes(E_RESP,5) E_RESP_EARLY
    set evtsubtypes(40,0) E_ENDTRIAL_INCORRECT
    set evtsubtypes(E_ENDTRIAL,0) E_ENDTRIAL_INCORRECT
    set evtsubtypes(40,1) E_ENDTRIAL_CORRECT
    set evtsubtypes(E_ENDTRIAL,1) E_ENDTRIAL_CORRECT
    set evtsubtypes(40,2) E_ENDTRIAL_ABORT
    set evtsubtypes(E_ENDTRIAL,2) E_ENDTRIAL_ABORT
    set evtsubtypes(41,0) E_ABORT_EM
    set evtsubtypes(E_ABORT,0) E_ABORT_EM
    set evtsubtypes(41,1) E_ABORT_LEVER
    set evtsubtypes(E_ABORT,1) E_ABORT_LEVER
    set evtsubtypes(41,2) E_ABORT_NORESPONSE
    set evtsubtypes(E_ABORT,2) E_ABORT_NORESPONSE
    set evtsubtypes(45,0) E_PHYS_RESP
    set evtsubtypes(E_PHYS,0) E_PHYS_RESP
    set evtsubtypes(45,1) E_PHYS_SPO2
    set evtsubtypes(E_PHYS,1) E_PHYS_SPO2
    set evtsubtypes(45,2) E_PHYS_AWPRESSURE
    set evtsubtypes(E_PHYS,2) E_PHYS_AWPRESSURE
    set evtsubtypes(45,3) E_PHYS_PULSE
    set evtsubtypes(E_PHYS,3) E_PHYS_PULSE
    set evtsubtypes(46,0) E_MRI_TRIGGER
    set evtsubtypes(E_MRI,0) E_MRI_TRIGGER

}

global serverref
set serverref	-1

global servsock
set servsock		-1

# Global definitions of event tpyes and subtypes 
global EV
set   EV(MAGIC)   0  
set   EV(NAME)   1  
set   EV(FILE)   2  
set   EV(USER)   3  
set   EV(TRACE)   4   
set   EV(PARAM)   5
set   EV(FSPIKE)   16  
set   EV(HSPIKE)   17  
set   EV(ID)   18  		 
set   EV(BEGINOBS)   19  
set   EV(ENDOBS)   20  
set   EV(ISI)   21  
set   EV(TRIALTYPE)   22  
set   EV(OBSTYPE)   23  
set   EV(EMLOG)   24  
set   EV(FIXSPOT)   25  
set   EV(EMPARAMS)   26  
set   EV(STIMULUS)   27  
set   EV(PATTERN)   28  
set   EV(STIMTYPE)   29  
set   EV(SAMPLE)   30  
set   EV(PROBE)   31   
set   EV(CUE)   32  
set   EV(TARGET)   33  
set   EV(DISTRACTOR)   34  
set   EV(SOUND)   35  
set   EV(FIXATE)   36  
set   EV(RESP)   37  
set   EV(SACCADE)   38  
set   EV(DECIDE)   39  
set   EV(ENDTRIAL)   40  
set   EV(ABORT)   41  
set   EV(REWARD)   42  
set   EV(DELAY)   43  
set   EV(PUNISH)   44  
set   EV(PHYS)   45
set   EV(MRI)   46
set   EV(STIMULATOR) 47

global SUBEV
set SUBEV(USER_START)		[expr $EV(USER)*256+0]
set SUBEV(USER_QUIT)		[expr $EV(USER)*256+1]
set SUBEV(USER_RESET)		[expr $EV(USER)*256+2]
set SUBEV(USER_SYSTEM)		[expr $EV(USER)*256+3]
set SUBEV(TRACE_ACT)		[expr $EV(TRACE)*256+0]
set SUBEV(TRACE_TRANS)		[expr $EV(TRACE)*256+1]
set SUBEV(TRACE_WAKE)		[expr $EV(TRACE)*256+2]
set SUBEV(TRACE_DEBUG)		[expr $EV(TRACE)*256+3]
set SUBEV(PARAM_NAME)		[expr $EV(PARAM)*256+0]
set SUBEV(PARAM_VAL)		[expr $EV(PARAM)*256+1]
set SUBEV(ID_ESS)		[expr $EV(ID)*256+0]
set SUBEV(ID_SUBJECT)		[expr $EV(ID)*256+1]
set SUBEV(EMLOG_STOP)		[expr $EV(EMLOG)*256+0]
set SUBEV(EMLOG_START)		[expr $EV(EMLOG)*256+1]
set SUBEV(EMLOG_RATE)		[expr $EV(EMLOG)*256+2]
set SUBEV(FIXSPOT_OFF)		[expr $EV(FIXSPOT)*256+0]
set SUBEV(FIXSPOT_ON)		[expr $EV(FIXSPOT)*256+1]
set SUBEV(FIXSPOT_SET)		[expr $EV(FIXSPOT)*256+2]
set SUBEV(EMPARAMS_SCALE)	[expr $EV(EMPARAMS)*256+0]
set SUBEV(EMPARAMS_CIRC)	[expr $EV(EMPARAMS)*256+1]
set SUBEV(EMPARAMS_RECT)	[expr $EV(EMPARAMS)*256+2]
set SUBEV(STIMULUS_OFF)		[expr $EV(STIMULUS)*256+0]
set SUBEV(STIMULUS_ON)		[expr $EV(STIMULUS)*256+1]
set SUBEV(STIMULUS_SET)		[expr $EV(STIMULUS)*256+2]
set SUBEV(PATTERN_OFF)		[expr $EV(PATTERN)*256+0]
set SUBEV(PATTERN_ON)		[expr $EV(PATTERN)*256+1]
set SUBEV(PATTERN_SET)		[expr $EV(PATTERN)*256+2]
set SUBEV(SAMPLE_OFF)		[expr $EV(SAMPLE)*256+0]
set SUBEV(SAMPLE_ON)		[expr $EV(SAMPLE)*256+1]
set SUBEV(SAMPLE_SET)		[expr $EV(SAMPLE)*256+2]
set SUBEV(PROBE_OFF)		[expr $EV(PROBE)*256+0]
set SUBEV(PROBE_ON)		[expr $EV(PROBE)*256+1]
set SUBEV(PROBE_SET)		[expr $EV(PROBE)*256+2]
set SUBEV(DISTRACTOR_OFF)	[expr $EV(DISTRACTOR)*256+0]
set SUBEV(DISTRACTOR_ON)	[expr $EV(DISTRACTOR)*256+1]
set SUBEV(CUE_OFF)		[expr $EV(CUE)*256+0]
set SUBEV(CUE_ON)		[expr $EV(CUE)*256+1]
set SUBEV(CUE_SET)		[expr $EV(CUE)*256+2]
set SUBEV(FIXATE_OUT)		[expr $EV(FIXATE)*256+0]
set SUBEV(FIXATE_IN)		[expr $EV(FIXATE)*256+1]
set SUBEV(RESP_LEFT)		[expr $EV(RESP)*256+0]
set SUBEV(RESP_RIGHT)		[expr $EV(RESP)*256+1]
set SUBEV(RESP_BOTH)		[expr $EV(RESP)*256+2]
set SUBEV(RESP_NONE)		[expr $EV(RESP)*256+3]
set SUBEV(RESP_MULTI)		[expr $EV(RESP)*256+4]
set SUBEV(RESP_EARLY)		[expr $EV(RESP)*256+5]
set SUBEV(ENDTRIAL_INCORRECT)	[expr $EV(ENDTRIAL)*256+0]
set SUBEV(ENDTRIAL_CORRECT)	[expr $EV(ENDTRIAL)*256+1]
set SUBEV(ENDTRIAL_ABORT)	[expr $EV(ENDTRIAL)*256+2]
set SUBEV(ABORT_EM)		[expr $EV(ABORT)*256+0]
set SUBEV(ABORT_LEVER)		[expr $EV(ABORT)*256+1]
set SUBEV(ABORT_NORESPONSE)	[expr $EV(ABORT)*256+2]
set SUBEV(ENDOBS_WRONG)		[expr $EV(ENDOBS)*256+0]
set SUBEV(ENDOBS_CORRECT)	[expr $EV(ENDOBS)*256+1]
set SUBEV(ENDOBS_QUIT)		[expr $EV(ENDOBS)*256+2]
set SUBEV(PHYS_RESP)	        [expr $EV(PHYS)*256+0]
set SUBEV(MRI_TRIGGER)	        [expr $EV(PHYS)*256+0]

set ESS_PARAM_FILE 0
set ESS_STIM_FILE 1
set ESS_DATA_FILE 2

set ESS_FILE_OPEN 0
set ESS_FILE_CLOSE 1

# This file contains all the code necessary to build an event viewer given
# an input parent widget and servername. After that, comes the code (largely
# similar) to build a trace event viewer. 

if {![catch {package present Tk}] && ![catch {package require Tktable}]} {
    package require BWidget

    namespace eval eventviewer {
	
    # Status Variables
    set pause 0
    set viewerexists 0
 
    # Information Variables
    set server ""
    set lasttime 0
    set obsperiod -1
    set eventnum 0
    set currentview 0

    # Widget (Name) Variables
    set table ""
    set pausebutton ""
    set spinbox ""
    
    # This procedure handles all the widget and variable initialization
    # for the viewer.
    proc buildEventViewer { server {parent NULL} } {
	if {$eventviewer::viewerexists} {
	    raise .eventviewer;
	    return
	}
	set eventviewer::viewerexists 1
	set eventviewer::server $server

	if {[string match $parent NULL]} {
	    set parent [toplevel .eventviewer]
	    wm resizable $parent 0 0
	    wm title $parent "Event Viewer"
	}

	bind $parent <Escape> [list destroy $parent ]

	if { $parent != "." } { 
	    set tframe [frame $parent.tframe]
	} else {
	    set tframe [frame .tframe]
	}
	set eventviewer::table [table $tframe.table -rows 30 -cols 4 -width 4 \
				    -yscrollcommand [list $tframe.vs set ] \
				    -xscrollcommand [list $tframe.hs set ] \
				    -height 20 -colwidth 20 -roworigin -1 -colorigin -1 -titlerows\
				    1 -colstretchmode last -selecttype row -sparsearray 0 -state \
				    disabled -cursor arrow -bordercursor arrow]
	scrollbar $tframe.vs -orient vertical -command [list $eventviewer::table yview ] 
	scrollbar $tframe.hs -orient horizontal -command [list $eventviewer::table xview ]
	pack $tframe.vs -side right -expand true -fill y
	pack $tframe.hs -side bottom -expand true -fill x
	pack [set smallframe [frame $parent.smallframe]] -side top -pady 3
	pack [set eventviewer::pausebutton [button $smallframe.pause -text \
		"Pause" -command {eventviewer::pauseProc} -width 12]] -side \
		left -padx 5

	set gv [LabelFrame $smallframe.sbframe -text "Obs Period" \
			   -width 12 -anchor e -padx 3]
	pack [set eventviewer::spinbox [SpinBox $gv.sb \
		-textvariable eventviewer::currentview \
		-modifycmd eventviewer::spinBoxCommand -width 3]] -padx 5 \
		-side left
	pack $gv

	
	$eventviewer::table width -1 20 0 8 1 8 2 20

	pack $tframe
	pack $eventviewer::table
	bind $eventviewer::table <Destroy> { eventviewer::cleanUpProc }
	::qpcs::listenTo $server -1
	::qpcs::addCallback eventviewer::viewerCallback ev
	::qpcs::addEventCallback eventviewer::resetCallback 3
	::qpcs::addEventCallback eventviewer::beginObsCallback 19
	
	return $parent
    }
    
    # This is the general viewing callback. It inserts the data about the 
    # current event into an array that can be associated with our table for
    # viewing.
    proc viewerCallback { tag dg } {
	if {$eventviewer::obsperiod == -1} { return }
	if {[expr [dl_tcllist $dg:type] == 4]} {return}
	incr eventviewer::eventnum

	set nrows [$eventviewer::table cget -rows]
	if { $eventviewer::eventnum > $nrows } {
	    $eventviewer::table configure -rows [expr $nrows+10]
	}
	
	eventviewer::setTableValue table$eventviewer::obsperiod \
		$eventviewer::eventnum -1 \
		$::qpcs::evttypes([dl_tcllist $dg:type],name)
	eventviewer::setTableValue table$eventviewer::obsperiod \
		$eventviewer::eventnum 0 [dl_tcllist $dg:subtype]
	eventviewer::setTableValue table$eventviewer::obsperiod \
		$eventviewer::eventnum 1 [expr [dl_tcllist $dg:time]\
		- $eventviewer::lasttime]
	eventviewer::setTableValue table$eventviewer::obsperiod \
		$eventviewer::eventnum 2 [dl_tcllist $dg:params]
	set eventviewer::lasttime [dl_tcllist $dg:time]
    }

    # This is the callback associated with the reset event. It cleans out all
    # the table arrays and resets the last event to 0
    proc resetCallback { tag dg } {
	if {[expr [dl_tcllist $dg:subtype] == 2]} {
	    set eventviewer::currentview 0
	    for {set i 0} {$i <= $eventviewer::obsperiod} {incr i 1} {
		eventviewer::cleanTableArray table$i
	    }
	    set eventviewer::obsperiod -1
	}
    }
    
    proc beginObsCallback { tag dg } {
	incr eventviewer::obsperiod 1
	set eventviewer::eventnum -1
	$eventviewer::spinbox configure -range [list 0 \
		$eventviewer::obsperiod 1]
	eventviewer::setTableValue table$eventviewer::obsperiod -1 -1 "Type"
	eventviewer::setTableValue table$eventviewer::obsperiod -1 0 "Subtype"
	eventviewer::setTableValue table$eventviewer::obsperiod -1 1 "Time"
	eventviewer::setTableValue table$eventviewer::obsperiod -1 2 \
		"Parameters"
	set eventviewer::lasttime 0
	if {$eventviewer::pause == 0} {
	    set eventviewer::currentview $eventviewer::obsperiod
	    eventviewer::spinBoxCommand
	}
    }
    
    # This procedure inserts the input data into the input array at the
    # input locations.
    proc setTableValue { array x y info } {
	upvar #0 eventviewer::$array a
	set a($x,$y) $info
    }
    
    # This procedure clears and unsets the input array.
    proc cleanTableArray { array } {
	upvar #0 eventviewer::$array a
	foreach val [array names a] {
	    unset a($val)
	}
	unset a
    }
    
    # This procedure cleans up after the eventviewer, deleting the contents
    # of all the storage arrays and removing its callbacks from the qpcs
    # database.
    proc cleanUpProc { } {
	::qpcs::removeCallback eventviewer::viewerCallback ev
	::qpcs::removeEventCallback eventviewer::resetCallback 3
	::qpcs::removeEventCallback eventviewer::beginObsCallback 19
	for {set i 0} {$i <= $eventviewer::obsperiod} {incr i 1} {
	    eventviewer::cleanTableArray table$i
	}
	set eventviewer::obsperiod -1
	set eventviewer::currentview 0
	set eventviewer::viewerexists 0
    }
    
    # This is the command called whenver the spinbox's value changes. It
    # changes the variable (and thus the data of) the table to reflect the
    # group numbered in the spinbox's entry
    proc spinBoxCommand { } {
	$eventviewer::table configure -variable \
	    eventviewer::table$eventviewer::currentview
    } 
    
    # Called when the event viewer's pause button is pressed, this
    # changes the pause (viewing mode) state of the viewer and 
    # updates teh text on the pause button to reflects this change.
    proc pauseProc { } {
	if {$eventviewer::pause == 0} {
	    set eventviewer::pause 1
	    $eventviewer::pausebutton configure -text "Unpause"
	} else {
	    set eventviewer::pause 0
	    $eventviewer::pausebutton configure -text "Pause"
	}
    }
}
    
    namespace eval spikeviewer {
	
	# Status Variables
	set pause 0
	set viewerexists 0
	
	# Information Variables
	set server ""
	set lasttime 0
	set obsperiod -1
	set eventnum 0
	set currentview 0
	
	# Widget (Name) Variables
	set table ""
	set pausebutton ""
	set spinbox ""
	
	# This procedure handles all the widget and variable initialization
	# for the viewer.
	proc buildSpikeViewer { server {parent NULL} } {
	    if {$spikeviewer::viewerexists} {return}
	    set spikeviewer::viewerexists 1
	    set spikeviewer::server $server
	    
	    if {[string match $parent NULL]} {
		set parent [toplevel .spikeviewer]
		wm resizable $parent 0 0
		wm title $parent "Event Viewer"
	    }
	    
	    bind $parent <Escape> [list destroy $parent ]
	    
	    set spikeviewer::table [table $parent.table -rows 50 -cols 4 \
					-width 4 \
					-height 20 -colwidth 20 \
					-roworigin -1 -colorigin -1 -titlerows\
					1 -colstretchmode last \
					-selecttype row -sparsearray 0 -state \
					disabled -cursor arrow \
					-bordercursor arrow]
	    pack [set smallframe [frame $parent.smallframe]] \
		-side top -pady 3
	    pack [set spikeviewer::pausebutton \
		      [button $smallframe.pause -text \
			   "Pause" -command {spikeviewer::pauseProc} \
			   -width 12]] -side \
		left -padx 5
	    
	    set gv [LabelFrame $smallframe.sbframe -text "Obs Period" \
			-width 12 -anchor e -padx 3]
	    pack [set spikeviewer::spinbox [SpinBox $gv.sb \
						-textvariable \
						spikeviewer::currentview \
						-modifycmd \
						spikeviewer::spinBoxCommand \
						-width 3]] -padx 5 \
		-side left
	    pack $gv
	    
	    $spikeviewer::table width -1 20 0 8 1 8 2 20
	    
	    pack $spikeviewer::table
	    bind $spikeviewer::table <Destroy> { spikeviewer::cleanUpProc }
	    ::qpcs::listenToSpikes $server -1
	    ::qpcs::addCallback spikeviewer::viewerCallback ev
	    ::qpcs::addEventCallback spikeviewer::resetCallback 3
	    ::qpcs::addEventCallback spikeviewer::beginObsCallback 19

	    return $parent
	}

	# This is the general viewing callback. It inserts the data about the 
	# current event into an array that can be associated with our table for
	# viewing.
	proc viewerCallback { tag dg } {
	    if {$spikeviewer::obsperiod == -1} { return }
	    if {[expr [dl_tcllist $dg:type] == 4]} {return}
	    incr spikeviewer::eventnum
	    spikeviewer::setTableValue table$spikeviewer::obsperiod \
		$spikeviewer::eventnum -1 \
		$::qpcs::evttypes([dl_tcllist $dg:type],name)
	    spikeviewer::setTableValue table$spikeviewer::obsperiod \
		$spikeviewer::eventnum 0 [dl_tcllist $dg:subtype]
	    spikeviewer::setTableValue table$spikeviewer::obsperiod \
		$spikeviewer::eventnum 1 [expr [dl_tcllist $dg:time]\
					      - $spikeviewer::lasttime]
	    spikeviewer::setTableValue table$spikeviewer::obsperiod \
		$spikeviewer::eventnum 2 [dl_tcllist $dg:params]
	    set spikeviewer::lasttime [dl_tcllist $dg:time]
	}
	
	# This is the callback associated with the reset event. It cleans out all
	# the table arrays and resets the last event to 0
	proc resetCallback { tag dg } {
	    if {[expr [dl_tcllist $dg:subtype] == 2]} {
		set spikeviewer::currentview 0
		for {set i 0} {$i <= $spikeviewer::obsperiod} {incr i 1} {
		    spikeviewer::cleanTableArray table$i
		}
		set spikeviewer::obsperiod -1
	    }
	}
	
	proc beginObsCallback { tag dg } {
	    incr spikeviewer::obsperiod 1
	    set spikeviewer::eventnum -1
	    $spikeviewer::spinbox configure -range [list 0 \
							$spikeviewer::obsperiod 1]
	    spikeviewer::setTableValue table$spikeviewer::obsperiod -1 -1 "Type"
	    spikeviewer::setTableValue table$spikeviewer::obsperiod -1 0 "Subtype"
	    spikeviewer::setTableValue table$spikeviewer::obsperiod -1 1 "Time"
	    spikeviewer::setTableValue table$spikeviewer::obsperiod -1 2 \
		"Parameters"
	    set spikeviewer::lasttime 0
	    if {$spikeviewer::pause == 0} {
		set spikeviewer::currentview $spikeviewer::obsperiod
		spikeviewer::spinBoxCommand
	    }
	}
	
	# This procedure inserts the input data into the input array at the
	# input locations.
	proc setTableValue { array x y info } {
	    upvar #0 spikeviewer::$array a
	    set a($x,$y) $info
	}
	
	# This procedure clears and unsets the input array.
	proc cleanTableArray { array } {
	    upvar #0 spikeviewer::$array a
	    foreach val [array names a] {
		unset a($val)
	    }
	    unset a
	}
	
	# This procedure cleans up after the spikeviewer, deleting the contents
	# of all the storage arrays and removing its callbacks from the qpcs
	# database.
	proc cleanUpProc { } {
	    ::qpcs::removeCallback spikeviewer::viewerCallback ev
	    ::qpcs::removeEventCallback spikeviewer::resetCallback 3
	    ::qpcs::removeEventCallback spikeviewer::beginObsCallback 19
	    for {set i 0} {$i <= $spikeviewer::obsperiod} {incr i 1} {
		spikeviewer::cleanTableArray table$i
	    }
	    set spikeviewer::obsperiod -1
	    set spikeviewer::currentview 0
	    set spikeviewer::viewerexists 0
	}
	
	# This is the command called whenver the spinbox's value changes. It
	# changes the variable (and thus the data of) the table to reflect the
	# group numbered in the spinbox's entry
	proc spinBoxCommand { } {
	    $spikeviewer::table configure -variable \
		spikeviewer::table$spikeviewer::currentview
	} 
	
	# Called when the event viewer's pause button is pressed, this
	# changes the pause (viewing mode) state of the viewer and 
	# updates teh text on the pause button to reflects this change.
	proc pauseProc { } {
	    if {$spikeviewer::pause == 0} {
		set spikeviewer::pause 1
		$spikeviewer::pausebutton configure -text "Unpause"
	    } else {
		set spikeviewer::pause 0
		$spikeviewer::pausebutton configure -text "Pause"
	    }
	}
    }
	
    namespace eval traceviewer {
    
    # Status Variables
    set trace 0
    set pause 0
    set viewerexists 0
 
    # Information Variables
    set server ""
    set lasttime 0
    set obsperiod -1
    set eventnum 0
    set currentview 0

    # Widget (Name) Variables
    set table ""
    set pausebutton ""
    set spinbox ""
    
    # This procedure handles all the widget and variable initialization
    # for the viewer.
    proc buildTraceViewer { server {parent NULL} } {
	if {$traceviewer::viewerexists} {
	    raise .traceviewer
	    return
	}
	set traceviewer::viewerexists 1
	set traceviewer::server $server

	if {[string match $parent NULL]} {
	    set parent [toplevel .traceviewer]
	    wm resizable $parent 0 0
	    wm title $parent "Trace Viewer"
	}

	bind $parent <Escape> [list destroy $parent ]


	if { $parent != "." } { 
	    set tframe [frame $parent.tframe]
	} else {
	    set tframe [frame .tframe]
	}
	
	set trace [::qpcs::user_setTrace $server -1]
	if {[llength $trace] == 2} {
	    set traceviewer::trace [lindex $trace 1]
	}

	set traceviewer::table [table $tframe.table -rows 50 -cols 4 -width 4 \
				    -yscrollcommand [list $tframe.vs set ] \
				    -xscrollcommand [list $tframe.hs set ] \
		-height 20 -colwidth 20 -roworigin -1 -colorigin -1 -titlerows\
		1 -colstretchmode last -selecttype row -sparsearray 0 -state \
		disabled -cursor arrow -bordercursor arrow]

	$traceviewer::table width -1 8 0 8 1 15 2 15

	scrollbar $tframe.vs -orient vertical -command [list $traceviewer::table yview ] 
	scrollbar $tframe.hs -orient horizontal -command [list $traceviewer::table xview ]
	pack $tframe.vs -side right -expand true -fill y
	pack $tframe.hs -side bottom -expand true -fill x
	
	pack [set bigframe [frame $parent.bigframe]] -pady 3
	pack [set smallframe [frame $bigframe.smallframe]] -side left -padx 10
	pack [set traceviewer::pausebutton [button $smallframe.pause -text \
		"Pause" -command {traceviewer::pauseProc} -width 8]] -side \
		bottom -pady 5

	set gv [LabelFrame $smallframe.sbframe -text "Obs Period" \
			   -width 12 -anchor e -padx 3]
	pack [set traceviewer::spinbox [SpinBox $gv.sb  \
		-textvariable traceviewer::currentview \
		-modifycmd traceviewer::spinBoxCommand -width 2]] -pady 5 \
		-side bottom
	pack $gv

	pack [set radframe [LabelFrame $bigframe.radframe -text "Trace Level"\
		-side top -anchor w -relief sunken -borderwidth 1]] -side left
	pack [radiobutton [$radframe getframe].n -text "None" \
		-variable traceviewer::trace -value 0 -command \
		{traceviewer::setTrace}] -anchor w  
	pack [radiobutton [$radframe getframe].ao -text "Action Only" \
		-variable traceviewer::trace -value 1 -command \
		{traceviewer::setTrace}] -anchor w  
	pack [radiobutton [$radframe getframe].aat -text "Action and \
		Transition" -variable traceviewer::trace -value 2 \
		-command {traceviewer::setTrace}] -anchor w  

	pack $tframe
	pack $traceviewer::table
	bind $traceviewer::table <Destroy> { traceviewer::cleanUpProc }
	::qpcs::listenTo $server -1
	::qpcs::addEventCallback traceviewer::traceViewerCallback 4
	::qpcs::addEventCallback traceviewer::traceResetCallback 3
	::qpcs::addEventCallback traceviewer::traceBeginObsCallback 19

	return $parent
    }

    # This is the general viewing callback. It inserts the data about the 
    # current event into an array that can be associated with our table for
    # viewing.
    proc traceViewerCallback { tag dg } {
	if {$traceviewer::obsperiod == -1} { return }
	incr traceviewer::eventnum
	
	set nrows [$traceviewer::table cget -rows]
	if { $traceviewer::eventnum > $nrows } {
	    $traceviewer::table configure -rows [expr $nrows+10]
	}
	
	set phrase [dl_tcllist $dg:params]
	scan $phrase "%s %s %s" one two three
	traceviewer::setTableValue table$traceviewer::obsperiod \
		$traceviewer::eventnum -1 [expr [dl_tcllist $dg:time]\
		- $traceviewer::lasttime]
	traceviewer::setTableValue table$traceviewer::obsperiod \
		$traceviewer::eventnum 0 [string trimright \
		[string trimleft $one \{] :]
	traceviewer::setTableValue table$traceviewer::obsperiod \
		$traceviewer::eventnum 1 [string trimleft [string trimleft $two "s"] "_"]
	traceviewer::setTableValue table$traceviewer::obsperiod \
		$traceviewer::eventnum 2 [string trimleft [string trimleft $three "s"] "_" ]
	set traceviewer::lasttime [dl_tcllist $dg:time]
    }
    
    # This is the callback associated with the reset event. It cleans out all
    # the table and trace arrays and resets the last event to 0
    proc traceResetCallback { tag dg } {
	if {[expr [dl_tcllist $dg:subtype] == 2]} {
	    set traceviewer::currentview 0
	    for {set i 0} {$i <= $traceviewer::obsperiod} {incr i 1} {
		traceviewer::cleanTableArray table$i
	    }
	    set traceviewer::obsperiod -1
	}
    }

    proc traceBeginObsCallback { tag dg } {
	incr traceviewer::obsperiod 1
	set traceviewer::eventnum -1
	$traceviewer::spinbox configure -range [list 0 \
		$traceviewer::obsperiod 1]
	traceviewer::setTableValue table$traceviewer::obsperiod -1 -1 "Time"
	traceviewer::setTableValue table$traceviewer::obsperiod -1 0 "Trace"
	traceviewer::setTableValue table$traceviewer::obsperiod -1 1 "To"
	traceviewer::setTableValue table$traceviewer::obsperiod -1 2 "From"
	set traceviewer::lasttime 0
	if {$traceviewer::pause == 0} {
	    set traceviewer::currentview $traceviewer::obsperiod
	    traceviewer::spinBoxCommand
	}
    }
    
    # This procedure inserts the input data into the input array at the
    # input locations.
    proc setTableValue { array x y info } {
	upvar #0 traceviewer::$array a
	set a($x,$y) $info
    }
    
    # This procedure clears and unsets the input array.
    proc cleanTableArray { array } {
	upvar #0 traceviewer::$array a
	foreach val [array names a] {
	    unset a($val)
	}
	unset a
    }

    # This procedure cleans up after the traceviewer, deleting the contents
    # of all the storage arrays and removing its callbacks from the qpcs
    # database.
    proc cleanUpProc { } {
	::qpcs::removeEventCallback traceviewer::traceViewerCallback 4
	::qpcs::removeEventCallback traceviewer::traceResetCallback 3
	::qpcs::removeEventCallback traceviewer::traceBeginObsCallback 19
	for {set i 0} {$i <= $traceviewer::obsperiod} {incr i 1} {
	    traceviewer::cleanTableArray table$i
	}
	set traceviewer::obsperiod -1
	set traceviewer::currentview 0
	set traceviewer::viewerexists 0
    }

    # This is the command called whenver the spinbox's value changes. It
    # changes the variable (and thus the data of) the table to reflect the
    # group numbered in the spinbox's entry
    proc spinBoxCommand { } {
	$traceviewer::table configure -variable \
		traceviewer::table$traceviewer::currentview
    } 

    # Called when the event viewer's pause button is pressed, this
    # changes the pause (viewing mode) state of the viewer and 
    # updates teh text on the pause button to reflects this change.
    proc pauseProc { } {
	if {$traceviewer::pause == 0} {
	    set traceviewer::pause 1
	    $traceviewer::pausebutton configure -text "Unpause"
	} else {
	    set traceviewer::pause 0
	    $traceviewer::pausebutton configure -text "Pause"
	}
    }

    # This procedure sets the state sytem trace value. The first test
    # is to see if the state system is running. The second tests whether
    # it is stopped.
    proc setTrace { } {
	if {![::qpcs::systemExists $traceviewer::server]} {
	    set traceviewer::trace 0
	    set traceviewer::lasttrace 0
	    return
	}
	if {[lindex [::qpcs::user_queryState $traceviewer::server] 1]} {
	    set traceviewer::trace [lindex [::qpcs::user_setTrace \
		    $traceviewer::server -1] 1]
	    return 
	}
	::qpcs::user_setTrace $traceviewer::server $traceviewer::trace
    }

}
}
