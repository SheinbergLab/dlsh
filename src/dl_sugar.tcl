# dl_sugar.tcl
#   Base syntactic sugar for dlsh: thin Tcl-level helpers layered on the C
#   dl_* primitives. This file is EMBEDDED into libdlsh at build time (see
#   cmake/EmbedTcl.cmake) and Tcl_Eval'd from Dl_Init -- after every dl_*
#   command it depends on is registered -- so these procs are available the
#   instant the package loads, with no dependence on the VFS lib path /
#   auto_path.
#
#   Keep these THIN. If an operation is already a single C dl_* call, use that
#   call directly rather than wrapping it here. The value of this file is
#   naming the operations experiments actually reach for, so they are
#   discoverable and hard to get subtly wrong.

# dl_randchooseLists l n
#
#   Draw n unique ELEMENTS from each sublist of l, independently per row.
#
#   dl_randchoose returns unique INDICES (n_i drawn from [0,m_i) for each row);
#   this maps those indices back onto each row's own candidates, so every trial
#   can have a different candidate set. Rows may differ in length (ragged).
#
#   n is either a scalar (broadcast to every row) or a list of per-row counts;
#   dl_randchoose handles both, so nothing needs replicating here.
#
#       dl_randchooseLists [dl_llist "0 2 4 6" "1 3 5 7"] 2   -> {6 0} {1 7}
#
#   Companion to dl_shuffleLists: shuffleLists permutes a whole row,
#   randchooseLists takes n of it. Sampling is uniform in both the chosen set
#   and its order, and is seeded by dl_srand.
proc dl_randchooseLists { l n } {
    if { [dl_datatype $l] != "list" } {
        error "dl_randchooseLists: list must be a list of lists"
    }
    dl_return [dl_choose $l [dl_randchoose [dl_lengths $l] $n]]
}
