CC=cl
LINK=link
RC=rc

CFLAGS=/nologo /GX $(EXTRA_CFLAGS) /DUSE_HASHMAPS
LFLAGS=/NOLOGO /NODEFAULTLIB:libcmt.lib
LIBS=user32.lib gdi32.lib advapi32.lib
LIBSDLL=pngdib.obj libpng.lib zdll.lib $(LIBS)

INCPNGDIB=soft\pngdib
INCCURL=soft\curl\include
LPPNGDIB=soft\pngdib 
LPLIBPNG=soft\libpng-1.2.8-lib\lib
LPZLIB=soft\zlib123-dll\lib
LPCURL=soft\curl\lib

all: dlls apps
	if exist output (rd /S /Q output)
	mkdir output
	copy *.exe output
	copy *.dll output
	
all_apps: apps
	if exist output (rd /S /Q output)
	mkdir output
	copy *.exe output

pngdib.obj:
	(cd soft\pngdib && nmake EXTRA_CFLAGS=$(EXTRA_CFLAGS))

dlls: kload.dll kserv.dll kserv-shpal.dll bserv.dll bserv_preview.dll fserv.dll bootserv.dll stadium.dll lodmixer.dll dxtools.dll clock.dll network.dll
apps: setup.exe lodcfg.exe filedec.exe diag.exe afsexport.exe afswalk.exe afstest.exe

release:
	$(MAKE) EXTRA_CFLAGS=/DMYDLL_RELEASE_BUILD

setup: setup.exe

imageutil.obj: imageutil.cpp imageutil.h
detect.obj: detect.cpp detect.h

kload.lib: kload.obj detect.obj d3dfont.obj imageutil.obj log.obj kload_config.obj hook.obj  hooklib.obj dxutil.obj afsreplace.obj apihijack.obj
kload.dll: kload.lib kload.res
	$(LINK) $(LFLAGS) /out:kload.dll /DLL kload.obj d3dfont.obj dxutil.obj detect.obj imageutil.obj log.obj kload_config.obj hook.obj hooklib.obj afsreplace.obj apihijack.obj kload.res $(LIBS) d3dx8.lib winmm.lib
kload.obj: kload.cpp kload.h
detect.obj: detect.cpp detect.h
kload.res: 
	$(RC) -r -fo kload.res kload.rc

kserv.lib: kserv.obj gdb.obj kserv_config.obj dxutil.obj crc32.obj afsreader.obj pngdib.obj
kserv.dll: kserv.lib kserv.res
	$(LINK) $(LFLAGS) /out:kserv.dll /DLL kserv.obj gdb.obj kserv_config.obj crc32.obj afsreader.obj dxutil.obj kserv.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) kload.lib
kserv.obj: kserv.cpp kserv.h shared.h kserv_config.h gdb.h lodcfg.h
kserv.res: 
	$(RC) -r -fo kserv.res kserv.rc

kserv-shpal.lib: kserv-shpal.obj gdb.obj kserv_config.obj dxutil.obj crc32.obj afsreader.obj pngdib.obj
kserv-shpal.dll: kserv-shpal.lib kserv-shpal.res
	$(LINK) $(LFLAGS) /out:kserv-shpal.dll /DLL kserv-shpal.obj gdb.obj kserv_config.obj crc32.obj afsreader.obj dxutil.obj kserv-shpal.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) kload.lib
kserv-shpal.obj: kserv-shpal.cpp kserv-shpal.h shared.h kserv_config.h gdb.h lodcfg.h
kserv-shpal.res: 
	$(RC) -r -fo kserv-shpal.res kserv-shpal.rc

bserv.lib: bserv.obj d3dfont.obj pngdib.obj
bserv.dll: bserv.lib bserv.res
	$(LINK) $(LFLAGS) /out:bserv.dll /DLL bserv.obj d3dfont.obj kload.lib bserv.res $(LIBSDLL) /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB)
bserv.obj: bserv.cpp bserv.h

bserv.res: 
	$(RC) -r -fo bserv.res bserv.rc
	
bserv_preview.lib: bserv_preview.obj d3dfont.obj pngdib.obj crc32.obj
bserv_preview.dll: bserv_preview.lib bserv_preview.res
	$(LINK) $(LFLAGS) /out:bserv_preview.dll /DLL bserv_preview.obj d3dfont.obj crc32.obj kload.lib bserv_preview.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) zdll.lib
bserv_preview.obj: bserv_preview.cpp bserv.h

bserv_preview.res: 
	$(RC) -r -fo bserv_preview.res bserv_preview.rc
	
fserv.lib: fserv.obj
fserv.dll: fserv.lib fserv.res
	$(LINK) $(LFLAGS) /out:fserv.dll /DLL fserv.obj kload.lib fserv.res $(LIBS)
fserv.obj: fserv.cpp fserv.h

fserv.res: 
	$(RC) -r -fo fserv.res fserv.rc

bootserv.lib: bootserv.obj
bootserv.dll: bootserv.lib bootserv.res
	$(LINK) $(LFLAGS) /out:bootserv.dll /DLL bootserv.obj kload.lib d3dx8.lib bootserv.res $(LIBS)
bootserv.obj: bootserv.cpp bootserv.h

bootserv.res: 
	$(RC) -r -fo bootserv.res bootserv.rc
	
	
stadium.lib: stadium.obj crc32.obj afsreader.obj hooklib.obj pngdib.obj
stadium.dll: stadium.lib stadium.res
	$(LINK) $(LFLAGS) /out:stadium.dll /DLL stadium.obj crc32.obj afsreader.obj kload.lib hooklib.obj stadium.res $(LIBSDLL) d3dx8.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB)
stadium.obj: stadium.cpp stadium.h

stadium.res: 
	$(RC) -r -fo stadium.res stadium.rc

lodmixer.lib: lodmixer.obj
lodmixer.dll: lodmixer.lib lodmixer.res
	$(LINK) $(LFLAGS) /out:lodmixer.dll /DLL lodmixer.obj kload.lib lodmixer.res $(LIBS)
lodmixer.obj: lodmixer.cpp lodmixer.h

lodmixer.res: 
	$(RC) -r -fo lodmixer.res lodmixer.rc
	
dxtools.lib: dxtools.obj
dxtools.dll: dxtools.lib dxtools.res
	$(LINK) $(LFLAGS) /out:dxtools.dll /DLL dxtools.obj kload.lib dxtools.res $(LIBS)
dxtools.obj: dxtools.cpp dxtools.h

dxtools.res: 
	$(RC) -r -fo dxtools.res dxtools.rc
	
savegames.lib: savegames.obj
savegames.dll: savegames.lib savegames.res
	$(LINK) $(LFLAGS) /out:savegames.dll /DLL savegames.obj kload.lib savegames.res $(LIBS)
savegames.obj: savegames.cpp savegames.h SGF.h

savegames.res: 
	rc -r -fo savegames.res savegames.rc

clock.lib: clock.obj
clock.dll: clock.lib clock.res
	$(LINK) $(LFLAGS) /out:clock.dll /DLL clock.obj kload.lib d3dx8.lib clock.res $(LIBS)
clock.obj: clock.cpp clock.h

network.lib: network.obj
network.dll: network.lib network.res
	$(LINK) $(LFLAGS) /out:network.dll /DLL network.obj kload.lib d3dx8.lib libcurl.lib network.res $(LIBS) /LIBPATH:$(LPCURL)
network.obj: network.cpp network.h

setupgui.obj: setupgui.cpp setupgui.h
setup.exe: setup.obj detect.obj setupgui.obj imageutil.obj
	$(LINK) $(LFLAGS) /out:setup.exe setup.obj detect.obj setupgui.obj imageutil.obj $(LIBS)
setup.obj: setup.cpp setup.h setupgui.h

lodcfg.exe: lodcfg.obj detect.obj lodcfgui.obj imageutil.obj
	$(LINK) $(LFLAGS) /out:lodcfg.exe lodcfg.obj detect.obj lodcfgui.obj imageutil.obj $(LIBS)

afstest.obj: afstest.cpp afsreader.h
afstest.exe: afstest.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afstest.exe afstest.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIBS)

afswalk.obj: afswalk.cpp afsreader.h
afswalk.exe: afswalk.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afswalk.exe afswalk.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIBS)

afsexport.obj: afsexport.cpp afsreader.h
afsexport.exe: afsexport.obj afsreader.obj crc32.obj alog.obj aconfig.obj
	$(LINK) $(LFLAGS) /out:afsexport.exe afsexport.obj afsreader.obj crc32.obj alog.obj aconfig.obj $(LIBS)
	
stadimp.obj: stadimp.cpp
stadimp.exe: stadimp.obj
	$(LINK) $(LFLAGS) /out:stadimp.exe stadimp.obj $(LIBS)

filedec.obj: filedec.cpp
filedec.exe: filedec.obj
	$(LINK) $(LFLAGS) /out:filedec.exe filedec.obj $(LIBS)
	
diaggui.obj: diaggui.cpp diaggui.h
diag.exe: diag.obj diaggui.obj
	$(LINK) $(LFLAGS) /out:diag.exe diag.obj diaggui.obj $(LIBS)
diag.obj: diag.cpp diag.h diaggui.h

	
.cpp.obj:
	$(CC) $(CFLAGS) -c $(INC) /I $(INCPNGDIB) /I $(INCCURL) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe
	(cd soft\pngdib && $(MAKE) clean)
	if exist output (rd /S /Q output)

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe *.log *~
	(cd soft\pngdib && $(MAKE) clean-all)
	if exist output (rd /S /Q output)
	
