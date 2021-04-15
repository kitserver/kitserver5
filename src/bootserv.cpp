// bootserv.cpp
#include <windows.h>
#include <stdio.h>
#include <ctime>
#include <map>
#include "bootserv.h"
#include "kload_exp.h"

KMOD k_boot={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};
HINSTANCE hInst;

#define DATALEN 15
enum {
    EDITMODE_CURRPLAYER_MOD, EDITMODE_CURRPLAYER_ORG, EDITMODE_CURRPLAYER_MOD_DIRECT,
    EDITMODE_FLAG, EDITPLAYERMODE_FLAG, EDITBOOTS_FLAG, EDITPLAYER_ID,
    PLAYERS_LINEUP, PLAYERID_IN_PLAYERDATA,
    LINEUP_RECORD_SIZE, LINEUP_BLOCKSIZE, PLAYERDATA_SIZE,
    BOOTTYPE_OFFSET, BOOTTYPE_RESET_MASK, BOOTTYPE_TEST_MASK,
};
DWORD dataArray[][DATALEN] = {
    // PES5 DEMO 2
    {
        0, 0, 0,
        0, 0, 0, 0,
        0, 0,
        0, 0, 0,
        0, 0, 0,
    },
    // PES5 
    {
        0, 0, 0x38f9520,
        0x38f97f8, 0x38f9ae4, 0x38f9be8, 0x3937218,
        0x3bcfb10, 0x3bd3e00,
        0x290, 0x60, 0x344,
        0x1a, 0xf0, 0x0f,
    },
    // WE9 
    {
        0, 0, 0x38f9520,
        0x38f97f8, 0x38f9ae4, 0x38f9be8, 0x3937218,
        0x3bcfb30, 0x3bd3e20,
        0x290, 0x60, 0x344,
        0x1a, 0xf0, 0x0f,
    },
    // WE9LE
    {
        0, 0, 0x3833e30,
        0x3834108, 0x38343f4, 0x38344f8, 0x388c070,
        0x3b57650, 0x3b5b940,
        0x290, 0x60, 0x344,
        0x1a, 0xf0, 0x0f,
    },
    // PES6
    {
        0x112e1c0, 0x112e1c8, 0,
        0x1108488, 0x11084e8, 0x1108830, 0x112e24a,
        0x3bdcbc0, 0x3bcf586, 
        0x240, 0x60, 0x348,
        0x1c, 0x0f, 0xf0,
    },
    // PES6 1.10
    {
        0x112f1c0, 0x112f1c8, 0,
        0x1109488, 0x11094e8, 0x1109830, 0x112f24a,
        0x3bddbc0, 0x3bd0586,
        0x240, 0x60, 0x348,
        0x1c, 0x0f, 0xf0,
    },
};
DWORD data[DATALEN];

#define CODELEN 2
enum {
    C_COPYPLAYERDATA_CS,
    C_RESETFLAG2_CS,
};
DWORD codeArray[][CODELEN] = {
    // PES5 DEMO 2
    {
        0,
        0,
    },
    // PES5
    {
        0x4dd5c0,
        0x50730a, //? - might be unnecessary
    },
    // WE9
    {
        0x4dda10,
        0x50777a, //?
    },
    // WE9LE
    {
        0x4dc0d0,
        0x50218a, //?
    },
    // PES6
    {
        0xa4b4b2,
        0x9c15a7,
    },
    // PES6 1.10
    {
        0xa4b612,
        0x9c1737,
    },
};
DWORD code[CODELEN];

class BootServConfig {
public:
    BootServConfig() : _randomBootsEnabled(false) {}
    bool _randomBootsEnabled;
};

class TexturePack {
public:
    TexturePack() : _big(NULL),_small(NULL),_textureFile(),_bigLoaded(false),_smallLoaded(false) {}
    ~TexturePack() {
        if (_big) _big->Release();
        if (_small) _small->Release();
    }
    IDirect3DTexture8* _big;
    IDirect3DTexture8* _small;
    string _textureFile;
    bool _bigLoaded;
    bool _smallLoaded;
};

//////////////////////////////////////////////////////
// Globals ///////////////////////////////////////////

static BootServConfig g_config;
vector<string> g_bootTextureFiles;
map<WORD,TexturePack> g_bootTexturePacks;
map<WORD,TexturePack> g_bootTexturePacksRandom;
IDirect3DTexture8* g_bootTextures[2][64]; //[big/small][32*team + posInTeam]
DWORD g_bootPlayerIds[64]; //[32*team + posInTeam]

//////////////////////////////////////////////////////

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void bootInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface);
void DumpTexture(IDirect3DTexture8* const ptexture);
void ReplaceTextureLevel(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT level);
void ReplaceBootTexture(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT width);
void ReplaceBootTexture2(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT height);
KEXPORT void bootUnlockRect(IDirect3DTexture8* self,UINT Level);
KEXPORT DWORD bootCopyPlayerData(DWORD p0, DWORD p1, DWORD p2);
void readConfig();
void readMap();
void populateTextureFilesVector(vector<string>& vec, string& currDir);
void bootBeginUniSelect();
DWORD bootResetFlag2();
int getPosition(DWORD blockAddr);
WORD getPlayerId(int pos);
bool isEditBootsMode();

//////////////////////////////////////////////////////
//
// FUNCTIONS
//
//////////////////////////////////////////////////////

// Calls IUnknown::Release() on an instance
void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_boot,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_boot,"Problem with safe-release");
    }
}

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_boot,"Attaching dll...");

		switch (GetPESInfo()->GameVersion) {
			case gvPES5PC: //support for PES5 PC...
			case gvWE9PC: //... and WE9 PC
            case gvWE9LEPC: //... and WE9:LE PC
				break;

            default:
                Log(&k_boot,"Your game version is currently not supported!");
                return false;
		}

		hInst=hInstance;
		RegisterKModule(&k_boot);
		HookFunction(hk_D3D_CreateDevice,(DWORD)bootInit);
		
		// read config
    	readConfig();
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_boot,"Detaching dll...");
		UnhookFunction(hk_D3D_CreateDevice,(DWORD)bootInit);
        UnhookFunction(hk_D3D_UnlockRect,(DWORD)bootUnlockRect);
        UnhookFunction(hk_BeginUniSelect,(DWORD)bootBeginUniSelect);
	}

	return true;
}

void bootInit(IDirect3D8* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice8** ppReturnedDeviceInterface) 
{
    Log(&k_boot, "Initializing bootserver...");
    memcpy(code, codeArray[GetPESInfo()->GameVersion], sizeof(code));
    memcpy(data, dataArray[GetPESInfo()->GameVersion], sizeof(data));

    HookFunction(hk_D3D_UnlockRect,(DWORD)bootUnlockRect);
    HookFunction(hk_BeginUniSelect,(DWORD)bootBeginUniSelect);
    MasterHookFunction(code[C_COPYPLAYERDATA_CS], 3, bootCopyPlayerData);
    //MasterHookFunction(code[C_RESETFLAG2_CS], 0, bootResetFlag2);

    // read the map
    readMap();

    if (g_config._randomBootsEnabled) {
        populateTextureFilesVector(g_bootTextureFiles, string(""));
        LogWithNumber(&k_boot, "Total # of boot textures found: %d", g_bootTextureFiles.size());
    }

    // seed the random generator
    time_t timer;
    srand(time(&timer));
}

void readConfig()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sfile[BUFLEN];
	WORD number=0;
	
	strcpy(tmp,GetPESInfo()->mydir);
	strcat(tmp,"\\bootserv.cfg");
	
	FILE* cfg=fopen(tmp, "rt");
	if (cfg==NULL) {
		return;
	}
	
	while (true) {
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		
		// skip comments
		comment=NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		ZeroMemory(sfile,BUFLEN);
        int val = 0;
		if (sscanf(str,"random-boots.enabled = %d",&val)==1) {
            g_config._randomBootsEnabled = (val==1);
        }

		if (feof(cfg)) break;
	}
	fclose(cfg);
    LogWithNumber(&k_boot, "readConfig: g_config._randomBootsEnabled = %d", 
            g_config._randomBootsEnabled);
}

void readMap()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sfile[BUFLEN];
	WORD number=0;
	
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\boots\\map.txt");
	
	FILE* cfg=fopen(tmp, "rt");
	if (cfg==NULL) {
		Log(&k_boot,"readMap: Couldn't find boots map!");
		return;
	}
	
	while (true) {
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		
		// skip comments
		comment=NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		ZeroMemory(sfile,BUFLEN);
		if (sscanf(str,"%d , \"%[^\"]\"",&number,sfile)==2) {
            TexturePack pack;
            pack._textureFile = sfile;
            // add to the map
            g_bootTexturePacks.insert(pair<WORD,TexturePack>(number,pack));
        }

		if (feof(cfg)) break;
	}
	fclose(cfg);
    LogWithNumber(&k_boot, "readMap: g_bootTexturePacks.size() = %d", g_bootTexturePacks.size());
}

void populateTextureFilesVector(vector<string>& vec, string& currDir)
{
    // traverse the "boots" folder and place all the filenames, with paths
    // relative to "boots" into the vector.
    
	WIN32_FIND_DATA fData;
	char pattern[512] = {0};
	ZeroMemory(pattern, sizeof(pattern));

    sprintf(pattern, "%sGDB\\boots\\%s*", 
            GetPESInfo()->gdbDir, currDir.c_str());
    LOG(&k_boot, "pattern: {%s}", pattern);

	HANDLE hff = FindFirstFile(pattern, &fData);
	if (hff != INVALID_HANDLE_VALUE) {
        while(true) {
            // check if this is a directory
            if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                // check for system folders
                if (fData.cFileName[0]!='.') {
                    // process the dir recursively
                    populateTextureFilesVector(
                            vec, currDir+string(fData.cFileName)+"\\");
                }
            } else if (stricmp(fData.cFileName + 
                        strlen(fData.cFileName)-4, ".png")==0
                    || stricmp(fData.cFileName + 
                        strlen(fData.cFileName)-4, ".bmp")==0) {
                // a BMP or a PNG: consider it a boot texture
                vec.push_back(currDir + string(fData.cFileName));
                LogWithString(&k_boot, 
                        "Found boot texture: %s", (char*)vec.back().c_str());
            }
            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);
    }
}

void releaseBootTextures()
{
    // release the boot textures, so that we don't consume too much memory for boots
    for (map<WORD,TexturePack>::iterator it = g_bootTexturePacks.begin();
            it != g_bootTexturePacks.end();
            it++) {
        if (it->second._big) {
            it->second._big->Release();
            it->second._big = NULL;
            it->second._bigLoaded = false;
            LogWithNumber(&k_boot, "Released big boot texture for player %d", it->first);
        }
        if (it->second._small) {
            it->second._small->Release();
            it->second._small = NULL;
            it->second._smallLoaded = false;
            LogWithNumber(&k_boot, "Released small boot texture for player %d", it->first);
        }
    }
    for (it = g_bootTexturePacksRandom.begin();
            it != g_bootTexturePacksRandom.end();
            it++) {
        if (it->second._big) {
            it->second._big->Release();
            it->second._big = NULL;
            it->second._bigLoaded = false;
            LogWithNumber(&k_boot, "Released big boot texture for player %d", it->first);
        }
        if (it->second._small) {
            it->second._small->Release();
            it->second._small = NULL;
            it->second._smallLoaded = false;
            LogWithNumber(&k_boot, "Released small boot texture for player %d", it->first);
        }
    }
}

void bootBeginUniSelect()
{
	Log(&k_boot,"bootBeginUniSelect: releasing boot textures");
    releaseBootTextures();
    // clear the map for random boot assignment
    g_bootTexturePacksRandom.clear();
}

DWORD bootResetFlag2()
{
	Log(&k_boot,"bootResetFlag2: releasing boot textures");
    releaseBootTextures();

    // call original
    DWORD result = MasterCallNext();
    return result;
}

vector<string>::iterator getRandomElement(vector<string>& vec)
{
    int pos = rand() % vec.size();
    return vec.begin() + pos;
}

static void SetDimensions(D3DXIMAGE_INFO *ii, UINT &w, UINT &h) {
    // Find closest smaller Power-Of-2 texture
    h = 256;
    while ((h<<1) <= ii->Height) h = h<<1;
    if (ii->Height == ii->Width) {
        w = h;
    }
    else {
        w = 170;
        while ((w<<1) <= ii->Width) w = w<<1;
    }
}

IDirect3DTexture8* getBootTexture(WORD playerId, bool big, D3DSURFACE_DESC* pDesc, UINT levels)
{
    IDirect3DTexture8* bootTexture = NULL;
    map<WORD,TexturePack>::iterator it = g_bootTexturePacks.find(playerId);
    if (it != g_bootTexturePacks.end()) {
        // map has an entry for this player
        if (big && it->second._bigLoaded) {
            // already looked up and the texture should be loaded
            return it->second._big;

        } else if (!big && it->second._smallLoaded) {
            // already looked up and the texture should be loaded
            return it->second._small;

        } else {
            // haven't tried to load the textures for this player yet.
            // Do it now.
			D3DXIMAGE_INFO ii;
            IDirect3DTexture8* temp;
            char filename[BUFLEN];
            sprintf(filename,"%sGDB\\boots\\%s", GetPESInfo()->gdbDir, it->second._textureFile.c_str());
            if (SUCCEEDED(D3DXGetImageInfoFromFile(filename, &ii))) {
                UINT w, h;
                SetDimensions(&ii, w, h);
                w = (big)? w : w/4;
                h = (big)? h : h/4;
                if (SUCCEEDED(D3DXCreateTextureFromFileEx(GetActiveDevice(), filename,
                            w, h, 1, 0,
                            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR,
                            0, NULL, NULL, &temp))) {
                    UINT Height = h;
                    UINT Width = Height*2;
                // create canvas
                char canvasFilename[512];
                    sprintf(canvasFilename, "%s\\bcanvas.png", GetPESInfo()->mydir);
                    if (SUCCEEDED(D3DXCreateTextureFromFileEx(
                                    GetActiveDevice(), canvasFilename,
                                    Width, Height, 1, 0, 
                                    D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                    D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                                    0, NULL, NULL, &bootTexture))) {
                    // copy the temp texture to canvas 3 times
                    ReplaceBootTexture2(bootTexture, temp, Height);
                }
                // release the temp texture, as we don't need it anymore
                temp->Release();

                // store in the map
                if (big) {
                        it->second._big = bootTexture;
                        LOG(&k_boot, "getBootTexture: loaded big (%dx%d) texture for player %d", w, h, playerId);
                    } else {
                        it->second._small = bootTexture;
                        LOG(&k_boot, "getBootTexture: loaded small (%dx%d) texture for player %d", w, h, playerId);
                    }

				} else {
					LogWithString(&k_boot, "D3DXCreateTextureFromFileEx FAILED for %s", filename);
				}
			}
            else {
                LogWithString(&k_boot, "D3DXGetImageInfoFromFile FAILED for %s", filename);
            }

            // update loaded flags, so that we only load each texture once
            if (big) {
                it->second._bigLoaded = true;
            } else {
                it->second._smallLoaded = true;
            }
        }

    } else if (g_config._randomBootsEnabled) {
        // not in the main map. 
        // try random boots map instead: if no entry, it will be created
        TexturePack& pack = g_bootTexturePacksRandom[playerId];
        if (big && pack._bigLoaded) {
            // already looked up and the texture should be loaded
            return pack._big;

        } else if (!big && pack._smallLoaded) {
            // already looked up and the texture should be loaded
            return pack._small;

        } else {
            // haven't tried to load this texture for this player yet.
            // Do it now. Select the texture randomly from the list, unless
            // already picked earlier
            if (pack._textureFile == "") {
                vector<string>::iterator vit = getRandomElement(g_bootTextureFiles);
                if (vit == g_bootTextureFiles.end()) return NULL;
                pack._textureFile = *vit;
            }

            IDirect3DTexture8* temp;
            char filename[BUFLEN];
            sprintf(filename,"%sGDB\\boots\\%s", GetPESInfo()->gdbDir, pack._textureFile.c_str());
            if (SUCCEEDED(D3DXCreateTextureFromFileEx(
                            GetActiveDevice(), filename,
                            0, 0, 1, 0, 
                            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                            0, NULL, NULL, &temp))) {
                // create canvas
                char canvasFilename[512];
                sprintf(canvasFilename, "%s\\bcanvas.png", GetPESInfo()->mydir);
                D3DSURFACE_DESC desc;
                temp->GetLevelDesc(0, &desc);
                UINT Height = (big)? desc.Height : desc.Height/4;
                UINT Width = Height*2;
                if (SUCCEEDED(D3DXCreateTextureFromFileEx(
                                GetActiveDevice(), canvasFilename,
                                Width, Height, 1, 0, 
                                D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                                0, NULL, NULL, &bootTexture))) {
                    // copy the temp texture to canvas 3 times
                    ReplaceBootTexture2(bootTexture, temp, Height);
                }
                // release the temp texture, as we don't need it anymore
                temp->Release();

                if (big) {
                    pack._big = bootTexture;
                    pack._bigLoaded = true;
                    LogWithNumberAndString(&k_boot, 
                            "getBootTexture: loaded random big texture for player %d: %s", 
                            playerId, (char*)pack._textureFile.c_str());
                } else {
                    pack._small = bootTexture;
                    pack._smallLoaded = true;
                    LogWithNumberAndString(&k_boot, 
                            "getBootTexture: loaded random small texture for player %d: %s", 
                            playerId, (char*)pack._textureFile.c_str());
                }

            } else {
                LogWithString(&k_boot, "D3DXCreateTextureFromFileEx FAILED for %s", filename);
            }
        }
    }
    return bootTexture;
}

bool isEditMode()
{
    return *(BYTE*)data[EDITMODE_FLAG] == 1;
}

bool isEditPlayerMode()
{
    return *(BYTE*)data[EDITPLAYERMODE_FLAG] == 1;
}

bool isEditBootsMode()
{
    return *(BYTE*)data[EDITBOOTS_FLAG] == 1;
}

DWORD bootCopyPlayerData(DWORD p0, DWORD p1, DWORD p2)
{
	DWORD playerNumber, addr;
	__asm mov playerNumber, ebx
	__asm mov addr, edi
	
	DWORD result = MasterCallNext(p0,p1,p2);

    bool needBootTypeReset = false;
    BYTE* pBootType = (BYTE*)(addr-data[BOOTTYPE_OFFSET]+1);
    if (g_config._randomBootsEnabled) {
        needBootTypeReset = true;
    } else {
        map<WORD,TexturePack>::iterator it = g_bootTexturePacks.find(playerNumber);
        needBootTypeReset = (it != g_bootTexturePacks.end());
    }

    if (needBootTypeReset) {
        *pBootType &= data[BOOTTYPE_RESET_MASK];
        LogWithTwoNumbers(&k_boot,"setting boot-type to 0 at %08x -> player %d",
                (DWORD)pBootType, playerNumber);
    }
	return result;
}

int getPosition(DWORD blockAddr)
{
    WORD val = *(WORD*)blockAddr;
    DebugWithNumber(&k_boot,"val: %04x", val);
    int pos = val - 0x2486 - 1;
    return pos;
}

WORD getPlayerId(int pos)
{
    // quick sanity-check
    if (pos<0 || pos>21) {
        return 0xffff; // unknown id
    }

    LINEUP_RECORD* lineupRecord =
        (LINEUP_RECORD*)(data[PLAYERS_LINEUP] + data[LINEUP_RECORD_SIZE]*pos);
    WORD playerId = *(WORD*)(data[PLAYERID_IN_PLAYERDATA] 
            + (lineupRecord->isRight*0x20 + lineupRecord->playerOrdinal)*data[PLAYERDATA_SIZE]);
    return playerId;
}

KEXPORT void bootUnlockRect(IDirect3DTexture8* self, UINT Level) 
{
    DWORD oldEBP;
    __asm mov oldEBP,ebp;

    static int count = 0;

    int levels = self->GetLevelCount();
    D3DSURFACE_DESC desc;
    if (SUCCEEDED(self->GetLevelDesc(0, &desc))) {
        if (((desc.Width==512 && desc.Height==256 && Level==0) 
                    || (desc.Width==128 && desc.Height==64 && Level==0))
                && *(DWORD*)(oldEBP+0x3c)==0 
                && *(DWORD*)(oldEBP+0x34) + 8 == *(DWORD*)(oldEBP+0x38)
                && (*(WORD*)(*(DWORD*)(oldEBP+0x2c)) & 0xfff0)==0x0510
                ) {

            WORD playerId = 0xffff;
            if (isEditMode()) {
                if (isEditBootsMode()) {
                    return; // no replacement in Edit Boots
                }
                // edit player
                if (desc.Width==512) {
                    playerId = *(WORD*)data[EDITPLAYER_ID];
                    // check boot-type: don't replace the texture if boot-type
                    // is not "editable". That may have some undesirable effects later
                    // if the texture becomes cached.
                    if (data[EDITMODE_CURRPLAYER_MOD]) { //PES6
                        DWORD* pBaseCopy = (DWORD*)data[EDITMODE_CURRPLAYER_MOD];
                        if (*pBaseCopy) {
                            BYTE bootType = *(BYTE*)(*pBaseCopy + 0x61) & data[BOOTTYPE_TEST_MASK];
                            if (bootType!=0) return;
                        }
                    } else if (data[EDITMODE_CURRPLAYER_MOD_DIRECT]) {  //PES5/WE9/WE9LE
                        DWORD pBaseCopy = data[EDITMODE_CURRPLAYER_MOD_DIRECT];
                        BYTE bootType = *(BYTE*)(pBaseCopy + 0x63) & data[BOOTTYPE_TEST_MASK];
                        if (bootType!=0) return;
                    }
                }
            } else {
                // match or training
                int pos = getPosition(*(DWORD*)(oldEBP+0x30));
                playerId = getPlayerId(pos);
            }
            //DumpTexture(self);
            //LogWithNumber(&k_boot, "bootUnlockRect: playerId = %d", playerId);

            bool isBig = desc.Width==512;
            IDirect3DTexture8* bootTexture = getBootTexture(playerId, isBig, &desc, levels);
            if (bootTexture) {
                // replace the texture
                ReplaceTextureLevel(self, bootTexture, 0);
            }
        }

    } else {
        Log(&k_boot, "GetLevelDesc FAILED");
    }
}

void ReplaceTextureLevel(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT level)
{
    IDirect3DSurface8* src = NULL;
    IDirect3DSurface8* dest = NULL;

    if (SUCCEEDED(srcTexture->GetSurfaceLevel(level, &dest))) {
        if (SUCCEEDED(repTexture->GetSurfaceLevel(level, &src))) {
            //DumpTexture(srcTexture);
            if (SUCCEEDED(D3DXLoadSurfaceFromSurface(
                            dest, NULL, NULL,
                            src, NULL, NULL,
                            D3DX_DEFAULT, 0))) {
                //LogWithNumber(&k_boot,"ReplaceTextureLevel: replacing level %d COMPLETE", level);

            } else {
                //LogWithNumber(&k_boot,"ReplaceTextureLevel: replacing level %d FAILED", level);
            }
            src->Release();
        }
        dest->Release();
    }
}

void ReplaceBootTexture(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT width)
{
    IDirect3DSurface8* src = NULL;
    IDirect3DSurface8* dest = NULL;

    RECT destRect;
    RECT srcRect = {0,0,170,256};
    if (width == 512) {
        destRect.top=0; destRect.left=0;
        destRect.bottom=256; destRect.right=170;
    } else {
        destRect.top=0; destRect.left=0;
        destRect.bottom=64; destRect.right=170/4;
    }

    if (SUCCEEDED(srcTexture->GetSurfaceLevel(0, &dest))) {
        if (SUCCEEDED(repTexture->GetSurfaceLevel(0, &src))) {
            // need 3 copies of the boot texture
            for (int i=0; i<3; i++) {
                destRect.left = (width==512) ? (i*170) : (i*170/4);
                destRect.right = (width==512) ? ((i+1)*170) : ((i+1)*170/4);
                if (SUCCEEDED(D3DXLoadSurfaceFromSurface(
                                dest, NULL, &destRect,
                                src, NULL, &srcRect,
                                D3DX_DEFAULT, 0))) {
                } else {
                }
            }
            src->Release();
        }
        dest->Release();
    }
}

void ReplaceBootTexture2(IDirect3DTexture8* srcTexture, IDirect3DTexture8* repTexture, UINT height)
{
    IDirect3DSurface8* src = NULL;
    IDirect3DSurface8* dest = NULL;

    float ratio = 170/256.0;
    UINT width = height*ratio;

    RECT srcRect = {0,0,width,height};
    RECT destRect = {0,0,width,height};

    if (SUCCEEDED(srcTexture->GetSurfaceLevel(0, &dest))) {
        if (SUCCEEDED(repTexture->GetSurfaceLevel(0, &src))) {
            // need 3 copies of the boot texture
            for (int i=0; i<3; i++) {
                destRect.left = i*width;
                destRect.right = (i+1)*width;
                if (SUCCEEDED(D3DXLoadSurfaceFromSurface(
                                dest, NULL, &destRect,
                                src, NULL, &srcRect,
                                D3DX_DEFAULT, 0))) {
                } else {
                }
            }
            src->Release();
        }
        dest->Release();
    }
}

void DumpTexture(IDirect3DTexture8* const ptexture) 
{
    static int count = 0;
    char buf[BUFLEN];
    sprintf(buf,"%s\\%03d_tex_%08x.bmp",
            GetPESInfo()->mydir, count++, (DWORD)ptexture);
    if (FAILED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, ptexture, NULL))) {
        LogWithString(&k_boot, "DumpTexture: failed to save texture to %s", buf);
    }
}

