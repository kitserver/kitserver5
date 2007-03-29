CC=cl
LINK=link
RC=rc

CFLAGS=/nologo /GX $(EXTRA_CFLAGS) /DUSE_HASHMAPS
LFLAGS=/NOLOGO /NODEFAULTLIB:libcmt.lib
LIBS=user32.lib gdi32.lib advapi32.lib
LIBSDLL=pngdib.obj libpng.lib zdll.lib $(LIBS)

INCPNGDIB=soft\pngdib
LPPNGDIB=soft\pngdib 
LPLIBPNG=soft\libpng-1.2.8-lib\lib
LPZLIB=soft\zlib123-dll\lib

all: apps
	if exist output (rd /S /Q output)
	mkdir output
	copy setup.exe output
	
all_apps: apps
	if exist output (rd /S /Q output)
	mkdir output
	copy setup.exe output

apps: setup.exe

release:
	$(MAKE) EXTRA_CFLAGS=/DMYDLL_RELEASE_BUILD

setup: setup.exe

imageutil.obj: imageutil.cpp imageutil.h
detect.obj: detect.cpp detect.h

setupgui.obj: setupgui.cpp setupgui.h
setup.obj: setup.cpp setup.h setupgui.h
setup.exe: setup.obj detect.obj setupgui.obj imageutil.obj
	$(LINK) $(LFLAGS) /out:setup.exe setup.obj detect.obj setupgui.obj imageutil.obj $(LIBS)
	
.cpp.obj:
	$(CC) $(CFLAGS) -c $(INC) /I $(INCPNGDIB) $<

clean:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe
	(cd soft\pngdib && $(MAKE) clean)
	if exist output (rd /S /Q output)

clean-all:
	del /Q /F *.exp *.lib *.obj *.res *.dll *.exe *.log *~
	(cd soft\pngdib && $(MAKE) clean-all)
	if exist output (rd /S /Q output)
	
