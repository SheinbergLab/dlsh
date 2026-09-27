CC               = cl
CFLAGS           = -W3 -O2 -DCRTAPI1=_cdecl -DCRTAPI2=_cdecl -nologo \
		   -D_WINNT -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 /J
CVARS            = -DWIN32 -D_WIN32 -D_MT -D_DLL -MD -DUSE_TCL_STUBS -DUSE_TK_STUBS -DUSE_INTERP_RESULT

INCLUDES	 = -I../src -I../src/lablib -Ic:/usr/local/include -Ic:/usr/local/include/hpdf

LINK             = link
LINKDEBUG        = -debug:full -debugtype:cv
LDFLAGS          = /NODEFAULTLIB:LIBCMT /INCREMENTAL:NO /PDB:NONE \
			/RELEASE /NOLOGO /MACHINE:X64 -dll /VERBOSE:LIB \
			/LIBPATH:c:\\usr\\local\\lib
TCLLIBS          = tclstub86.lib 
ZLIB		 = zlibstatic.lib
LZ4LIB           = liblz4_static64.lib
PDFLIB		 = hpdf.lib
LABLIB           = lablib64.lib
JANSSON          = jansson.lib
OBJ	= .obj
EXE	= .exe

DLLS = dlsh64.dll
OBJECTS =   dfana.obj dlarith.obj dfevt.obj dmana.obj \
	tcl_df.obj tcl_dl.obj tcl_dm.obj tcl_dlg.obj \
	base_cg.obj dgjson.obj 

LABLIB_OBJECTS = gbufutl$(OBJ) gbuf$(OBJ) cg_ps$(OBJ) \
	axes$(OBJ) cgraph$(OBJ) \
	timer$(OBJ) utilc_unix$(OBJ) randvars$(OBJ) prmutil$(OBJ) \
	dfutils$(OBJ) df$(OBJ) dynio$(OBJ) rawapi$(OBJ) lodepng$(OBJ) \
	lz4utils$(OBJ) dslog$(OBJ) 

all: $(DLLS)

dlsh64.dll: $(OBJECTS) $(LABLIB_OBJECTS) dlsh_pkg.obj dlsh.def
	$(LINK) $(LDFLAGS) -out:dlsh64.dll -def:dlsh.def \
	$(OBJECTS) $(LABLIB_OBJECTS)  \
	dlsh_pkg.obj \
	$(TCLLIBS) $(ZLIB) $(LZ4LIB) $(JANSSON) $(PDFLIB) \
	GDI32.lib USER32.lib ADVAPI32.LIB
	copy dlsh64.dll c:\\usr\\local\\lib\\dlsh
	copy dlsh64.lib c:\\usr\\local\\lib

pdfgen$(OBJ): ../src/lablib/pdfgen.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

gbufutl$(OBJ): ../src/lablib/gbufutl.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

gbuf$(OBJ): ../src/lablib/gbuf.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

cg_ps$(OBJ): ../src/lablib/cg_ps.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

axes$(OBJ): ../src/lablib/axes.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

cgraph$(OBJ): ../src/lablib/cgraph.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

timer$(OBJ): ../src/lablib/timer.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

utilc_unix$(OBJ): ../src/lablib/utilc_unix.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

randvars$(OBJ): ../src/lablib/randvars.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

prmutil$(OBJ): ../src/lablib/prmutil.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

dfutils$(OBJ): ../src/lablib/dfutils.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

df$(OBJ): ../src/lablib/df.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

dynio$(OBJ): ../src/lablib/dynio.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

rawapi$(OBJ): ../src/lablib/rawapi.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

lodepng$(OBJ): ../src/lablib/lodepng.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

lz4utils$(OBJ): ../src/lablib/lz4utils.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

dslog$(OBJ): ../src/lablib/dslog.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

%$(OBJ): ../src/%.c
	$(CC) -c $(CFLAGS) $(CVARS) $(INCLUDES) $<

