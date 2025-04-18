By default this will install the following:

# libraries
/usr/local/lib/libdlsh-static.a
/usr/local/lib/libdlsh.dylib
/usr/local/lib/libdg.a

# headers
/usr/local/include/cgraph.h
/usr/local/include/df.h
/usr/local/include/dfana.h
/usr/local/include/dynio.h
/usr/local/include/gbuf.h
/usr/local/include/tcl_dl.h

After installation you can check what was installed:

pkgutil --pkgs | grep sheinberglab
pkgutil --files org.sheinberglab.dlsh
pkgutil --files org.sheinberglab.dg

To uninstall:

sudo rm /usr/local/lib/libdlsh-static.a /usr/local/lib/libdlsh.dylib /usr/local/lib/libdg.a
sudo rm /usr/local/include/cgraph.h /usr/local/include/df.h /usr/local/include/dfana.h /usr/local/include/dynio.h /usr/local/include/gbuf.h /usr/local/include/tcl_dl.h
sudo pkgutil --forget org.sheinberglab.dlsh
sudo pkgutil --forget org.sheinberglab.dg
