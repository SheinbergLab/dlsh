set dlsh_library [file join [zipfs root] dlsh/lib/dlsh]
set files "dlbasic dlcompat dlinput dlmodules dlsort dmbasic events fitfuncs \
           plot regress stats"
foreach f $files {
    source [file join $dlsh_library $f.tcl]
}
source [file join $dlsh_library dlshrc]
