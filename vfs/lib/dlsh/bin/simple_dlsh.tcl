package require tkcon
set ::tkcon::PRIV(root) .console  ;# optional---defaults to .tkcon
set ::tkcon::OPT(slaveeval) { package require dlsh; \
    dl_noOp; package require loaddata; 
    set datadir c:/home/sheinb/projects/encounter/data; }
pack [button .b -text Console -command { tkcon show }]
