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
};

#define REF_KIT_HQ 0
#define REF_KIT_LO 1

///// Graphics //////////////////

struct CUSTOMVERTEX { 
	FLOAT x,y,z,w;
	DWORD color;
};

struct CUSTOMVERTEX2 { 
	FLOAT x,y,z,w;
	DWORD color;
	FLOAT tu, tv;
};


CUSTOMVERTEX2 g_preview[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 0.0f}, //1
	{0.0f, 256.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 1.0f}, //2
	{136.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 0.0f}, //3
	{136.0f, 256.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 1.0f}, //4
};

CUSTOMVERTEX g_preview_outline[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}, //1
	{0.0f, 258.0f, 0.0f, 1.0f, 0xffffffff}, //2
	{138.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}, //3
	{138.0f, 258.0f, 0.0f, 1.0f, 0xffffffff}, //4
};

CUSTOMVERTEX g_preview_outline2[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{0.0f, 260.0f, 0.0f, 1.0f, 0xff000000}, //2
	{140.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //3
	{140.0f, 260.0f, 0.0f, 1.0f, 0xff000000}, //4
};

// Image preview
static IDirect3DVertexBuffer8* g_pVB_preview = NULL;
static IDirect3DVertexBuffer8* g_pVB_preview_outline = NULL;
static IDirect3DVertexBuffer8* g_pVB_preview_outline2 = NULL;

static IDirect3DTexture8* g_preview_tex = NULL;
static IDirect3DDevice8* g_device = NULL;

////////////////////////////////

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
#define D3DFVF_CUSTOMVERTEX2 (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)


//End of graphic related stuff
/////////////////////

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
//static bool g_newRefKit = false;

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

/////////// definition of graphic variables and functions
static bool g_needsRestore = TRUE;
static bool g_newPrev = false;
static DWORD g_dwSavedStateBlock = 0L;
static DWORD g_dwDrawOverlayStateBlock = 0L;

void SafeRelease(LPVOID ppObj);
void CustomGraphicReset(IDirect3DDevice8* self, LPVOID params);
void CustomGraphicCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);
HRESULT InvalidateDeviceObjects(IDirect3DDevice8* dev);
HRESULT DeleteDeviceObjects(IDirect3DDevice8* dev);
HRESULT RestoreDeviceObjects(IDirect3DDevice8* dev);
void DrawPreview(IDirect3DDevice8* dev);

///////////


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
		HookFunction(hk_D3D_CreateDevice,(DWORD)CustomGraphicCreateDevice);
		HookFunction(hk_DrawKitSelectInfo,(DWORD)refksrvShowMenu);
		HookFunction(hk_OnShowMenu,(DWORD)refksrvBeginUniSelect);
        HookFunction(hk_OnHideMenu,(DWORD)refksrvEndUniSelect);
		HookFunction(hk_D3D_Reset,(DWORD)CustomGraphicReset);
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
		UnhookFunction(hk_D3D_Reset,(DWORD)CustomGraphicReset);

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
		
		SafeRelease( &g_preview_tex );
		g_newPrev=true;
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
			autoRandomMode = false;
			SetRefKit(selectedKit+1);
			g_userChoice = true;
		} 
		else if (wParam == refksrv_config.keyPrev) {
			autoRandomMode = false;
			if (selectedKit<0)
				SetRefKit(numKits-1);
			else
				SetRefKit(selectedKit-1);
			g_userChoice = true;
		} 
		else if (wParam == refksrv_config.keyReset) {
			SetRefKit(-1);
			if (!autoRandomMode) {
        		autoRandomMode = true;
        		g_userChoice = true;
        	} 
			else {
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

		} 
		else if (wParam == refksrv_config.keyRandom) {
			autoRandomMode = true;
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


	
    //draw Referee Kit preview
    DrawPreview(g_device);
	
	return;
};


void refksrvBeginUniSelect()
{
    //Log(&k_stadium, "stadBeginUniSelect");
    
    isSelectMode=true;
    dksiSetMenuTitle("Referee kit selection");
    
    // invalidate preview texture
    SafeRelease( &g_preview_tex );
    g_newPrev = true;


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






////////////////////// Graphic functions


void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_refksrv,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_refksrv,"Problem with safe-release");
    }
}



void CustomGraphicCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface)
{
    g_device = *ppReturnedDeviceInterface;
    return;
}


/* New Reset function */
void CustomGraphicReset(IDirect3DDevice8* self, LPVOID params)
{
	Log(&k_refksrv,"CustomGraphicReset: cleaning-up.");

	InvalidateDeviceObjects(self);
	DeleteDeviceObjects(self);

    g_needsRestore = TRUE;
    g_device = self;
	return;
}


void SetPosition(CUSTOMVERTEX2* dest, CUSTOMVERTEX2* src, int n, int x, int y) 
{
    FLOAT xratio = GetPESInfo()->bbWidth / 1024.0;
    FLOAT yratio = GetPESInfo()->bbHeight / 768.0;
    for (int i=0; i<n; i++) {
        dest[i].x = (FLOAT)(int)((src[i].x + x) * xratio);
        dest[i].y = (FLOAT)(int)((src[i].y + y) * yratio);
    }
}

void SetPosition(CUSTOMVERTEX* dest, CUSTOMVERTEX* src, int n, int x, int y) 
{
    FLOAT xratio = GetPESInfo()->bbWidth / 1024.0;
    FLOAT yratio = GetPESInfo()->bbHeight / 768.0;
    for (int i=0; i<n; i++) {
        dest[i].x = (FLOAT)(int)((src[i].x + x) * xratio);
        dest[i].y = (FLOAT)(int)((src[i].y + y) * yratio);
    }
}

/* creates vertex buffers */
HRESULT InitVB(IDirect3DDevice8* dev)
{
	VOID* pVertices;

	// create vertex buffers
	// preview
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview)))
	{
		Log(&k_refksrv,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_refksrv,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview->Lock(0, sizeof(g_preview), (BYTE**)&pVertices, 0)))
	{
		Log(&k_refksrv,"g_pVB_preview->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview, sizeof(g_preview));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_preview, sizeof(g_preview)/sizeof(CUSTOMVERTEX2), 
            512-64, 310);
	g_pVB_preview->Unlock();

	// preview outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview_outline)))
	{
		Log(&k_refksrv,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_refksrv,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline->Lock(0, sizeof(g_preview_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_refksrv,"g_pVB_preview_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview_outline, sizeof(g_preview_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_preview_outline, sizeof(g_preview_outline)/sizeof(CUSTOMVERTEX), 
          512-65, 309);
	g_pVB_preview_outline->Unlock();

	// preview outline2
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview_outline2), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview_outline2)))
	{
		Log(&k_refksrv,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_refksrv,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline2->Lock(0, sizeof(g_preview_outline2), (BYTE**)&pVertices, 0)))
	{
		Log(&k_refksrv,"g_pVB_preview_outline2->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview_outline2, sizeof(g_preview_outline2));
	SetPosition((CUSTOMVERTEX*)pVertices, g_preview_outline2, sizeof(g_preview_outline2)/sizeof(CUSTOMVERTEX), 
            512-66, 308);
	g_pVB_preview_outline2->Unlock();

    return S_OK;
}

void DrawPreview(IDirect3DDevice8* dev)
{
	if (selectedKit<0) return;
	if (g_needsRestore) 
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_refksrv,"DrawPreview: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_refksrv,"DrawPreview: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
        D3DVIEWPORT8 vp;
        dev->GetViewport(&vp);
        LogWithNumber(&k_refksrv,"VP: %d",vp.X);
        LogWithNumber(&k_refksrv,"VP: %d",vp.Y);
        LogWithNumber(&k_refksrv,"VP: %d",vp.Width);
        LogWithNumber(&k_refksrv,"VP: %d",vp.Height);
        LogWithDouble(&k_refksrv,"VP: %f",vp.MinZ);
        LogWithDouble(&k_refksrv,"VP: %f",vp.MaxZ);
	}

	// render
	dev->BeginScene();

	// setup renderstate
	//dev->CaptureStateBlock( g_dwSavedStateBlock );
	//dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );
    
    if (!g_preview_tex && g_newPrev) {
        char buf[2048];
        sprintf(buf, "%s\\GDB\\referee kits\\%s\\preview.png", GetPESInfo()->gdbDir, folder);
        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                    0, NULL, NULL, &g_preview_tex))) {
            // try "preview.bmp"
            sprintf(buf, "%s\\GDB\\referee kits\\%s\\preview.bmp", GetPESInfo()->gdbDir, folder);
            if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                        0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                        D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                        0, NULL, NULL, &g_preview_tex))) {
                Log(&k_refksrv,"FAILED to load image for stadium preview.");
            }
        }
        g_newPrev = false;
    }
    if (g_preview_tex) {
        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_preview_outline2, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        dev->SetStreamSource( 0, g_pVB_preview_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

        // texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_preview_tex);
        dev->SetStreamSource( 0, g_pVB_preview, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
    }

	// restore the modified renderstates
	//dev->ApplyStateBlock( g_dwSavedStateBlock );

	dev->EndScene();
}

void DeleteStateBlocks(IDirect3DDevice8* dev)
{
	// Delete the state blocks
	try
	{
        //LogWithNumber(&k_refksrv,"dev = %08x", (DWORD)dev);
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        //LogWithNumber(&k_refksrv,"vtab = %08x", (DWORD)vtab);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            //LogWithNumber(&k_refksrv,"vtab[VTAB_DELETESTATEBLOCK] = %08x", (DWORD)vtab[VTAB_DELETESTATEBLOCK]);
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_refksrv,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_refksrv,"g_dwDrawOverlayStateBlock deleted.");
            }
        }
	}
	catch (...)
	{
        // problem deleting state block
	}

	g_dwSavedStateBlock = 0L;
	g_dwDrawOverlayStateBlock = 0L;
}

//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT InvalidateDeviceObjects(IDirect3DDevice8* dev)
{
	TRACE(&k_refksrv,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_refksrv,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // stadium preview
	SafeRelease( &g_pVB_preview );
	SafeRelease( &g_pVB_preview_outline );
	SafeRelease( &g_pVB_preview_outline2 );

	Log(&k_refksrv,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_refksrv,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Destroys all device-dependent objects
//-----------------------------------------------------------------------------
HRESULT DeleteDeviceObjects(IDirect3DDevice8* dev)
{
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT RestoreDeviceObjects(IDirect3DDevice8* dev)
{
    HRESULT hr = InitVB(dev);
    if (FAILED(hr))
    {
		Log(&k_refksrv,"InitVB() failed.");
        return hr;
    }
	Log(&k_refksrv,"InitVB() done.");

	// Create the state blocks for rendering overlay graphics
	for( UINT which=0; which<2; which++ )
	{
		dev->BeginStateBlock();

        dev->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        dev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture( 0, g_preview_tex );

		if( which==0 )
			dev->EndStateBlock( &g_dwSavedStateBlock );
		else
			dev->EndStateBlock( &g_dwDrawOverlayStateBlock );
	}

    return S_OK;
}


////////////////////////