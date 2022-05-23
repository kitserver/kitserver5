CC=cl
LINK=link
RC=rc

CFLAGS=/nologo /MT /EHsc /wd4731 /wd4477 /D_WINNT_WIN32=0x501 /DKS_JMP_SHIFT=4 $(EXTRA_CFLAGS) /DUSE_HASHMAPS /D_WIN32
LFLAGS=/NOLOGO /NODEFAULTLIB:libci.lib /LIBPATH:soft\dxsdk81\lib
LIBS=gdi32.lib user32.lib advapi32.lib
LIBSDLL=pngdib.obj libpng.lib zdll.lib $(LIBS)

INCLUDE=$(INCLUDE);soft\dxsdk81\include
LIB=$(LIB);soft\dxsdk81\lib

INCPNGDIB=soft\pngdib
INCCURL=soft\curl\include
INCARCHIVE=soft\libarchive-2.4.12-1\include
LPPNGDIB=soft\pngdib 
LPLIBPNG=soft\libpng-1.2.8-lib\lib
LPZLIB=soft\zlib123-dll\lib
LPARCHIVE=soft\libarchive-2.4.12-1\lib
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

$(LPPNGDIB)\pngdib.obj:
	(cd soft\pngdib && nmake EXTRA_CFLAGS=$(EXTRA_CFLAGS))

dlls: kload.dll kserv.dll bserv.dll fserv.dll bootserv.dll stadium.dll lodmixer.dll dxtools.dll clock.dll gameplay.dll referees.dll refksrv.dll camerazoomer.dll network.dll speeder.dll #afs2fs.dll #kserv-shpal.dll
apps: setup.exe lodcfg.exe #filedec.exe diag.exe afsexport.exe afswalk.exe afstest.exe

!if "$(debug)"=="1"
EXTRA_CFLAGS=/DDEBUG
!else
EXTRA_CFLAGS=/DMYDLL_RELEASE_BUILD
!endif

setup: setup.exe

imageutil.obj: imageutil.cpp imageutil.h
detect.obj: detect.cpp detect.h
d3dfont.obj: d3dfont.cpp d3dfont.h
hook.obj: hook.cpp hook.h

kload.lib: kload.obj detect.obj d3dfont.obj imageutil.obj log.obj kload_config.obj hook.obj  hooklib.obj dxutil.obj afsreplace.obj apihijack.obj
kload.dll: kload.lib kload.res
	$(LINK) $(LFLAGS) /out:kload.dll /DLL kload.obj d3dfont.obj dxutil.obj detect.obj imageutil.obj log.obj kload_config.obj hook.obj hooklib.obj afsreplace.obj apihijack.obj kload.res $(LIBS) d3dx8.lib winmm.lib
kload.obj: kload.cpp kload.h kload_config.h hook.h manage.h
detect.obj: detect.cpp detect.h
kload.res: 
	$(RC) -r -fo kload.res kload.rc

kserv.lib: kserv.obj gdb.obj kserv_config.obj dxutil.obj crc32.obj afsreader.obj $(LPPNGDIB)\pngdib.obj
kserv.dll: kserv.lib kserv.res
	$(LINK) $(LFLAGS) /out:kserv.dll /DLL kserv.obj gdb.obj kserv_config.obj crc32.obj afsreader.obj dxutil.obj kserv.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) kload.lib
kserv.obj: kserv.cpp kserv.h shared.h kserv_config.h gdb.h lodcfg.h
kserv.res: 
	$(RC) -r -fo kserv.res kserv.rc

kserv-shpal.lib: kserv-shpal.obj gdb.obj kserv_config.obj dxutil.obj crc32.obj afsreader.obj $(LPPNGDIB)\pngdib.obj
kserv-shpal.dll: kserv-shpal.lib kserv-shpal.res
	$(LINK) $(LFLAGS) /out:kserv-shpal.dll /DLL kserv-shpal.obj gdb.obj kserv_config.obj crc32.obj afsreader.obj dxutil.obj kserv-shpal.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) kload.lib
kserv-shpal.obj: kserv-shpal.cpp kserv-shpal.h shared.h kserv_config.h gdb.h lodcfg.h
kserv-shpal.res: 
	$(RC) -r -fo kserv-shpal.res kserv-shpal.rc

bserv.lib: bserv.obj d3dfont.obj $(LPPNGDIB)\pngdib.obj crc32.obj
bserv.dll: bserv.lib bserv.res
	$(LINK) $(LFLAGS) /out:bserv.dll /DLL bserv.obj d3dfont.obj crc32.obj kload.lib bserv.res $(LIBSDLL) d3dx8.lib winmm.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB) zdll.lib
bserv.obj: bserv.cpp bserv.h
bserv.res: 
	$(RC) -r -fo bserv.res bserv.rc
	
fserv.lib: fserv.obj fserv_config.obj
fserv.dll: fserv.lib fserv.res crc32.obj
	$(LINK) $(LFLAGS) /out:fserv.dll /DLL fserv.obj fserv_config.obj crc32.obj kload.lib fserv.res d3dx8.lib $(LIBS)
fserv.obj: fserv.cpp fserv.h
fserv_config.obj: fserv_config.cpp fserv_config.h

fserv.res: 
	$(RC) -r -fo fserv.res fserv.rc

bootserv.lib: bootserv.obj
bootserv.dll: bootserv.lib bootserv.res
	$(LINK) $(LFLAGS) /out:bootserv.dll /DLL bootserv.obj kload.lib d3dx8.lib bootserv.res $(LIBS)
bootserv.obj: bootserv.cpp bootserv.h

bootserv.res: 
	$(RC) -r -fo bootserv.res bootserv.rc
	
	
stadium.lib: stadium.obj crc32.obj afsreader.obj hooklib.obj
stadium.dll: stadium.lib stadium.res
	$(LINK) $(LFLAGS) /out:stadium.dll /DLL stadium.obj crc32.obj afsreader.obj kload.lib hooklib.obj stadium.res $(LIBSDLL) d3dx8.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB)
stadium.obj: stadium.cpp stadium.h

stadium.res: 
	$(RC) -r -fo stadium.res stadium.rc

afs2fs.lib: afs2fs.obj crc32.obj afsreader.obj hooklib.obj log.obj
afs2fs.dll: afs2fs.lib afs2fs.res
	$(LINK) $(LFLAGS) /out:afs2fs.dll /DLL afs2fs.obj crc32.obj afsreader.obj kload.lib hooklib.obj afs2fs.res $(LIBSDLL) d3dx8.lib /LIBPATH:$(LPPNGDIB) /LIBPATH:$(LPLIBPNG) /LIBPATH:$(LPZLIB)
afs2fs.obj: afs2fs.cpp afs2fs.h afs2fs_addr.h

afs2fs.res: 
	$(RC) -r -fo afs2fs.res afs2fs.rc

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

gameplay.lib: gameplay.obj
gameplay.dll: gameplay.lib gameplay.res
	$(LINK) $(LFLAGS) /out:gameplay.dll /DLL gameplay.obj kload.lib DXSDK\lib\d3dx8.lib gameplay.res $(LIBS)
gameplay.obj: gameplay.cpp gameplay.h

referees.lib: referees.obj
referees.dll: referees.lib referees.res
	$(LINK) $(LFLAGS) /out:referees.dll /DLL referees.obj kload.lib DXSDK\lib\d3dx8.lib referees.res $(LIBS)
referees.obj: referees.cpp referees.h


camerazoomer.lib: camerazoomer.obj
camerazoomer.dll: camerazoomer.lib camerazoomer.res
	$(LINK) $(LFLAGS) /out:camerazoomer.dll /DLL camerazoomer.obj kload.lib d3dx8.lib camerazoomer.res $(LIBS)
camerazoomer.obj: camerazoomer.cpp camerazoomer.h

refksrv.lib: refksrv.obj
refksrv.dll: refksrv.lib refksrv.res
	$(LINK) $(LFLAGS) /out:refksrv.dll /DLL refksrv.obj kload.lib d3dx8.lib refksrv.res $(LIBS)
refksrv.obj: refksrv.cpp refksrv.h


network.lib: network.obj md5.obj
network.dll: network.lib network.res
	$(LINK) $(LFLAGS) /out:network.dll /DLL network.obj md5.obj kload.lib d3dx8.lib libcurl.lib libarchive.lib network.res $(LIBS) /LIBPATH:$(LPCURL) /LIBPATH:$(LPARCHIVE)
network.obj: network.cpp network.h
md5.obj: md5.c md5.h

speeder.lib: speeder.obj apihijack.obj
speeder.dll: speeder.lib speeder.res
	$(LINK) $(LFLAGS) /out:speeder.dll /DLL speeder.obj kload.lib apihijack.obj speeder.res $(LIBS) 
speeder.obj: speeder.cpp speeder.h apihijack.h

setupgui.obj: setupgui.cpp setupgui.h
setup.exe: setup.obj detect.obj setupgui.obj imageutil.obj setup.res
	$(LINK) $(LFLAGS) /out:setup.exe setup.obj detect.obj setupgui.obj imageutil.obj setup.res $(LIBS)
setup.obj: setup.cpp setup.h setupgui.h
setup.res: setup.rc
	$(RC) -r -fo setup.res setup.rc

lodcfg.exe: lodcfg.obj detect.obj lodcfgui.obj imageutil.obj
	$(LINK) $(LFLAGS) /out:lodcfg.exe lodcfg.obj detect.obj lodcfgui.obj imageutil.obj $(LIBS)
lodcfg.obj: lodcfg.cpp lodcfg.h
lodcfgui.obj: lodcfgui.cpp lodcfgui.h

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
	$(CC) $(CFLAGS) -c $(INC) /I $(INCPNGDIB) /I $(INCCURL) /I $(INCARCHIVE) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe
	(cd soft\pngdib && $(MAKE) clean)
	if exist output (rd /S /Q output)

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe *.log *~
	(cd soft\pngdib && $(MAKE) clean-all)
	if exist output (rd /S /Q output)
	
