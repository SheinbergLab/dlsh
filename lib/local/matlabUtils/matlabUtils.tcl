package provide matlabUtils 0.1


namespace eval ::mlu {
    proc checkMLOpen {} {
	#this is a little more complicated than it needs to be
	#but if you have loaded Matlab, opened and closed, than
	#typing something like m_eval or m_put doesn't return an error
	#even though nothing is executing, so this is a work around.
	#Maybe there is something simpler
	if {[catch "m_eval {a = mean(1:3);}" cout] } {
	    #gets the case where load_Matlab has never been invoked
	    return 0
	} elseif  {[catch "m_get a" cout]} {
	    #gets the case where load_Matlab has been invoked,
	    #but there is not open matlab command window
	    return 0 
	} else {
	    return 1
	}
    }
    
    proc openMatlab { } {
	load_Matlab
	m_open
	return 1
    }
}

puts "Namespace for matlabUtils is ::mlu";update