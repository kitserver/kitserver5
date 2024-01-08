// scoreboard.cpp
#include <windows.h>
#include <stdio.h>
#include "scoreboard.h"
#include "kload_exp.h"
#include "afsreplace.h"
#include <map>
#include <string>

KMOD k_scrsrv={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

SCRSRV_CFG scrsrv_config = {
    DEFAULT_SELECTED_SCR,
    DEFAULT_PREVIEW_ENABLED,
    DEFAULT_KEY_RESET,
    DEFAULT_KEY_RANDOM,
    DEFAULT_KEY_PREV,
    DEFAULT_KEY_NEXT,
    DEFAULT_AUTO_RANDOM_MODE,
};


DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;

#define DATALEN 19
enum {
    SCR_GRAPHICS, SCR_FIRST_OPD, TOTAL_SCR_OPD, 
	CHAMPION_OPD, TOTAL_CHAMP_OPD,
	ENTRY_GRAPHICS, ENTRY_EXH_OPD, ENTRY_L_OPD, TOTAL_ENTRY_L_OPD, ENTRY_C_OPD, TOTAL_ENTRY_C_OPD,
	SUBS_BOARD,
	FLAG_4040, FLAG_2020,
	FORM_SCREEN, LOADING_SCREEN_0, LOADING_SCREEN_1, RESULT, SCORE_FONT,
};

static DWORD dtaArray[][DATALEN] = {
	// PES5 DEMO 2
	{
		0, 0, 0,
		0, 0,
		0, 0, 0, 0, 0, 0,
		0,
		0, 0,
		0, 0, 0, 0, 0,
	},
	// PES5
	{
		113, 498, 15, 
		147, 14,
		6, 182, 212, 7, 232, 7,
		51,
		466, 464,
		91, 114, 115, 118, 497,
	},
	// WE9
	{
		113, 498, 15, 
		147, 14,
		6, 182, 212, 7, 232, 7,
		51,
		466, 464,
		91, 114, 115, 118, 497,
	},
	// WE9:LE
	{
		112, 505, 15, 
		153, 14,
		5, 188, 218, 7, 238, 7,
		57,
		473, 471,
		90, 113, 114, 117, 504,
	},
};

static DWORD dta[DATALEN];

static char* GAME_FOLDER[] = {
    "\\pes5_we9\\",
    "\\pes5_we9\\",
    "\\pes5_we9\\",
    "\\we9le\\",
};




static char* FILE_NAMES[] = {
    "game_2d.txs",
    "game_2d.opd",
	"champion.opd",
	"enter.txs",
	"enter.opd",
	"subs_board.bin",
	"flags_big.flg",
	"flags_small.flg",
	"form.txs",
	"loading_screen_0.txs",
	"loading_screen_1.txs",
	"result.txs",
	"score_font.fnt",
};
enum {
	SCOREBOARD_TEXTURE,
	SCOREBOARD_OPD,
	CHAMPION,
	ENTER_TEXTURE,
	ENTER_OPD,
	SUBS_BOARD_MDL,
	FLAGS_BIG,
	FLAGS_SMALL,
	FORM,
	L_SCREEN_0,
	L_SCREEN_1,
	RESULT_TXS,
	EXTRA_FONT,
};


#define VECTOR_FIND(vector,id) ((id<vector.size() || id>=0) ? vector[id] : NULL)

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

int selectedScr=-1;
vector<string*> g_scoreboardsVector;

bool isSelectMode=false;
bool autoRandomMode = false;
static bool g_userChoice = true;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
bool readConfig(SCRSRV_CFG* config, char* cfgFile);
void populateScoreboardsVector(vector<string*>& vec, string& currDir);
void InitModule();
bool scrsrvReadNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD* numPages);
bool scrsrvAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
void SetScr(DWORD id);
void scrsrvKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void scrsrvUniSelect();
void scrsrvShowMenu();
void scrsrvBeginUniSelect();
void scrsrvEndUniSelect();

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
		Log(&k_scrsrv,"Attaching dll...");


		int v=GetPESInfo()->GameVersion;
		switch (v){
			case gvPES5PC: //support for PES5 PC...
			case gvWE9PC: //support for WE9 PC...
			case gvWE9LEPC: //... and WE9:LE PC
				goto GameVersIsOK;
				break;
			default:
				Log(&k_scrsrv,"Your game version is currently not supported!");
				return false;
		}

		//Everything is OK!
		GameVersIsOK:

		hInst=hInstance;
		RegisterKModule(&k_scrsrv);

		// initialize addresses
		memcpy(dta, dtaArray[v], sizeof(dta));

		HookFunction(hk_D3D_Create,(DWORD)InitModule);
		HookFunction(hk_GetNumPages,(DWORD)scrsrvReadNumPages);
		HookFunction(hk_AfterReadFile,(DWORD)scrsrvAfterReadFile);
	    HookFunction(hk_Input,(DWORD)scrsrvKeyboardProc);
		HookFunction(hk_BeginUniSelect, (DWORD)scrsrvUniSelect);
		HookFunction(hk_D3D_CreateDevice,(DWORD)CustomGraphicCreateDevice);
		HookFunction(hk_DrawKitSelectInfo,(DWORD)scrsrvShowMenu);
		HookFunction(hk_OnShowMenu,(DWORD)scrsrvBeginUniSelect);
        HookFunction(hk_OnHideMenu,(DWORD)scrsrvEndUniSelect);
		HookFunction(hk_D3D_Reset,(DWORD)CustomGraphicReset);
	}
	else if (dwReason == DLL_PROCESS_DETACH){
		Log(&k_scrsrv,"Detaching dll...");
		
		//save settings
        char scrCfg[BUFLEN];
		scrsrv_config.selectedScr=selectedScr;
        scrsrv_config.autoRandomMode = autoRandomMode;
	    ZeroMemory(scrCfg, BUFLEN);
	    sprintf(scrCfg, "%s\\scoreboard.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(scrCfg, "wb");
	    if (f) {
	        fwrite(&scrsrv_config, sizeof(SCRSRV_CFG), 1, f);
	        fclose(f);
			Log(&k_scrsrv,"Preferences saved.");
	    }

		//Unhooking everything

		UnhookFunction(hk_GetNumPages,(DWORD)scrsrvReadNumPages);
		UnhookFunction(hk_AfterReadFile,(DWORD)scrsrvAfterReadFile);
		UnhookFunction(hk_Input,(DWORD)scrsrvKeyboardProc);
		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)scrsrvShowMenu);
		UnhookFunction(hk_D3D_Reset,(DWORD)CustomGraphicReset);

		// free the vector of scoreboards
		g_scoreboardsVector.clear();
		


		Log(&k_scrsrv,"Detaching done.");
	}
	return true;
}

void InitModule(){

	UnhookFunction(hk_D3D_Create,(DWORD)InitModule);

    //load settings
    char scrCfg[BUFLEN];
    ZeroMemory(scrCfg, BUFLEN);
    sprintf(scrCfg, "%s\\scoreboard.dat", GetPESInfo()->mydir);
    LOG(&k_scrsrv, "reading: %s", scrCfg);
    FILE* f = fopen(scrCfg, "rb");
    if (f) {
        fread(&scrsrv_config, sizeof(SCRSRV_CFG), 1, f);
        fclose(f);
        autoRandomMode = scrsrv_config.autoRandomMode;
    } else {
        scrsrv_config.selectedScr=-1;
        g_userChoice = false;
    };
    LOG(&k_scrsrv, "selected scoreboard: %d", scrsrv_config.selectedScr);

    //read preview and virtual keys settings
    scrsrv_config.previewEnabled = TRUE;
    ZeroMemory(scrCfg, BUFLEN);
    sprintf(scrCfg, "%s\\scoreboard.cfg", GetPESInfo()->mydir);
    readConfig(&scrsrv_config, scrCfg);
	// Searching for folders inside the scoreboard folder
	populateScoreboardsVector(g_scoreboardsVector, string(""));
	LogWithNumber(&k_scrsrv, "Total # of scoreboards found: %d", g_scoreboardsVector.size());
	
    //ReadScoreboards();
    SetScr(scrsrv_config.selectedScr);
	
	Log(&k_scrsrv, "Module initialized.");
}


void populateScoreboardsVector(vector<string*>& vec, string& currDir)
{
    // traverse the "scoreboards" folder and place all the folders, with paths
    // relative to "scoreboards" into the vector.

    WIN32_FIND_DATA fData;
    char pattern[512] = {0};
    ZeroMemory(pattern, sizeof(pattern));

    sprintf(pattern, "%sGDB\\scoreboards\\%s*",
            GetPESInfo()->gdbDir, currDir.c_str());
    LOG(&k_scrsrv, "pattern: {%s}", pattern);

    HANDLE hff = FindFirstFile(pattern, &fData);
    if (hff != INVALID_HANDLE_VALUE) {
        while(true) {
            // check if this is a directory
            if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // check for system folders
                if (fData.cFileName[0]!='.') {
					std::string* mValue = NULL;
					mValue = new std::string(currDir + string(fData.cFileName));
					vec.push_back(mValue);
					LogWithString(&k_scrsrv,
							"Found scoreboard: %s", (char*)vec.back()->c_str());
                }
            }
            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);
    }
}


bool scrsrvReadNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages)
{
    DWORD fileSize = 0;
    char filename[512] = {0};
	std::string* folderString = VECTOR_FIND(g_scoreboardsVector, selectedScr);
	if (folderString != NULL) {
		if (afsId == 1) { // 0_text.afs
			if (fileId >= dta[SCR_FIRST_OPD] && fileId < dta[SCR_FIRST_OPD] + dta[TOTAL_SCR_OPD]) {
				// game_2d.opd
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[SCOREBOARD_OPD]);
				//////////////////////////////////
			}else if (fileId >= dta[CHAMPION_OPD] && fileId < dta[CHAMPION_OPD] + dta[TOTAL_CHAMP_OPD]){
				// champion.opd
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[CHAMPION]);
				//////////////////////////////////
			}else if (
				(fileId == dta[ENTRY_EXH_OPD]) || // Exhibition entry opd
				(fileId >= dta[ENTRY_L_OPD] && fileId < dta[ENTRY_L_OPD] + dta[TOTAL_ENTRY_L_OPD]) || // League entry opds
				(fileId >= dta[ENTRY_C_OPD] && fileId < dta[ENTRY_C_OPD] + dta[TOTAL_ENTRY_C_OPD]) // Cup entry opds
				){
				// enter.opd
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[ENTER_OPD]);
			}else if (fileId == dta[SUBS_BOARD]){
				// subs_board.bin
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[SUBS_BOARD_MDL]);
			}else if (fileId == dta[FLAG_4040]){
				// flags_small.flg
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[FLAGS_BIG]);
			}else if (fileId == dta[FLAG_2020]){
				// flags_big.flg
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[FLAGS_SMALL]);
			}else if (fileId == dta[SCORE_FONT]){
				// score_font.fnt
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[EXTRA_FONT]);
			}


			HANDLE TempHandle=CreateFile(filename,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
			if (TempHandle!=INVALID_HANDLE_VALUE) {
				fileSize=GetFileSize(TempHandle,NULL);
				CloseHandle(TempHandle);
			} else {
				fileSize=0;
			};

			if (fileSize > 0) {
				LogWithString(&k_scrsrv, "scrsrvReadNumPages: found file: %s", filename);
				LogWithTwoNumbers(&k_scrsrv,"scrsrvReadNumPages: had size: %08x pages (%08x bytes)", 
						orgNumPages, orgNumPages*0x800);

				// adjust buffer size to fit file
				*numPages = ((fileSize-1)>>0x0b)+1;
				LOG(&k_scrsrv,
						"scrsrvReadNumPages: new size: %08x pages (%08x bytes)", 
						*numPages, (*numPages)*0x800);
				return true;
			}
		}else if(afsId == 3){ // x_text.afs

			if (fileId == dta[SCR_GRAPHICS]) {
				// game_2d.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[SCOREBOARD_TEXTURE]);
				//////////////////////////////////
			}else if (fileId == dta[ENTRY_GRAPHICS]) {
				// enter.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[ENTER_TEXTURE]);
			}else if (fileId == dta[FORM_SCREEN]) {
				// form.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[FORM]);
			}else if (fileId == dta[LOADING_SCREEN_0]) {
				// loading_screen_0.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[L_SCREEN_0]);
			}else if (fileId == dta[LOADING_SCREEN_1]) {
				// loading_screen_1.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[L_SCREEN_1]);
			}else if (fileId == dta[RESULT]) {
				// result.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[RESULT_TXS]);
			}
			
			HANDLE TempHandle=CreateFile(filename,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
			if (TempHandle!=INVALID_HANDLE_VALUE) {
				fileSize=GetFileSize(TempHandle,NULL);
				CloseHandle(TempHandle);
			} else {
				fileSize=0;
			};

			if (fileSize > 0) {
				LogWithString(&k_scrsrv, "scrsrvReadNumPages: found file: %s", filename);
				LogWithTwoNumbers(&k_scrsrv,"scrsrvReadNumPages: had size: %08x pages (%08x bytes)", 
						orgNumPages, orgNumPages*0x800);

				// adjust buffer size to fit file
				*numPages = ((fileSize-1)>>0x0b)+1;
				LOG(&k_scrsrv,
						"scrsrvReadNumPages: new size: %08x pages (%08x bytes)", 
						*numPages, (*numPages)*0x800);
				return true;
			}

		}

	}
	return false;
}


bool scrsrvAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped){
	DWORD afsId = GetAfsIdByOpenHandle(hFile);
    DWORD filesize = 0;
    char filename[512] = {0};
	std::string* folderString = VECTOR_FIND(g_scoreboardsVector, selectedScr);
	if (folderString != NULL) {
		// ensure it is 0_TEXT.afs
		if (afsId == 1) {
			DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - (*lpNumberOfBytesRead);
			DWORD fileId = GetProbableFileIdForHandle(afsId, offset, hFile);
			if (fileId >= dta[SCR_FIRST_OPD] && fileId < dta[SCR_FIRST_OPD] + dta[TOTAL_SCR_OPD]) {
				// game_2d.opd
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[SCOREBOARD_OPD]);
				LOG(&k_scrsrv, 
				"--> READING game_2d opd (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId >= dta[CHAMPION_OPD] && fileId < dta[CHAMPION_OPD] + dta[TOTAL_CHAMP_OPD]){
				// champion.opd
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[CHAMPION]);
				LOG(&k_scrsrv, 
				"--> READING champion.opd (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (
				(fileId == dta[ENTRY_EXH_OPD]) || // Exhibition entry opd
				(fileId >= dta[ENTRY_L_OPD] && fileId < dta[ENTRY_L_OPD] + dta[TOTAL_ENTRY_L_OPD]) || // League entry opds
				(fileId >= dta[ENTRY_C_OPD] && fileId < dta[ENTRY_C_OPD] + dta[TOTAL_ENTRY_C_OPD]) // Cup entry opds
				){
				// enter.opd
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[ENTER_OPD]);
				LOG(&k_scrsrv, 
				"--> READING enter.opd (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[SUBS_BOARD]){
				// subs_board.bin
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[SUBS_BOARD_MDL]);
				LOG(&k_scrsrv, 
				"--> READING subs_board.bin (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[FLAG_4040]){
				// subs_board.bin
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[FLAGS_BIG]);
				LOG(&k_scrsrv, 
				"--> READING FLAGS_BIG.bin (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[FLAG_2020]){
				// subs_board.bin
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[FLAGS_SMALL]);
				LOG(&k_scrsrv, 
				"--> READING subs_board.bin (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[SCORE_FONT]){
				// subs_board.bin
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[EXTRA_FONT]);
				LOG(&k_scrsrv, 
				"--> READING subs_board.bin (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}
						
			else{
				return false;
			}
			LOG(&k_scrsrv, "loading (%s)", filename);
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
				LOG(&k_scrsrv, 
				"offset=%08x, GetOffsetByFileId()=%08x, curr_offset=%08x",
				offset, GetOffsetByFileId(afsId, fileId), curr_offset);
				DWORD bytesToRead = *lpNumberOfBytesRead;
				if (filesize < curr_offset + *lpNumberOfBytesRead) {
					bytesToRead = filesize - curr_offset;
				}
				DWORD bytesRead = 0;
				SetFilePointer(handle, curr_offset, NULL, FILE_BEGIN);
				LOG(&k_scrsrv, "reading 0x%x bytes (0x%x) from {%s}", 
				bytesToRead, *lpNumberOfBytesRead, filename);
				ReadFile(handle, lpBuffer, bytesToRead, &bytesRead, lpOverlapped);
				LOG(&k_scrsrv, "read 0x%x bytes from {%s}", bytesRead, filename);
				if (*lpNumberOfBytesRead - bytesRead > 0) {
					DEBUGLOG(&k_scrsrv, "zeroing out 0x%0x bytes",
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
				LOG(&k_scrsrv, "Unable to read file {%s} must be protected or opened", filename);
			}
			return result;
		}else if(afsId == 3){ // x_text.afs
			DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - (*lpNumberOfBytesRead);
			DWORD fileId = GetProbableFileIdForHandle(afsId, offset, hFile);
			if (fileId == dta[SCR_GRAPHICS]) {
				// game_2d.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[SCOREBOARD_TEXTURE]);
				LOG(&k_scrsrv, 
				"--> READING game_2d texture (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[ENTRY_GRAPHICS]) {
				// enter.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[ENTER_TEXTURE]);
				LOG(&k_scrsrv, 
				"--> READING enter texture (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[FORM_SCREEN]) {
				// FORM.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[FORM]);
				LOG(&k_scrsrv, 
				"--> READING FORM texture (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[LOADING_SCREEN_0]) {
				// enter.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[L_SCREEN_0]);
				LOG(&k_scrsrv, 
				"--> READING LOADING_SCREEN_0 (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[LOADING_SCREEN_1]) {
				// enter.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[L_SCREEN_1]);
				LOG(&k_scrsrv, 
				"--> READING LOADING_SCREEN_1 texture (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}else if (fileId == dta[RESULT]) {
				// enter.txs
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\scoreboards\\");
				strcat(filename,(char*)folderString->c_str());
				strcat(filename,GAME_FOLDER[GetPESInfo()->GameVersion]);
				strcat(filename,FILE_NAMES[RESULT_TXS]);
				LOG(&k_scrsrv, 
				"--> READING RESULT texture (%d): offset:%08x, bytes:%08x <--",
				fileId, offset, *lpNumberOfBytesRead);
			}
			
			
			
			
			else{
				return false;
			}
			LOG(&k_scrsrv, "loading (%s)", filename);
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
				LOG(&k_scrsrv, 
				"offset=%08x, GetOffsetByFileId()=%08x, curr_offset=%08x",
				offset, GetOffsetByFileId(afsId, fileId), curr_offset);
				DWORD bytesToRead = *lpNumberOfBytesRead;
				if (filesize < curr_offset + *lpNumberOfBytesRead) {
					bytesToRead = filesize - curr_offset;
				}
				DWORD bytesRead = 0;
				SetFilePointer(handle, curr_offset, NULL, FILE_BEGIN);
				LOG(&k_scrsrv, "reading 0x%x bytes (0x%x) from {%s}", 
				bytesToRead, *lpNumberOfBytesRead, filename);
				ReadFile(handle, lpBuffer, bytesToRead, &bytesRead, lpOverlapped);
				LOG(&k_scrsrv, "read 0x%x bytes from {%s}", bytesRead, filename);
				if (*lpNumberOfBytesRead - bytesRead > 0) {
					DEBUGLOG(&k_scrsrv, "zeroing out 0x%0x bytes",
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
				LOG(&k_scrsrv, "Unable to read file {%s} must be protected or opened", filename);
			}
			return result;
		}
	}
	return false;
}


void SetScr(DWORD id)
{
	char tmp[BUFLEN];
	selectedScr = (id<g_scoreboardsVector.size() && id>=0) ?  id : -1;

	if (selectedScr>=0) {
		SafeRelease( &g_preview_tex );
		g_newPrev=true;
	};
	
	return;
};


void scrsrvKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	if ((!isSelectMode) || (code1 < 0))
		return; 

	if ((code1==HC_ACTION) && (lParam & 0x80000000)) {
		if (wParam == scrsrv_config.keyNext) {
			autoRandomMode = false;
			SetScr(selectedScr+1);
			g_userChoice = true;
		} 
		else if (wParam == scrsrv_config.keyPrev) {
			autoRandomMode = false;
			if (selectedScr<0)
				SetScr(g_scoreboardsVector.size()-1);
			else
				SetScr(selectedScr-1);
			g_userChoice = true;
		} 
		else if (wParam == scrsrv_config.keyReset) {
			SetScr(-1);
			if (!autoRandomMode) {
        		autoRandomMode = true;
        		g_userChoice = true;
        	} 
			else {
				SetScr(-1);
				g_userChoice = false;
				autoRandomMode = false;
			}
			
			if (autoRandomMode) {
				LARGE_INTEGER num;
				QueryPerformanceCounter(&num);
				int iterations = num.LowPart % MAX_ITERATIONS;
				for (int j=0;j<iterations;j++) {
					SetScr(selectedScr+1);
		        }
			}

		} 
		else if (wParam == scrsrv_config.keyRandom) {
			autoRandomMode = true;
			LARGE_INTEGER num;
			QueryPerformanceCounter(&num);
			int iterations = num.LowPart % MAX_ITERATIONS;
			for (int j=0;j<iterations;j++)
				SetScr(selectedScr+1);
			g_userChoice = true;
		}
	}
	
	return;
};


void scrsrvUniSelect()
{

	if (autoRandomMode) {
		LARGE_INTEGER num;
		QueryPerformanceCounter(&num);
		int iterations = num.LowPart % MAX_ITERATIONS;
		for (int j=0;j<iterations;j++) {
			SetScr(selectedScr+1);
        }
	}
	return;
}


void scrsrvShowMenu()
{
	SIZE size;
	DWORD color = 0xffffffff; // white
    char text[512];
	ZeroMemory(text, sizeof(text));
	if (selectedScr<0){
		strcpy(text, "Scoreboard: game choice");
	}else{
		sprintf(text, "Scoreboard: %s", g_scoreboardsVector[selectedScr]->c_str());
	}
	UINT g_bbWidth=GetPESInfo()->bbWidth;
	UINT g_bbHeight=GetPESInfo()->bbHeight;
	double stretchX=GetPESInfo()->stretchX;
	double stretchY=GetPESInfo()->stretchY;
	if (selectedScr<0) {
		color = 0xffc0c0c0; // gray if is game choice
    } 
	if (autoRandomMode)
		color = 0xffaad0ff; // light blue for randomly selected kit

	KGetTextExtent(text,16,&size);
	KDrawText((g_bbWidth-size.cx)/2,g_bbHeight*0.75,color,16,text,true);


    //draw Scr preview
    if (scrsrv_config.previewEnabled) {
		DrawPreview(g_device);
    }
	return;

};


void scrsrvBeginUniSelect()
{
   
    isSelectMode=true;
    dksiSetMenuTitle("Scoreboard selection");
    
    // invalidate preview texture
    SafeRelease(&g_preview_tex );
    g_newPrev = true;


    return;
}

void scrsrvEndUniSelect()
{
    
    isSelectMode=false;

}


/**
 * Returns true if successful.
 */
bool readConfig(SCRSRV_CFG* config, char* cfgFile)
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
			LogWithNumber(&k_scrsrv,"ReadConfig: preview = (%d)", value);
            config->previewEnabled = (value == 1);
		}
        else if (lstrcmpi(name, "key.reset")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_scrsrv,"ReadConfig: key.reset = (0x%02x)", value);
            config->keyReset = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.random")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_scrsrv,"ReadConfig: key.random = (0x%02x)", value);
            config->keyRandom = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.prev")==0 || lstrcmpi(name, "key.previous")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_scrsrv,"ReadConfig: key.prev = (0x%02x)", value);
            config->keyPrev = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.next")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_scrsrv,"ReadConfig: key.next = (0x%02x)", value);
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
            Log(&k_scrsrv,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_scrsrv,"Problem with safe-release");
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
	Log(&k_scrsrv,"CustomGraphicReset: cleaning-up.");

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
		Log(&k_scrsrv,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_scrsrv,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview->Lock(0, sizeof(g_preview), (BYTE**)&pVertices, 0)))
	{
		Log(&k_scrsrv,"g_pVB_preview->Lock() failed.");
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
		Log(&k_scrsrv,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_scrsrv,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline->Lock(0, sizeof(g_preview_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_scrsrv,"g_pVB_preview_outline->Lock() failed.");
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
		Log(&k_scrsrv,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_scrsrv,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline2->Lock(0, sizeof(g_preview_outline2), (BYTE**)&pVertices, 0)))
	{
		Log(&k_scrsrv,"g_pVB_preview_outline2->Lock() failed.");
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
	if (selectedScr<0) return;
	if (g_needsRestore) 
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_scrsrv,"DrawPreview: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_scrsrv,"DrawPreview: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
        D3DVIEWPORT8 vp;
        dev->GetViewport(&vp);
        LogWithNumber(&k_scrsrv,"VP: %d",vp.X);
        LogWithNumber(&k_scrsrv,"VP: %d",vp.Y);
        LogWithNumber(&k_scrsrv,"VP: %d",vp.Width);
        LogWithNumber(&k_scrsrv,"VP: %d",vp.Height);
        LogWithDouble(&k_scrsrv,"VP: %f",vp.MinZ);
        LogWithDouble(&k_scrsrv,"VP: %f",vp.MaxZ);
	}

	// render
	dev->BeginScene();
	std::string* folderString = g_scoreboardsVector[selectedScr];
	// setup renderstate
	//dev->CaptureStateBlock( g_dwSavedStateBlock );
	//dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );
    
    if (!g_preview_tex && g_newPrev) {
        char buf[2048];
        sprintf(buf, "%s\\GDB\\scoreboards\\%s\\preview.png", GetPESInfo()->gdbDir, folderString);
        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                    0, NULL, NULL, &g_preview_tex))) {
            // try "preview.bmp"
            sprintf(buf, "%s\\GDB\\scoreboards\\%s\\preview.bmp", GetPESInfo()->gdbDir, folderString);
            if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                        0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                        D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                        0, NULL, NULL, &g_preview_tex))) {
                Log(&k_scrsrv,"FAILED to load image for referee kits preview.");
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
        //LogWithNumber(&k_scrsrv,"dev = %08x", (DWORD)dev);
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        //LogWithNumber(&k_scrsrv,"vtab = %08x", (DWORD)vtab);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            //LogWithNumber(&k_scrsrv,"vtab[VTAB_DELETESTATEBLOCK] = %08x", (DWORD)vtab[VTAB_DELETESTATEBLOCK]);
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_scrsrv,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_scrsrv,"g_dwDrawOverlayStateBlock deleted.");
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
	TRACE(&k_scrsrv,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_scrsrv,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // referee kits preview
	SafeRelease( &g_pVB_preview );
	SafeRelease( &g_pVB_preview_outline );
	SafeRelease( &g_pVB_preview_outline2 );

	Log(&k_scrsrv,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_scrsrv,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");
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
		Log(&k_scrsrv,"InitVB() failed.");
        return hr;
    }
	Log(&k_scrsrv,"InitVB() done.");

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