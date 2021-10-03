#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include "d3dfont.h"
#include "bserv.h"
#include "crc32.h"
#include "kload_exp.h"
#include "afsreplace.h"
#include "soft\zlib123-dll\include\zlib.h"

#include <map>
#include <pngdib.h>

KMOD k_bserv={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

#define MAX_NUM_BALL_FILES 22

#define HOME 0
#define AWAY 1

AFSENTRY AFSArray[MAX_NUM_BALL_FILES];
BSERV_CFG bserv_cfg = {
    DEFAULT_SELECTED_BALL,
    DEFAULT_PREVIEW_ENABLED,
    DEFAULT_KEY_RESET_BALL,
    DEFAULT_KEY_RANDOM_BALL,
    DEFAULT_KEY_PREV_BALL,
    DEFAULT_KEY_NEXT_BALL,
    DEFAULT_HOME_TEAM_CHOICE,
    DEFAULT_AUTO_RANDOM_MODE,
};
int selectedBall=-1;
DWORD numBalls=0;
BALLS *balls;

bool isSelectMode=false;
char display[BUFLEN];
char model[BUFLEN];
char texture[BUFLEN];
static std::map<WORD,int> g_HomeBallMap;
static std::map<string,int> g_BallIdMap;
static bool g_homeTeamChoice = false;
static bool g_userChoice = true;
bool autoRandomMode = false;
bool noHomeBall = true;

DWORD gdbBallAddr=0;
DWORD gdbBallSize=0;
DWORD gdbBallCRC=0;
BYTE* ballTextureBuf=NULL;
int ballTextureSize=0;
RECT ballTextureRect;
bool isPNGtexture=false;
char currTextureName[BUFLEN];
IDirect3DTexture8* g_lastBallTex;

#define MAP_FIND(map,key) map[key]
#define MAP_CONTAINS(map,key) (map.find(key)!=map.end())

//preview
#define D3DFVF_BALLVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
DWORD BVTX=0x100;
struct BALLVERTEX { 
    float x,y,z;
    DWORD unknown[3];
    float tu, tv;
};

//preview lighting
#define D3DFVF_LIGHTVERTEX (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
struct LIGHTVERTEX { 
    float x,y,z;
    float nx,ny,nz;
    float tu, tv;
};

LIGHTVERTEX g_lighting[] = {
	{-5.95f, -5.95f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}, //1
	{-5.95f,  5.95f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}, //2
	{ 5.95f, -5.95f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}, //3
	{ 5.95f,  5.95f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f}, //4
};

#define FACTOR 5.0f

static IDirect3DVertexBuffer8* g_pVB_preview = NULL;
static IDirect3DVertexBuffer8* g_pVB_preview_lighting = NULL;
static IDirect3DTexture8* g_preview_tex = NULL;
static IDirect3DTexture8* g_lighting_tex = NULL;
static IDirect3DIndexBuffer8* g_pIB_preview = NULL;

BYTE* g_previewData=0;
BYTE* g_previewIBData=NULL;
DWORD g_previewStride=0;
DWORD g_previewNumVertices=0;
DWORD g_previewSize=0;
DWORD g_previewNumPrimitives=0;

static bool g_needsRestore = TRUE;
static bool g_newBall = false;
static DWORD g_dwSavedStateBlock = 0L;
static DWORD g_dwDrawOverlayStateBlock = 0L;

void DumpBuffer(char* filename, LPVOID buf, DWORD len);
void SafeRelease(LPVOID ppObj);
BOOL FileExists(char* filename);
void DrawBallPreview(IDirect3DDevice8* dev);
void bservReset(IDirect3DDevice8* self, LPVOID params);
HRESULT InitVB(IDirect3DDevice8* dev);
void DeleteStateBlocks(IDirect3DDevice8* dev);
HRESULT InvalidateDeviceObjects(IDirect3DDevice8* dev);
HRESULT DeleteDeviceObjects(IDirect3DDevice8* dev);
HRESULT RestoreDeviceObjects(IDirect3DDevice8* dev);
void ReadBallModel();
void ResizeLightingRect(LIGHTVERTEX* dta, int n);
BOOL ReadConfig(BSERV_CFG* config, char* cfgFile);

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitBserv();
bool bservGetNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages);
bool bservAfterReadFile(HANDLE hFile, LPVOID lpBuffer, 
        DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,  
        LPOVERLAPPED lpOverlapped);
void bservUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2, DWORD result);
void SaveAFSAddr(HANDLE file,DWORD FileNumber,AFSENTRY* afs,DWORD tmp);
DWORD bservGetFileFromAFS(DWORD afsId, DWORD fileId);
void ReadBalls();
void AddBall(LPTSTR sdisplay,LPTSTR smodel,LPTSTR stexture);
void FreeBalls();
void SetBall(DWORD id);
void bservKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void bservBeginUniSelect();
void BeginDrawBallLabel();
void EndDrawBallLabel();
void DrawBallLabel(IDirect3DDevice8* self);
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename);
static int read_file_to_mem(char *fn,unsigned char **ppfiledta, int *pfilesize);
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size);
void FreePNGTexture(BITMAPINFO* bitmap);

bool CreateBallTextureBuf();
void FreeBallTextureBuf(BYTE **buf);
HRESULT STDMETHODCALLTYPE bservCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,UINT levels,
	DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture, DWORD src, bool* IsProcessed);
void bservUnlockRect(IDirect3DTexture8* self,UINT Level);

void updateHomeBall();
DWORD SetBallName(char** names, DWORD numNames, DWORD p3, DWORD p4, DWORD p5, DWORD p6, DWORD p7);


void DumpBuffer(char* filename, LPVOID buf, DWORD len)
{
    FILE* f = fopen(filename,"wb");
    if (f) {
        fwrite(buf, len, 1, f);
        fclose(f);
    }
}


// Calls IUnknown::Release() on an instance
void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_bserv,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_bserv,"Problem with safe-release");
    }
}

BOOL FileExists(char* filename)
{
    TRACE4(&k_bserv,"FileExists: Checking file: %s", filename);
    HANDLE hFile;
    hFile = CreateFile(TEXT(filename),        // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL, // normal file
                       NULL);                 // no attr. template
     
    if (hFile == INVALID_HANDLE_VALUE) 
    { 
        return FALSE;
    }
    CloseHandle(hFile);
    return TRUE;
}

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{	
		Log(&k_bserv,"Attaching dll...");
		
		switch (GetPESInfo()->GameVersion) {
			case gvPES5PC: //support for PES5 PC...
			case gvWE9PC: //... and WE9 PC
            case gvWE9LEPC: //... and WE9:LE PC
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_bserv,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:
		
		RegisterKModule(&k_bserv);
		
		memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
		memcpy(dta, dtaArray[GetPESInfo()->GameVersion], sizeof(dta));
		
		
		
		HookFunction(hk_D3D_CreateDevice,(DWORD)InitBserv);
        HookFunction(hk_GetNumPages,(DWORD)bservGetNumPages);
		HookFunction(hk_AfterReadFile,(DWORD)bservAfterReadFile);
		HookFunction(hk_D3D_CreateTexture,(DWORD)bservCreateTexture);
		HookFunction(hk_D3D_UnlockRect,(DWORD)bservUnlockRect);
		
		HookFunction(hk_Unpack,(DWORD)bservUnpack);
	    HookFunction(hk_Input,(DWORD)bservKeyboardProc);
	    		
		HookFunction(hk_BeginUniSelect, (DWORD)bservBeginUniSelect);
	    HookFunction(hk_DrawKitSelectInfo,(DWORD)DrawBallLabel);
	    HookFunction(hk_OnShowMenu,(DWORD)BeginDrawBallLabel);
	    HookFunction(hk_OnHideMenu,(DWORD)EndDrawBallLabel);
	    HookFunction(hk_D3D_Reset,(DWORD)bservReset);

        strcpy(currTextureName,"\0");
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_bserv,"Detaching dll...");
		
		//save settings
        char ballCfg[BUFLEN];
		bserv_cfg.selectedBall=selectedBall;
        bserv_cfg.homeTeamChoice = g_homeTeamChoice;
        bserv_cfg.autoRandomMode = autoRandomMode;
	    ZeroMemory(ballCfg, BUFLEN);
	    sprintf(ballCfg, "%s\\bserv.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(ballCfg, "wb");
	    if (f) {
	        fwrite(&bserv_cfg, sizeof(BSERV_CFG), 1, f);
	        fclose(f);
	    }
		
		UnhookFunction(hk_D3D_CreateTexture,(DWORD)bservCreateTexture);
		UnhookFunction(hk_D3D_UnlockRect,(DWORD)bservUnlockRect);
		
		UnhookFunction(hk_AfterReadFile,(DWORD)bservAfterReadFile);
		
		UnhookFunction(hk_Unpack,(DWORD)bservUnpack);
		UnhookFunction(hk_Input,(DWORD)bservKeyboardProc);
		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)DrawBallLabel);
		UnhookFunction(hk_D3D_Reset,(DWORD)bservReset);
		
		//MasterUnhookFunction(code[C_GETFILEFROMAFS_CS],bservGetFileFromAFS);
		MasterUnhookFunction(code[C_SETBALLNAME_CS],SetBallName);
		
		if (g_previewData!=NULL)
			HeapFree(GetProcessHeap(), 0, g_previewData);
			
		if (g_previewIBData!=NULL)
			HeapFree(GetProcessHeap(), 0, g_previewIBData);
			
		SafeRelease( &g_preview_tex );
		SafeRelease( &g_lighting_tex );
		
		FreeBallTextureBuf(&ballTextureBuf);
		FreeBalls();

		g_BallIdMap.clear();
		g_HomeBallMap.clear();
		
		Log(&k_bserv,"Detaching done.");
	};

	return true;
};

void InitBserv()
{
	//MasterHookFunction(code[C_GETFILEFROMAFS_CS], 2, bservGetFileFromAFS);
	MasterHookFunction(code[C_SETBALLNAME_CS], 7, SetBallName);

    //load settings
    char ballCfg[BUFLEN];
    ZeroMemory(ballCfg, BUFLEN);
    sprintf(ballCfg, "%s\\bserv.dat", GetPESInfo()->mydir);
    LOG(&k_bserv, "reading: %s", ballCfg);
    FILE* f = fopen(ballCfg, "rb");
    if (f) {
        fread(&bserv_cfg, sizeof(BSERV_CFG), 1, f);
        fclose(f);
        g_homeTeamChoice = bserv_cfg.homeTeamChoice;
        autoRandomMode = bserv_cfg.autoRandomMode;
    } else {
        bserv_cfg.selectedBall=-1;
        g_userChoice = false;
    };
    LOG(&k_bserv, "selected ball: %d", bserv_cfg.selectedBall);

    //read preview and virtual keys settings
    bserv_cfg.previewEnabled = TRUE;
    ZeroMemory(ballCfg, BUFLEN);
    sprintf(ballCfg, "%s\\bserv.cfg", GetPESInfo()->mydir);
    ReadConfig(&bserv_cfg, ballCfg);
    
    // get afs file information
    char tmp[BUFLEN];
    strcpy(tmp,GetPESInfo()->pesdir);
    strcat(tmp,"dat\\0_text.afs");
    LOG(&k_bserv,"reading: {%s}", tmp);
    
    HANDLE TempHandle=CreateFile(
            tmp,GENERIC_READ,3,NULL,
            OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    DWORD HeapAddress=(DWORD)HeapAlloc(
            GetProcessHeap(),HEAP_ZERO_MEMORY,8);
    
    for (int i=0;i<dta[NUM_BALL_FILES];i++)
        SaveAFSAddr(TempHandle,i,&(AFSArray[i]),HeapAddress);
    
    HeapFree(GetProcessHeap(),HEAP_ZERO_MEMORY,(LPVOID)HeapAddress);
    CloseHandle(TempHandle);
    
    ReadBalls();
    SetBall(bserv_cfg.selectedBall);
}

bool bservAfterReadFile(HANDLE hFile, 
        LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped)
{
	int found=-1;
	char tmp[BUFLEN];

    DWORD afsId = GetAfsIdByOpenHandle(hFile);
    if (afsId != 1)
        return false;

    DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT)
        - (*lpNumberOfBytesRead);
    DWORD fileId = GetProbableFileIdForHandle(afsId, offset, hFile);
    if (fileId == 0xffffffff) 
        return false;

	for (int i=0;i<dta[NUM_BALL_FILES];i++) {
		if (AFSArray[i].Buffer==(DWORD)lpBuffer) AFSArray[i].Buffer=0;
		if (AFSArray[i].AFSAddr==offset) {found=i;break;};
	}
	if (found!=-1) {
		AFSArray[found].Buffer=(DWORD)lpBuffer;
		
		if (found==fileId && fileId%2==0) {
			//replace the model
			strcpy(tmp,GetPESInfo()->gdbDir);
			strcat(tmp,"GDB\\balls\\mdl\\");
			strcat(tmp,model);
			
			HANDLE handle = CreateFile(tmp,GENERIC_READ,FILE_SHARE_READ,NULL,
                OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            if (handle!=INVALID_HANDLE_VALUE) {
                DWORD filesize = GetFileSize(handle,NULL);
                DWORD curr_offset = offset - GetOffsetByFileId(afsId, fileId);
                LOG(&k_bserv, 
                        "offset=%08x, GetOffsetByFileId()=%08x, curr_offset=%08x",
                        offset, GetOffsetByFileId(afsId, fileId), curr_offset);
                DWORD bytesToRead = *lpNumberOfBytesRead;
                if (filesize < curr_offset + *lpNumberOfBytesRead) {
                    bytesToRead = filesize - curr_offset;
                }

                DWORD bytesRead = 0;
                SetFilePointer(handle, curr_offset, NULL, FILE_BEGIN);
                LOG(&k_bserv, "reading 0x%x bytes (0x%x) from {%s}", 
                        bytesToRead, *lpNumberOfBytesRead, tmp);
                ReadFile(handle, lpBuffer, bytesToRead, &bytesRead, lpOverlapped);
                LOG(&k_bserv, "read 0x%x bytes from {%s}", bytesRead, tmp);
                if (*lpNumberOfBytesRead - bytesRead > 0) {
                    DEBUGLOG(&k_bserv, "zeroing out 0x%0x bytes",
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
 
                LOG(&k_bserv, "Ball model replaced");
                return true;
            }
		}
	}
    return false;
}

void bservUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2, DWORD result)
{
	int found=-1;

	if (selectedBall<0) return;
	
	for (int i=0;i<dta[NUM_BALL_FILES];i++) {
		if (AFSArray[i].Buffer==(addr1-0x20)) {found=i;break;};
	};
	
	if (found!=-1 && found%2==1) {
		//texture
		gdbBallAddr=addr2;
		gdbBallSize=*size2;
		gdbBallCRC=GetCRC((BYTE*)gdbBallAddr,gdbBallSize);
	};
	return;
};

void SaveAFSAddr(HANDLE file,DWORD FileNumber,AFSENTRY* afs,DWORD tmp)
{
	DWORD NBW=0;
	afs->FileNumber=FileNumber;
	SetFilePointer(file,8*(FileNumber+1),0,0);
	ReadFile(file,(LPVOID)tmp,8,&NBW,0);
	afs->AFSAddr=*(DWORD*)tmp;
	afs->FileSize=*(DWORD*)(tmp+4);
	afs->Buffer=0;
	return;
};

bool bservGetNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages)
{
    DWORD fileSize = 0;
    char tmp[BUFLEN];

	if (selectedBall>=0 && afsId == 1 && fileId<dta[NUM_BALL_FILES] && fileId%2==0) {
		strcpy(tmp,GetPESInfo()->gdbDir);
		strcat(tmp,"GDB\\balls\\mdl\\");
		strcat(tmp,model);

		HANDLE TempHandle=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if (TempHandle!=INVALID_HANDLE_VALUE) {
			fileSize=GetFileSize(TempHandle,NULL);
			CloseHandle(TempHandle);
		} else {
			fileSize=0;
		};
		
		if (fileSize > 0) {
            LogWithString(&k_bserv, "bservGetNumPages: found GDB ball model file: %s", tmp);
            LogWithTwoNumbers(&k_bserv,"bservGetNumPages: had size: %08x pages (%08x bytes)", 
                    orgNumPages, orgNumPages*0x800);

            *numPages = ((fileSize-1)>>0x0b)+1;
            LogWithTwoNumbers(&k_bserv,"bservGetNumPages: new size: %08x pages (%08x bytes)", 
                    *numPages, (*numPages)*0x800);
            return true;
		}
    }

    return false;
}

DWORD bservGetFileFromAFS(DWORD afsId, DWORD fileId)
{
	DWORD orgNumPages = 0;
    DWORD fileSize = 0;
    DWORD* pPageLenTable = NULL;
    
    char tmp[BUFLEN];

	if (selectedBall>=0 && afsId == 1 && fileId<dta[NUM_BALL_FILES] && fileId%2==0) {
		strcpy(tmp,GetPESInfo()->gdbDir);
		strcat(tmp,"GDB\\balls\\mdl\\");
		strcat(tmp,model);

		HANDLE TempHandle=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if (TempHandle!=INVALID_HANDLE_VALUE) {
			fileSize=GetFileSize(TempHandle,NULL);
			CloseHandle(TempHandle);
		} else {
			fileSize=0;
		};
		
		if (fileSize > 0) {
            LogWithString(&k_bserv, "bservGetFileFromAFS: found GDB ball model file: %s", tmp);

            // find buffer size (in pages)
            DWORD* g_pageLenTable = (DWORD*)dta[AFS_PAGELEN_TABLE];
            pPageLenTable = (DWORD*)(g_pageLenTable[afsId] + 0x11c);
            orgNumPages = pPageLenTable[fileId];
            LogWithTwoNumbers(&k_bserv,"bservGetFileFromAFS: had size: %08x pages (%08x bytes)", 
                    orgNumPages, orgNumPages*0x800);

            // adjust buffer size to fit GDB ball model file
            DWORD numPages = ((fileSize-1)>>0x0b)+1;
            pPageLenTable[fileId] = numPages;
            LogWithTwoNumbers(&k_bserv,"bservGetFileFromAFS: new size: %08x pages (%08x bytes)", 
                    numPages, numPages*0x800);
		}

    }

    // call original
    DWORD result = MasterCallNext(afsId, fileId);

    // restore original filesize in pagelen-table
    if (pPageLenTable) {
        pPageLenTable[fileId] = orgNumPages;
    }

    return result;
};



void ReadBalls()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sdisplay[BUFLEN], smodel[BUFLEN], stexture[BUFLEN];
		
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\balls\\map.txt");
	
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
		ZeroMemory(stexture,BUFLEN);
		if (sscanf(str,"\"%[^\"]\",\"%[^\"]\",\"%[^\"]\"",sdisplay,smodel,stexture)==3)
			AddBall(sdisplay,smodel,stexture);
	};
	fclose(cfg);

	//home map
	
    char mapFile[BUFLEN];
    ZeroMemory(mapFile,BUFLEN);

    sprintf(mapFile, "%sGDB\\balls\\home_map.txt", GetPESInfo()->gdbDir);
    FILE* map = fopen(mapFile, "rt");
    if (map == NULL) {
        LOG(&k_bserv, "Unable to find home-ball-map: %s", mapFile);
        return;
    }

	// go line by line
    char buf[BUFLEN];
	while (!feof(map))
	{
		ZeroMemory(buf, BUFLEN);
		fgets(buf, BUFLEN, map);
		if (lstrlen(buf) == 0) break;

		// strip off comments
		char* comm = strstr(buf, "#");
		if (comm != NULL) comm[0] = '\0';

        // find team id
        WORD teamId = 0xffff;
        if (sscanf(buf, "%d", &teamId)==1) {
            LogWithNumber(&k_bserv, "teamId = %d", teamId);
            char* ballname = NULL;
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
                    ballname = start + 1;
                } else {
                    // just take the rest of the line
                    ballname = pComma + 1;
                }

                LOG(&k_bserv, "ballname = {%s}", ballname);

                // store in the home-ball map
                if (MAP_CONTAINS(g_BallIdMap, ballname)) {
                	g_HomeBallMap[teamId] = g_BallIdMap[ballname];
                }
            }
        }
    }
    fclose(map);
	
	return;
};


void AddBall(LPTSTR sdisplay,LPTSTR smodel,LPTSTR stexture)
{
	BALLS *tmp=new BALLS[numBalls+1];
	memcpy(tmp,balls,numBalls*sizeof(BALLS));
	delete balls;
	balls=tmp;
	
	balls[numBalls].display=new char [strlen(sdisplay)+1];
	strcpy(balls[numBalls].display,sdisplay);
	
	balls[numBalls].model=new char [strlen(smodel)+1];
	strcpy(balls[numBalls].model,smodel);
	
	balls[numBalls].texture=new char [strlen(stexture)+1];
	strcpy(balls[numBalls].texture,stexture);

	g_BallIdMap[sdisplay]=numBalls;

	numBalls++;
	return;
};

void FreeBalls()
{
	for (int i=0;i<numBalls;i++) {
		delete balls[i].display;
		delete balls[i].model;
		delete balls[i].texture;
	};
	delete balls;
	numBalls=0;
	selectedBall=-1;
	return;
};

void SetBall(DWORD id)
{
	char tmp[BUFLEN];
	
	if (id<numBalls)
		selectedBall=id;
	else if (id<0)
		selectedBall=-1;
	else
		selectedBall=-1;
		
	if (selectedBall<0) {
		strcpy(tmp,"game choice");
		strcpy(model,"\0");
		strcpy(texture,"\0");
	} else {
		strcpy(tmp,balls[selectedBall].display);
		strcpy(model,balls[selectedBall].model);
		strcpy(texture,balls[selectedBall].texture);
		
		SafeRelease( &g_preview_tex );
		g_newBall=true;
	};
	
	strcpy(display,"Ball: ");
	strcat(display,tmp);
	
	return;
};

void bservKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	if ((!isSelectMode) || (code1 < 0))
		return; 

	if ((code1==HC_ACTION) && (lParam & 0x80000000)) {
		if (wParam == bserv_cfg.keyNextBall) {
			SetBall(selectedBall+1);
			g_homeTeamChoice = false;
			g_userChoice = true;
		} else if (wParam == bserv_cfg.keyPrevBall) {
			if (selectedBall<0)
				SetBall(numBalls-1);
			else
				SetBall(selectedBall-1);
			g_homeTeamChoice = false;
			g_userChoice = true;
		} else if (wParam == bserv_cfg.keyResetBall) {
			SetBall(-1);
			if (!autoRandomMode && g_homeTeamChoice) {
        		autoRandomMode = true;
        		g_homeTeamChoice = false;
        		g_userChoice = true;
	            
        	} else {
	        	if (g_userChoice) g_homeTeamChoice = true;
				SetBall(-1);
				g_homeTeamChoice = !g_homeTeamChoice;
				updateHomeBall();
				g_userChoice = false;
				autoRandomMode = false;
			}
			
			if (autoRandomMode || noHomeBall) {
				LARGE_INTEGER num;
				QueryPerformanceCounter(&num);
				int iterations = num.LowPart % MAX_ITERATIONS;
				for (int j=0;j<iterations;j++) {
					SetBall(selectedBall+1);
		        }
			}

		} else if (wParam == bserv_cfg.keyRandomBall) {
			LARGE_INTEGER num;
			QueryPerformanceCounter(&num);
			int iterations = num.LowPart % MAX_ITERATIONS;
			for (int j=0;j<iterations;j++)
				SetBall(selectedBall+1);

			g_homeTeamChoice = false;
			g_userChoice = true;
		}
	}
	
	return;
};

void bservBeginUniSelect()
{
	updateHomeBall();
	
	if (autoRandomMode || noHomeBall) {
		LARGE_INTEGER num;
		QueryPerformanceCounter(&num);
		int iterations = num.LowPart % MAX_ITERATIONS;
		for (int j=0;j<iterations;j++) {
			SetBall(selectedBall+1);
        }
	}
	return;
}

void BeginDrawBallLabel()
{
	isSelectMode=true;
	dksiSetMenuTitle("Ball selection");
	
	SafeRelease( &g_preview_tex );
    g_newBall = true;
	return;
};

void EndDrawBallLabel()
{
	isSelectMode=false;
	return;
};

void DrawBallLabel(IDirect3DDevice8* self)
{
	SIZE size;
	DWORD color = 0xffffffff; // white
	
	if (selectedBall<0)
		color = 0xffc0c0c0; // gray if ball is game choice
		
	if (autoRandomMode)
		color = 0xffaad0ff; // light blue for randomly selected ball
		
	if (g_homeTeamChoice) {
		if (noHomeBall)
			color = 0xffffaa00; // orange for home-choice, but team has no ball
		else
			color = 0xffffffb0; // pale yellow if ball is home-team choice
	}
		
	UINT g_bbWidth=GetPESInfo()->bbWidth;
	UINT g_bbHeight=GetPESInfo()->bbHeight;
	double stretchX=GetPESInfo()->stretchX;
	double stretchY=GetPESInfo()->stretchY;
	
	KGetTextExtent(display,16,&size);
	//draw shadow
	if (selectedBall>=0 || g_homeTeamChoice || autoRandomMode)
		KDrawText((g_bbWidth-size.cx)/2+2*stretchX,g_bbHeight*0.75+2*stretchY,0xff000000,16,display,true);
	//print ball label
	KDrawText((g_bbWidth-size.cx)/2,g_bbHeight*0.75,color,16,display,true);

    //draw ball preview
    if (bserv_cfg.previewEnabled) {
        DrawBallPreview(self);
    }
	return;
};

// Load texture from PNG file. Returns the size of loaded texture
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename)
{
	TRACE4(&k_bserv,"LoadPNGTexture: loading %s", filename);
    DWORD size = 0;

    PNGDIB *pngdib;
    LPBITMAPINFOHEADER* ppDIB = (LPBITMAPINFOHEADER*)tex;

    pngdib = pngdib_p2d_init();
	//TRACE(&k_bserv,"LoadPNGTexture: structure initialized");

    BYTE* memblk;
    int memblksize;
    if (read_file_to_mem(filename,&memblk, &memblksize)!=0) {
        TRACE(&k_bserv,"LoadPNGTexture: unable to read PNG file");
        pngdib_done(pngdib);
        return 0;
    }
    //TRACE(&k_bserv,"LoadPNGTexture: file read into memory");

    pngdib_p2d_set_png_memblk(pngdib,memblk,memblksize);
	pngdib_p2d_set_use_file_bg(pngdib,1);
	pngdib_p2d_run(pngdib);

	//TRACE(&k_bserv,"LoadPNGTexture: run done");
    pngdib_p2d_get_dib(pngdib, ppDIB, (int*)&size);
	//TRACE(&k_bserv,"LoadPNGTexture: get_dib done");

    pngdib_done(pngdib);
	TRACE(&k_bserv,"LoadPNGTexture: done done");

	TRACE2(&k_bserv,"LoadPNGTexture: *ppDIB = %08x", (DWORD)*ppDIB);
    if (*ppDIB == NULL) {
		TRACE(&k_bserv,"LoadPNGTexture: ERROR - unable to load PNG image.");
        return 0;
    }

    // read transparency values from tRNS chunk
    // and put them into DIB's RGBQUAD.rgbReserved fields
    ApplyAlphaChunk((RGBQUAD*)&((BITMAPINFO*)*ppDIB)->bmiColors, memblk, memblksize);

    HeapFree(GetProcessHeap(), 0, memblk);

	TRACE(&k_bserv,"LoadPNGTexture: done");
	return size;
};

// Read a file into a memory block.
static int read_file_to_mem(char *fn,unsigned char **ppfiledta, int *pfilesize)
{
	HANDLE hfile;
	DWORD fsize;
	//unsigned char *fbuf;
	BYTE* fbuf;
	DWORD bytesread;

	hfile=CreateFile(fn,GENERIC_READ,FILE_SHARE_READ,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hfile==INVALID_HANDLE_VALUE) return 1;

	fsize=GetFileSize(hfile,NULL);
	if(fsize>0) {
		//fbuf=(unsigned char*)GlobalAlloc(GPTR,fsize);
		//fbuf=(unsigned char*)calloc(fsize,1);
        fbuf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fsize);
		if(fbuf) {
			if(ReadFile(hfile,(void*)fbuf,fsize,&bytesread,NULL)) {
				if(bytesread==fsize) { 
					(*ppfiledta)  = fbuf;
					(*pfilesize) = (int)fsize;
					CloseHandle(hfile);
					return 0;   // success
				}
			}
			free((void*)fbuf);
		}
	}
	CloseHandle(hfile);
	return 1;  // error
};

/**
 * Extracts alpha values from tRNS chunk and applies stores
 * them in the RGBQUADs of the DIB
 */
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size)
{
    // find the tRNS chunk
    DWORD offset = 8;
    while (offset < size) {
        PNG_CHUNK_HEADER* chunk = (PNG_CHUNK_HEADER*)(memblk + offset);
        if (chunk->dwName == MAKEFOURCC('t','R','N','S')) {
            int numColors = SWAPBYTES(chunk->dwSize);
            BYTE* alphaValues = memblk + offset + sizeof(chunk->dwSize) + sizeof(chunk->dwName);
            for (int i=0; i<numColors; i++) {
                palette[i].rgbReserved = (alphaValues[i]==0xff) ? 0x80 : alphaValues[i]/2;
            }
        }
        // move on to next chunk
        offset += sizeof(chunk->dwSize) + sizeof(chunk->dwName) + 
            SWAPBYTES(chunk->dwSize) + sizeof(DWORD); // last one is CRC
    }
}

void FreePNGTexture(BITMAPINFO* bitmap)
{
	if (bitmap != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)bitmap);
	}
}

void DrawBallPreview(IDirect3DDevice8* dev)
{
	if (strlen(model)==0 || strlen(texture)==0)
		return;
	
	if (g_needsRestore || g_newBall) 
	{
		if (g_newBall) {
			InvalidateDeviceObjects(dev);
			DeleteDeviceObjects(dev);
			g_needsRestore = TRUE;
		};
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_bserv,"DrawBallPreview: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_bserv,"DrawBallPreview: RestoreDeviceObjects() done.");
        g_needsRestore = FALSE;
	}
	
	// render
	dev->BeginScene();

	// setup renderstate
	dev->CaptureStateBlock( g_dwSavedStateBlock );
	dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );
    
    if (!g_preview_tex && g_newBall) {
        char buf[2048];
        sprintf(buf, "%s\\GDB\\balls\\%s", GetPESInfo()->gdbDir, texture);
        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 4, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR,
                    0, NULL, NULL, &g_preview_tex))) {
            Log(&k_bserv,"FAILED to load image for ball preview.");
		}
        g_newBall = false;
    }
    
    if (g_preview_tex && g_pVB_preview && g_pIB_preview) {

        // For our world matrix, we will just rotate the object about the y-axis.
        D3DXMATRIXA16 matWorldRot;

        // Set up the rotation matrix to generate 1 full rotation (2*PI radians) 
        // every 5000 ms. To avoid the loss of precision inherent in very high 
        // floating point numbers, the system time is modulated by the rotation 
        // period before conversion to a radian angle.
        UINT  iTime  = timeGetTime() % 5000;
        FLOAT fAngle = iTime * (2.0f * D3DX_PI) / 5000.0f;
        D3DXMatrixRotationY( &matWorldRot, fAngle );

        // set up translation matrix to move the ball down
        D3DXMATRIX matWorldTrans;
        D3DXMatrixTranslation(&matWorldTrans, 0.0, -260.0, 0.0);
        dev->SetTransform( D3DTS_WORLD, &(matWorldRot * matWorldTrans));

        // Set up our view matrix. A view matrix can be defined given an eye point,
        // a point to lookat, and a direction for which way is up. Here, we set the
        // eye 100 units back along the z-axis, look at the
        // origin, and define "up" to be in the y-direction.
        D3DXVECTOR3 vEyePt( 0.0f, 0.0f, -FACTOR*4 );
        D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
        D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
        D3DXMATRIXA16 matView;
        D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
        dev->SetTransform( D3DTS_VIEW, &matView );

        // For the projection matrix, we set up a orthogonal transform.
        float ar_correction = GetPESInfo()->bbWidth / (float)GetPESInfo()->bbHeight * 0.75;
        D3DXMATRIXA16 matProj;
        
        D3DXMatrixOrthoLH( &matProj, 1024*ar_correction, 768, -FACTOR*4, FACTOR*4 );
        dev->SetTransform( D3DTS_PROJECTION, &matProj );

        // texture
        dev->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0, 0);
        dev->SetIndices(g_pIB_preview,0);
        dev->SetVertexShader(D3DFVF_BALLVERTEX);
		dev->SetStreamSource( 0, g_pVB_preview, g_previewStride);
		dev->SetTexture(0, g_preview_tex);
		//dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		
        dev->DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, g_previewNumVertices,
        							0, g_previewNumPrimitives-2);
        							
        // draw lighting billboard
        if (g_lighting_tex && g_pVB_preview_lighting) {
            D3DXMATRIXA16 matWorldRot;
            D3DXMatrixRotationY( &matWorldRot, 0.0f );
            D3DXMATRIX matWorldTrans;
            D3DXMatrixTranslation(&matWorldTrans, 0.0, -260.0, 0.0);
            dev->SetTransform( D3DTS_WORLD, &(matWorldRot * matWorldTrans));

            dev->SetIndices(NULL,0);
            dev->SetVertexShader(D3DFVF_LIGHTVERTEX);
            dev->SetStreamSource( 0, g_pVB_preview_lighting, sizeof(LIGHTVERTEX));
            dev->SetTexture(0, g_lighting_tex);
            dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
            dev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
            dev->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );
            dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
            dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        }
    }

	// restore the modified renderstates
	dev->ApplyStateBlock( g_dwSavedStateBlock );

	dev->EndScene();
	
	return;
};

void bservReset(IDirect3DDevice8* self, LPVOID params)
{
	Log(&k_bserv,"bservReset: cleaning-up.");
	
	InvalidateDeviceObjects(self);
	DeleteDeviceObjects(self);
	
	g_needsRestore = TRUE;
	
	return;
}

/* creates vertex buffers */
HRESULT InitVB(IDirect3DDevice8* dev)
{
	VOID* pVertices;

	ReadBallModel();

	// create vertex buffers
	if (!g_pVB_preview && g_previewData) {
        if (FAILED(dev->CreateVertexBuffer(g_previewSize, D3DUSAGE_WRITEONLY, D3DFVF_BALLVERTEX,
                                            D3DPOOL_MANAGED, &g_pVB_preview)))
        {
            Log(&k_bserv,"CreateVertexBuffer() failed.");
            return E_FAIL;
        }
        Log(&k_bserv,"CreateVertexBuffer() done.");
        if (FAILED(g_pVB_preview->Lock(0, g_previewSize, (BYTE**)&pVertices, 0)))
		{
			Log(&k_bserv,"g_pVB_preview->Lock() failed.");
			return E_FAIL;
		}
		memcpy(pVertices, g_previewData, g_previewSize);
		g_pVB_preview->Unlock();
	};

	if (!g_pVB_preview_lighting) {
        if (FAILED(dev->CreateVertexBuffer(sizeof(g_lighting), D3DUSAGE_WRITEONLY, D3DFVF_LIGHTVERTEX,
                                            D3DPOOL_MANAGED, &g_pVB_preview_lighting)))
        {
            Log(&k_bserv,"CreateVertexBuffer() failed.");
            return E_FAIL;
        }
        Log(&k_bserv,"CreateVertexBuffer() done.");
        if (FAILED(g_pVB_preview_lighting->Lock(0, sizeof(g_lighting), (BYTE**)&pVertices, 0)))
        {
            Log(&k_bserv,"g_pVB_preview_lighting->Lock() failed.");
            return E_FAIL;
        }
        ResizeLightingRect(g_lighting, sizeof(g_lighting)/sizeof(LIGHTVERTEX));
        memcpy(pVertices, g_lighting, sizeof(g_lighting));
        g_pVB_preview_lighting->Unlock();
	}

    // create index buffers
	if (!g_pIB_preview && g_previewIBData) {
        if (FAILED(dev->CreateIndexBuffer(g_previewNumPrimitives*2, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                            D3DPOOL_MANAGED, &g_pIB_preview)))
        {
            Log(&k_bserv,"CreateIndexBuffer() failed.");
            return E_FAIL;
        }
        Log(&k_bserv,"CreateIndexBuffer() done.");
        if (FAILED(g_pIB_preview->Lock(0, g_previewNumPrimitives*2, (BYTE**)&pVertices, 0)))
		{
			Log(&k_bserv,"g_pIB_preview->Lock() failed.");
			return E_FAIL;
		}
		memcpy(pVertices, g_previewIBData, g_previewNumPrimitives*2);
		g_pIB_preview->Unlock();
	};

    return S_OK;
}



void DeleteStateBlocks(IDirect3DDevice8* dev)
{
	// Delete the state blocks
	try
	{
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_bserv,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_bserv,"g_dwDrawOverlayStateBlock deleted.");
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
	TRACE(&k_bserv,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_bserv,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // ball preview
	SafeRelease( &g_pVB_preview );
	SafeRelease( &g_pIB_preview );
	SafeRelease( &g_pVB_preview_lighting );

	Log(&k_bserv,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_bserv,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");
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
		Log(&k_bserv,"InitVB() failed.");
        return hr;
    }
	Log(&k_bserv,"InitVB() done.");

    // create reflections/shading texture
    if (!g_lighting_tex) {
        char buf[2048];
        sprintf(buf, "%s\\blight.png", GetPESInfo()->mydir);
        if (FAILED(D3DXCreateTextureFromFileEx(dev, buf, 
                    0, 0, 4, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                    D3DX_FILTER_NONE, D3DX_FILTER_LINEAR,
                    0xffffffff, NULL, NULL, &g_lighting_tex))) {
            Log(&k_bserv,"FAILED to load image for ball lighting.");
		}
    }
    
	// Create the state blocks for rendering overlay graphics
	for( UINT which=0; which<2; which++ )
	{
		dev->BeginStateBlock();
		/*
		dev->SetTextureStageState(0,D3DTSS_MAGFILTER,D3DTEXF_LINEAR);
		dev->SetTextureStageState(0,D3DTSS_MINFILTER,D3DTEXF_POINT);
		dev->SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);
		dev->SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
        dev->SetVertexShader( D3DFVF_BALLVERTEX );
        dev->SetRenderState(D3DRS_SPECULARENABLE,1);
		dev->SetTexture(0, g_preview_tex);
		dev->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
		dev->SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
        */

        D3DXMATRIXA16 matWorld;
        dev->SetTransform( D3DTS_WORLD, &matWorld );
        D3DXMATRIXA16 matView;
        dev->SetTransform( D3DTS_VIEW, &matView );
        D3DXMATRIXA16 matProj;
        dev->SetTransform( D3DTS_PROJECTION, &matProj );

		//dev->SetTexture(1, NULL);
        dev->SetIndices(g_pIB_preview,0);
        dev->SetVertexShader(D3DFVF_BALLVERTEX);
		dev->SetStreamSource( 0, g_pVB_preview, g_previewStride);
		dev->SetTexture(0, g_preview_tex);
		//dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        dev->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		
		if( which==0 )
			dev->EndStateBlock( &g_dwSavedStateBlock );
		else
			dev->EndStateBlock( &g_dwDrawOverlayStateBlock );
	}
    return S_OK;
}

void ReadBallModel()
{
	char buf[2048];
	BYTE* mdlFileCompr;
	int mdlFileSizeCompr=0;
	BYTE* mdlFile;
	DWORD mdlFileSize=0;
	
	// clear data buffers
	if (g_previewData!=NULL) {
		HeapFree(GetProcessHeap(), 0, g_previewData);
        g_previewData = NULL;
    }
	if (g_previewIBData!=NULL) {
		HeapFree(GetProcessHeap(), 0, g_previewIBData);
        g_previewIBData = NULL;
    }
	
    // try to read the model file
    sprintf(buf, "%sGDB\\balls\\mdl\\%s", GetPESInfo()->gdbDir, model);
	if (read_file_to_mem(buf,&mdlFileCompr,&mdlFileSizeCompr) != 0) {
        LogWithString(&k_bserv, "Unable to read ball model: %s", model);
        return;
    }

	mdlFileSizeCompr=*(DWORD*)(mdlFileCompr+4);
	mdlFileSize=*(DWORD*)(mdlFileCompr+8);
	mdlFile=(BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,mdlFileSize);
	
	int retval = uncompress((UCHAR*)mdlFile,&mdlFileSize,(UCHAR*)(mdlFileCompr+32),mdlFileSizeCompr);
    if (retval != Z_OK) {
        LogWithString(&k_bserv, "Unable to uncompress ball model: %s", model);
        HeapFree(GetProcessHeap(), 0, mdlFileCompr);
        return;
    }
	HeapFree(GetProcessHeap(), 0, mdlFileCompr);

	DWORD start1=*(DWORD*)(mdlFile+16);
	BYTE* indexStart=mdlFile+*(DWORD*)(mdlFile+20);

	DWORD vertHeaderStart=*(DWORD*)(mdlFile+start1+4);
	
	g_previewNumVertices=*(WORD*)(mdlFile+start1+vertHeaderStart);
	g_previewStride=*(WORD*)(mdlFile+start1+vertHeaderStart+2);
	g_previewSize=g_previewNumVertices*g_previewStride;
	
	g_previewData=(BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,g_previewSize);
	memcpy(g_previewData,mdlFile+start1+vertHeaderStart+8,g_previewSize);
	
	g_previewNumPrimitives=*(WORD*)(indexStart);
	
	g_previewIBData=(BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,g_previewNumPrimitives*2);
	memcpy(g_previewIBData,indexStart+2,g_previewNumPrimitives*2);
	
	HeapFree(GetProcessHeap(), 0, mdlFile);
	
	
	BALLVERTEX* bv=(BALLVERTEX*)g_previewData;
	for (int i=0;i<g_previewNumVertices;i++) {
		bv[i].x=bv[i].x*FACTOR;
		bv[i].y=bv[i].y*FACTOR;
		bv[i].z=bv[i].z*-FACTOR;
	};
	
	return;
};

void ResizeLightingRect(LIGHTVERTEX* dta, int n)
{
    static bool resized = false;
	if (!resized) { 
        for (int i=0;i<n;i++) {
            dta[i].x=dta[i].x*FACTOR;
            dta[i].y=dta[i].y*FACTOR;
        }
        resized = true;
    }
}

/**
 * Returns true if successful.
 */
BOOL ReadConfig(BSERV_CFG* config, char* cfgFile)
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
			LogWithNumber(&k_bserv,"ReadConfig: preview = (%d)", value);
            config->previewEnabled = (value == 1);
		}
        else if (lstrcmpi(name, "key.resetBall")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_bserv,"ReadConfig: key.resetBall = (0x%02x)", value);
            config->keyResetBall = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.randomBall")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_bserv,"ReadConfig: key.randomBall = (0x%02x)", value);
            config->keyRandomBall = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.prevBall")==0 || lstrcmpi(name, "key.previousBall")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_bserv,"ReadConfig: key.prevBall = (0x%02x)", value);
            config->keyPrevBall = (BYTE)value;
        }
        else if (lstrcmpi(name, "key.nextBall")==0) {
            if (sscanf(pValue, "%x", &value)!=1) continue;
			LogWithNumber(&k_bserv,"ReadConfig: key.nextBall = (0x%02x)", value);
            config->keyNextBall = (BYTE)value;
        }
	}
	fclose(cfg);
	return true;
}

bool CreateBallTextureBuf()
{
	BYTE* result=NULL;
	char tmp[BUFLEN];
	ZeroMemory(tmp,BUFLEN);
	DWORD size;
	BITMAPINFOHEADER *bmpinfo=NULL;
	
	if (selectedBall<0) return false;
		
	if (ballTextureBuf!=NULL && lstrcmpi(currTextureName,texture)==0)
		return true;
		
	sprintf(tmp,"%sGDB\\balls\\%s",GetPESInfo()->gdbDir,texture);
	
	if (!FileExists(tmp))
		return false;
	
	if (lstrcmpi(tmp + lstrlen(tmp)-4, ".png")==0) {
		ballTextureSize=LoadPNGTexture((BITMAPINFO**)&ballTextureBuf,tmp);
		if (ballTextureBuf==NULL)
			return false;
		bmpinfo=(BITMAPINFOHEADER*)ballTextureBuf;
		isPNGtexture=true;

	} else if (lstrcmpi(tmp + lstrlen(tmp)-4, ".bmp")==0) {
		read_file_to_mem(tmp,&ballTextureBuf,&ballTextureSize);
		if (ballTextureBuf==NULL)
			return false;
		bmpinfo=(BITMAPINFOHEADER*)((DWORD)ballTextureBuf+sizeof(BITMAPFILEHEADER));
		isPNGtexture=false;
		
	} else {
		return false;
	};
	ballTextureRect.right=bmpinfo->biWidth;
	ballTextureRect.bottom=bmpinfo->biHeight;
	
	strcpy(currTextureName,texture);
	
	return true;
}

void FreeBallTextureBuf(BYTE **pBuf)
{
	if (*pBuf!=NULL) {
        if (isPNGtexture) {
            FreePNGTexture((BITMAPINFO*)*pBuf);
            TRACE(&k_bserv, "Ball texture (PNG) buffer freed");
        }
        else {
            HeapFree(GetProcessHeap(), 0, *pBuf);
            TRACE(&k_bserv, "Ball texture (BMP) buffer freed");
        }
        *pBuf = NULL;
    }
	
	strcpy(currTextureName,"\0");
	
	return;
}


HRESULT STDMETHODCALLTYPE bservCreateTexture(
        IDirect3DDevice8* self, UINT width, UINT height,UINT levels,
        DWORD usage, D3DFORMAT format, D3DPOOL pool, 
        IDirect3DTexture8** ppTexture, DWORD src, bool* IsProcessed)
{
	HRESULT res=D3D_OK;
	RECT texSize;
	
	if (*IsProcessed==true)
		return res;
	
	g_lastBallTex=NULL;
	
	if (IsBadReadPtr((BYTE*)src,gdbBallSize) || 
            gdbBallCRC!=GetCRC((BYTE*)src,gdbBallSize)) {
		//wrong CRC -> data changed
		gdbBallAddr=0;
		gdbBallSize=0;
		gdbBallCRC=0;
	}
	
	if (src!=0 && src==gdbBallAddr) {
		TRACE(&k_bserv,"bservCreateTexture called for ball texture.");
		
		if (!CreateBallTextureBuf()) 
            goto NoReplacingTexture;
		
        DWORD w = max(width, ballTextureRect.right);
        DWORD h = max(height, ballTextureRect.bottom);
		res = OrgCreateTexture(
                self, w, h, levels,usage,format,pool,ppTexture);
		g_lastBallTex = *ppTexture;
		TRACE2(&k_bserv,"tex = %08x", (DWORD)g_lastBallTex);
	}

	NoReplacingTexture:

	if (g_lastBallTex != NULL)
		*IsProcessed=true;

	return res;
}

void bservUnlockRect(IDirect3DTexture8* self,UINT Level)
{
	//TRACE2X(&k_bserv,"bservUnlockRect: Processing texture %x, level %d",(DWORD)self,Level);
	
	if (ballTextureBuf==NULL || g_lastBallTex==NULL)
		return;
		
	IDirect3DSurface8* g_surf = NULL;

	if (SUCCEEDED(g_lastBallTex->GetSurfaceLevel(0, &g_surf))) {
		if (SUCCEEDED(D3DXLoadSurfaceFromFileInMemory(
						g_surf, NULL, NULL, //destination
						ballTextureBuf, ballTextureSize, NULL, //source
						D3DX_DEFAULT, 0, NULL)))
		{ 
			TRACE(&k_bserv,"Replacing ball texture COMPLETE");
		}
		else
		{
			TRACE(&k_bserv,"Replacing ball texture FAILED");
		}

		g_surf->Release();
	}

	g_lastBallTex=NULL;
    FreeBallTextureBuf(&ballTextureBuf);

	return;
};

DWORD SetBallName(char** names, DWORD numNames, DWORD p3, DWORD p4, DWORD p5, DWORD p6, DWORD p7)
{
	if (selectedBall>=0 && numNames==3) {
		//strcpy(names[1],balls[selectedBall].display);
		names[1]=balls[selectedBall].display;
	};
	
	return MasterCallNext(names,numNames,p3,p4,p5,p6,p7);
};

WORD GetTeamId(int which)
{
    BYTE* mlData;
    if (dta[TEAM_IDS]==0) return 0xffff;
    WORD id = ((WORD*)dta[TEAM_IDS])[which];
    if (id==0xf2 || id==0xf3) {
        switch (id) {
            case 0xf2:
                // master league team (home)
                mlData = *((BYTE**)dta[ML_HOME_AREA]);
                id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
                break;
            case 0xf3:
                // master league team (away)
                mlData = *((BYTE**)dta[ML_AWAY_AREA]);
                id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
                break;
        }
    }
    return id;
}

void updateHomeBall()
{
	if (g_homeTeamChoice) {
        WORD teamId = GetTeamId(HOME);
        std::map<WORD,int>::iterator hit = g_HomeBallMap.find(teamId);
        if (hit !=  g_HomeBallMap.end()) {
            SetBall(hit->second);
            noHomeBall = false;

        } else {
        	noHomeBall = true;
            return;
        }
    } else {
    	noHomeBall = false;
    }
    return;
}
