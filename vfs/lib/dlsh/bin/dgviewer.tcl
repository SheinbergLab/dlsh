package require dlsh
package require tkdnd
package require tkcon

set tablecount 0

# Allow program to shut itself down when no more dg viewers remain
proc shutdown {varname args} {
    upvar #0 $varname var
    if { $var == 0 } {
	exit
    }
}

proc showgroups { files } {
    foreach file $files { showgroup $file }
}

proc showgroup { file } {
    set g [dg_read $file]
    set table [dg_view $g]
    set toplevel $table
    tkdnd::drop_target register $toplevel *
    bind $toplevel <<Drop:DND_Files>> {showgroups %D}
    if { [winfo toplevel $toplevel] == $toplevel } {
	wm title $toplevel $file
    }
    cd [file dirname $file]
    bind $toplevel <Control-h> { tkcon show }

    # Keep running total of number of open viewers
    wm protocol $toplevel WM_DELETE_WINDOW \
	"incr ::tablecount -1; destroy $toplevel"
    incr ::tablecount 1

    trace add variable ::tablecount write "shutdown tablecount"
}

showgroups $argv
wm withdraw .
