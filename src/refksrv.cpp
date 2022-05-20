// refksrv.cpp
#include <windows.h>
#include <stdio.h>
#include "refksrv.h"
#include "kload_exp.h"
#include "afsreplace.h"
#include <map>

KMOD k_refksrv={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

REFKSRV_CFG refksrv_config = {
    DEFAULT_SELECTED_REF_KIT,
    DEFAULT_PREVIEW_ENABLED,
    DEFAULT_KEY_RESET,
    DEFAULT_KEY_RANDOM,
    DEFAULT_KEY_PREV,
    DEFAULT_KEY_NEXT,
    DEFAULT_AUTO_RANDOM_MODE,
};


DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;

#define DATALEN 17
enum {
    REF_KIT_1, REF_KIT_1_LO, REF_KIT_2, REF_KIT_2_LO,
	REF_KIT_3, REF_KIT_3_LO, REF_KIT_4, REF_KIT_4_LO,
	REF_KIT_5, REF_KIT_5_LO, REF_KIT_6, REF_KIT_6_LO,
	REF_KIT_7, REF_KIT_7_LO, REF_KIT_8, REF_KIT_8_LO,
	REF_KIT_MODEL,
};

static DWORD dtaArray[][DATALEN] = {
	// PES5 DEMO 2
	{
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0,
	},
	// PES5
	{
		382, 383, 384, 385,
		386, 387, 388, 389,
		390, 391, 392, 393,
		394, 395, 396, 397,
		378,
	},
	// WE9
	{
		382, 383, 384, 385,
		386, 387, 388, 389,
		390, 391, 392, 393,
		394, 395, 396, 397,
		378,
	},
	// WE9:LE
	{
		388, 389, 390, 391,
		392, 393, 394, 395,
		396, 397, 398, 399,
		400, 401, 402, 403,
		384,
	},
};

static DWORD dta[DATALEN];

static char* FILE_NAMES[] = {
    "ref_kit.str",
    "ref_kit_lo.str",
    "preview.png",
};

#define REF_KIT_HQ 0
#define REF_KIT_LO 1
#define PREVIEW 2

int selectedKit=-1;
DWORD numKits=0;
REF_KITS *refKits;

bool isSelectMode=false;
char display[BUFLEN];
char model[BUFLEN];
char folder[BUFLEN];
bool autoRandomMode = false;
static bool g_userChoice = true;
static std::map<string,int> g_KitIdMap;
static bool g_newRefKit = false;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
bool readConfig(REFKSRV_CFG* config, char* cfgFile);
void InitModule();
bool refksrvReadNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD* numPages);
bool refksrvAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
void ReadRefKits();
void AddKit(LPTSTR sdisplay,LPTSTR smodel,LPTSTR sfolder);
void SetRefKit(DWORD id);
void FreeRefKits();
void refksrvKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void refksrvUniSelect();
void refksrvShowMenu();
void refksrvBeginUniSelect();
void refksrvEndUniSelect();

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved){
	if (dwReason == DLL_PROCESS_ATTACH){
		Log(&k_refksrv,"Attaching dll...");


		int v=GetPESInfo()->GameVersion;
		switch (v){
			case gvPES5PC: //support for PES5 PC...
			case gvWE9PC: //support for WE9 PC...
			case gvWE9LEPC: //... and WE9:LE PC
				goto GameVersIsOK;
				break;
			default:
				Log(&k_refksrv,"Your game version is currently not supported!");
				return false;
		}

		//Everything is OK!
		GameVersIsOK:

		hInst=hInstance;
		RegisterKModule(&k_refksrv);

		// initialize addresses
		memcpy(dta, dtaArray[v], sizeof(dta));

		HookFunction(hk_D3D_Create,(DWORD)InitModule);
		HookFunction(hk_GetNumPages,(DWORD)refksrvReadNumPages);
		HookFunction(hk_AfterReadFile,(DWORD)refksrvAfterReadFile);
	    HookFunction(hk_Input,(DWORD)refksrvKeyboardProc);
		HookFunction(hk_BeginUniSelect, (DWORD)refksrvUniSelect);
		HookFunction(hk_DrawKitSelectInfo,(DWORD)refksrvShowMenu);
		HookFunction(hk_OnShowMenu,(DWORD)refksrvBeginUniSelect);
        HookFunction(hk_OnHideMenu,(DWORD)refksrvEndUniSelect);
	}
	else if (dwReason == DLL_PROCESS_DETACH){
		Log(&k_refksrv,"Detaching dll...");
		
		//save settings
        char refkCfg[BUFLEN];
		refksrv_config.selectedRefKit=selectedKit;
        refksrv_config.autoRandomMode = autoRandomMode;
	    ZeroMemory(refkCfg, BUFLEN);
	    sprintf(refkCfg, "%s\\refksrv.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(refkCfg, "wb");
	    if (f) {
	        fwrite(&refksrv_config, sizeof(REFKSRV_CFG), 1, f);
	        fclose(f);
			Log(&k_refksrv,"Preferences saved.");
	    }

		//Unhooking everything

		UnhookFunction(hk_GetNumPages,(DWORD)refksrvReadNumPages);
		UnhookFunction(hk_AfterReadFile,(DWORD)refksrvAfterReadFile);
		UnhookFunction(hk_Input,(DWORD)refksrvKeyboardProc);
		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)refksrvShowMenu);

		FreeRefKits();

		g_KitIdMap.clear();
		


		Log(&k_refksrv,"Detaching done.");
	}
	return true;
}

void InitModule(){

	UnhookFunction(hk_D3D_Create,(DWORD)InitModule);

    //load settings
    char refkCfg[BUFLEN];
    ZeroMemory(refkCfg, BUFLEN);
    sprintf(refkCfg, "%s\\refksrv.dat", GetPESInfo()->mydir);
    LOG(&k_refksrv, "reading: %s", refkCfg);
    FILE* f = fopen(refkCfg, "rb");
    if (f) {
        fread(&refksrv_config, sizeof(REFKSRV_CFG), 1, f);
        fclose(f);
        autoRandomMode = refksrv_config.autoRandomMode;
    } else {
        refksrv_config.selectedRefKit=-1;
        g_userChoice = false;
    };
    LOG(&k_refksrv, "selected ref kit: %d", refksrv_config.selectedRefKit);

    //read preview and virtual keys settings
    refksrv_config.previewEnabled = TRUE;
    ZeroMemory(refkCfg, BUFLEN);
    sprintf(refkCfg, "%s\\refksrv.cfg", GetPESInfo()->mydir);
    readConfig(&refksrv_config, refkCfg);

    ReadRefKits();
    SetRefKit(refksrv_config.selectedRefKit);
	
	Log(&k_refksrv, "Module initialized.");
}


bool refksrvReadNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages)
{
    DWORD fileSize = 0;
    char filename[512] = {0};

    if (afsId == 1) { // 0_text.afs
		if (fileId == dta[REF_KIT_MODEL]) {
			// Model
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referee kits\\mdl\\");
			strcat(filename,model);
			//////////////////////////////////
		}else if (fileId >= dta[REF_KIT_1] && fileId <= dta[REF_KIT_8_LO] && fileId%2==0){
			// HQ Texture
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referee kits\\");
			strcat(filename,folder);
			strcat(filename,"\\");
			strcat(filename,FILE_NAMES[REF_KIT_HQ]);
		}
		else if (fileId >= dta[REF_KIT_1] && fileId <= dta[REF_KIT_8_LO] && fileId%2==1) {
			// LO Texture
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referee kits\\");
			strcat(filename,folder);
			strcat(filename,"\\");
			strcat(filename,FILE_NAMES[REF_KIT_LO]);
		}
		HANDLE TempHandle=CreateFile(filename,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if (TempHandle!=INVALID_HANDLE_VALUE) {
			fileSize=GetFileSize(TempHandle,NULL);
			CloseHandle(TempHandle);
		} else {
			fileSize=0;
		};

		if (fileSize > 0) {
			LogWithString(&k_refksrv, "refksrvReadNumPages: found file: %s", filename);
			LogWithTwoNumbers(&k_refksrv,"refksrvReadNumPages: had size: %08x pages (%08x bytes)", 
					orgNumPages, orgNumPages*0x800);

			// adjust buffer size to fit file
			*numPages = ((fileSize-1)>>0x0b)+1;
			LOG(&k_refksrv,
					"refksrvReadNumPages: new size: %08x pages (%08x bytes)", 
					*numPages, (*numPages)*0x800);
			return true;
		}
    }
    return false;
}


bool refksrvAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped){
	DWORD afsId = GetAfsIdByOpenHandle(hFile);
    DWORD filesize = 0;
    char filename[512] = {0};

	// ensure it is 0_TEXT.afs
	if (afsId == 1) {
		DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - (*lpNumberOfBytesRead);
		DWORD fileId = GetProbableFileIdForHandle(afsId, offset, hFile);
		if (fileId == dta[REF_KIT_MODEL]) {
			// Model
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referee kits\\mdl\\");
			strcat(filename,model);
			LOG(&k_refksrv, 
			"--> READING REFEREE MODEL (%d): offset:%08x, bytes:%08x <--",
			fileId, offset, *lpNumberOfBytesRead);

		}
		else if (fileId >= dta[REF_KIT_1] && fileId <= dta[REF_KIT_8_LO] && fileId%2==0){
			// HQ Texture
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referee kits\\");
			strcat(filename,folder);
			strcat(filename,"\\");
			strcat(filename,FILE_NAMES[REF_KIT_HQ]);
			//strcat(filename,".str");
			LOG(&k_refksrv, 
			"--> READING REFEREE HQ KIT (%d): offset:%08x, bytes:%08x <--",
			fileId, offset, *lpNumberOfBytesRead);
		}
		else if (fileId >= dta[REF_KIT_1] && fileId <= dta[REF_KIT_8_LO] && fileId%2==1) {
			// LO Texture
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referee kits\\");
			strcat(filename,folder);
			strcat(filename,"\\");
			strcat(filename,FILE_NAMES[REF_KIT_LO]);
			LOG(&k_refksrv, 
			"--> READING REFEREE LO KIT (%d): offset:%08x, bytes:%08x <--",
			fileId, offset, *lpNumberOfBytesRead);
		}
		else{
			return false;
		}
		LOG(&k_refksrv, "loading (%s)", filename);
		HANDLE handle = CreateFile(
		filename, 
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
		bool result = false;
		if (handle!=INVALID_HANDLE_VALUE) {
			filesize = GetFileSize(handle,NULL);
			DWORD curr_offset = offset - GetOffsetByFileId(afsId, fileId);
			LOG(&k_refksrv, 
			"offset=%08x, GetOffsetByFileId()=%08x, curr_offset=%08x",
			offset, GetOffsetByFileId(afsId, fileId), curr_offset);
			DWORD bytesToRead = *lpNumberOfBytesRead;
			if (filesize < curr_offset + *lpNumberOfBytesRead) {
				bytesToRead = filesize - curr_offset;
			}
			DWORD bytesRead = 0;
			SetFilePointer(handle, curr_offset, NULL, FILE_BEGIN);
			LOG(&k_refksrv, "reading 0x%x bytes (0x%x) from {%s}", 
			bytesToRead, *lpNumberOfBytesRead, filename);
			ReadFile(handle, lpBuffer, bytesToRead, &bytesRead, lpOverlapped);
			LOG(&k_refksrv, "read 0x%x bytes from {%s}", bytesRead, filename);
			if (*lpNumberOfBytesRead - bytesRead > 0) {
				DEBUGLOG(&k_refksrv, "zeroing out 0x%0x bytes",
				*lpNumberOfBytesRead - bytesRead);
				memset(
				(BYTE*)lpBuffer+bytesRead, 0, *lpNumberOfBytesRead-bytesRead);
			}
			CloseHandle(handle);
			// set next likely read
			if (filesize > curr_offset + bytesRead) {
				SetNextProbableReadForHandle(
				afsId, offset+bytesRead, fileId, hFile);
			}
			result = true;
		}
		else {
			LOG(&k_refksrv, "Unable to read file {%s} must be protected or opened", filename);
		}
		return result;
	}
	return false;
}

void ReadRefKits()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sdisplay[BUFLEN], smodel[BUFLEN], sfolder[BUFLEN];
		
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\referee kits\\map.txt");
	
	FILE* cfg=fopen(tmp, "rt");
	if (cfg==NULL) return;
	while (true) {
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;
		
		// skip comments
		comment=NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		ZeroMemory(sdisplay,BUFLEN);
		ZeroMemory(smodel,BUFLEN);
		ZeroMemory(sfolder,BUFLEN);
		if (sscanf(str,"\"%[^\"]\",\"%[^\"]\",\"%[^\"]\"",sdisplay,smodel,sfolder)==3)
			AddKit(sdisplay,smodel,sfolder);
	};
	fclose(cfg);	
	return;
};



void AddKit(LPTSTR sdisplay,LPTSTR smodel,LPTSTR sfolder)
{
	REF_KITS *tmp=new REF_KITS[numKits+1];
	memcpy(tmp,refKits,numKits*sizeof(REF_KITS));
	delete refKits;
	refKits=tmp;
	
	refKits[numKits].display=new char [strlen(sdisplay)+1];
	strcpy(refKits[numKits].display,sdisplay);
	
	refKits[numKits].model=new char [strlen(smodel)+1];
	strcpy(refKits[numKits].model,smodel);
	
	refKits[numKits].folder=new char [strlen(sfolder)+1];
	strcpy(refKits[numKits].folder,sfolder);

	g_KitIdMap[sdisplay]=numKits;

	numKits++;
	return;
};


void SetRefKit(DWORD id)
{
	char tmp[BUFLEN];
	
	if (id<numKits)
		selectedKit=id;
	else if (id<0)
		selectedKit=-1;
	else
		selectedKit=-1;
		
	if (selectedKit<0) {
		strcpy(tmp,"game choice");
		strcpy(model,"\0");
		strcpy(folder,"\0");
	} else {
		strcpy(tmp,refKits[selectedKit].display);
		strcpy(model,refKits[selectedKit].model);
		strcpy(folder,refKits[selectedKit].folder);
		
		//SafeRelease( &g_preview_tex );
		g_newRefKit=true;
	};
	
	strcpy(display,"Referee Kit: ");
	strcat(display,tmp);
	
	return;
};

void FreeRefKits()
{
	for (int i=0;i<numKits;i++) {
		delete refKits[i].display;
		delete refKits[i].model;
		delete refKits[i].folder;
	};
	delete refKits;
	numKits=0;
	selectedKit=-1;
	return;
};


void refksrvKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	if ((!isSelectMode) || (code1 < 0))
		return; 

	if ((code1==HC_ACTION) && (lParam & 0x80000000)) {
		if (wParam == refksrv_config.keyNext) {
			SetRefKit(selectedKit+1);
			g_userChoice = true;
		} else if (wParam == refksrv_config.keyPrev) {
			if (selectedKit<0)
				SetRefKit(numKits-1);
			else
				SetRefKit(selectedKit-1);
			g_userChoice = true;
		} else if (wParam == refksrv_config.keyReset) {
			SetRefKit(-1);
			if (!autoRandomMode) {
        		autoRandomMode = true;
        		g_userChoice = true;
	            
        	} else {
				SetRefKit(-1);
				g_userChoice = false;
				autoRandomMode = false;
			}
			
			if (autoRandomMode) {
				LARGE_INTEGER num;
				QueryPerformanceCounter(&num);
				int iterations = num.LowPart % MAX_ITERATIONS;
				for (int j=0;j<iterations;j++) {
					SetRefKit(selectedKit+1);
		        }
			}

		} else if (wParam == refksrv_config.keyRandom) {
			LARGE_INTEGER num;
			QueryPerformanceCounter(&num);
			int iterations = num.LowPart % MAX_ITERATIONS;
			for (int j=0;j<iterations;j++)
				SetRefKit(selectedKit+1);

			g_userChoice = true;
		}
	}
	
	return;
};


void refksrvUniSelect()
{

	if (autoRandomMode) {
		LARGE_INTEGER num;
		QueryPerformanceCounter(&num);
		int iterations = num.LowPart % MAX_ITERATIONS;
		for (int j=0;j<iterations;j++) {
			SetRefKit(selectedKit+1);
        }
	}
	return;
}


void refksrvShowMenu()
{
	SIZE size;
	DWORD color = 0xffffffff; // white
    char text[512];
	UINT g_bbWidth=GetPESInfo()->bbWidth;
	UINT g_bbHeight=GetPESInfo()->bbHeight;
	double stretchX=GetPESInfo()->stretchX;
	double stretchY=GetPESInfo()->stretchY;
	if (selectedKit<0) {
		color = 0xffc0c0c0; // gray if stadium is game choice
    } 
	if (autoRandomMode)
		color = 0xffaad0ff; // light blue for randomly selected ball

	KGetTextExtent(display,16,&size);
	KDrawText((g_bbWidth-size.cx)/2,g_bbHeight*0.75,color,16,display,true);


	/*
    //draw Referee Kit preview
    DrawPreview(g_device);
	*/
	return;
};


void refksrvBeginUniSelect()
{
    //Log(&k_stadium, "stadBeginUniSelect");
    
    isSelectMode=true;
    dksiSetMenuTitle("Referee kit selection");
    
    // invalidate preview texture
    //SafeRelease( &g_preview_tex );
    //g_newStad = true;


    //HookFunction(hk_Input,(DWORD)stadKeyboardProc);
    return;
}

void refksrvEndUniSelect()
{
    
    isSelectMode=false;

}


/**
 * Returns true if successful.
 */
bool readConfig(REFKSRV_CFG* config, char* cfgFile)
{
	if (config == NULL) return false;

	FILE* cfg = fopen(cfgFile, "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (!feof(cfg))
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (lstrcmp(name, "preview")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_refksrv,"ReadConfig: preview = (%d)", value);
            config->previewEnabled = (value == 1);
		}
        else if (lstrcmpi(name, "key.reset")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_refksrv,"ReadConfig: key.reset = (0x%02x)", value);
            config->keyReset = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.random")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_refksrv,"ReadConfig: key.random = (0x%02x)", value);
            config->keyRandom = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.prev")==0 || lstrcmpi(name, "key.previous")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_refksrv,"ReadConfig: key.prev = (0x%02x)", value);
            config->keyPrev = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.next")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_refksrv,"ReadConfig: key.next = (0x%02x)", value);
            config->keyNext = (BYTE)value;
        }
	}
	fclose(cfg);
	return true;
}