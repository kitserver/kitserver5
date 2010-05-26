// stadium.cpp
#include <windows.h>
#include <stdio.h>
#include "stadium.h"
#include "kload_exp.h"
#include "hooklib.h"
#include "afsreplace.h"

//0x5052a0
//0x5c73b6: call 858950
//0x524522: call 89a209
//0x5245ce: call 89a209

#include <map>
#include <string>

#define MAXFILENAME 4096
#define MAXLINELEN 2048
#define HOME 0
#define AWAY 1

KMOD k_stadium={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
HHOOK g_hKeyboardHook=NULL;

///// multi-step reads //////////

typedef struct _MultiStepReadKey {
    HANDLE hfile;
    DWORD dwOffset;
} MultiStepReadKey;

typedef struct _MultiStepRead {
    DWORD dwOffset;
    DWORD bytesRead;
    DWORD afsId;
    DWORD fileId;
    MEMITEMINFO* info;
} MultiStepRead;

bool operator<(const MultiStepReadKey& a, const MultiStepReadKey& b) 
{
    if (a.hfile<b.hfile) return true;
    else if (a.hfile==b.hfile && a.dwOffset<b.dwOffset) return true;
    return false;
}

bool operator==(const MultiStepReadKey& a, const MultiStepReadKey& b)
{
    return a.hfile==b.hfile && a.dwOffset==b.dwOffset;
}

map<MultiStepReadKey,MultiStepRead> _msrs;

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
	{0.0f, 64.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 1.0f}, //2
	{128.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 0.0f}, //3
	{128.0f, 64.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 1.0f}, //4
};

CUSTOMVERTEX g_preview_outline[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}, //1
	{0.0f, 66.0f, 0.0f, 1.0f, 0xffffffff}, //2
	{130.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}, //3
	{130.0f, 66.0f, 0.0f, 1.0f, 0xffffffff}, //4
};

CUSTOMVERTEX g_preview_outline2[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{0.0f, 68.0f, 0.0f, 1.0f, 0xff000000}, //2
	{132.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //3
	{132.0f, 68.0f, 0.0f, 1.0f, 0xff000000}, //4
};

// Stadium preview
static IDirect3DVertexBuffer8* g_pVB_preview = NULL;
static IDirect3DVertexBuffer8* g_pVB_preview_outline = NULL;
static IDirect3DVertexBuffer8* g_pVB_preview_outline2 = NULL;

static IDirect3DTexture8* g_preview_tex = NULL;
static IDirect3DDevice8* g_device = NULL;

////////////////////////////////

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
#define D3DFVF_CUSTOMVERTEX2 (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

static bool g_needsRestore = TRUE;
static bool g_newStad = false;
static DWORD g_dwSavedStateBlock = 0L;
static DWORD g_dwDrawOverlayStateBlock = 0L;

static CRITICAL_SECTION g_cs;
static bool bOthersHooked = false;
static BYTE g_rfCode[] = {0,0,0,0,0,0};

typedef DWORD (*GETFILEFROMAFS)(DWORD,DWORD);
GETFILEFROMAFS GetFileFromAFS = NULL;

typedef DWORD (*GETSTRINGPROC)(DWORD,DWORD);
GETSTRINGPROC GetString = NULL;

typedef DWORD (*GETSTRING2PROC)(DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD);
GETSTRING2PROC GetString2 = NULL;

typedef DWORD (*WRITENUMPROC)(char* dest, char* format, DWORD num);
//WRITENUMPROC WriteBuilt = NULL;
//WRITENUMPROC WriteCapacity = NULL;

typedef DWORD (*SETLCMPROC)(DWORD);
SETLCMPROC SetLCM = NULL;

/* hook manager and hook points */
hook_point g_hp_GetFileFromAFS;
hook_point g_hp_WriteBuilt;
hook_point g_hp_WriteCapacity;


//#define MAP_FIND(map,key) ((map.count(key)>0) ? map[key] : NULL)
#define MAP_FIND(map,key) map[key]
#define MAP_CONTAINS(map,key) (map.count(key)>0)

#define CODELEN 15
enum {
    C_GETFILEFROMAFS_CS, C_GETFILEFROMAFS, C_READFILE_CS,
    C_GETSTRING, C_GETSTRING_CS,
    C_GETSTRING2, C_GETSTRING2_CS,
    C_WRITENUM, C_WRITENUM_CS1, C_WRITENUM_CS2,
    C_SETLCM_CS, C_SETLCM, C_STADIUMCHANGE, C_ADVANCESTADIUM,
    C_READNUMPAGES,
};
static DWORD codeArray[][CODELEN] = {
	// PES5 DEMO 2
	{0, 0, 0, 
     0, 0,
     0, 0,
     0, 0, 0,
     0, 0, 0, 0,
     0, 
     },
	// PES5
	{0x5b7ea2, 0x873f40, 0x877994,
     0x51c9e0, 0x5298aa,
     0x9c4d10, 0x9b1243,
     0x89e511, 0x5298f2, 0x52999e,
     0x52a1e8, 0x529a10, 0x529d16, 0x529d22,
     0x874767,
     },
	// WE9
	{0x5b82b2, 0x874410, 0x877e74,
     0x51ce20, 0x529cda,
     0x9c5020, 0x9b1523,
     0x89e9e1, 0x529d22, 0x529dce,
     0x52a618, 0x529e40, 0x52a146, 0x52a152,
     0x874c37,
     },
	// WE9:LE
	{0x5df692, 0x5a9150, 0x59ade4,
     0x517820, 0x5244da,
     0x9c7da0, 0x9b3ec3,
     0x89a209, 0x524522, 0x5245ce,
     0x524e18, 0x524640, 0x524946, 0x524952,
     0x5a9977,
     },
};

#define DATALEN 17
enum {
    NUM_FILES, NUM_STADS, STAD_FIRST, NOU_CAMP_SHIFT_ID, SHIFT, 
    ADBOARD_TEX_FIRST, NUM_ADBOARD_TEX, DELLA_ALPI_ADBOARDS,
    AFS_PAGELEN_TABLE,
    TEAM_IDS, ML_HOME_AREA, ML_AWAY_AREA, DELLA_ALPI,
    STADIUM_TEXT_TABLE, STADIUM_TEXT_LEN, RANDOM_STADIUM_FLAG,
    ISVIEWSTADIUMMODE,
};
static DWORD dataArray[][DATALEN] = {
	// PES5 DEMO 2
	{0, 0, 0, 0, 0, 
     0, 0, 0,
     0,
     0, 0, 0, 0,
     0, 0,
     0,
     },
	// PES5
	{66, 35, 9090, 9694, 4, 
     9075, 14, 9085,
     0x3bfff00,
     0x3be0f40, 0x38b77a4, 0x38b77a8, 10348,
     0x38b77bc, 61, 0x3b7ee28,
     0x00fe0a70,
     },
	// WE9
	{66, 35, 9090, 9694, 4, 
     9075, 14, 9085,
     0x3bfff20,
     0x3be0f60, 0x38b77a4, 0x38b77a8, 10348,
     0x38b77bc, 61, 0x3b7ee48,
     0x00fe0a78,
     },
    // WE9:LE
	{66, 35, 9099, 9703, 4, 
     9082, 16, 9092,
     0x3adef40,
     0x3b68a80, 0x37f20b4, 0x37f20b8, 10357,
     0x37f20cc, 61, 0x3adb168,
     0x00f1aa00,
     },
};

static DWORD* g_pageLenTable = NULL;

static char* FILE_NAMES[] = {
    "1_day_fine\\crowd_a0.str",
    "1_day_fine\\crowd_a1.str",
    "1_day_fine\\crowd_a2.str",
    "1_day_fine\\crowd_a3.str",
    "1_day_fine\\crowd_h0.str",
    "1_day_fine\\crowd_h1.str",
    "1_day_fine\\crowd_h2.str",
    "1_day_fine\\crowd_h3.str",
    "1_day_fine\\stad1_main.bin",
    "1_day_fine\\stad2_entrance.bin",
    "1_day_fine\\stad3_adboards.bin",
    "2_day_rain\\crowd_a0.str",
    "2_day_rain\\crowd_a1.str",
    "2_day_rain\\crowd_a2.str",
    "2_day_rain\\crowd_a3.str",
    "2_day_rain\\crowd_h0.str",
    "2_day_rain\\crowd_h1.str",
    "2_day_rain\\crowd_h2.str",
    "2_day_rain\\crowd_h3.str",
    "2_day_rain\\stad1_main.bin",
    "2_day_rain\\stad2_entrance.bin",
    "2_day_rain\\stad3_adboards.bin",
    "3_day_snow\\crowd_a0.str",
    "3_day_snow\\crowd_a1.str",
    "3_day_snow\\crowd_a2.str",
    "3_day_snow\\crowd_a3.str",
    "3_day_snow\\crowd_h0.str",
    "3_day_snow\\crowd_h1.str",
    "3_day_snow\\crowd_h2.str",
    "3_day_snow\\crowd_h3.str",
    "3_day_snow\\stad1_main.bin",
    "3_day_snow\\stad2_entrance.bin",
    "3_day_snow\\stad3_adboards.bin",
    "4_night_fine\\crowd_a0.str",
    "4_night_fine\\crowd_a1.str",
    "4_night_fine\\crowd_a2.str",
    "4_night_fine\\crowd_a3.str",
    "4_night_fine\\crowd_h0.str",
    "4_night_fine\\crowd_h1.str",
    "4_night_fine\\crowd_h2.str",
    "4_night_fine\\crowd_h3.str",
    "4_night_fine\\stad1_main.bin",
    "4_night_fine\\stad2_entrance.bin",
    "4_night_fine\\stad3_adboards.bin",
    "5_night_rain\\crowd_a0.str",
    "5_night_rain\\crowd_a1.str",
    "5_night_rain\\crowd_a2.str",
    "5_night_rain\\crowd_a3.str",
    "5_night_rain\\crowd_h0.str",
    "5_night_rain\\crowd_h1.str",
    "5_night_rain\\crowd_h2.str",
    "5_night_rain\\crowd_h3.str",
    "5_night_rain\\stad1_main.bin",
    "5_night_rain\\stad2_entrance.bin",
    "5_night_rain\\stad3_adboards.bin",
    "6_night_snow\\crowd_a0.str",
    "6_night_snow\\crowd_a1.str",
    "6_night_snow\\crowd_a2.str",
    "6_night_snow\\crowd_a3.str",
    "6_night_snow\\crowd_h0.str",
    "6_night_snow\\crowd_h1.str",
    "6_night_snow\\crowd_h2.str",
    "6_night_snow\\crowd_h3.str",
    "6_night_snow\\stad1_main.bin",
    "6_night_snow\\stad2_entrance.bin",
    "6_night_snow\\stad3_adboards.bin",
    "adboards_tex\\default.bin",
};

#define STAD_MAIN(x) (x==8 || x==19 || x==30 || x==41 || x==52 || x==63)
#define STAD_ADBOARDS(x) (x==10 || x==21 || x==32 || x==43 || x==54 || x==65)
#define ADBOARDS 66

// comparator for string pointers
struct ltstr
{
  bool operator()(std::string* s1, std::string* s2) const
  {
    return *s1 < *s2;
  }
};

static char* WEATHER_NAMES[] = {
	"Random weather",
	"Day / Fine",
	"Day / Rain",
	"Day / Snow",
	"Night / Fine",
	"Night / Rain",
	"Night / Snow",
};

static DWORD code[CODELEN];
static DWORD data[DATALEN];

static std::map<DWORD,bool> g_AFS_idMap;
static std::map<DWORD,MEMITEMINFO*> g_AFS_offsetMap;
static std::map<WORD,std::string*> g_HomeStadiumMap;
static std::map<std::string*,STADINFO*,ltstr> g_stadiumMap;
static std::map<std::string*,STADINFO*,ltstr>::iterator g_stadiumMapIterator = NULL;

static bool hasGdbStadiums = false;
static char g_stadiumText[61];
static bool viewGdbStadiums = false;
static BYTE weather = 0;
static bool isViewStadiumMode = false;
static bool needsReload=false;
static BYTE savedStadiumChange[6];
static BYTE savedAdvanceStadium[1];
static bool savedChangeStadiumCode=false;

static DWORD g_afsId = 0xffffffff;
static DWORD g_fileId = 0xffffffff;
static DWORD g_stadId = 0xffffffff;
static char g_stadFileSpec[BUFLEN];
static bool g_gameChoice = true;
static bool g_homeTeamChoice = false;
static bool isSelectMode=false;

void SafeRelease(LPVOID ppObj);
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitStadiumServer();
void stadAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped);
DWORD stadGetString(DWORD param0, DWORD text);
DWORD stadGetString2(DWORD ppText, DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD);
DWORD stadWriteBuilt(char* dest, char* format, DWORD num);
DWORD stadWriteCapacity(char* dest, char* format, DWORD num);
DWORD stadSetLCM(DWORD p1);
void CheckViewStadiumMode();
int GetStadId(DWORD id);
int GetFileId(DWORD id);
static void InitStadiumMaps();
static WORD GetTeamId(int which);
DWORD FindAdboardsFile(char* filename);
DWORD FindStadiumFile(DWORD stadFileId, char* filename);

void stadKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void stadShowMenu();
void stadBeginUniSelect();
void stadEndUniSelect();
void stadPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void stadReset(IDirect3DDevice8* self, LPVOID params);
void stadCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);

HRESULT InvalidateDeviceObjects(IDirect3DDevice8* dev);
HRESULT DeleteDeviceObjects(IDirect3DDevice8* dev);
HRESULT RestoreDeviceObjects(IDirect3DDevice8* dev);
void DrawPreview(IDirect3DDevice8* dev);
bool stadReadNumPages(DWORD base, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages);

// Calls IUnknown::Release() on an instance
void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_stadium,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_stadium,"Problem with safe-release");
    }
}

/**
 * Read stadium information into structure
 */
static void ReadStadInfo(char* folderName, STADINFO* stadInfo)
{
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
    sprintf(filename, "%sGDB\\stadiums\\%s\\info.txt", GetPESInfo()->gdbDir, folderName);

    //initialize to defaults
    stadInfo->built = 1991;
    stadInfo->capacity = 54321;
    strcpy(stadInfo->city,"\0");

    FILE* f = fopen(filename, "rt");
    if (f) {
        // go line by line
        char buf[MAXLINELEN];
        while (!feof(f))
        {
            ZeroMemory(buf, MAXLINELEN);
            fgets(buf, MAXLINELEN, f);
            if (lstrlen(buf) == 0) break;

            // strip off comments
            char* comm = strstr(buf, "#");
            if (comm != NULL) comm[0] = '\0';

            // read values
            char key[255];
            char city[255];
            bool processed=false;
            int value;
            
            
            if (sscanf(buf, "%s = %s", key, &city) == 2)
            	if (stricmp(key,"city")==0) {
            		strcpy(stadInfo->city,city);
            		processed=true;
            	};
            
            if (!processed)
	            if (sscanf(buf, "%s = %d", key, &value) == 2) {
	                if (stricmp(key,"built")==0) {
	                    stadInfo->built = value;
	                } else if (stricmp(key,"capacity")==0) {
	                    stadInfo->capacity = value;
	                }
	            }
        }
        fclose(f);
    }
}

/**
 * Initialize home-stadium map.
 */
static void InitStadiumMaps()
{
    // step 1: enumarate all stadiums
	WIN32_FIND_DATA fData;
	char pattern[MAXLINELEN];
	ZeroMemory(pattern, MAXLINELEN);

    sprintf(pattern, "%sGDB\\stadiums\\*", GetPESInfo()->gdbDir);

	HANDLE hff = FindFirstFile(pattern, &fData);
	if (hff != INVALID_HANDLE_VALUE) {
        while(true)
        {
            // check if this is a directory
            if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // check for system folders
                if (fData.cFileName[0]!='.') {
                    LogWithString(&k_stadium, "found stadium: {%s}", fData.cFileName);

                    //built/capacity structure
                    STADINFO* stadInfo = new STADINFO();
                    ReadStadInfo(fData.cFileName, stadInfo);

                    // store in the home-stadium map
                    g_stadiumMap[new std::string(fData.cFileName)] = stadInfo;
                }
            }
            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);
    }

    // initialize stadium iterator
    g_stadiumMapIterator = g_stadiumMap.begin();

    // step 2: read map.txt
    char mapFile[MAXFILENAME];
    ZeroMemory(mapFile,MAXFILENAME);
	memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));

    sprintf(mapFile, "%sGDB\\stadiums\\map.txt", GetPESInfo()->gdbDir);
    FILE* map = fopen(mapFile, "rt");
    if (map == NULL) {
        LogWithString(&k_stadium, "Unable to find stadium-map: %s", mapFile);
        return;
    }

	// go line by line
    char buf[MAXLINELEN];
	while (!feof(map))
	{
		ZeroMemory(buf, MAXLINELEN);
		fgets(buf, MAXLINELEN, map);
		if (lstrlen(buf) == 0) break;

		// strip off comments
		char* comm = strstr(buf, "#");
		if (comm != NULL) comm[0] = '\0';

        // find team id
        WORD teamId = 0xffff;
        if (sscanf(buf, "%d", &teamId)==1) {
            LogWithNumber(&k_stadium, "teamId = %d", teamId);
            char* foldername = NULL;
            // look for comma
            char* pComma = strstr(buf,",");
            if (pComma) {
                // what follows is the filename.
                // It can be contained within double quotes, so 
                // strip those off, if found.
                char* start = NULL;
                char* end = NULL;
                start = strstr(pComma + 1,"\"");
                if (start) end = strstr(start + 1,"\"");
                if (start && end) {
                    // take what inside the quotes
                    end[0]='\0';
                    foldername = start + 1;
                } else {
                    // just take the rest of the line
                    foldername = pComma + 1;
                }

                LogWithString(&k_stadium, "foldername = {%s}", foldername);

                // store in the home-stadium map
                g_HomeStadiumMap[teamId] = new std::string(foldername);
            }
        }
    }
    fclose(map);


}

/**
 * Return currently selected (home or away) team ID.
 */
static WORD GetTeamId(int which)
{
    BYTE* mlData;
    if (data[TEAM_IDS]==0) return 0xffff;
    WORD id = ((WORD*)data[TEAM_IDS])[which];
    if (id==0xf2 || id==0xf3) {
        switch (id) {
            case 0xf2:
                // master league team (home)
                mlData = *((BYTE**)data[ML_HOME_AREA]);
                id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
                break;
            case 0xf3:
                // master league team (away)
                mlData = *((BYTE**)data[ML_AWAY_AREA]);
                id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
                break;
        }
    }
    return id;
}

// entry point
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_stadium,"Attaching dll...");
		
		hInst=hInstance;
        InitializeCriticalSection(&g_cs);

		RegisterKModule(&k_stadium);
		
		HookFunction(hk_D3D_Create,(DWORD)InitStadiumServer);
		HookFunction(hk_AfterReadFile,(DWORD)stadAfterReadFile);
        HookFunction(hk_GetNumPages, (DWORD)stadReadNumPages);

		HookFunction(hk_DrawKitSelectInfo,(DWORD)stadShowMenu);
        HookFunction(hk_OnShowMenu,(DWORD)stadBeginUniSelect);
        HookFunction(hk_OnHideMenu,(DWORD)stadEndUniSelect);
        
		HookFunction(hk_D3D_CreateDevice,(DWORD)stadCreateDevice);
		HookFunction(hk_D3D_Present,(DWORD)stadPresent);
		HookFunction(hk_D3D_Reset,(DWORD)stadReset);
        
		HookFunction(hk_Input,(DWORD)stadKeyboardProc);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_stadium,"Detaching dll...");
        DeleteCriticalSection(&g_cs);

        /*
        MasterUnhookFunction(code[C_GETSTRING_CS], stadGetString);
        Log(&k_stadium, "GetString unhooked.");
        MasterUnhookFunction(code[C_GETSTRING2_CS], stadGetString2);
        Log(&k_stadium, "GetString2 unhooked.");

        MasterUnhookFunction(code[C_WRITENUM_CS1], stadWriteBuilt);
        Log(&k_stadium, "WriteBuilt unhooked.");
        MasterUnhookFunction(code[C_WRITENUM_CS2], stadWriteCapacity);
        Log(&k_stadium, "WriteCapacity unhooked.");
			
        MasterUnhookFunction(code[C_SETLCM_CS], stadSetLCM);
        Log(&k_stadium, "SetLCM unhooked.");
        

		UnhookFunction(hk_D3D_Create,(DWORD)InitStadiumServer);
		UnhookFunction(hk_AfterReadFile,(DWORD)stadAfterReadFile);

		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)stadShowMenu);
        UnhookFunction(hk_OnShowMenu,(DWORD)stadBeginUniSelect);
        UnhookFunction(hk_OnHideMenu,(DWORD)stadEndUniSelect);   
        
        UnhookFunction(hk_D3D_Present,(DWORD)stadPresent);
        */
	};

	return true;
}

void InitStadiumServer()
{
    int i,j;

	memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
	memcpy(data, dataArray[GetPESInfo()->GameVersion], sizeof(data));

    // reset stadium filename buffer
    ZeroMemory(g_stadFileSpec, BUFLEN);

    FILE* f = fopen("dat\\0_text.afs","rb");
    if (!f) {
        Log(&k_stadium, "InitStadiumServer: problem opening 0_text.afs for reading.");
        return;
    }

    // read offsets for stadium files
	for (i=0; i<data[NUM_STADS]; i++) {
        for (j=0; j<data[NUM_FILES]; j++) {
            DWORD id = data[STAD_FIRST] + i*data[NUM_FILES] + j;
            id = (id > data[NOU_CAMP_SHIFT_ID])?(id + data[SHIFT]):id;
            // store id in id-map
            g_AFS_idMap[id] = true;

            MEMITEMINFO* info = (MEMITEMINFO*)HeapAlloc(GetProcessHeap(), 
                    HEAP_ZERO_MEMORY, sizeof(MEMITEMINFO));
            if (!info) {
                Log(&k_stadium, "InitStadiumServer: problem allocating MEMITEMINFO");
                continue;
            }
            info->id = id;
            ReadItemInfoById(f, id, &info->afsItemInfo, 0);
            TRACE2(&k_stadium, "info->id = %08x", info->id);
            TRACE2(&k_stadium, "dwOffset = %08x", info->afsItemInfo.dwOffset);
            TRACE2(&k_stadium, "dwSize = %08x", info->afsItemInfo.dwSize);
            // store in offset map
            g_AFS_offsetMap[info->afsItemInfo.dwOffset] = info;
        }
    }

    // read offsets for adboard textures
	for (i=0; i<data[NUM_ADBOARD_TEX]; i++) {
        DWORD id = data[ADBOARD_TEX_FIRST] + i;
        // store id in id-map
        g_AFS_idMap[id] = true;

        MEMITEMINFO* info = (MEMITEMINFO*)HeapAlloc(GetProcessHeap(), 
                HEAP_ZERO_MEMORY, sizeof(MEMITEMINFO));
        if (!info) {
            Log(&k_stadium, "InitStadiumServer: problem allocating MEMITEMINFO");
            continue;
        }
        info->id = id;
        ReadItemInfoById(f, id, &info->afsItemInfo, 0);
        TRACE2(&k_stadium, "info->id = %08x", info->id);
        TRACE2(&k_stadium, "dwOffset = %08x", info->afsItemInfo.dwOffset);
        TRACE2(&k_stadium, "dwSize = %08x", info->afsItemInfo.dwSize);
        // store in offset map
        g_AFS_offsetMap[info->afsItemInfo.dwOffset] = info;
    }

    fclose(f);

    // initialize with info from "map.txt"
    InitStadiumMaps();
    
    // check we have any stadiums in GDB
    if (g_stadiumMap.begin() != g_stadiumMap.end())
    	hasGdbStadiums=true;

	UnhookFunction(hk_D3D_Create,(DWORD)InitStadiumServer);

	return;
}

int GetStadId(DWORD id)
{
    int sid = id - data[STAD_FIRST];
    sid = (id > data[NOU_CAMP_SHIFT_ID])?(sid - data[SHIFT]):sid;
    return sid / data[NUM_FILES];
}

int GetFileId(DWORD id)
{
    int sid = id - data[STAD_FIRST];
    sid = (id > data[NOU_CAMP_SHIFT_ID])?(sid - data[SHIFT]):sid;
    return sid % data[NUM_FILES];
}

int GetStadiumBase(DWORD stadId)
{
    int base = data[STAD_FIRST] + stadId * data[NUM_FILES];
    base = (base > data[NOU_CAMP_SHIFT_ID])?(base + data[SHIFT]):base;
    LogWithTwoNumbers(&k_stadium, "GetStadiumBase(%d): base = %d", stadId, base);
    return base;
}

DWORD FindAdboardsFile(char* filename)
{
	LCM* lcm=(LCM*)data[TEAM_IDS];
    // force full stadium reload next time
    BYTE* randomStad = (BYTE*)data[RANDOM_STADIUM_FLAG];
    *randomStad = *randomStad | 0x01;
    Log(&k_stadium, "Flag set for full stadium reload.");

    if (isViewStadiumMode && viewGdbStadiums)
    {
        return 0; // don't use adboards texture in "View Stadiums"
    }
    else if (g_gameChoice) 
    {
        return 0; //game choice stadium
    }

    if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        LogWithNumber(&k_stadium, "FindAdboardsFile: home team = %d", teamId);
        std::string* folderString = MAP_FIND(g_HomeStadiumMap,teamId);
        if (folderString != NULL) {
            LogWithString(&k_stadium, "FindAdboardsFile: has a home stadium: %s", 
                    (char*)folderString->c_str());

            sprintf(filename,"%sGDB\\stadiums\\%s\\%s", 
                    GetPESInfo()->gdbDir, (char*)folderString->c_str(), FILE_NAMES[ADBOARDS]);
        }
    } else {
        sprintf(filename,"%sGDB\\stadiums\\%s\\%s", 
                GetPESInfo()->gdbDir, g_stadiumMapIterator->first->c_str(), FILE_NAMES[ADBOARDS]);
    }

    //sprintf(filename,"%sGDB\\stadiums\\Santiago Bernabeu\\%s", 
    //        GetPESInfo()->gdbDir, FILE_NAMES[ADBOARDS]);

    HANDLE hfile;
    DWORD fsize = 0;
    hfile = CreateFile(filename, GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (hfile!=INVALID_HANDLE_VALUE) {
        fsize = GetFileSize(hfile,NULL);
        CloseHandle(hfile);
    }

    return fsize;
}

DWORD FindStadiumFile(DWORD stadFileId, char* filename)
{
    if (isViewStadiumMode) {
        if (!viewGdbStadiums) {
            return 0; // not a gdb stadium - in "View Stadiums"
        }
        else if (STAD_ADBOARDS(stadFileId)) {
            return 0; // don't load adboards for GDB stads in "View Stadiums"
        }
    } else if (g_gameChoice) {
        return 0; //game choice stadium - not in "View Stadiums"
    }

    LogWithNumber(&k_stadium, "g_gameChoice = %d", g_gameChoice);
    LogWithNumber(&k_stadium, "isViewStadiumMode = %d", isViewStadiumMode);
    LogWithNumber(&k_stadium, "viewGdbStadiums = %d", viewGdbStadiums);

    if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        LogWithNumber(&k_stadium, "FindStadiumFile: home team = %d", teamId);
        std::string* folderString = MAP_FIND(g_HomeStadiumMap,teamId);
        if (folderString != NULL) {
            LogWithString(&k_stadium, "FindStadiumFile: has a home stadium: %s", 
                    (char*)folderString->c_str());

            sprintf(filename,"%sGDB\\stadiums\\%s\\%s", 
                    GetPESInfo()->gdbDir, (char*)folderString->c_str(), FILE_NAMES[stadFileId]);
        }
    } else {
        sprintf(filename,"%sGDB\\stadiums\\%s\\%s", 
                GetPESInfo()->gdbDir, g_stadiumMapIterator->first->c_str(), FILE_NAMES[stadFileId]);
    }

    //sprintf(filename,"%sGDB\\stadiums\\Santiago Bernabeu\\%s", 
    //        GetPESInfo()->gdbDir, FILE_NAMES[stadFileId]);

    HANDLE hfile;
    DWORD fsize = 0;
    hfile = CreateFile(filename, GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (hfile!=INVALID_HANDLE_VALUE) {
        fsize = GetFileSize(hfile,NULL);
        CloseHandle(hfile);
    }

    return fsize;
}

void DumpBuffer(char* filename, LPVOID buf, DWORD len)
{
    FILE* f = fopen(filename,"wb");
    if (f) {
        fwrite(buf, len, 1, f);
        fclose(f);
    }
}

// determines item id, based on offset.
MEMITEMINFO* FindMemItemInfoByOffset(DWORD offset)
{
    //EnterCriticalSection(&g_cs);
    MEMITEMINFO* info = MAP_FIND(g_AFS_offsetMap, offset);
    //LeaveCriticalSection(&g_cs);
    return info;
}

void stadAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped)
{
	MEMITEMINFO* info = NULL;

    DWORD afsId = GetAfsIdByOpenHandle(hFile);
    //LogWithNumber(&k_stadium, "stadAfterReadFile:: afsId=%d", afsId);

    bool multiStepReadNext = false;
    map<MultiStepReadKey,MultiStepRead>::iterator mit;
    MultiStepReadKey mkey;
    mkey.hfile = hFile;
    DWORD _dwOffset1(0);
    
    // ensure it is 0_TEXT.afs
    if (afsId == 1) {
        _dwOffset1 = SetFilePointer(hFile, 0, NULL, FILE_CURRENT)
            - (*lpNumberOfBytesRead);
        //LogWithThreeNumbers(&k_stadium,"stadAfterReadFile: f=%08x, off=%08x, btr=%08x", (DWORD)hFile, _dwOffset1, nNumberOfBytesToRead);

        // check multi-step-read map first
        mkey.dwOffset = _dwOffset1;
        //LogWithTwoNumbers(&k_stadium, "Looking for entry with key (%08x,%08x)...",
        //        (DWORD)mkey.hfile, mkey.dwOffset);
        mit = _msrs.find(mkey);
        if (mit != _msrs.end())
        {
            info = mit->second.info;
            g_afsId = mit->second.afsId;
            g_fileId = mit->second.fileId;
            multiStepReadNext = true;
        }
        else
        {
            // search for such offset
            info = FindMemItemInfoByOffset(_dwOffset1);
        }
    }

	//if (info != NULL && info->id == g_fileId && g_afsId == 1) {
	if (info != NULL) {
        LogWithNumber(&k_stadium,"stadAfterReadFile: info->id = %d", info->id);
        LogWithNumber(&k_stadium,"stadAfterReadFile: lpBuffer = %08x", (DWORD)lpBuffer);
        LogWithNumber(&k_stadium,"stadAfterReadFile: nNumberOfBytesToRead = %08x", nNumberOfBytesToRead);

        //bool switchStadium = false;
        if (info->id >= data[STAD_FIRST] ) {
            int stadId = GetStadId(info->id);
            int fileId = GetFileId(info->id);
            LogWithNumber(&k_stadium,"stadAfterReadFile: stadium: %d", stadId);
            LogWithString(&k_stadium,"stadAfterReadFile: file: %s", FILE_NAMES[fileId]);
        } else {
            LogWithString(&k_stadium,"stadAfterReadFile: file: %s", FILE_NAMES[ADBOARDS]);
        }

        if (strlen(g_stadFileSpec)>0) {
            // stadium file available: load it into the buffer
            HANDLE hfile;
            DWORD fsize;
            DWORD bytesread;
            hfile = CreateFile(g_stadFileSpec,GENERIC_READ,FILE_SHARE_READ,NULL,
                OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            if (hfile!=INVALID_HANDLE_VALUE) {
                fsize = GetFileSize(hfile,NULL);
                if (multiStepReadNext)
                {
                    // seek proper position
                    DWORD newPos = SetFilePointer(hfile, mit->second.bytesRead, NULL, FILE_BEGIN);
                    LogWithNumber(&k_stadium, "New file pointer: %08x", newPos);
                    fsize -= mit->second.bytesRead;
                    LogWithNumber(&k_stadium, "Bytes remain in the file: %08x", fsize);
                }
                int bytesToRead = min(fsize, nNumberOfBytesToRead);
                LogWithNumber(&k_stadium,"stadAfterReadFile: trying to read %08x bytes...", bytesToRead);
                ZeroMemory(lpBuffer,nNumberOfBytesToRead);
                ReadFile(hfile,lpBuffer,bytesToRead,&bytesread,NULL);
                LogWithNumber(&k_stadium,"stadAfterReadFile: bytesread = %08x", bytesread);

                // check if reached the end
                DWORD curPos = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
                DWORD endPos = SetFilePointer(hfile, 0, NULL, FILE_END);
                bool allRead = curPos == endPos;
                LogWithNumber(&k_stadium, "allRead = %d", allRead);
                CloseHandle(hfile);

                // multi-step reads
                // ================
                if (multiStepReadNext || bytesToRead < fsize)
                {
                    if (multiStepReadNext)
                    {
                        _msrs.erase(mit);
                        LogWithTwoNumbers(&k_stadium, "Erased multi-step-read entry for key: (%08x,%08x)", (DWORD)mit->first.hfile, mit->first.dwOffset);
                    }

                    if (!allRead)
                    {
                        MultiStepReadKey msrKey;
                        msrKey.hfile = hFile;
                        msrKey.dwOffset = _dwOffset1 + bytesread;

                        MultiStepRead msr;
                        msr.dwOffset = _dwOffset1;
                        if (multiStepReadNext)
                            msr.bytesRead = mit->second.bytesRead + bytesread;
                        else
                            msr.bytesRead = bytesread;
                        msr.afsId = g_afsId;
                        msr.fileId = g_fileId;
                        msr.info = info;

                        LogWithTwoNumbers(&k_stadium,"stadAfterReadFile: putting new entry with key: (%08x,%08x)", (DWORD)msrKey.hfile, msrKey.dwOffset);
                        _msrs[msrKey] = msr;
                    }
                    else 
                    {
                        //if (multiStepReadNext)
                        //    __asm { int 3 }
                    }
                }
                else
                    g_stadFileSpec[0]='\0';
            }
            else
                g_stadFileSpec[0]='\0';
        }

        g_afsId = 0xffffffff;
        g_fileId = 0xffffffff;
    }

	CheckViewStadiumMode();

	return;
}

void stadBeginUniSelect()
{
    //Log(&k_stadium, "stadBeginUniSelect");
    
    isSelectMode=true;
    dksiSetMenuTitle("Stadium selection");
    
    // invalidate preview texture
    SafeRelease( &g_preview_tex );
    g_newStad = true;

    return;
}

void stadEndUniSelect()
{
    //Log(&k_stadium, "stadEndUniSelect");
    
    isSelectMode=false;
    
    return;
}

void stadKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	
	if (code1 < 0)
		return;
		
	if ((code1!=HC_ACTION) || !(lParam & 0x80000000))
		return;

	if (isViewStadiumMode) {
		if (wParam == KeySwitchGdbStadiums && hasGdbStadiums) {
			viewGdbStadiums = !viewGdbStadiums;
			needsReload=true;
		
		} else if (wParam == KeySwitchWeather) {
			weather++;
			if (weather>6)
				weather=0;
			
			needsReload=true;

		} else if (wParam == KeyNextStadium && hasGdbStadiums && viewGdbStadiums) {
            g_stadiumMapIterator++;
            if (g_stadiumMapIterator == g_stadiumMap.end()) {
                g_stadiumMapIterator = g_stadiumMap.begin();
            }
			needsReload=true;
        } else if (wParam == KeyPrevStadium && hasGdbStadiums && viewGdbStadiums) {
	        if (g_stadiumMapIterator == g_stadiumMap.begin()) {
	            g_stadiumMapIterator = g_stadiumMap.end();
	        }
	        g_stadiumMapIterator--;
			needsReload=true;
        }
		
		if (needsReload) {
			if (!savedChangeStadiumCode) {
				memcpy(savedStadiumChange,(BYTE*)(code[C_STADIUMCHANGE]),6);
				memcpy(savedAdvanceStadium,(BYTE*)code[C_ADVANCESTADIUM],1);
				savedChangeStadiumCode=true;
			};
			
			//by nopping out these commands, we force a full
			//reload of the stadium with going to the next one
			safeMemset((BYTE*)(code[C_STADIUMCHANGE]),0x90,6);
			safeMemset((BYTE*)(code[C_ADVANCESTADIUM]),0x90,1);
		};
		
		return;
	};	
	

	if (isSelectMode && hasGdbStadiums) {
		if (wParam == KeyNextStadium) {
	        if (g_gameChoice || g_homeTeamChoice) {
	            g_stadiumMapIterator = g_stadiumMap.begin();
	        } else {
	            g_stadiumMapIterator++;
	            if (g_stadiumMapIterator == g_stadiumMap.end()) {
	                g_stadiumMapIterator = g_stadiumMap.begin();
	            }
	        }
	        g_gameChoice = false;
	        g_homeTeamChoice = false;

            // invalidate preview texture
            SafeRelease( &g_preview_tex );
            g_newStad = true;
	
		} else if (wParam == KeyPrevStadium) {
	        if (g_gameChoice || g_homeTeamChoice) {
	            g_stadiumMapIterator = g_stadiumMap.end();
	        }
	        if (g_stadiumMapIterator == g_stadiumMap.begin()) {
	            g_stadiumMapIterator = g_stadiumMap.end();
	        }
	        g_stadiumMapIterator--;
	        g_gameChoice = false;
	        g_homeTeamChoice = false;

            // invalidate preview texture
            SafeRelease( &g_preview_tex );
            g_newStad = true;
	
		} else if (wParam == KeyResetStadium) {
			//reset/homeTeamChoice
	        g_gameChoice = !g_gameChoice;
	        g_homeTeamChoice = !g_gameChoice;

            // invalidate preview texture
            SafeRelease( &g_preview_tex );
            g_newStad = true;
	
		} else if (wParam == KeyRandomStadium) {
            //random
            LARGE_INTEGER num;
            QueryPerformanceCounter(&num);
            int iterations = num.LowPart % MAX_ITERATIONS;
            for (int j=0;j<iterations;j++) {
                g_stadiumMapIterator++;
                if (g_stadiumMapIterator == g_stadiumMap.end()) {
                    g_stadiumMapIterator = g_stadiumMap.begin();
                }
            }
            g_gameChoice = false;
            g_homeTeamChoice = false;

            // invalidate preview texture
            SafeRelease( &g_preview_tex );
            g_newStad = true;
		};
		
		return;
	};
	
	return;
};



void stadShowMenu()
{
	SIZE size;
	DWORD color = 0xffffffff; // white
    char text[512];
	
    ZeroMemory(text, sizeof(text));
	if (g_gameChoice) {
		color = 0xffc0c0c0; // gray if stadium is game choice
        strcpy(text, "Stadium: game choice");

    } else if (g_homeTeamChoice) {
		color = 0xffffffc0; // pale yellow if stadium is home-team choice
        strcpy(text, "Stadium: home team");
    } else {
		//print stadium information
        sprintf(text,"Built: %d",g_stadiumMapIterator->second->built);
		KDrawText(24,576,color,16,text);
		
		sprintf(text,"Capacity: %d",g_stadiumMapIterator->second->capacity);
		KDrawText(24,616,color,16,text);

		if (strlen(g_stadiumMapIterator->second->city)>0) {
			sprintf(text,"City: %s",g_stadiumMapIterator->second->city);
			KDrawText(24,656,color,16,text);
		};
		
		//if possible add some preview images of the stadium here
		
		sprintf(text, "Stadium: %s", g_stadiumMapIterator->first->c_str());
    }
		
	KGetTextExtent(text, 16, &size);
	//draw shadow
	if (!g_gameChoice) {
		KDrawText(
                (GetPESInfo()->bbWidth-size.cx)/2+3*GetPESInfo()->stretchX,GetPESInfo()->bbHeight*0.75+
                3*GetPESInfo()->stretchY,0xff000000,16,text,true
        );
    }
    
	//print stadium label
	KDrawText((GetPESInfo()->bbWidth-size.cx)/2,GetPESInfo()->bbHeight*0.75,color,16,text,true);

    //draw stadium preview
    DrawPreview(g_device);
	return;
};

void stadCreateDevice(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface)
{
    g_device = *ppReturnedDeviceInterface;
    return;
}

void stadPresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
    g_device = self;
	if (!isViewStadiumMode) return;
	
	if (viewGdbStadiums) {
		KDrawText(659,42,0xff000000,16,"1: Stadiums from GDB");
		KDrawText(661,44,0xff000000,16,"1: Stadiums from GDB");
		KDrawText(659,44,0xff000000,16,"1: Stadiums from GDB");
		KDrawText(661,42,0xff000000,16,"1: Stadiums from GDB");
		KDrawText(660,43,0xffffffc0,16,"1: Stadiums from GDB");
	} else {
		KDrawText(659,42,0xffffffc0,16,"1: Ingame stadiums");
		KDrawText(661,44,0xffffffc0,16,"1: Ingame stadiums");
		KDrawText(659,44,0xffffffc0,16,"1: Ingame stadiums");
		KDrawText(661,42,0xffffffc0,16,"1: Ingame stadiums");
		KDrawText(660,43,0xff000000,16,"1: Ingame stadiums");
	};
	
	DWORD color=0xffffffc0;
	DWORD color2=0xff000000;
	
	if (weather==0) {
		color=0xff000000;
		color2=0xffffffc0;
	};

	char buf[1024];
	sprintf(buf,"2: %s",WEATHER_NAMES[weather]);

	KDrawText(659,78,color2,16,buf);
	KDrawText(661,80,color2,16,buf);
	KDrawText(661,78,color2,16,buf);
	KDrawText(659,80,color2,16,buf);
	KDrawText(660,79,color,16,buf);
	
	//print city
	if (viewGdbStadiums && strlen(g_stadiumMapIterator->second->city)>0) {
		sprintf(buf,"City: %s",g_stadiumMapIterator->second->city);
		color=0xff000000; //black font
		color2=0xffffffff; //white border
		KDrawText(52,117,color2,20,buf);
		KDrawText(52,121,color2,20,buf);
		KDrawText(50,119,color2,20,buf);
		KDrawText(54,119,color2,20,buf);
		KDrawText(52,119,color,20,buf);
	};

	return;
};

bool IsStadiumText(char* text) 
{
    DWORD base = *((DWORD*)data[STADIUM_TEXT_TABLE]);
    for (int i=0; i<data[NUM_STADS]; i++) {
        if (base + STADIUM_TEXT_LEN*i == (DWORD)text) return true;
    }
    return false;
}

/* New Reset function */
void stadReset(IDirect3DDevice8* self, LPVOID params)
{
	Log(&k_stadium,"stadReset: cleaning-up.");

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
		Log(&k_stadium,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_stadium,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview->Lock(0, sizeof(g_preview), (BYTE**)&pVertices, 0)))
	{
		Log(&k_stadium,"g_pVB_preview->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview, sizeof(g_preview));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_preview, sizeof(g_preview)/sizeof(CUSTOMVERTEX2), 
            512-64, 628);
	g_pVB_preview->Unlock();

	// preview outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview_outline)))
	{
		Log(&k_stadium,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_stadium,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline->Lock(0, sizeof(g_preview_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_stadium,"g_pVB_preview_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview_outline, sizeof(g_preview_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_preview_outline, sizeof(g_preview_outline)/sizeof(CUSTOMVERTEX), 
          512-65, 627);
	g_pVB_preview_outline->Unlock();

	// preview outline2
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview_outline2), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview_outline2)))
	{
		Log(&k_stadium,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_stadium,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline2->Lock(0, sizeof(g_preview_outline2), (BYTE**)&pVertices, 0)))
	{
		Log(&k_stadium,"g_pVB_preview_outline2->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview_outline2, sizeof(g_preview_outline2));
	SetPosition((CUSTOMVERTEX*)pVertices, g_preview_outline2, sizeof(g_preview_outline2)/sizeof(CUSTOMVERTEX), 
            512-66, 626);
	g_pVB_preview_outline2->Unlock();

    return S_OK;
}

void DrawPreview(IDirect3DDevice8* dev)
{
    if (g_gameChoice) {
        return;
    }

	if (g_needsRestore) 
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_stadium,"DrawPreview: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_stadium,"DrawPreview: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
        D3DVIEWPORT8 vp;
        dev->GetViewport(&vp);
        LogWithNumber(&k_stadium,"VP: %d",vp.X);
        LogWithNumber(&k_stadium,"VP: %d",vp.Y);
        LogWithNumber(&k_stadium,"VP: %d",vp.Width);
        LogWithNumber(&k_stadium,"VP: %d",vp.Height);
        LogWithDouble(&k_stadium,"VP: %f",vp.MinZ);
        LogWithDouble(&k_stadium,"VP: %f",vp.MaxZ);
	}

    char* stadName = (char*)g_stadiumMapIterator->first->c_str();
    if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        std::string* folderString = MAP_FIND(g_HomeStadiumMap,teamId);
        if (folderString != NULL) {
            stadName = (char*)folderString->c_str();
        } else {
            return;
        }
    }

	// render
	dev->BeginScene();

	// setup renderstate
	//dev->CaptureStateBlock( g_dwSavedStateBlock );
	//dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );
    
    if (!g_preview_tex && g_newStad) {
        char buf[2048];
        sprintf(buf, "%s\\GDB\\stadiums\\%s\\preview.png", GetPESInfo()->gdbDir, stadName);
        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                    0, NULL, NULL, &g_preview_tex))) {
            // try "preview.bmp"
            sprintf(buf, "%s\\GDB\\stadiums\\%s\\preview.bmp", GetPESInfo()->gdbDir, stadName);
            if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                        0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                        D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                        0, NULL, NULL, &g_preview_tex))) {
                Log(&k_stadium,"FAILED to load image for stadium preview.");
            }
        }
        g_newStad = false;
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
        //LogWithNumber(&k_stadium,"dev = %08x", (DWORD)dev);
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        //LogWithNumber(&k_stadium,"vtab = %08x", (DWORD)vtab);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            //LogWithNumber(&k_stadium,"vtab[VTAB_DELETESTATEBLOCK] = %08x", (DWORD)vtab[VTAB_DELETESTATEBLOCK]);
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_stadium,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_stadium,"g_dwDrawOverlayStateBlock deleted.");
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
	TRACE(&k_stadium,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_stadium,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // stadium preview
	SafeRelease( &g_pVB_preview );
	SafeRelease( &g_pVB_preview_outline );
	SafeRelease( &g_pVB_preview_outline2 );

	Log(&k_stadium,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_stadium,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");
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
		Log(&k_stadium,"InitVB() failed.");
        return hr;
    }
	Log(&k_stadium,"InitVB() done.");

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

/**
 * Used in "View Stadiums" mode
 */
DWORD stadGetString(DWORD param, DWORD pText)
{
    char* text = (char*)pText;
	if (viewGdbStadiums) {
        if (!needsReload) {
            //advance iterator
            g_stadiumMapIterator++;
            if (g_stadiumMapIterator == g_stadiumMap.end()) {
                g_stadiumMapIterator = g_stadiumMap.begin();
            }
        }
        text = (char*)g_stadiumMapIterator->first->c_str();
    }
    LogWithString(&k_stadium, "stadGetString: text = %s", text);

    // call the original
    pText = (DWORD)text;
    DWORD result = MasterCallNext(param, pText);

    return result;
}

DWORD stadGetString2(DWORD ppText, DWORD p1, DWORD p2, DWORD p3, 
        DWORD p4, DWORD p5, DWORD p6, DWORD p7)
{
    LogWithString(&k_stadium, "stadGetString2: ppText = %s", *(char**)ppText);
    char* text = *(char**)ppText;
    strcpy(g_stadiumText, text);

    if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        LogWithNumber(&k_stadium, "stadGetString2: home team = %d", teamId);
        std::string* folderString = MAP_FIND(g_HomeStadiumMap,teamId);
        if (folderString != NULL) {
            LogWithString(&k_stadium, "stadGetString2: has a home stadium: %s", 
                    (char*)folderString->c_str());
            strcpy(text, (char*)folderString->c_str());
        }
    } else if (!g_gameChoice) {
        strcpy(text, g_stadiumMapIterator->first->c_str());
    }
    LogWithString(&k_stadium, "stadGetString2: ppText = %s", *(char**)ppText);

    // call the original
    DWORD result = MasterCallNext(ppText, p1,p2,p3,p4,p5,p6,p7);

    strcpy(text, g_stadiumText);
    //LogWithString(&k_stadium, "stadGetString2: ppText = %s", *(char**)ppText);

    return result;
}

DWORD stadWriteBuilt(char* dest, char* format, DWORD num)
{
    if (viewGdbStadiums) {
        num = g_stadiumMapIterator->second->built;
    }

    // call the original
    DWORD result = MasterCallNext(dest, format, num);

    return result;
}

DWORD stadWriteCapacity(char* dest, char* format, DWORD num)
{
    if (viewGdbStadiums) {
        num = g_stadiumMapIterator->second->capacity;
    }

    // call the original
    DWORD result = MasterCallNext(dest, format, num);

	if (needsReload) {
		//time to write the commands back
		memcpy((BYTE*)code[C_STADIUMCHANGE],savedStadiumChange,6);
		memcpy((BYTE*)code[C_ADVANCESTADIUM],savedAdvanceStadium,1);
		needsReload=false;
	};

	CheckViewStadiumMode();
    
    return result;
}

void CheckViewStadiumMode()
{	
	if (data[ISVIEWSTADIUMMODE]!=0 && *(BYTE*)data[ISVIEWSTADIUMMODE] != 0) {
		if (!isViewStadiumMode) {
			//reset parameters if reentering View Stadium mode
			viewGdbStadiums=false;
			weather=0;
			g_stadiumMapIterator = g_stadiumMap.begin();
		};
		isViewStadiumMode=true;
	} else
		isViewStadiumMode=false;
		
	return;
};
	
DWORD stadSetLCM(DWORD p1)
{
	stadEndUniSelect();
	
	LCM* lcm=(LCM*)data[TEAM_IDS];
	
    DWORD result = MasterCallNext(p1);
 	
	lcm->effects=1;
	
	if (weather==0) {
        return result;
    }
	
	//day or night
	if (weather<4)
		lcm->timeOfDay=0;
	else
		lcm->timeOfDay=1;
		
	//weather
	lcm->weather = (weather-1) % 3;
	
	//if snow, then we need winter, else summer
	if (lcm->weather==2)
		lcm->season=1;
	else
		lcm->season=0;
	
	//doesn't work
	//lcm->crowdStance=1;
	//lcm->homeCrowd=1;
	
	return result;
};

bool stadReadNumPages(DWORD afsId, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages)
{
    DWORD fileSize = 0;
    //LogWithNumber(&k_stadium, "stadReadNumPages CALLED with afsId=%d",
    //        afsId);

    g_afsId = afsId;
    g_fileId = fileId;

    if (!bOthersHooked) {
        MasterHookFunction(code[C_GETSTRING_CS], 2, stadGetString);
        Log(&k_stadium, "hooked GetString");
        MasterHookFunction(code[C_GETSTRING2_CS], 8, stadGetString2);
        Log(&k_stadium, "hooked GetString2");

        MasterHookFunction(code[C_WRITENUM_CS1], 3, stadWriteBuilt);
        Log(&k_stadium, "hooked WriteBuilt");
        MasterHookFunction(code[C_WRITENUM_CS2], 3, stadWriteCapacity);
        Log(&k_stadium, "hooked WriteCapacity");

        MasterHookFunction(code[C_SETLCM_CS], 1, stadSetLCM);
        Log(&k_stadium, "hooked SetLCM");

        bOthersHooked = true;
    }

    if (afsId == 1) { // 0_text.afs
        if (MAP_CONTAINS(g_AFS_idMap, fileId)) {
            LogWithTwoNumbers(&k_stadium,"stadReadNumPages: afsId=%d, fileId=%d", afsId, fileId);
            if (fileId < data[STAD_FIRST]) {
                // adboard textures
                if (g_stadId != 0xffffffff) {
                    // we know the stadium id

                    // force Della Alpi adboards
                    //fileId = data[DELLA_ALPI_ADBOARDS];

                    //g_stadFileSpec[0]='\0';
                    fileSize = FindAdboardsFile(g_stadFileSpec);
                }
            } else {
                // stadium files

                // check if stadium file exists
                int stadId = GetStadId(fileId);
                int stadFileId = GetFileId(fileId);

                LogWithTwoNumbers(&k_stadium,"stadReadNumPages: stadId=%d, stadFileId=%d", stadId, stadFileId);

                // force Della Alpi stadium
                //fileId = stadFileId + data[DELLA_ALPI];

                // remember current stadium ID
                if (STAD_MAIN(stadFileId)) {
                    g_stadId = stadId;
                }
                
                fileSize = FindStadiumFile(stadFileId, g_stadFileSpec);
            }

            if (fileSize > 0) {
                LogWithString(&k_stadium, "stadReadNumPages: found GDB stadium file: %s", g_stadFileSpec);

                LogWithTwoNumbers(&k_stadium,"stadReadNumPages: had size: %08x pages (%08x bytes)", 
                        orgNumPages, orgNumPages*0x800);

                // adjust buffer size to fit GDB stadium file
                *numPages = fileSize/0x800 + ((fileSize&0x7ff)!=0);
                LogWithTwoNumbers(&k_stadium,"stadReadNumPages: new size: %08x pages (%08x bytes)", 
                        *numPages, (*numPages)*0x800);
                return true;
            }
        }
    }
    return false;
}

