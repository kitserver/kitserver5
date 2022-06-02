// referees.cpp
#include <windows.h>
#include <stdio.h>
#include "referees.h"
#include "kload_exp.h"
#include "afsreplace.h"

KMOD k_referees={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;

#define DATALEN 15
enum {
    REF_START_ADDRESS, REF_DATA_SIZE, 
	SELECTED_REF, SELECT_REF_FLAG, SELECT_TOTAL_REF,
	REF_FACE_W, REF_FACE_Y, REF_FACE_R, REF_FACE_K,
	REF_FACE_W_ID, REF_FACE_Y_ID, REF_FACE_R_ID, REF_FACE_K_ID,
	REF_HAIRFILE, REF_HAIRFILE_ID, 
};

static DWORD dtaArray[][DATALEN] = {
	// PES5 DEMO 2
	{
		0, 0, 
		0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0, 
		0, 0,
	},
	// PES5
	{
		0xff6bb0, 104, 
		0xfe0d6c, 0xfe0d78, 0xfe0d74, // SELECTED_REF, SELECT_REF_FLAG are not used anymore
		2212, 2357, 1953, 1896,
		258, 138, 56, 31, 
		4123, 1299, //Last dog hairstyle
	},
	// WE9
	{
		0xff6bb0, 104, 
		0xfe0d6c, 0xfe0d78, 0xfe0d74,
		2212, 2357, 1953, 1896,
		258, 138, 56, 31, 
		4123, 1299, //Last dog hairstyle
	},
	// WE9:LE
	{
		0xf77650, 104, 
		0, 0, 0xf1ad04,
		2219, 2364, 1960, 1903,
		258, 138, 56, 31, 
		4130, 1299, //Last dog hairstyle
	},
};

static DWORD dta[DATALEN];
#define FREE_NATIONALITY 108
#define TOTAL_NATIONALITIES 109

const string nationalities[] = {"Austria", "Belgium", "Bulgaria", "Croatia", "Czech Republic", "Denmark", "England", "Finland", "France", "Germany", "Greece", 
"Hungary", "Ireland", "Italy", "Latvia", "Netherlands", "Northern Ireland", "Norway", "Poland", "Portugal", "Romania", "Russia", "Scotland", "Serbia and Montenegro", 
"Slovakia", "Slovenia", "Spain", "Sweden", "Switzerland", "Turkey", "Ukraine", "Wales", "Cameroon", "Cote d'Ivoire", "Morocco", "Nigeria", 
"Senegal", "South Africa", "Tunisia", "Costa Rica", "Mexico", "USA", "Argentina", "Brazil", "Chile", "Colombia", 
"Ecuador", "Paraguay", "Peru", "Uruguay", "Venezuela", "China", "Iran", "Japan", "Saudi Arabia", "South Korea", "Australia", "Albania", "Armenia", "Belarus", 
"Bosnia and Herzegovina", "Cyprus", "Georgia", "Estonia", "Faroe Islands", "Iceland", "Israel", "Lithuania", "Luxembourg", "Macedonia", "Moldova", "Algeria", 
"Angola", "Burkina Faso", "Cape Verde", "Congo", "DR Congo", "Egypt", "Equatorial Guinea", "Gabon", "Gambia", "Ghana", "Guinea", "Guinea-Bissau", "Liberia", 
"Libya", "Mali", "Mauritius", "Mozambique", "Namibia", "Sierra Leone", "Togo", "Zambia", "Zimbabwe", "Canada", "Grenada", "Guadeloupe", "Guatemala", "Honduras", 
"Jamaica", "Martinique", "Netherlands Antilles", "Panama", "Trinidad and Tobago", "Bolivia", "Guyana", "Uzbekistan", "New Zealand", "Free Nationality" };


unsigned long rand64()
{
    static unsigned long seed;
    seed = seed * 6364136223846793005 + 1442695040888963407;
    return seed;
}


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
	{0.0f, 96.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 1.0f}, //2
	{96.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 0.0f}, //3
	{96.0f, 96.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 1.0f}, //4
};

CUSTOMVERTEX g_preview_outline[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}, //1
	{0.0f, 98.0f, 0.0f, 1.0f, 0xffffffff}, //2
	{98.0f, 0.0f, 0.0f, 1.0f, 0xffffffff}, //3
	{98.0f, 98.0f, 0.0f, 1.0f, 0xffffffff}, //4
};

CUSTOMVERTEX g_preview_outline2[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{0.0f, 100.0f, 0.0f, 1.0f, 0xff000000}, //2
	{100.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //3
	{100.0f, 100.0f, 0.0f, 1.0f, 0xff000000}, //4
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


config_t referees_config;
REFEREE_INFO ref1Info;
REFEREE_INFO ref2Info;
REFEREE_INFO ref3Info;
REFEREE_INFO ref4Info;
DWORD strictness;
bool isSelectMode=false;
static bool g_userChoice = true;
bool autoRandomMode = false;
int selectedReferee = -1;
DWORD numReferees = 0;
REFEREES *refereesMap;
char referee1Folder[BUFLEN];
char referee2Folder[BUFLEN];
char referee3Folder[BUFLEN];
char referee4Folder[BUFLEN];
char referee1Display[BUFLEN];
char referee2Display[BUFLEN];
char referee3Display[BUFLEN];
char referee4Display[BUFLEN];
static std::map<string,int> g_RefereesIdMap;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitReferees();
bool refereesReadNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD* numPages);
bool refereesAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
void SetAdditionalRefereesData();
void SetRefereesGlobalStrictness();
void ApplyStrictness(DWORD strictness);
void SetRefereeData();
int GetNationalityIndex (string nationality);
int GetStrictness (string strictness);
void ReadRefereeMap();
void AddReferee(LPTSTR sreferee1, LPTSTR sreferee2, LPTSTR sreferee3, LPTSTR sreferee4);
void SetReferee(DWORD id);
void FreeReferees();
void refereeKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void refereeUniSelect();
void refereeShowMenu();
void refereeBeginUniSelect();
void refereeEndUniSelect();
bool readRefInfo(REFEREE_INFO* config, char* cfgFile);
bool readConfig(config_t& config);

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



EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_referees,"Attaching dll...");
		int v=GetPESInfo()->GameVersion;

		switch (v) {
			case gvPES5PC: //support for PES5 PC...
			case gvWE9PC: //support for WE9 PC...
            case gvWE9LEPC: //... and WE9:LE PC
				break;
            default:
                Log(&k_referees,"Your game version is currently not supported!");
                return false;
		}

		hInst=hInstance;

		RegisterKModule(&k_referees);

		memcpy(dta, dtaArray[v], sizeof(dta));
		
		HookFunction(hk_D3D_Create,(DWORD)InitReferees);
		HookFunction(hk_GetNumPages,(DWORD)refereesReadNumPages);
		HookFunction(hk_AfterReadFile,(DWORD)refereesAfterReadFile);
		HookFunction(hk_AfterReadFile,(DWORD)SetAdditionalRefereesData);
	    HookFunction(hk_Input,(DWORD)refereeKeyboardProc);
		HookFunction(hk_BeginUniSelect, (DWORD)refereeUniSelect);
		HookFunction(hk_D3D_CreateDevice,(DWORD)CustomGraphicCreateDevice);
		HookFunction(hk_DrawKitSelectInfo,(DWORD)refereeShowMenu);
		HookFunction(hk_OnShowMenu,(DWORD)refereeBeginUniSelect);
        HookFunction(hk_OnHideMenu,(DWORD)refereeEndUniSelect);
		HookFunction(hk_D3D_Reset,(DWORD)CustomGraphicReset);
		HookFunction(hk_ProcessPlayerData,(DWORD)SetRefereesGlobalStrictness);
		HookFunction(hk_ProcessPlayerData,(DWORD)SetRefereeData);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_referees,"Detaching dll...");
		
		//save settings
        char refereeDat[BUFLEN];
		referees_config.selectedReferee=selectedReferee;
        referees_config.autoRandomMode = autoRandomMode;
	    ZeroMemory(refereeDat, BUFLEN);
	    sprintf(refereeDat, "%s\\referees.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(refereeDat, "wb");
	    if (f) {
			fwrite(&referees_config.selectedReferee, sizeof(referees_config.selectedReferee), 1, f);
			fwrite(&referees_config.previewEnabled, sizeof(referees_config.previewEnabled), 1, f);
			fwrite(&referees_config.keyReset, 1, 1, f);
			fwrite(&referees_config.keyRandom, 1, 1, f);
			fwrite(&referees_config.keyPrev, 1, 1, f);
			fwrite(&referees_config.keyNext, 1, 1, f);
			fwrite(&referees_config.autoRandomMode, sizeof(referees_config.autoRandomMode), 1, f);
	        fclose(f);
			Log(&k_referees,"Preferences saved.");
	    }
		
		
		UnhookFunction(hk_GetNumPages,(DWORD)refereesReadNumPages);
		UnhookFunction(hk_AfterReadFile,(DWORD)refereesAfterReadFile);
		//UnhookFunction(hk_ProcessPlayerData,(DWORD)SetRefereesData2);
		UnhookFunction(hk_Input,(DWORD)refereeKeyboardProc);
		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)refereeShowMenu);
		UnhookFunction(hk_D3D_Reset,(DWORD)CustomGraphicReset);

		UnhookFunction(hk_AfterReadFile,(DWORD)SetAdditionalRefereesData);
		UnhookFunction(hk_ProcessPlayerData,(DWORD)SetRefereesGlobalStrictness);
		UnhookFunction(hk_ProcessPlayerData,(DWORD)SetRefereeData);

		FreeReferees();

		g_RefereesIdMap.clear();


		Log(&k_referees,"Detaching done.");
	}

	return true;
}

void InitReferees()
{
	UnhookFunction(hk_D3D_Create,(DWORD)InitReferees);
    //load settings
    char refereeDat[BUFLEN];
    ZeroMemory(refereeDat, BUFLEN);
    sprintf(refereeDat, "%s\\referees.dat", GetPESInfo()->mydir);
    LOG(&k_referees, "reading: %s", refereeDat);
    FILE* f = fopen(refereeDat, "rb");
    if (f) {
        fread(&referees_config.selectedReferee, sizeof(referees_config.selectedReferee), 1, f);
        fread(&referees_config.previewEnabled, sizeof(referees_config.previewEnabled), 1, f);
        fread(&referees_config.keyReset, 1, 1, f);
        fread(&referees_config.keyRandom, 1, 1, f);
        fread(&referees_config.keyPrev, 1, 1, f);
        fread(&referees_config.keyNext, 1, 1, f);
        fread(&referees_config.autoRandomMode, sizeof(referees_config.autoRandomMode), 1, f);
        fclose(f);
        autoRandomMode = referees_config.autoRandomMode;
    } else {
        referees_config.selectedReferee=-1;
        g_userChoice = false;
    };
    LOG(&k_referees, "selected referee: %d", referees_config.selectedReferee);

	// read configuration
    readConfig(referees_config);

    ReadRefereeMap();
    SetReferee(referees_config.selectedReferee);

	Log(&k_referees, "Module initialized.");
}



bool refereesReadNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages)
{
    DWORD fileSize = 0;
    char filename[BUFLEN] = {0};

    if (afsId == 1) { // 0_text.afs
		if (fileId == dta[REF_FACE_W] || fileId == dta[REF_FACE_Y] || fileId == dta[REF_FACE_R] || fileId == dta[REF_FACE_K]) {
			// face
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referees\\");
			strcat(filename,referee1Folder);
			strcat(filename,"\\face.bin");
			//strcat(filename,model);
			//////////////////////////////////
		}else if (fileId == dta[REF_HAIRFILE]){
			// hair
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referees\\");
			strcat(filename,referee1Folder);
			strcat(filename,"\\hair.bin");
			//strcat(filename,folder);
			//strcat(filename,"\\");
			//strcat(filename,FILE_NAMES[REF_KIT_HQ]);
		}
		HANDLE TempHandle=CreateFile(filename,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if (TempHandle!=INVALID_HANDLE_VALUE) {
			fileSize=GetFileSize(TempHandle,NULL);
			CloseHandle(TempHandle);
		} else {
			fileSize=0;
		};

		if (fileSize > 0) {
			LogWithString(&k_referees, "refereesReadNumPages: found file: %s", filename);
			LogWithTwoNumbers(&k_referees,"refereesReadNumPages: had size: %08x pages (%08x bytes)", 
					orgNumPages, orgNumPages*0x800);

			// adjust buffer size to fit file
			*numPages = ((fileSize-1)>>0x0b)+1;
			LOG(&k_referees,
					"refereesReadNumPages: new size: %08x pages (%08x bytes)", 
					*numPages, (*numPages)*0x800);
			return true;
		}
    }
    return false;
}


bool refereesAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped){
	DWORD afsId = GetAfsIdByOpenHandle(hFile);
    DWORD filesize = 0;
    char filename[BUFLEN] = {0};

	// ensure it is 0_TEXT.afs
	if (afsId == 1) {
		DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - (*lpNumberOfBytesRead);
		DWORD fileId = GetProbableFileIdForHandle(afsId, offset, hFile);
		if (fileId == dta[REF_FACE_W] || fileId == dta[REF_FACE_Y] || fileId == dta[REF_FACE_R] || fileId == dta[REF_FACE_K]) {
			// face
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referees\\");
			strcat(filename,referee1Folder);
			strcat(filename,"\\face.bin");
			//strcat(filename,model);
			//////////////////////////////////
		}else if (fileId == dta[REF_HAIRFILE]){
			// hair
			strcpy(filename,GetPESInfo()->gdbDir);
			strcat(filename,"GDB\\referees\\");
			strcat(filename,referee1Folder);
			strcat(filename,"\\hair.bin");
			//strcat(filename,folder);
			//strcat(filename,"\\");
			//strcat(filename,FILE_NAMES[REF_KIT_HQ]);
		}

		else{
			return false;
		}
		LOG(&k_referees, "loading (%s)", filename);
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
			LOG(&k_referees, 
			"offset=%08x, GetOffsetByFileId()=%08x, curr_offset=%08x",
			offset, GetOffsetByFileId(afsId, fileId), curr_offset);
			DWORD bytesToRead = *lpNumberOfBytesRead;
			if (filesize < curr_offset + *lpNumberOfBytesRead) {
				bytesToRead = filesize - curr_offset;
			}
			DWORD bytesRead = 0;
			SetFilePointer(handle, curr_offset, NULL, FILE_BEGIN);
			LOG(&k_referees, "reading 0x%x bytes (0x%x) from {%s}", 
			bytesToRead, *lpNumberOfBytesRead, filename);
			ReadFile(handle, lpBuffer, bytesToRead, &bytesRead, lpOverlapped);
			LOG(&k_referees, "read 0x%x bytes from {%s}", bytesRead, filename);
			if (*lpNumberOfBytesRead - bytesRead > 0) {
				DEBUGLOG(&k_referees, "zeroing out 0x%0x bytes",
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
			LOG(&k_referees, "Unable to read file {%s} must be protected or opened", filename);
		}
		return result;
	}
	return false;
}

void ReadRefereeMap()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sreferee1[BUFLEN], sreferee2[BUFLEN], sreferee3[BUFLEN], sreferee4[BUFLEN];
		
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\referees\\map.txt");
	
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
		ZeroMemory(sreferee1,BUFLEN);
		ZeroMemory(sreferee2,BUFLEN);
		ZeroMemory(sreferee3,BUFLEN);
		ZeroMemory(sreferee4,BUFLEN);
		if (sscanf(str,"\"%[^\"]\",\"%[^\"]\",\"%[^\"]\",\"%[^\"]\"",sreferee1,sreferee2,sreferee3,sreferee4)==4)
			Log(&k_referees, "adding referees");

			AddReferee(sreferee1,sreferee2,sreferee3,sreferee4);
	};
	fclose(cfg);	
	return;
};

void AddReferee(LPTSTR sreferee1, LPTSTR sreferee2, LPTSTR sreferee3, LPTSTR sreferee4)
{
	REFEREES *tmp=new REFEREES[numReferees+1];
	memcpy(tmp,refereesMap,numReferees*sizeof(REFEREES));
	delete refereesMap;
	refereesMap=tmp;


	// Loading ref 1 info
	refereesMap[numReferees].referee1Folder=new char [strlen(sreferee1)+1];
	strcpy(refereesMap[numReferees].referee1Folder,sreferee1);

	char ref1InfoLoc[BUFLEN] = {0};
	strcpy(ref1InfoLoc,GetPESInfo()->gdbDir);
	strcat(ref1InfoLoc,"GDB\\referees\\");
	strcat(ref1InfoLoc,sreferee1);
	strcat(ref1InfoLoc,"\\referee.txt");

	if (readRefInfo(&ref1Info, ref1InfoLoc)){
		refereesMap[numReferees].referee1Display=new char [strlen(ref1Info.name)+1];
		strcpy(refereesMap[numReferees].referee1Display,ref1Info.name);
	}
	else{
		refereesMap[numReferees].referee1Display=new char [strlen("game choice")+1];
		strcpy(refereesMap[numReferees].referee1Display,"game choice");
	}


	// Loading ref 2 info

	refereesMap[numReferees].referee2Folder=new char [strlen(sreferee2)+1];
	strcpy(refereesMap[numReferees].referee2Folder,sreferee2);

	char ref2InfoLoc[BUFLEN] = {0};
	strcpy(ref2InfoLoc,GetPESInfo()->gdbDir);
	strcat(ref2InfoLoc,"GDB\\referees\\");
	strcat(ref2InfoLoc,sreferee2);
	strcat(ref2InfoLoc,"\\referee.txt");

	if (readRefInfo(&ref2Info, ref2InfoLoc)){
		refereesMap[numReferees].referee2Display=new char [strlen(ref2Info.name)+1];
		strcpy(refereesMap[numReferees].referee2Display,ref2Info.name);
	}
	else{
		refereesMap[numReferees].referee2Display=new char [strlen("game choice")+1];
		strcpy(refereesMap[numReferees].referee2Display,"game choice");
	}

	// Loading ref 3 info

	refereesMap[numReferees].referee3Folder=new char [strlen(sreferee3)+1];
	strcpy(refereesMap[numReferees].referee3Folder,sreferee3);
	char ref3InfoLoc[BUFLEN] = {0};
	strcpy(ref3InfoLoc,GetPESInfo()->gdbDir);
	strcat(ref3InfoLoc,"GDB\\referees\\");
	strcat(ref3InfoLoc,sreferee3);
	strcat(ref3InfoLoc,"\\referee.txt");

	if (readRefInfo(&ref3Info, ref3InfoLoc)){
		refereesMap[numReferees].referee3Display=new char [strlen(ref3Info.name)+1];
		strcpy(refereesMap[numReferees].referee3Display,ref3Info.name);
	}
	else{
		refereesMap[numReferees].referee3Display=new char [strlen("game choice")+1];
		strcpy(refereesMap[numReferees].referee3Display,"game choice");
	}

	// Loading ref 4 info

	refereesMap[numReferees].referee4Folder=new char [strlen(sreferee4)+1];
	strcpy(refereesMap[numReferees].referee4Folder,sreferee4);
	char ref4InfoLoc[BUFLEN] = {0};
	strcpy(ref4InfoLoc,GetPESInfo()->gdbDir);
	strcat(ref4InfoLoc,"GDB\\referees\\");
	strcat(ref4InfoLoc,sreferee4);
	strcat(ref4InfoLoc,"\\referee.txt");

	if (readRefInfo(&ref4Info, ref4InfoLoc)){
		refereesMap[numReferees].referee4Display=new char [strlen(ref4Info.name)+1];
		strcpy(refereesMap[numReferees].referee4Display,ref4Info.name);
	}
	else{
		refereesMap[numReferees].referee4Display=new char [strlen("game choice")+1];
		strcpy(refereesMap[numReferees].referee4Display,"game choice");
	}

	g_RefereesIdMap[sreferee1]=numReferees;

	numReferees++;
	return;
};

void SetReferee(DWORD id)
{
	char tmp1Display[BUFLEN];
	char tmp2Display[BUFLEN];
	char tmp3Display[BUFLEN];
	char tmp4Display[BUFLEN];
	
	if (id<numReferees)
		selectedReferee=id;
	else if (id<0)
		selectedReferee=-1;
	else
		selectedReferee=-1;
		
	if (selectedReferee<0) {
		strcpy(tmp1Display,"game choice");
		strcpy(tmp2Display,"game choice");
		strcpy(tmp3Display,"game choice");
		strcpy(tmp4Display,"game choice");
		strcpy(referee1Folder,"\0");
		strcpy(referee2Folder,"\0");
		strcpy(referee3Folder,"\0");
		strcpy(referee4Folder,"\0");
	} else {

		/*
		strcpy(referee1,refereesMap[selectedReferee].referee1);
		strcpy(referee2,refereesMap[selectedReferee].referee2);
		strcpy(referee3,refereesMap[selectedReferee].referee3);
		strcpy(referee4,refereesMap[selectedReferee].referee4);
		*/
		strcpy(tmp1Display,refereesMap[selectedReferee].referee1Display);
		strcpy(tmp2Display,refereesMap[selectedReferee].referee2Display);
		strcpy(tmp3Display,refereesMap[selectedReferee].referee3Display);
		strcpy(tmp4Display,refereesMap[selectedReferee].referee4Display);
		strcpy(referee1Folder,refereesMap[selectedReferee].referee1Folder);
		strcpy(referee2Folder,refereesMap[selectedReferee].referee2Folder);
		strcpy(referee3Folder,refereesMap[selectedReferee].referee3Folder);
		strcpy(referee4Folder,refereesMap[selectedReferee].referee4Folder);

		SafeRelease( &g_preview_tex );
		g_newPrev=true;
	};
	
	strcpy(referee1Display,"Referee: ");
	strcat(referee1Display,tmp1Display);
	strcpy(referee2Display,"Referee: ");
	strcat(referee2Display,tmp2Display);
	strcpy(referee3Display,"Referee: ");
	strcat(referee3Display,tmp3Display);
	strcpy(referee4Display,"Referee: ");
	strcat(referee4Display,tmp4Display);
	
	return;
};

void FreeReferees()
{
	for (int i=0;i<numReferees;i++) {
		delete refereesMap[i].referee1Folder;
		delete refereesMap[i].referee2Folder;
		delete refereesMap[i].referee3Folder;
		delete refereesMap[i].referee4Folder;
		delete refereesMap[i].referee1Display;
		delete refereesMap[i].referee2Display;
		delete refereesMap[i].referee3Display;
		delete refereesMap[i].referee4Display;
	};
	delete refereesMap;
	numReferees=0;
	selectedReferee=-1;
	return;
};




void SetAdditionalRefereesData()
{

	/*bool somevar = false;
	if (somevar){
	if (VirtualProtect((LPVOID)dta[SELECT_REF_FLAG], 5, newProtection, &protection)) {
		//Disabling selecting another referee
		*(BYTE*)(dta[SELECT_REF_FLAG]) = 0; 
		//Enforce referee id 0
		*(BYTE*)(dta[SELECTED_REF]) = 0; 
		Log(&k_referees, "Referee menu disable and enforced referee id 0");
	};
	};*/
	if (referees_config.additionalRefsExhibitionEnabled) {
		if (VirtualProtect((LPVOID)dta[SELECT_TOTAL_REF], 4, newProtection, &protection)) {
			*(BYTE*)dta[SELECT_TOTAL_REF] = 22;
			//Log(&k_referees,"Additional referees in exhibition mode added.");
		}
		else {
			Log(&k_referees, "Problem adding additional referees in exhibition mode.");
		}
	}
}

int GetNationalityIndex (string nationality)
{
	int index = 0;
	while (index < TOTAL_NATIONALITIES){
		if (nationalities[index] == nationality) return index;
		++index;
	}
	return FREE_NATIONALITY; // return Free Nationality to avoid errors
}

int GetStrictness (string strictness)
{
	
	if(strictness=="Lenient") return 336855042;
	else if (strictness=="Average")	return 842150460;
	else if (strictness=="Strict") return 1482184792;
	else if (strictness=="Default") return 1;
	else return 0;
}

void SetRefereeData()
{	

    const char* refereesFolderArray[4] = {
		referee1Folder,
		referee2Folder,
		referee3Folder,
		referee4Folder,
	};
	for (int i = 0; i < 4; i++){
		char refereeFolder[BUFLEN];
		strcat(refereeFolder,refereesFolderArray[i]);

		char refInfoLoc[BUFLEN] = {0};
		strcpy(refInfoLoc,GetPESInfo()->gdbDir);
		strcat(refInfoLoc,"GDB\\referees\\");
		strcat(refInfoLoc,refereeFolder);
		strcat(refInfoLoc,"\\referee.txt");
		REFEREE_INFO refInfo;

		if (readRefInfo(&refInfo, refInfoLoc)){
			if (referees_config.debug) LogWithString(&k_referees, "reading %s% file Sucess!!", refInfoLoc);
			DWORD startAddress = dta[REF_START_ADDRESS] + (i * dta[REF_DATA_SIZE]);
			DWORD faceWID;
			DWORD faceYID;
			DWORD faceRID;
			DWORD faceKID;
			DWORD hairStyleID;
			
			if (VirtualProtect((LPVOID)startAddress, dta[REF_DATA_SIZE], newProtection, &protection)) {
				//LPTSTR nation = "Spain";
				BYTE skinColour;
				BYTE faceType = 2;
				//BYTE height = 180;
				//BYTE weight = 90;
				//BYTE skinRelink = 64; // Not sure, could be kit relink

				*(BYTE*)(startAddress + 4) = GetNationalityIndex(refInfo.nationality); //Nationality
				*(BYTE*)(startAddress + 8) = GetNationalityIndex(refInfo.nationality); //Nationality 2
				//*(BYTE*)(dta[REF_START_ADDRESS] + 20) = skinRelink; //Skin relink
				skinColour = refInfo.skinColour - 1;
				*(BYTE*)(startAddress + 21) = skinColour; //Skin colour
				*(BYTE*)(startAddress + 23) = faceType; //face type force to preset
				// According to skin colour we set the corresponding face id number
				switch(skinColour){
					case 0:
						*(WORD*)(startAddress + 36) = dta[REF_FACE_W_ID]; //face id
						break;
					case 1:
						*(WORD*)(startAddress + 36) = dta[REF_FACE_Y_ID]; //face id
						break;
					case 2:
						*(WORD*)(startAddress + 36) = dta[REF_FACE_R_ID]; //face id
						break;
					case 3:
						*(WORD*)(startAddress + 36) = dta[REF_FACE_K_ID]; //face id
						break;
					default:
						if (referees_config.debug) Log(&k_referees, "Error skin out of range");
						break;
				}
				*(WORD*)(startAddress + 24) = dta[REF_HAIRFILE_ID]; //hairstyle
				//*(BYTE*)(dta[REF_START_ADDRESS] + 26) = 2; //hair colour pattern, is really needed?
				*(BYTE*)(startAddress + 29) = refInfo.height; //height
				*(BYTE*)(startAddress + 30) = refInfo.weight; //weight
				DWORD refereeStrictness = GetStrictness(refInfo.cardStrictness);
				if (refereeStrictness == 0){
					ApplyStrictness(refInfo.customCardStrictness);
				}
				else if (refereeStrictness==1){
					// do nothing will use default options or global
					if (referees_config.debug) Log(&k_referees, "Referee will use default strictness or global in referee.cfg file");
				}
				else{
					ApplyStrictness(refereeStrictness);
				}

				if (referees_config.debug) Log(&k_referees, "Referee data loaded into memory");
			};
		}
		else{
			if (referees_config.debug) LogWithString(&k_referees, "Error reading referee txt file = %s%", refInfoLoc);
		}
	}
}


void SetRefereesGlobalStrictness()
{	
	if (referees_config.cardStrictnessEnabled && referees_config.randomCardStrictnessEnabled) {
		srand(GetTickCount());
		strictness = rand64() % (referees_config.cardstrictness_random_maximum + 1 - referees_config.cardstrictness_random_minimum) + referees_config.cardstrictness_random_minimum;
	}
	else if (referees_config.cardStrictnessEnabled && !referees_config.randomCardStrictnessEnabled) {
		strictness = referees_config.card_strictness;
	}
	else{
		return;
	}
	ApplyStrictness(strictness);

	//HookFunction(hk_ProcessPlayerData,(DWORD)SetRefereesData2);
}


void ApplyStrictness(DWORD strictness)
{
	if (VirtualProtect((LPVOID)dta[REF_START_ADDRESS], dta[REF_DATA_SIZE], newProtection, &protection)) {
			*(DWORD*)(dta[REF_START_ADDRESS] + 60) = strictness;
			*(DWORD*)(dta[REF_START_ADDRESS] + 64) = strictness;
			*(DWORD*)(dta[REF_START_ADDRESS] + 76) = strictness;
			LogWithNumber(&k_referees, "Card strictness: %d", *(DWORD*)(dta[REF_START_ADDRESS] + 60));
			LogWithNumber(&k_referees, "Card strictness: %d", *(DWORD*)(dta[REF_START_ADDRESS] + 60));
			LogWithNumber(&k_referees, "Card strictness: %d", *(DWORD*)(dta[REF_START_ADDRESS] + 60));
	}
	else {
		Log(&k_referees, "Problem changing card strictness.");
	}
}

void refereeKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	if ((!isSelectMode) || (code1 < 0))
		return; 

	if ((code1==HC_ACTION) && (lParam & 0x80000000)) {
		if (wParam == referees_config.keyNext) {
			autoRandomMode = false;
			SetReferee(selectedReferee+1);
			g_userChoice = true;
		} 
		else if (wParam == referees_config.keyPrev) {
			autoRandomMode = false;
			if (selectedReferee<0)
				SetReferee(numReferees-1);
			else
				SetReferee(selectedReferee-1);
			g_userChoice = true;
		} 
		else if (wParam == referees_config.keyReset) {
			SetReferee(-1);
			if (!autoRandomMode) {
        		autoRandomMode = true;
        		g_userChoice = true;
        	} 
			else {
				SetReferee(-1);
				g_userChoice = false;
				autoRandomMode = false;
			}
			
			if (autoRandomMode) {
				LARGE_INTEGER num;
				QueryPerformanceCounter(&num);
				int iterations = num.LowPart % MAX_ITERATIONS;
				for (int j=0;j<iterations;j++) {
					SetReferee(selectedReferee+1);
		        }
			}

		} 
		else if (wParam == referees_config.keyRandom) {
			autoRandomMode = true;
			LARGE_INTEGER num;
			QueryPerformanceCounter(&num);
			int iterations = num.LowPart % MAX_ITERATIONS;
			for (int j=0;j<iterations;j++)
				SetReferee(selectedReferee+1);
			g_userChoice = true;
		}
	}
	
	return;
};

void refereeUniSelect()
{

	if (autoRandomMode) {
		LARGE_INTEGER num;
		QueryPerformanceCounter(&num);
		int iterations = num.LowPart % MAX_ITERATIONS;
		for (int j=0;j<iterations;j++) {
			SetReferee(selectedReferee+1);
        }
	}
	return;
}

void refereeShowMenu()
{
	SIZE size;
	DWORD color = 0xffffffff; // white
    char text[BUFLEN];
	UINT g_bbWidth=GetPESInfo()->bbWidth;
	UINT g_bbHeight=GetPESInfo()->bbHeight;
	double stretchX=GetPESInfo()->stretchX;
	double stretchY=GetPESInfo()->stretchY;
	if (selectedReferee<0) {
		color = 0xffc0c0c0; // gray if is game choice
    } 
	if (autoRandomMode)
		color = 0xffaad0ff; // light blue for randomly selected kit

	KGetTextExtent(referee1Display,16,&size);
	KDrawText((g_bbWidth-size.cx)/2,g_bbHeight*0.75,color,16,referee1Display,true);


    //draw Referee Kit preview
    if (referees_config.previewEnabled) {
		DrawPreview(g_device);
    }
	return;

};


void refereeBeginUniSelect()
{
   
    isSelectMode=true;
    dksiSetMenuTitle("Referee selection");
    
    // invalidate preview texture
    SafeRelease( &g_preview_tex );
    g_newPrev = true;


    return;
}

void refereeEndUniSelect()
{
    
    isSelectMode=false;

}




/**
 * Returns true if successful.
 */
bool readConfig(config_t& config)
{
    string cfgFile(GetPESInfo()->mydir);
    cfgFile += "\\referees.cfg";

	FILE* cfg = fopen(cfgFile.c_str(), "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;

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

		if (strcmp(name, "additional-refs-exhibition.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: additional-refs-exhibition.enabled = (%d)", value);
			config.additionalRefsExhibitionEnabled = (value > 0);
		}
		else if (strcmp(name, "card-strictness.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: card-strictness.enabled = (%d)", value);
			config.cardStrictnessEnabled = (value > 0);
		}
		else if (strcmp(name, "debug")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: debug = (%d)", value);
			config.debug = (value > 0);
		}
		else if (strcmp(name, "random-card-strictness.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: random-card-strictness.enabled = (%d)", value);
			config.randomCardStrictnessEnabled = (value > 0);
		}
		else if (strcmp(name, "card-strictness.random.minimum")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: card-strictness.random.minimum = (%d)", value);
			config.cardstrictness_random_minimum = value;
		}
		else if (strcmp(name, "card-strictness.random.maximum")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: card-strictness.random.maximum = (%d)", value);
			config.cardstrictness_random_maximum = value;
		}
		else if (strcmp(name, "card.strictness")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_referees, "ReadConfig: card.strictness = (%d)", value);
			config.card_strictness = value;
		}
	}
	fclose(cfg);
	return true;
}



/**
 * Returns true if successful.
 */
bool readRefInfo(REFEREE_INFO* config, char* cfgFile)
{
	if (referees_config.debug) 	LogWithString(&k_referees,"ReadConfig: trying to read = \"%s\"", cfgFile);

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

		if (lstrcmp(name, "name")==0)
		{
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;

			char buf[BUFLEN];
			
			ZeroMemory(buf, BUFLEN);
			memcpy(buf, startQuote + 1, endQuote - startQuote - 1);

			if (referees_config.debug) LogWithString(&k_referees,"readRefInfo: referee name = \"%s\"", buf);
			config->name = new char[strlen(buf)+1];
			strcpy(config->name,buf);
		}
		else if (strcmp(name, "nationality")==0)
		{
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;

			char buf[BUFLEN];
			
			ZeroMemory(buf, BUFLEN);
			memcpy(buf, startQuote + 1, endQuote - startQuote - 1);

			if (referees_config.debug) LogWithString(&k_referees,"readRefInfo: referee nationality = \"%s\"", buf);
			config->nationality = new char[strlen(buf)+1];
			strcpy(config->nationality,buf);
		}
		else if (lstrcmpi(name, "height")==0) {
			if (sscanf(pValue, "%d", &value)!=1) continue;
			if (referees_config.debug) LogWithNumber(&k_referees,"readRefInfo: height = (%d)", value);
            config->height = (BYTE)value;
        }
		else if (lstrcmpi(name, "weight")==0) {
			if (sscanf(pValue, "%d", &value)!=1) continue;
			if (referees_config.debug) LogWithNumber(&k_referees,"readRefInfo: weight = (%d)", value);
            config->weight = (BYTE)value;
        }
		else if (lstrcmpi(name, "skinColour")==0) {
			if (sscanf(pValue, "%d", &value)!=1) continue;
			if (referees_config.debug) LogWithNumber(&k_referees,"readRefInfo: skinColour = (%d)", value);
            config->skinColour = (BYTE)value;
        }
		else if (strcmp(name, "cardStrictness")==0)
		{
			char* startQuote = strstr(pValue, "\"");
			if (startQuote == NULL) continue;
			char* endQuote = strstr(startQuote + 1, "\"");
			if (endQuote == NULL) continue;

			char buf[BUFLEN];
			
			ZeroMemory(buf, BUFLEN);
			memcpy(buf, startQuote + 1, endQuote - startQuote - 1);

			if (referees_config.debug) LogWithString(&k_referees,"readRefInfo: referee cardStrictness = \"%s\"", buf);
			config->cardStrictness = new char[strlen(buf)+1];
			strcpy(config->cardStrictness,buf);
		}
		else if (strcmp(name, "customCardStrictness")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			if (referees_config.debug) LogWithNumber(&k_referees, "ReadConfig: customCardStrictness = (%d)", value);
			config->customCardStrictness = value;
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
            Log(&k_referees,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_referees,"Problem with safe-release");
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
	Log(&k_referees,"CustomGraphicReset: cleaning-up.");

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
		Log(&k_referees,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_referees,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview->Lock(0, sizeof(g_preview), (BYTE**)&pVertices, 0)))
	{
		Log(&k_referees,"g_pVB_preview->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview, sizeof(g_preview));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_preview, sizeof(g_preview)/sizeof(CUSTOMVERTEX2), 
            512-64, 620);
	g_pVB_preview->Unlock();

	// preview outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview_outline)))
	{
		Log(&k_referees,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_referees,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline->Lock(0, sizeof(g_preview_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_referees,"g_pVB_preview_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview_outline, sizeof(g_preview_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_preview_outline, sizeof(g_preview_outline)/sizeof(CUSTOMVERTEX), 
          512-65, 619);
	g_pVB_preview_outline->Unlock();

	// preview outline2
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_preview_outline2), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_preview_outline2)))
	{
		Log(&k_referees,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_referees,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_preview_outline2->Lock(0, sizeof(g_preview_outline2), (BYTE**)&pVertices, 0)))
	{
		Log(&k_referees,"g_pVB_preview_outline2->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_preview_outline2, sizeof(g_preview_outline2));
	SetPosition((CUSTOMVERTEX*)pVertices, g_preview_outline2, sizeof(g_preview_outline2)/sizeof(CUSTOMVERTEX), 
            512-66, 618);
	g_pVB_preview_outline2->Unlock();

    return S_OK;
}

void DrawPreview(IDirect3DDevice8* dev)
{
	if (selectedReferee<0) return;
	if (g_needsRestore) 
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_referees,"DrawPreview: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_referees,"DrawPreview: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
        D3DVIEWPORT8 vp;
        dev->GetViewport(&vp);
        LogWithNumber(&k_referees,"VP: %d",vp.X);
        LogWithNumber(&k_referees,"VP: %d",vp.Y);
        LogWithNumber(&k_referees,"VP: %d",vp.Width);
        LogWithNumber(&k_referees,"VP: %d",vp.Height);
        LogWithDouble(&k_referees,"VP: %f",vp.MinZ);
        LogWithDouble(&k_referees,"VP: %f",vp.MaxZ);
	}

	// render
	dev->BeginScene();

	// setup renderstate
	//dev->CaptureStateBlock( g_dwSavedStateBlock );
	//dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );
    
    if (!g_preview_tex && g_newPrev) {
        char buf[BUFLEN];
        sprintf(buf, "%s\\GDB\\referees\\%s\\preview.png", GetPESInfo()->gdbDir, referee1Folder);
		LOG(&k_referees,"Trying to load preview image located at {%s}.", buf);

        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                    0, NULL, NULL, &g_preview_tex))) {
            // try "preview.bmp"
            sprintf(buf, "%s\\GDB\\referees\\%s\\preview.bmp", GetPESInfo()->gdbDir, referee1Folder);
            if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                        0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                        D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                        0, NULL, NULL, &g_preview_tex))) {
                Log(&k_referees,"FAILED to load image for referee preview.");
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
        //LogWithNumber(&k_referees,"dev = %08x", (DWORD)dev);
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        //LogWithNumber(&k_referees,"vtab = %08x", (DWORD)vtab);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            //LogWithNumber(&k_referees,"vtab[VTAB_DELETESTATEBLOCK] = %08x", (DWORD)vtab[VTAB_DELETESTATEBLOCK]);
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_referees,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_referees,"g_dwDrawOverlayStateBlock deleted.");
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
	TRACE(&k_referees,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_referees,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // referee kits preview
	SafeRelease( &g_pVB_preview );
	SafeRelease( &g_pVB_preview_outline );
	SafeRelease( &g_pVB_preview_outline2 );

	Log(&k_referees,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_referees,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");
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
		Log(&k_referees,"InitVB() failed.");
        return hr;
    }
	Log(&k_referees,"InitVB() done.");

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


