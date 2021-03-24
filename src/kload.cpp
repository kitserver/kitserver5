/* KitServer Loader */

#include <windows.h>
#define _COMPILING_KLOAD
#include <stdio.h>

#include "manage.h"
#include "kload_config.h"
#include "log.h"
#include "hook.h"
#include "imageutil.h"
#include "detect.h"
#include "kload.h"
#include "apihijack.h"

#include <map>

KMOD k_kload={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
KLOAD_CONFIG g_config={0,NULL,FALSE,FALSE,0,1.0f};
PESINFO g_pesinfo={NULL,NULL,NULL,NULL,NULL,NULL,DEFAULT_GDB_DIR,-1,NULL,0,0,0.0,0.0};

SAVEGAMEFUNCS* SavegameFuncs;
SAVEGAMEEXPFUNCS SavegameExpFuncs[numSaveGameExpModules];

std::map<DWORD,THREEDWORDS> g_SpecialAfsIds;
std::map<DWORD,THREEDWORDS>::iterator g_SpecialAfsIdsIterator;
DWORD currSpecialAfsId=0;

// global hook manager
static hook_manager _hook_manager;
DWORD lastCallSite=0;

extern char* GAME[];

#ifndef MYDLL_RELEASE_BUILD
#define KLOG(mydir,lib) { char klogFile[BUFLEN];\
    ZeroMemory(klogFile, BUFLEN);\
    sprintf(klogFile, "%s\\kload.log", mydir);\
    FILE* klog = fopen(klogFile, "at");\
    if (klog) { fprintf(klog, "Loading library: %s\n", lib); }\
    fclose(klog); }
#else
#define KLOG(mydir,lib)
#endif

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void SetPESInfo();

#ifndef __in
#define __in
#endif
#ifndef __in_opt
#define __in_opt
#endif

KEXPORT HANDLE WINAPI Override_CreateFileW(
  __in      LPCWSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile);

KEXPORT HANDLE WINAPI Override_CreateFileA(
  __in      LPCSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile);

KEXPORT BOOL WINAPI Override_CloseHandle(
  __in  HANDLE hObject);

/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	DWORD wbytes, procId; 

    char tmp[BUFLEN];

	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst=hInstance;
		
		SetPESInfo();
		
		OpenLog(g_pesinfo.logName);
		
		// read configuration
		char cfgFile[BUFLEN];
		ZeroMemory(cfgFile, BUFLEN);
		lstrcpy(cfgFile, g_pesinfo.mydir); 
		lstrcat(cfgFile, CONFIG_FILE);

		//Enable logging at the beginning
		k_kload.debug=1;
		Log(&k_kload,"Log started.");
		RegisterKModule(&k_kload);
		
		ReadConfig(&g_config, cfgFile);
		
		// adjust gdbDir, if it is specified as relative path
		if (g_pesinfo.gdbDir[0] == '.')
		{
			// it's a relative path. Therefore do it relative to mydir
			char temp[BUFLEN];
			ZeroMemory(temp, BUFLEN);
			lstrcpy(temp,g_pesinfo.mydir); 
			lstrcat(temp, g_pesinfo.gdbDir);
			delete g_pesinfo.gdbDir;
			g_pesinfo.gdbDir=new char[strlen(temp)+1];
			ZeroMemory(g_pesinfo.gdbDir, strlen(temp)+1);
			lstrcpy(g_pesinfo.gdbDir, temp);
		}
		
		//if debugging should'nt be enabled, delete log file
		if (k_kload.debug<1) {
			CloseLog();
			DeleteFile(g_pesinfo.logName);
		} else {
			strcpy(tmp,GetPESInfo()->mydir);
			strcat(tmp,"mlog.tmp");
			OpenMLog(200,tmp);
			TRACE(&k_kload,"Opened memory log.");
		};		
		
		LogWithString(&k_kload,"Game version: %s", GAME[g_pesinfo.GameVersion]);
		InitAddresses(g_pesinfo.GameVersion);
		
		ZeroMemory(SavegameExpFuncs,sizeof(SavegameExpFuncs));
		
		_hook_manager.SetCallHandler(MasterCallFirst);
		
        // load libs
        for (int i=0;i<g_config.numDLLs;i++) {
        	if (g_config.dllnames[i]==NULL) continue;
        	LogWithString(&k_kload,"Loading module \"%s\" ...",g_config.dllnames[i]);
        	if (LoadLibrary(g_config.dllnames[i])==NULL)
        		strcpy(tmp," NOT");
        	else
        		strcpy(tmp,"\0");
			LogWithString(&k_kload,"... was%s successful!",tmp);
        };
		
		HookDirect3DCreate8();

        // The hooking of game routines should happen later:
        // when the game calls Direct3DCreate8. This is a requirement
        // for SECUROM-encrypted executables (such as WE9.exe, WE9LEK.exe)        
    }

	else if (dwReason == DLL_PROCESS_DETACH)
	{
		// Release the device
        if (GetActiveDevice()) {
            GetActiveDevice()->Release();
        }
        
        //UnhookReadFile();
        //UnhookOthers();
        //UnhookKeyb();
		
		TRACE(&k_kload,"Closing memory log.");
		strcpy(tmp,GetPESInfo()->mydir);
		strcat(tmp,"mlog.tmp");
		CloseMLog(tmp);
		
		g_SpecialAfsIds.clear();
		
		/* close specific log file */
		Log(&k_kload,"Closing log.");
		CloseLog();
	}

	return TRUE;    /* ok */
}

void SetPESInfo()
{
	char mydir[BUFLEN];
	char processfile[BUFLEN];
	char *shortProcessfile;
	char shortProcessfileNoExt[BUFLEN];
	char logName[BUFLEN];
    char tmp[BUFLEN];
	
	/* determine my directory */
	ZeroMemory(tmp, BUFLEN);
    GetModuleFileName(hInst, tmp, BUFLEN);
	ZeroMemory(mydir, BUFLEN);
	GetLongPathName(tmp, mydir, BUFLEN);
	char *q = mydir + lstrlen(mydir);
	while ((q != mydir) && (*q != '\\')) { *q = '\0'; q--; }

	g_pesinfo.mydir=new char[strlen(mydir)+1];
	strcpy(g_pesinfo.mydir,mydir);
	
	g_pesinfo.hProc = GetModuleHandle(NULL);
	
	ZeroMemory(tmp, BUFLEN);
    GetModuleFileName(NULL, tmp, BUFLEN);
	ZeroMemory(processfile, BUFLEN);
	GetLongPathName(tmp, processfile, BUFLEN);
	
	g_pesinfo.processfile=new char[strlen(processfile)+1];
	strcpy(g_pesinfo.processfile,processfile);
	
	char *q1 = processfile + lstrlen(processfile);
	while ((q1 != processfile) && (*q1 != '\\')) { *q1 = '\0'; q1--; }
	
	g_pesinfo.pesdir=new char[strlen(processfile)+1];
	strcpy(g_pesinfo.pesdir,processfile);

	ZeroMemory(tmp, BUFLEN);
    GetModuleFileName(NULL, tmp, BUFLEN);
	ZeroMemory(processfile, BUFLEN);
	GetLongPathName(tmp, processfile, BUFLEN);

	char* zero = processfile + lstrlen(processfile);
	char* p = zero; while ((p != processfile) && (*p != '\\')) p--;
	if (*p == '\\') p++;
	shortProcessfile = p;
	
	g_pesinfo.shortProcessfile=new char[strlen(shortProcessfile)+1];
	strcpy(g_pesinfo.shortProcessfile,shortProcessfile);
		
	// save short filename without ".exe" extension.
	ZeroMemory(shortProcessfileNoExt, BUFLEN);
	char* ext = shortProcessfile + lstrlen(shortProcessfile) - 4;
	if (lstrcmpi(ext, ".exe")==0) {
		memcpy(shortProcessfileNoExt, shortProcessfile, ext - shortProcessfile); 
	}
	else {
		lstrcpy(shortProcessfileNoExt, shortProcessfile);
	}
	
	g_pesinfo.shortProcessfileNoExt=new char[strlen(shortProcessfileNoExt)+1];
	strcpy(g_pesinfo.shortProcessfileNoExt,shortProcessfileNoExt);
	
	/* open log file, specific for this process */
	ZeroMemory(logName, BUFLEN);
	lstrcpy(logName, mydir);
	lstrcat(logName, shortProcessfileNoExt); 
	lstrcat(logName, ".log");
	
	g_pesinfo.logName=new char[strlen(logName)+1];
	strcpy(g_pesinfo.logName,logName);
	
	g_pesinfo.GameVersion=GetGameVersion();
	
	return;
};

KEXPORT PESINFO* GetPESInfo()
{
	return &g_pesinfo;
};

KEXPORT void RegisterKModule(KMOD *k)
{
	LogWithTwoStrings(&k_kload,"Registering module %s (\"%s\")",k->NameShort,k->NameLong);
	return;
};

KEXPORT DWORD SetDebugLevel(DWORD ModDebugLevel)
{
	if (k_kload.debug<1)
		return 0;
	else if (k_kload.debug==255)
		return 255;
	else
		return ModDebugLevel;
};

//for savegame manager module
KEXPORT void SetSavegameExpFuncs(BYTE module, BYTE type, DWORD addr)
{
	if (module>=numSaveGameExpModules) return;
	if (type>=SGEF_LAST) return;
	
	switch (type) {
	case SGEF_GIVEIDTOMODULE:
		SavegameExpFuncs[module].giveIdToModule=(GIVEIDTOMODULE)addr;
		break;
	};
	
	return;
};

KEXPORT SAVEGAMEEXPFUNCS* GetSavegameExpFuncs(DWORD* num)
{
	if (num!=NULL)
		*num=numSaveGameExpModules;
	
	return &(SavegameExpFuncs[0]);
};


KEXPORT void SetSavegameFuncs(SAVEGAMEFUNCS* sgf)
{
	SavegameFuncs=sgf;
	return;
};

KEXPORT SAVEGAMEFUNCS* GetSavegameFuncs()
{
	return SavegameFuncs;
};


KEXPORT THREEDWORDS* GetSpecialAfsFileInfo(DWORD id)
{
	if (g_SpecialAfsIds.find(id) == g_SpecialAfsIds.end())
		return NULL;
	
	return &(g_SpecialAfsIds[id]);
};

KEXPORT DWORD GetNextSpecialAfsFileId(DWORD dw1, DWORD dw2, DWORD dw3)
{
	DWORD result=0;
	
	//search for any existing set with the same values
	for (g_SpecialAfsIdsIterator=g_SpecialAfsIds.begin();
			g_SpecialAfsIdsIterator != g_SpecialAfsIds.end(); g_SpecialAfsIdsIterator++) {
		if (g_SpecialAfsIdsIterator->second.dw1==dw1 &&
			g_SpecialAfsIdsIterator->second.dw2==dw2 &&
			g_SpecialAfsIdsIterator->second.dw3==dw3) {
				return g_SpecialAfsIdsIterator->first;
		};
	};
	
	THREEDWORDS threeDWORDs;
	threeDWORDs.dw1=dw1;
	threeDWORDs.dw2=dw2;
	threeDWORDs.dw3=dw3;
	
	//not using the fifth half byte makes the GetFileFromAFS() function fail
	result=currSpecialAfsId;
	result=((result & 0xff0000)<<4) | (result & 0xffff);
	result|=0x80070000;
		
	g_SpecialAfsIds[result]=threeDWORDs;
	
	currSpecialAfsId++;
	currSpecialAfsId%=0x1000000;
	
	return result;
};


KEXPORT bool MasterHookFunction(DWORD call_site, DWORD numArgs, void* target)
{
    hook_point hp(call_site, numArgs, (DWORD)target);
    return _hook_manager.hook(hp);
}

KEXPORT bool MasterUnhookFunction(DWORD call_site, void* target)
{
    hook_point hp(call_site, 0, (DWORD)target);
    return _hook_manager.unhook(hp);
}

DWORD oldEBP1;

DWORD MasterCallFirst(...)
{
	DWORD oldEAX, oldEBX, oldECX, oldEDX;
	DWORD oldEBP, oldEDI, oldESI;
	__asm {
		mov oldEAX, eax
		mov oldEBX, ebx
		mov oldECX, ecx
		mov oldEDX, edx
		mov oldEBP, ebp
		mov oldEBP1, ebp
		mov oldEDI, edi
		mov oldESI, esi
	};
	
	DWORD arg;
	DWORD result=0;

	DWORD call_site=*(DWORD*)(oldEBP+4)-5;
	DWORD addr=_hook_manager.getFirstTarget(call_site);
	
	if (addr==0) return;
	
	DWORD before=lastCallSite;
	lastCallSite=call_site;
	
	DWORD numArgs=_hook_manager.getNumArgs(call_site);
	bool wasJump=_hook_manager.getType(call_site)==HOOKTYPE_JMP;
	
	//writing this as inline assembler allows to
	//give as much parameters as we want and more
	//important, we can restore all registers
	__asm {
		//push ebp
	};
	
	for (int i=numArgs-1;i>=0;i--) {
		if (wasJump)
			arg=*((DWORD*)oldEBP+3+i);
		else
			arg=*((DWORD*)oldEBP+2+i);
		__asm mov eax, arg
		__asm push eax
	};
	
	__asm {
		//restore registers
		mov eax, oldEAX
		mov ebx, oldEBX
		mov ecx, oldECX
		mov edx, oldEDX
		mov edi, oldEDI
		mov esi, oldESI
		//mov ebp, oldEBP
		//mov ebp, [ebp]
		
		call ds:[addr]
		
		mov result, eax
		
	};
	
	for (i=0;i<numArgs;i++)
		__asm pop eax
	
	__asm {
		//pop ebp
		mov eax, result
	};
	
	lastCallSite=before;
	
	if (wasJump) {
		__asm {
			mov eax, oldEBP1
			add ebp, 4
			mov eax, [eax]
			mov [ebp], eax
		};
	};
	
	return result;
};

KEXPORT DWORD MasterCallNext(...)
{
	if (lastCallSite==0) return 0;
	
	DWORD oldEAX, oldEBX, oldECX, oldEDX;
	DWORD oldEBP, oldEDI, oldESI;
	__asm {
		mov oldEAX, eax
		mov oldEBX, ebx
		mov oldECX, ecx
		mov oldEDX, edx
		mov oldEBP, ebp
		mov oldEDI, edi
		mov oldESI, esi
	};
	
	DWORD result=0;
	DWORD arg;
	
	DWORD addr=_hook_manager.getNextTarget(lastCallSite);
	if (addr==0) return 0;
	
	DWORD numArgs=_hook_manager.getNumArgs(lastCallSite);

	__asm {
		//push ebp
	};
	
	for (int i=numArgs-1;i>=0;i--) {
		arg=*((DWORD*)oldEBP+2+i);
		__asm mov eax, arg
		__asm push eax
	};
	
	__asm {
		//restore registers
		mov eax, oldEAX
		mov ebx, oldEBX
		mov ecx, oldECX
		mov edx, oldEDX
		mov edi, oldEDI
		mov esi, oldESI
		//mov ebp, oldEBP
		//mov ebp, [ebp]
		
		call ds:[addr]
		
		mov result, eax
	};
	
	for (i=0;i<numArgs;i++)
		__asm pop eax
	
	__asm mov eax, result
	
	return result;
}

/*
KEXPORT DWORD GetAfsIdByOpenHandle(HANDLE handle)
{
    for (int i=0; i<8; i++) {
        if (_afsHandles[i] == handle) 
            return i;
    }
    return 0xffffffff;
}

KEXPORT BOOL IsOptionHandle(HANDLE handle)
{
    return handle != INVALID_HANDLE_VALUE
       && _optionHandle == handle;
}

KEXPORT HANDLE WINAPI Override_CreateFileA(
  __in      LPCSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile)
{
    HANDLE handle = CreateFileA(
            lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);

    char* shortName = strrchr(lpFileName,'\\');
    if (shortName) {
        if (_strnicmp(shortName+1,"0_so",4)==0)
            _afsHandles[0] = handle;
        else if (_strnicmp(shortName+1,"0_te",4)==0)
            _afsHandles[1] = handle;
        else if (_strnicmp(shortName+2,"_so",3)==0)
            _afsHandles[2] = handle;
        else if (_strnicmp(shortName+2,"_te",3)==0)
            _afsHandles[3] = handle;
        else if (_strnicmp(lpFileName+strlen(lpFileName)-3,"OPT",3)==0)
        {
            _optionHandle = handle;
            LOG(&k_kload, "CreateFileA: OPTION FILE");
        }
    }
    //LOG(&k_kload, "CreateFile: {%s} --> %d", lpFileName, (DWORD)handle);
    return handle;
}

KEXPORT HANDLE WINAPI Override_CreateFileW(
  __in      LPCWSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile)
{
    HANDLE handle = CreateFileW(
            lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);

    wchar_t* shortName = wcsrchr(lpFileName,L'\\');
    if (shortName) {
        if (_wcsnicmp(shortName+1,L"0_so",4)==0)
            _afsHandles[0] = handle;
        else if (_wcsnicmp(shortName+1,L"0_te",4)==0)
            _afsHandles[1] = handle;
        else if (_wcsnicmp(shortName+2,L"_so",3)==0)
            _afsHandles[2] = handle;
        else if (_wcsnicmp(shortName+2,L"_te",3)==0)
            _afsHandles[3] = handle;
        else if (_wcsnicmp(lpFileName+wcslen(lpFileName)-3,L"OPT",3)==0)
        {
            LOG(&k_kload, "CreateFileW: OPTION FILE");
            _optionHandle = handle;
        }
    }
    //LOG(&k_kload, "CreateFile: {%s} --> %d", lpFileName, (DWORD)handle);
    return handle;
}

KEXPORT BOOL WINAPI Override_CloseHandle(
  __in HANDLE hObject)
{
    BOOL result = CloseHandle(hObject);

    for (int i=0; i<8; i++) {
        if (_afsHandles[i] == hObject)
            _afsHandles[i] = INVALID_HANDLE_VALUE;
    }
    if (_optionHandle == hObject)
        _optionHandle = INVALID_HANDLE_VALUE;

    //LOG(&k_kload, "CloseHandle: {%d}", hObject);
    return result;
}
*/
