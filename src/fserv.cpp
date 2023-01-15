// fserv.cpp
#include <windows.h>
#include <stdio.h>
#include "fserv.h"
#include "fserv_config.h"
#include "kload_exp.h"
#include "crc32.h"

#include <map>
#include <deque>

KMOD k_fserv={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

bool dump_it = false;

HINSTANCE hInst;
bool Inited=false;
bool savegameModPresent=false;
BYTE g_savedDWORDFaceIDfix[2];
BYTE g_savedCalcHairFile[18];
BYTE g_savedCalcSpHairFile[15];
BYTE g_savedCopyPlayerData[5];
BYTE g_savedReplCopyPlayerData[11];

bool bGetHairTranspHooked=false;
bool bEditCopyPlayerDataHooked=false;

FSERV_CONFIG *g_config;

COPYPLAYERDATA orgCopyPlayerData=NULL;
GETHAIRTRANSP orgGetHairTransp=NULL;
EDITCOPYPLAYERDATA orgEditCopyPlayerData=NULL;

DWORD StartsStdFaces[4];
DWORD NumStdFaces[4];
DWORD NumTotalFaces[4];
DWORD MaxNumStdFaces;

DWORD NumStdHair;
DWORD NumTotalHair;

std::map<DWORD,LPVOID> g_Buffers;
std::map<DWORD,LPVOID>::iterator g_BuffersIterator;
//Stores the filenames to a face id
//std::map<DWORD,char*> g_Faces[4];
std::map<DWORD,char*> g_Faces;
std::map<DWORD,char*>::iterator g_FacesIterator;
//DWORD numFaces[4];
DWORD numFaces;

std::map<DWORD,char*> g_Hair;
std::map<DWORD,char*>::iterator g_HairIterator;
std::map<DWORD,BYTE> g_HairTransp;
DWORD numHair=0;
//Stores larger face id for players
std::map<DWORD,DWORD> g_Players;
std::map<DWORD,DWORD> g_PlayersHair;
std::map<DWORD,DWORD> g_PlayersAddr;
std::map<DWORD,DWORD> g_PlayersAddr2;
std::map<DWORD,DWORD> g_SpecialFaceHair;
std::map<DWORD,DWORD>::iterator g_PlayersIterator;

BYTE isInEditPlayerMode=0, isInEditPlayerList=0;
DWORD lastPlayerNumber=0, lastFaceID=0, lastSkincolor=0;
DWORD lastHairID=0, lastGDBHairID=0, lastGDBHairRes=0;
bool lastWasFromGDB=false, lastHairWasFromGDB=false, hasChanged=true;
char lastPlayerNumberString[BUFLEN],lastFaceFileString[BUFLEN];
char lastHairFileString[BUFLEN];

std::deque<string> g_faceFilesBig;
std::deque<string> g_faceFilesSmall;
std::map<IDirect3DTexture8*,string> g_BigFaceTextures;
std::map<IDirect3DTexture8*,string> g_SmallFaceTextures;

std::deque<string> g_hairFilesBig;
std::deque<string> g_hairFilesSmall;
std::map<IDirect3DTexture8*,string> g_BigHairTextures;
std::map<IDirect3DTexture8*,string> g_SmallHairTextures;

std::map<string,WORD> g_FBTexFileToTid;
std::map<WORD,string> g_FBTexTidToFile;
std::map<string,WORD> g_FSTexFileToTid;
std::map<WORD,string> g_FSTexTidToFile;
std::map<string,WORD> g_HBTexFileToTid;
std::map<WORD,string> g_HBTexTidToFile;
std::map<string,WORD> g_HSTexFileToTid;
std::map<WORD,string> g_HSTexTidToFile;

CRITICAL_SECTION _face_cs;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitFserv();

void ReadPlayerInfo(PLAYERINFO* pi, DWORD StartAddr);
void WritePlayerInfo(PLAYERINFO* pi, DWORD StartAddr);
bool FileExists(const char* filename);

void GetGDBFaces();
void AddPlayerFace(DWORD PlayerNumber,char* sfile);
DWORD GetIDForFaceName(DWORD sk, char* sfile);

void GetGDBHair();
void AddPlayerHair(DWORD PlayerNumber,char* sfile,BYTE transp);
DWORD GetIDForHairName(char* sfile);

DWORD CalcHairFile(BYTE Caller);

DWORD GetNextSpecialAfsFileIdForFace(DWORD FaceID, BYTE Skincolor);

void SavePlayerList();
void FreeAllBuffers();
void fservFileFromAFS(DWORD infoBlock);
bool fservFreeMemory(DWORD addr);
void fservProcessPlayerData(DWORD ESI, DWORD* PlayerNumber);
void PrintPlayerInfo(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void fservCopyPlayerData(DWORD p1, DWORD p2, DWORD p3);
void fservReplCopyPlayerData();
DWORD fservEditCopyPlayerData(DWORD playerNumber, DWORD p2);
BYTE fservGetHairTransp(DWORD addr);

void fservConnectAddrToId(DWORD addr, DWORD id);

SAVEGAMEFUNCS* sgf;

/**
 * Utility function to dump any data to file.
 */
void DumpData(BYTE* addr, DWORD size)
{
    static int count = 0;

    // prepare filename
    char name[BUFLEN];
    ZeroMemory(name, BUFLEN);
    sprintf(name, "%s%s%02d.dat", GetPESInfo()->mydir, "dd", count++);

    FILE* f = fopen(name, "wb");
    if (f != NULL)
    {
        fwrite(addr, size, 1, f);
        fclose(f);
        LogWithString(&k_fserv,"Dumped to file %s.",name);
    }
}


HRESULT STDMETHODCALLTYPE fservCreateTexture(
        IDirect3DDevice8* self, UINT width, UINT height,UINT levels,
        DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture,
        DWORD src, bool* IsProcessed);
void fservUnlockRect(IDirect3DTexture8* self,UINT Level);

void SetFaceDimensions(D3DXIMAGE_INFO *ii, UINT &w, UINT &h) {
    if (g_config->npot_textures) {
        // Not-Power-Of-2 textures allowed
        w = (ii->Width/64)*64;
        h = (ii->Height/128)*128;
    }
    else {
        // Find closest smallest Power-Of-2 texture
        w = HD_FACE_MIN_WIDTH;
        while ((w<<1) <= ii->Width) w = w<<1;
        h = HD_FACE_MIN_HEIGHT;
        while ((h<<1) <= ii->Height) h = h<<1;
    }
    // clamp to max dimensions, if set
    if (g_config->hd_face_max_width > 0 && w > g_config->hd_face_max_width) {
        w = g_config->hd_face_max_width;
    }
    if (g_config->hd_face_max_height > 0 && h > g_config->hd_face_max_height) {
        h = g_config->hd_face_max_height;
    }
    w = (w < HD_FACE_MIN_WIDTH)?HD_FACE_MIN_WIDTH:w;
    h = (h < HD_FACE_MIN_HEIGHT)?HD_FACE_MIN_HEIGHT:h;
}

void SetHairDimensions(D3DXIMAGE_INFO *ii, UINT &w, UINT &h) {
    if (g_config->npot_textures) {
        // Not-Power-Of-2 textures allowed
        w = (ii->Width/128)*128;
        h = (ii->Height/64)*64;
    }
    else {
        // Find closest smallest Power-Of-2 texture
        w = HD_HAIR_MIN_WIDTH;
        while ((w<<1) <= ii->Width) w = w<<1;
        h = HD_HAIR_MIN_HEIGHT;
        while ((h<<1) <= ii->Height) h = h<<1;
    }
    // clamp to max dimensions, if set
    if (g_config->hd_hair_max_width > 0 && w > g_config->hd_hair_max_width) {
        w = g_config->hd_hair_max_width;
    }
    if (g_config->hd_hair_max_height > 0 && h > g_config->hd_hair_max_height) {
        h = g_config->hd_hair_max_height;
    }
    w = (w < HD_HAIR_MIN_WIDTH)?HD_HAIR_MIN_WIDTH:w;
    h = (h < HD_HAIR_MIN_HEIGHT)?HD_HAIR_MIN_HEIGHT:h;
}

void StoreInTexMaps(std::map<string,WORD>& s2w, std::map<WORD,string>& w2s, WORD tid, string hdfilename) {
      EnterCriticalSection(&_face_cs);
/**
      std::map<string,WORD>::iterator sit;
      sit = s2w.find(hdfilename);
      if (sit != s2w.end()) {
            std::map<WORD,string>::iterator wit;
           wit = w2s.find(sit->second);
            if (wit != w2s.end()) {
                w2s.erase(wit);
            }
      }
**/
      w2s[tid] = hdfilename;
      s2w[hdfilename] = tid;
      LeaveCriticalSection(&_face_cs);
}


HRESULT STDMETHODCALLTYPE fservCreateTexture(
        IDirect3DDevice8* self, UINT width, UINT height,UINT levels,
        DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture,
        DWORD src, bool* IsProcessed) {

    D3DXIMAGE_INFO ii;
    HRESULT res = D3D_OK;

    //LOG(&k_fserv, "CreateTexture (%dx%dx%d): %08x (src=%08x)",
    //    width, height, levels, (DWORD)ppTexture, src);

    // faces
    if (width == 64 && height == 128 && levels == 1) {
        if (g_faceFilesBig.size()>0) {
            string filename = g_faceFilesBig.front();
            g_faceFilesBig.pop_front();
            LOG(&k_fserv, ">>> Looks like a big face texture (%dx%dx%d): src=%08x",
                width, height, levels, src);
			dump_it = true;

            string hdfilename = filename.substr(0,filename.size()-4);
            hdfilename += ".png";
            ZeroMemory(&ii, sizeof(D3DXIMAGE_INFO));
            if (SUCCEEDED(D3DXGetImageInfoFromFile(hdfilename.c_str(), &ii))) {
                LOG(&k_fserv, "HD face exists: (%dx%d) %s", ii.Width, ii.Height, hdfilename.c_str());
                UINT w,h;
                SetFaceDimensions(&ii, w, h);
                res = OrgCreateTexture(self, w, h, levels, usage, format, pool, ppTexture);
                if (res == D3D_OK) {
                    LOG(&k_fserv, "Created big HD face texture: (%dx%d)", w, h);
                    *IsProcessed = true;
                    g_BigFaceTextures[*ppTexture] = hdfilename;

                    WORD tid = *(WORD*)(src + 0x0c);
                    StoreInTexMaps(g_FBTexFileToTid, g_FBTexTidToFile, tid, hdfilename);
                    LOG(&k_fserv, "Storing tid=%04x (src=%08x) for (%dx%d) and filename: %s", tid, src, width, height, hdfilename.c_str());                }
            }
        }
        else if (src!=0) {
            WORD tid = *(WORD*)(src + 0x0c);
            //LOG(&k_fserv, "Looking up big face tid=%04x (src=%08x)", tid, src);
            std::map<WORD,string>::iterator it = g_FBTexTidToFile.find(tid);
            if (it != g_FBTexTidToFile.end()) {
                LOG(&k_fserv, "Big face tex from already seen tid: %04x", tid);
                ZeroMemory(&ii, sizeof(D3DXIMAGE_INFO));
                if (SUCCEEDED(D3DXGetImageInfoFromFile(it->second.c_str(), &ii))) {
					LOG(&k_fserv, "HD face exists: (%dx%d) %s", ii.Width, ii.Height, it->second.c_str());
					UINT w,h;
					SetFaceDimensions(&ii, w, h);
					res = OrgCreateTexture(self, w, h, levels, usage, format, pool, ppTexture);
					if (res == D3D_OK) {
						LOG(&k_fserv, "Created big HD face texture: (%dx%d)", w, h);
						*IsProcessed = true;
						g_BigFaceTextures[*ppTexture] = it->second;
					}
				}
			}
		}
    }
    else if (width == 32 && height == 64 && levels == 1) {
        if (g_faceFilesSmall.size()>0) {
            string filename = g_faceFilesSmall.front();
            g_faceFilesSmall.pop_front();
            LOG(&k_fserv, ">>> Looks like a small face texture (%dx%dx%d): src=%08x",
                width, height, levels, src);
			

            string hdfilename = filename.substr(0,filename.size()-4);
            hdfilename += ".png";
            ZeroMemory(&ii, sizeof(D3DXIMAGE_INFO));
            if (SUCCEEDED(D3DXGetImageInfoFromFile(hdfilename.c_str(), &ii))) {
                LOG(&k_fserv, "HD face exists: (%dx%d) %s", ii.Width, ii.Height, hdfilename.c_str());
                UINT w,h;
                SetFaceDimensions(&ii, w, h);
                res = OrgCreateTexture(self, w/2, h/2, levels, usage, format, pool, ppTexture);
                if (res == D3D_OK) {
                    LOG(&k_fserv, "Created small HD face texture: (%dx%d)", w/2, h/2);
                    *IsProcessed = true;
                    g_SmallFaceTextures[*ppTexture] = hdfilename;
					WORD tid = *(WORD*)(src + 0x0c);
					StoreInTexMaps(g_FSTexFileToTid, g_FSTexTidToFile, tid, hdfilename);
                    LOG(&k_fserv, "Storing tid=%04x (src=%08x) for (%dx%d) and filename: %s", tid, src, width, height, hdfilename.c_str());
                }
            }
        }
        else if (src!=0) {
            WORD tid = *(WORD*)(src + 0x0c);
            //LOG(&k_fserv, "Looking up small face tid=%04x (src=%08x)", tid, src);
            std::map<WORD,string>::iterator it = g_FSTexTidToFile.find(tid);
            if (it != g_FSTexTidToFile.end()) {
                LOG(&k_fserv, "Small face tex from already seen tid: %04x", tid);
                ZeroMemory(&ii, sizeof(D3DXIMAGE_INFO));
                if (SUCCEEDED(D3DXGetImageInfoFromFile(it->second.c_str(), &ii))) {
					LOG(&k_fserv, "HD face exists: (%dx%d) %s", ii.Width, ii.Height, it->second.c_str());
					UINT w,h;
					SetFaceDimensions(&ii, w, h);
					res = OrgCreateTexture(self, w/2, h/2, levels, usage, format, pool, ppTexture);
					if (res == D3D_OK) {
						LOG(&k_fserv, "Created small HD face texture: (%dx%d)", w/2, h/2);
						*IsProcessed = true;
						g_SmallFaceTextures[*ppTexture] = it->second;
					}
				}
			}
		}
    }

    // hair
    if (width == 128 && height == 64 && levels >= 1) {
        if (g_hairFilesBig.size() > 0) {
            //std::deque<string>::iterator qit;
            //for (qit = g_hairFilesBig.begin(); qit != g_hairFilesBig.end(); qit++) {
            //  LOG(&k_fserv, ">>> g_hairFilesBig has: %s", qit->c_str());
            //}            
			string filename = g_hairFilesBig.front();
            g_hairFilesBig.pop_front();
            LOG(&k_fserv, ">>> Looks like a big hair texture (%dx%dx%d): src=%08x",
                width, height, levels, src);

            string hdfilename = filename.substr(0,filename.size()-4);
            hdfilename += ".png";
            ZeroMemory(&ii, sizeof(D3DXIMAGE_INFO));
            if (SUCCEEDED(D3DXGetImageInfoFromFile(hdfilename.c_str(), &ii))) {
                LOG(&k_fserv, "HD hair exists: (%dx%d) %s", ii.Width, ii.Height, hdfilename.c_str());
                UINT w,h;
                SetHairDimensions(&ii, w, h);
                res = OrgCreateTexture(self, w, h, levels, usage, format, pool, ppTexture);
                if (res == D3D_OK) {
                    LOG(&k_fserv, "Created big HD hair texture: (%dx%d)", w, h);
                    *IsProcessed = true;
                    g_BigHairTextures[*ppTexture] = hdfilename;
                    WORD tid = *(WORD*)(src + 0x0c);
                    StoreInTexMaps(g_HBTexFileToTid, g_HBTexTidToFile, tid, hdfilename);
                    LOG(&k_fserv, "Storing tid=%04x (src=%08x) for (%dx%d) and filename: %s", tid, src, width, height, hdfilename.c_str());
                }
                else {
                    LOG(&k_fserv, "ERROR calling OrgCreateTexture (%dx%d) for big hair tex", w, h);
                }
            }
        }
        else if (src!=0) {
            WORD tid = *(WORD*)((BYTE*)src+0x0c);
            //LOG(&k_fserv, "Looking up big hair tid=%04x (src=%08x)", tid, src);
            std::map<WORD,string>::iterator it = g_HBTexTidToFile.find(tid);
            string filename;
            if (it != g_HBTexTidToFile.end()) {
                filename = it->second;
            }
            if (!filename.empty()) {
                LOG(&k_fserv, "Big hair tex from already seen tid: %04x", tid);
                ZeroMemory(&ii, sizeof(D3DXIMAGE_INFO));
                if (SUCCEEDED(D3DXGetImageInfoFromFile(filename.c_str(), &ii))) {
                    LOG(&k_fserv, "HD hair exists: (%dx%d) %s", ii.Width, ii.Height, filename.c_str());
                    UINT w,h;
                    SetHairDimensions(&ii, w, h);
                    res = OrgCreateTexture(self, w, h, levels, usage, format, pool, ppTexture);
                    if (res == D3D_OK) {
                        LOG(&k_fserv, "Created big HD hair texture: (%dx%d)", w, h);
                        *IsProcessed = true;
                        g_BigHairTextures[*ppTexture] = filename;
                    }
                    else {
                        LOG(&k_fserv, "ERROR calling OrgCreateTexture (%dx%d) for big hair tex", w, h);
                    }
                }
                else {
                    LOG(&k_fserv, "ERROR calling D3DXGetImageInfoFromFile for: %s", filename.c_str());
                }
            }
        }
    }
    else if (width == 64 && height == 32 && levels >= 1) {
        if (g_hairFilesSmall.size()>0) {
            //std::deque<string>::iterator qit;
            //for (qit = g_hairFilesSmall.begin(); qit != g_hairFilesSmall.end(); qit++) {
            //  LOG(&k_fserv, ">>> g_hairFilesSmall has: %s", qit->c_str());
            //}
            string filename = g_hairFilesSmall.front();
            g_hairFilesSmall.pop_front();
            LOG(&k_fserv, ">>> Looks like a small hair texture (%dx%dx%d): src=%08x",
                width, height, levels, src);

            string hdfilename = filename.substr(0,filename.size()-4);
            hdfilename += ".png";
            ZeroMemory(&ii, sizeof(D3DXIMAGE_INFO));
            if (SUCCEEDED(D3DXGetImageInfoFromFile(hdfilename.c_str(), &ii))) {
                LOG(&k_fserv, "HD hair exists: (%dx%d) %s", ii.Width, ii.Height, hdfilename.c_str());
                UINT w,h;
                SetHairDimensions(&ii, w, h);
                res = OrgCreateTexture(self, w/2, h/2, levels, usage, format, pool, ppTexture);
                if (res == D3D_OK) {
                    LOG(&k_fserv, "Created small HD hair texture: (%dx%d)", w/2, h/2);
                    *IsProcessed = true;
                    g_SmallHairTextures[*ppTexture] = hdfilename;

                    WORD tid = *(WORD*)(src + 0x0c);
                    StoreInTexMaps(g_HSTexFileToTid, g_HSTexTidToFile, tid, hdfilename);
                    LOG(&k_fserv, "Storing tid=%04x (src=%08x) for (%dx%d) and filename: %s", tid, src, width, height, hdfilename.c_str());
                }
                else {
                    LOG(&k_fserv, "ERROR calling OrgCreateTexture (%dx%d) for small hair tex", w, h);
                }
            }
        }
        else if (src!=0) {
            WORD tid = *(WORD*)((BYTE*)src+0x0c);
            //LOG(&k_fserv, "Looking up small hair tid=%04x (src=%08x)", tid, src);
            std::map<WORD,string>::iterator it = g_HSTexTidToFile.find(tid);
            string filename;
            if (it != g_HSTexTidToFile.end()) {
                filename = it->second;
            }
            if (!filename.empty()) {
                LOG(&k_fserv, "Small hair tex from already seen tid: %04x", tid);
                ZeroMemory(&ii, sizeof(D3DXIMAGE_INFO));
                if (SUCCEEDED(D3DXGetImageInfoFromFile(filename.c_str(), &ii))) {
                    LOG(&k_fserv, "HD hair exists: (%dx%d) %s", ii.Width, ii.Height, filename.c_str());
                    UINT w,h;
                    SetHairDimensions(&ii, w, h);
                    res = OrgCreateTexture(self, w/2, h/2, levels, usage, format, pool, ppTexture);
                    if (res == D3D_OK) {
                        LOG(&k_fserv, "Created small HD hair texture: (%dx%d)", w/2, h/2);
                        *IsProcessed = true;
                        g_SmallHairTextures[*ppTexture] = filename;
                    }
                    else {
                        LOG(&k_fserv, "ERROR calling OrgCreateTexture (%dx%d) for small hair tex", w, h);
                    }
                }
                else {
                    LOG(&k_fserv, "ERROR calling D3DXGetImageInfoFromFile for: %s", filename.c_str());
                }
            }
        }
    }
    return res;
}

void fservLoadTextureFromFile(IDirect3DTexture8* tex, const char *filename)
{
    IDirect3DSurface8* surf;
    LOG(&k_fserv, "fservLoadTextureFromFile: [%08x] %s", (DWORD)tex, filename);
    HRESULT res = tex->GetSurfaceLevel(0, &surf);
    if (SUCCEEDED(res))
    {
        if (FileExists(filename)) {
            if (SUCCEEDED(D3DXLoadSurfaceFromFile(
                            surf, NULL, NULL,
                            filename, NULL,
                            D3DX_FILTER_LINEAR, 0, NULL))) { 
                LOG(&k_fserv, "Loaded texture from: %s", filename);
            }
            else {
                LOG(&k_fserv, "FAILED Loaded face texture from: %s", filename);
            }
        }
        surf->Release();
    }
    else {
        LOG(&k_fserv, "FAILED tex->GetSurfaceLevel for: [%08x] %s", (DWORD)tex, filename);
    }
}

void fservUnlockRect(IDirect3DTexture8* self,UINT Level) {
    static int count = 1;

    // check faces
    std::map<IDirect3DTexture8*,string>::iterator it;
    it = g_BigFaceTextures.find(self);
    if (it != g_BigFaceTextures.end()) {
        // replace with HD texture
        fservLoadTextureFromFile(self, it->second.c_str());

        if (g_config->dump_textures && dump_it) {
            char buf[BUFLEN];
            string fname = it->second.substr(it->second.rfind("\\")+1);
            sprintf(buf,"%s\\%03d - face - %s.bmp", GetPESInfo()->mydir, count, fname.c_str());
            if (SUCCEEDED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, self, NULL))) {
                LOG(&k_fserv, "Saved texture to: %s", buf);
            }
            else {
                LOG(&k_fserv, "FAILED to save texture to: %s", buf);
            }
        }

        count++;
        g_BigFaceTextures.erase(it);
    }
    it = g_SmallFaceTextures.find(self);
    if (it != g_SmallFaceTextures.end()) {
        // replace with HD texture
        fservLoadTextureFromFile(self, it->second.c_str());

        if (g_config->dump_textures) {
            char buf[BUFLEN];
            string fname = it->second.substr(it->second.rfind("\\")+1);
            sprintf(buf,"%s\\%03d - face - %s.bmp", GetPESInfo()->mydir, count, fname.c_str());
            if (SUCCEEDED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, self, NULL))) {
                LOG(&k_fserv, "Saved texture to: %s", buf);
            }
            else {
                LOG(&k_fserv, "FAILED to save texture to: %s", buf);
            }
			dump_it = true;
        }

        count++;
        g_SmallFaceTextures.erase(it);
    }

    // check hairs
    it = g_BigHairTextures.find(self);
    if (it != g_BigHairTextures.end()) {
        // replace with HD texture
        fservLoadTextureFromFile(self, it->second.c_str());

        if (g_config->dump_textures) {
            char buf[BUFLEN];
            string fname = it->second.substr(it->second.rfind("\\")+1);
            sprintf(buf,"%s\\%03d - hair - %s.bmp", GetPESInfo()->mydir, count, fname.c_str());
            if (SUCCEEDED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, self, NULL))) {
                LOG(&k_fserv, "Saved texture to: %s", buf);
            }
            else {
                LOG(&k_fserv, "FAILED to save texture to: %s", buf);
            }
			dump_it = true;
        }

        count++;
        g_BigHairTextures.erase(it);
    }
    it = g_SmallHairTextures.find(self);
    if (it != g_SmallHairTextures.end()) {
        // replace with HD texture
        fservLoadTextureFromFile(self, it->second.c_str());

        if (g_config->dump_textures) {
            char buf[BUFLEN];
            string fname = it->second.substr(it->second.rfind("\\")+1);
            sprintf(buf,"%s\\%03d - hair - %s.bmp", GetPESInfo()->mydir, count, fname.c_str());
            if (SUCCEEDED(D3DXSaveTextureToFile(buf, D3DXIFF_BMP, self, NULL))) {
                LOG(&k_fserv, "Saved texture to: %s", buf);
            }
            else {
                LOG(&k_fserv, "FAILED to save texture to: %s", buf);
            }
			dump_it = true;
        }

        count++;
        g_SmallHairTextures.erase(it);
    }
}

bool HookProcAtAddr(DWORD proc, DWORD proc_cs, DWORD newproc, char* sproc, char* sproc_cs)
{
    if (proc != 0 && proc_cs != 0)
    {
        BYTE* bptr = (BYTE*)proc_cs;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            DWORD* ptr = (DWORD*)(proc_cs + 1);
            ptr[0] = newproc - (DWORD)(proc_cs + 5);
            LogWithTwoStrings(&k_fserv,"%s HOOKED at %s", sproc, sproc_cs);
            return true;
        }
    }
    return false;
}

bool UnhookProcAtAddr(bool flag, DWORD proc, DWORD proc_cs, char* sproc, char* sproc_cs)
{
    if (flag && proc !=0 && proc_cs != 0)
    {
        BYTE* bptr = (BYTE*)proc_cs;
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(bptr, 8, newProtection, &protection)) {
            DWORD* ptr = (DWORD*)(proc_cs + 1);
            ptr[0] = (DWORD)proc - (DWORD)(proc_cs + 5);
            LogWithTwoStrings(&k_fserv,"%s UNHOOKED at %s", sproc, sproc_cs);
            return true;
        }
    }
    return false;
}


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	int i,j;
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_fserv,"Attaching dll...");
		
		hInst=hInstance;
		
		int v=GetPESInfo()->GameVersion;
		switch (v) {
			case gvPES5PC:
			case gvWE9PC:
			case gvWE9LEPC:
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_fserv,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:

		RegisterKModule(&k_fserv);

        g_config = new FSERV_CONFIG();
        g_config->dump_textures = DEFAULT_DUMP_TEXTURES;
        g_config->hd_face_max_width = DEFAULT_HD_FACE_MAX_WIDTH;
        g_config->hd_face_max_height = DEFAULT_HD_FACE_MAX_HEIGHT;
        g_config->hd_hair_max_width = DEFAULT_HD_HAIR_MAX_WIDTH;
        g_config->hd_hair_max_height = DEFAULT_HD_HAIR_MAX_HEIGHT;

		// read configuration
		char cfgFile[BUFLEN];
		ZeroMemory(cfgFile, BUFLEN);
		strcpy(cfgFile, GetPESInfo()->mydir); 
		strcat(cfgFile, CONFIG_FILE);
		ReadConfig(g_config, cfgFile);
	
		//copy the FaceIDs for the right game version
		memcpy(fIDs,fIDsArray[v-1],sizeof(fIDs));	
		orgCopyPlayerData=(COPYPLAYERDATA)fIDs[C_COPYPLAYERDATA];
		orgEditCopyPlayerData=(EDITCOPYPLAYERDATA)fIDs[C_EDITCOPYPLAYERDATA];
		orgGetHairTransp=(GETHAIRTRANSP)fIDs[C_GETHAIRTRANSP];

		//tell the savegame manager module about our functions
		SetSavegameExpFuncs(SGEFM_FSERV,SGEF_GIVEIDTOMODULE,(DWORD)&fservConnectAddrToId);
		
		HookFunction(hk_D3D_Create,(DWORD)InitFserv);

        HookFunction(hk_D3D_CreateTexture,(DWORD)fservCreateTexture);
        HookFunction(hk_D3D_UnlockRect,(DWORD)fservUnlockRect);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_fserv,"Detaching dll...");
		SavePlayerList();
		DeleteCriticalSection(&_face_cs);

        // log face textures
        std::map<IDirect3DTexture8*,string>::iterator it;
        for (it = g_BigFaceTextures.begin(); it != g_BigFaceTextures.end(); it++) {
            LOG(&k_fserv,"Big face tex: %08x --> %s", (DWORD)it->first, it->second.c_str());
        }
        for (it = g_SmallFaceTextures.begin(); it != g_SmallFaceTextures.end(); it++) {
            LOG(&k_fserv,"Small face tex: %08x --> %s", (DWORD)it->first, it->second.c_str());
        }
		
		UnhookFunction(hk_FileFromAFS,(DWORD)fservFileFromAFS);
		UnhookFunction(hk_BeforeFreeMemory,(DWORD)fservFreeMemory);
		UnhookFunction(hk_ProcessPlayerData,(DWORD)fservProcessPlayerData);
		UnhookFunction(hk_D3D_Present,(DWORD)PrintPlayerInfo);
		
		DWORD protection=0, newProtection=PAGE_EXECUTE_READWRITE;
		if (fIDs[FIX_DWORDFACEID] != 0) {
            if (VirtualProtect((BYTE*)fIDs[FIX_DWORDFACEID], 2, newProtection, &protection)) {
                memcpy((BYTE*)fIDs[FIX_DWORDFACEID], g_savedDWORDFaceIDfix, 2);
            }
        }
		if (fIDs[CALCHAIRID] != 0) {
            if (VirtualProtect((BYTE*)fIDs[CALCHAIRID], 18, newProtection, &protection)) {
                memcpy((BYTE*)fIDs[CALCHAIRID], g_savedCalcHairFile, 18);
            }
        }
		if (fIDs[CALCSPHAIRID] != 0) {
			if (VirtualProtect((BYTE*)fIDs[CALCSPHAIRID], 15, newProtection, &protection)) {
                memcpy((BYTE*)fIDs[CALCSPHAIRID], g_savedCalcSpHairFile, 15);
            }
        }
		if (fIDs[C_COPYPLAYERDATA_CS] != 0)
			if (VirtualProtect((BYTE*)fIDs[C_COPYPLAYERDATA_CS], 5, newProtection, &protection))
				memcpy((BYTE*)fIDs[C_COPYPLAYERDATA_CS], g_savedCopyPlayerData, 5);
			
		if (fIDs[C_REPL_COPYPLAYERDATA_CS] != 0)
			if (VirtualProtect((BYTE*)fIDs[C_REPL_COPYPLAYERDATA_CS], 11, newProtection, &protection))
				memcpy((BYTE*)fIDs[C_REPL_COPYPLAYERDATA_CS], g_savedReplCopyPlayerData, 11);
			
		bGetHairTranspHooked=UnhookProcAtAddr(bGetHairTranspHooked,fIDs[C_GETHAIRTRANSP],
			fIDs[C_GETHAIRTRANSP_CS],"C_GETHAIRTRANSP","C_GETHAIRTRANSP_CS");
			
		bEditCopyPlayerDataHooked=UnhookProcAtAddr(bEditCopyPlayerDataHooked,fIDs[C_EDITCOPYPLAYERDATA],
			fIDs[C_EDITCOPYPLAYERDATA_CS],"C_EDITCOPYPLAYERDATA","C_EDITCOPYPLAYERDATA_CS");
			
		FreeAllBuffers();
        /*
		for (i=0;i<4;i++) {
			for (j=0;j<numFaces[i];j++)
				if (g_Faces[i][j] != NULL)
					delete g_Faces[i][j];
			
			g_Faces[i].clear();
		};
        */
		for (j=0;j<numFaces;j++)
			if (g_Faces[j] != NULL)
					delete g_Faces[j];
		
		for (j=0;j<numHair;j++)
			if (g_Hair[j] != NULL)
					delete g_Hair[j];
		
		g_Hair.clear();
		g_HairTransp.clear();
		
		g_Players.clear();
		g_PlayersHair.clear();
		g_PlayersAddr.clear();
		g_PlayersAddr2.clear();
		g_SpecialFaceHair.clear();
		
		
		Log(&k_fserv,"Detaching done.");
	};
	
	return true;
};

void InitFserv()
{
	sgf=GetSavegameFuncs();
	if (sgf!=NULL)
		savegameModPresent=true;
	InitializeCriticalSection(&_face_cs);

	HookFunction(hk_FileFromAFS,(DWORD)fservFileFromAFS);
	HookFunction(hk_BeforeFreeMemory,(DWORD)fservFreeMemory);
	HookFunction(hk_ProcessPlayerData,(DWORD)fservProcessPlayerData);
	HookFunction(hk_D3D_Present,(DWORD)PrintPlayerInfo);
	
    MaxNumStdFaces=0;
	for (int i=0;i<4;i++) {
		StartsStdFaces[i]=fIDs[STARTW+i];
		NumStdFaces[i]=fIDs[STDW+i];
        if (MaxNumStdFaces < NumStdFaces[i])
            MaxNumStdFaces = NumStdFaces[i];
	};
	
	NumStdHair=fIDs[STDHAIR];

	Log(&k_fserv, "hooking various functions");

	DWORD protection=0, newProtection=PAGE_EXECUTE_READWRITE;
	BYTE* bptr=NULL;
	DWORD* ptr=NULL;
	
	// make PES read the face id as DWORD value rather than as WORD value
	if (fIDs[FIX_DWORDFACEID] != 0)
	{
		bptr = (BYTE*)fIDs[FIX_DWORDFACEID];
	    memcpy(g_savedDWORDFaceIDfix, bptr, 2);
	    if (VirtualProtect(bptr, 2, newProtection, &protection)) {
	        bptr[0]=0x90;
	        bptr[1]=0x8B;
	    };
	};
	
	if (fIDs[CALCHAIRID] != 0)
	{
		bptr = (BYTE*)fIDs[CALCHAIRID];
		ptr = (DWORD*)(fIDs[CALCHAIRID]+1);
	    memcpy(g_savedCalcHairFile, bptr, 18);
	    if (VirtualProtect(bptr, 18, newProtection, &protection)) {
	        bptr[0]=0xe8;
	        ptr[0] = (DWORD)CalcHairFile - (DWORD)(fIDs[CALCHAIRID] + 5);
	        for (int j=5;j<18;j++)
	        	bptr[j]=0x90;
	    };
	};
	
	if (fIDs[CALCSPHAIRID] != 0)
	{
		bptr = (BYTE*)fIDs[CALCSPHAIRID];
		ptr = (DWORD*)(fIDs[CALCSPHAIRID]+1);
	    memcpy(g_savedCalcSpHairFile, bptr, 15);
	    if (VirtualProtect(bptr, 15, newProtection, &protection)) {
	        bptr[0]=0xe8;
	        ptr[0] = (DWORD)CalcHairFile - (DWORD)(fIDs[CALCSPHAIRID] + 5);
	        for (int j=5;j<15;j++)
	        	bptr[j]=0x90;
	    };
	};
	
	if (fIDs[C_COPYPLAYERDATA_CS] != 0)
	{
		bptr = (BYTE*)fIDs[C_COPYPLAYERDATA_CS];
		ptr = (DWORD*)(fIDs[C_COPYPLAYERDATA_CS]+1);
	    memcpy(g_savedCopyPlayerData, bptr, 5);
	    if (VirtualProtect(bptr, 5, newProtection, &protection)) {
	        bptr[0]=0xe8;
	        ptr[0] = (DWORD)fservCopyPlayerData - (DWORD)(fIDs[C_COPYPLAYERDATA_CS] + 5);
	    };
	};
	
	if (fIDs[C_REPL_COPYPLAYERDATA_CS] != 0)
	{
		bptr = (BYTE*)fIDs[C_REPL_COPYPLAYERDATA_CS];
		ptr = (DWORD*)(fIDs[C_REPL_COPYPLAYERDATA_CS]+1);
	    memcpy(g_savedReplCopyPlayerData, bptr, 11);
	    if (VirtualProtect(bptr, 11, newProtection, &protection)) {
	        bptr[0]=0xe8;
	        ptr[0] = (DWORD)fservReplCopyPlayerData - (DWORD)(fIDs[C_REPL_COPYPLAYERDATA_CS] + 5);
			for (int j=5;j<11;j++)
	        	bptr[j]=0x90;
	    };
	};
	
	bGetHairTranspHooked=HookProcAtAddr(fIDs[C_GETHAIRTRANSP],fIDs[C_GETHAIRTRANSP_CS],
		(DWORD)fservGetHairTransp,"C_GETHAIRTRANSP","C_GETHAIRTRANSP_CS");
		
	bEditCopyPlayerDataHooked=HookProcAtAddr(fIDs[C_EDITCOPYPLAYERDATA],fIDs[C_EDITCOPYPLAYERDATA_CS],
		(DWORD)fservEditCopyPlayerData,"C_EDITCOPYPLAYERDATA","C_EDITCOPYPLAYERDATA_CS");
			
	Log(&k_fserv, "hooking done");
	
	UnhookFunction(hk_D3D_Create,(DWORD)InitFserv);


	return;
};

//Unfortunately player attributs have to be read out manually
//as KONAMI reserved just as much bits for them as needed
void ReadPlayerInfo(PLAYERINFO* pi, DWORD StartAddr)
{
	DWORD Temp;
	
	//LogWithNumber(&k_fserv,"ReadPlayerInfo from address %x",StartAddr);
	
	Temp=*(BYTE*)(StartAddr+90);
	pi->SkinColor=(Temp >> 1) & 3;

	Temp=*(WORD*)(StartAddr+100);
	pi->FaceID=(Temp >> 3) & 0x1FF;

	Temp=*(BYTE*)(StartAddr+107);
	pi->FaceSet=(Temp >> 2) & 3;
	return;
};

void WritePlayerInfo(PLAYERINFO* pi, DWORD StartAddr)
{
	DWORD Temp;
	
	//LogWithNumber(&k_fserv,"WritePlayerInfo to address %x",StartAddr);
	
	Temp=*(BYTE*)(StartAddr+90);
	Temp=(Temp & 0xF9) + (pi->SkinColor<<1);
	*(BYTE*)(StartAddr+90)=(BYTE)Temp;

	Temp=*(WORD*)(StartAddr+100);
	Temp=(Temp & 0xF007) + (pi->FaceID<<3);
	*(WORD*)(StartAddr+100)=(WORD)Temp;

	Temp=*(BYTE*)(StartAddr+107);
	Temp=(Temp & 0xF3) + (pi->FaceSet<<2);
	*(BYTE*)(StartAddr+107)=(BYTE)Temp;

	return;
};

bool FileExists(const char* filename)
{
    TRACE4(&k_fserv,"FileExists: Checking file: %s", (char*)filename);
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
};


void GetGDBFaces()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sfile[BUFLEN];
	DWORD number=0;
	
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\faces\\map.txt");
	
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
		ZeroMemory(sfile,BUFLEN);
		if (sscanf(str,"%d,\"%[^\"]\"",&number,sfile)==2)
			AddPlayerFace(number,sfile);
	};
	fclose(cfg);
	
	for (int i=0;i<4;i++) {
		//NumTotalFaces[i]=NumStdFaces[i]+numFaces[i];
		NumTotalFaces[i]=MaxNumStdFaces+numFaces;
		LogWithTwoNumbers(&k_fserv,"Number of faces for skincolor %d is %d.",i,NumTotalFaces[i]);
	};
	return;
};

void AddPlayerFace(DWORD PlayerNumber,char* sfile)
{
	LogWithNumberAndString(&k_fserv,"Player # %d gets face %s.",PlayerNumber,sfile);
	
	char tmpFilename[BUFLEN];
	strcpy(tmpFilename,GetPESInfo()->gdbDir);
	strcat(tmpFilename,"GDB\\faces\\");
	strcat(tmpFilename,sfile);
	if (!FileExists(tmpFilename)) {
		Log(&k_fserv,"File doesn't exist, line is ignored!");
		return;
	};
	
	PLAYERINFO Player;
	DWORD GPIaddr=GetPlayerInfo(PlayerNumber,0);
	if (GPIaddr==0) return;
	ReadPlayerInfo(&Player,GPIaddr);
	DWORD sk=Player.SkinColor;
    DWORD newId=GetIDForFaceName(sk,sfile);
    if (newId==0xFFFFFFFF) {
        Log(&k_fserv,"Face hasn't been listed yet.");
        newId=MaxNumStdFaces+numFaces;//NumStdFaces[sk]+numFaces[sk];
        //g_Faces[sk][newId]=new char[strlen(sfile)+1];
        g_Faces[newId]=new char[strlen(sfile)+1];
        //strcpy(g_Faces[sk][newId],sfile);
        strcpy(g_Faces[newId],sfile);
        //numFaces[sk]++;
        numFaces++;
    }
    LogWithNumber(&k_fserv,"Assigned face id is %d.",newId);
    g_Players[PlayerNumber]=newId;
	return;
};

DWORD GetIDForFaceName(DWORD sk, char* sfile)
{
	//for (g_FacesIterator=g_Faces[sk].begin();g_FacesIterator!=g_Faces[sk].end();g_FacesIterator++) {
	for (g_FacesIterator=g_Faces.begin();g_FacesIterator!=g_Faces.end();g_FacesIterator++) {
		if (g_FacesIterator->second != NULL)
			if (strcmp(g_FacesIterator->second,sfile)==0)
				return g_FacesIterator->first;
	};
	
	return 0xFFFFFFFF;
};

void GetGDBHair()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sfile[BUFLEN];
	DWORD number=0, scanRes=0, transp=255;
	
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\hair\\map.txt");
	
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
		number=0;
		ZeroMemory(sfile,BUFLEN);
		transp=255;
		scanRes=sscanf(str,"%d,\"%[^\"]\",%d",&number,sfile,&transp);
		if (transp>255) transp=255;
		switch (scanRes) {
		case 3:
			AddPlayerHair(number,sfile,transp);
			break;
		case 2:
			AddPlayerHair(number,sfile,255);
			break;
		};
	};
	fclose(cfg);
	
	NumTotalHair=NumStdHair+numHair;
	LogWithNumber(&k_fserv,"Number of hair is %d.",NumTotalHair);
	return;
};

void AddPlayerHair(DWORD PlayerNumber,char* sfile,BYTE transp)
{
	LogWithNumberAndString(&k_fserv,"Player # %d gets hair %s.",PlayerNumber,sfile);
	
	char tmpFilename[BUFLEN];
	strcpy(tmpFilename,GetPESInfo()->gdbDir);
	strcat(tmpFilename,"GDB\\hair\\");
	strcat(tmpFilename,sfile);
	if (!FileExists(tmpFilename)) {
		Log(&k_fserv,"File doesn't exist, line is ignored!");
		return;
	};
	
	DWORD newId=GetIDForHairName(sfile);
	if (newId==0xFFFFFFFF) {
		Log(&k_fserv,"Hair hasn't been listed yet.");
		newId=NumStdHair+numHair;
		g_Hair[newId]=new char[strlen(sfile)+1];
		strcpy(g_Hair[newId],sfile);
		numHair++;
	};
	LogWithNumber(&k_fserv,"Assigned hair id is %d.",newId);
	g_PlayersHair[PlayerNumber]=newId;
	g_HairTransp[PlayerNumber]=transp;
	return;
};

DWORD GetIDForHairName(char* sfile)
{
	for (g_HairIterator=g_Hair.begin();g_HairIterator!=g_Hair.end();g_HairIterator++) {
		if (g_HairIterator->second != NULL)
			if (strcmp(g_HairIterator->second,sfile)==0)
				return g_HairIterator->first;
	};
	
	return 0xFFFFFFFF;
};

void SavePlayerList()
{
	#define SPL_RESERVED_SIZE 512*1024
	DWORD NBW=0;
	char list[SPL_RESERVED_SIZE];
	char Temp[1024];
	char NameTemp[256];
	DWORD GPIAddr=0;
	PLAYERINFO pi;
	DWORD num=0;
	
	strcpy(Temp,GetPESInfo()->mydir);
	strcat(Temp,"PlayerList.txt");
	HANDLE TempHandle=CreateFile(Temp,GENERIC_WRITE,3,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
	if (TempHandle==INVALID_HANDLE_VALUE) return;
	SetFilePointer(TempHandle,0,0,0);
	
	strcpy(list,"PLAYER LIST\r\n-----\r\n\r\n");

	for (int i=0;i<=HIGHEST_PLAYER_ID;i++) {
		GPIAddr=GetPlayerInfo(i,0);
		if (GPIAddr!=0) {
			ReadPlayerInfo(&pi,GPIAddr);
			wcstombs(NameTemp,(LPWSTR)GPIAddr,32);
			if (pi.FaceSet==FACESET_NONE)
				sprintf(Temp,"%.5d - %s (Skincolor: %d)\r\n",i,NameTemp,pi.SkinColor);
			else if (pi.FaceSet==FACESET_NORMAL)
				sprintf(Temp,"%.5d - %s (Skincolor: %d, FaceID: %d)\r\n",i,NameTemp,pi.SkinColor,pi.FaceID);
			else if (pi.FaceSet==FACESET_SPECIAL)
				sprintf(Temp,"%.5d - %s (Skincolor: %d, FaceID: %d [Special Face])\r\n",i,NameTemp,pi.SkinColor,pi.FaceID);
			
			num++;
			strcat(list,Temp);
			//sprintf(Temp,"%.8x\r\n",GPIAddr);
			//strcat(list,Temp);
		};
		if (strlen(list)>(SPL_RESERVED_SIZE-2*1024)) {
			WriteFile(TempHandle,list,strlen(list),&NBW,0);
			strcpy(list,"\0");
		};	
	};

	sprintf(Temp,"\r\n\r\n-----\r\nNumber of players: %d\r\n",num);
	strcat(list,Temp);

	WriteFile(TempHandle,list,strlen(list),&NBW,0);
	CloseHandle(TempHandle);

	return;
};


void fservFileFromAFS(DWORD infoBlock)
{
	FreeAllBuffers();
	if (infoBlock==0) return;
	
	INFOBLOCK* ib=(INFOBLOCK*)infoBlock;
	DWORD FaceID=0, HairID=0;
	BYTE Skincolor=0;
	char filename[BUFLEN];
	LPVOID FileBuffer;
	DWORD usedPlayerNumber;
	BYTE* requestedData=NULL;
	DATAOFMEMORY* dataOfMemory=NULL;
	DWORD FS=0, NBW=0;
	HANDLE file=NULL;
    map<DWORD,char*>::iterator it;

	for (int i=0;i<4;i++)
		((WORD*)fIDs[NUM_FACES_ARRAY])[i]=NumTotalFaces[i];
		
	*((WORD*)fIDs[NUM_HAIR])=NumTotalHair;
	
	//TRACE2(&k_fserv,"fservFileFromAFS for %x",ib->FileID);
	
	THREEDWORDS* threeDWORDs=GetSpecialAfsFileInfo(ib->FileID);
	if (threeDWORDs==NULL)
			goto Processed;
 	
 	switch (threeDWORDs->dw1) {
 	case AFSSIG_FACEDATA:
 	case AFSSIG_HAIRDATA:
 		//get data from the cache
 		if (!savegameModPresent) {
 			Log(&k_fserv,"Savegame Module is not present!");
 			goto Processed;
 		};
 		
 		usedPlayerNumber=threeDWORDs->dw2;
 		if (usedPlayerNumber>=FIRSTREPLPLAYERID && usedPlayerNumber<(FIRSTREPLPLAYERID+64)) {
			requestedData=sgf->RequestReplayPlayerData(threeDWORDs->dw1-AFSSIG_FACEDATA,usedPlayerNumber-FIRSTREPLPLAYERID);
		} else if (usedPlayerNumber>=0x7060 && usedPlayerNumber<0x70A0) {
			requestedData=sgf->RequestMLPlayerData(threeDWORDs->dw1-AFSSIG_FACEDATA,usedPlayerNumber-0x7060);
		} else {
			Log(&k_fserv,"This is NO player id which should have data!");
			goto Processed;
		};
		
		if (requestedData==NULL) {
			Log(&k_fserv,"FAILED to get data from replay!");
			goto Processed;
		};
		
		dataOfMemory=(DATAOFMEMORY*)requestedData;
		if (dataOfMemory->type != 1 || dataOfMemory->size==0) {
			Log(&k_fserv,"FAILED because of invalid saved data!");
			goto Processed;
		};
		
		LogWithNumber(&k_fserv,"Copying data with size of %d bytes...",dataOfMemory->size);
		FileBuffer=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,dataOfMemory->size);
		g_Buffers[(DWORD)FileBuffer]=FileBuffer;
		memcpy(FileBuffer,dataOfMemory->dta,dataOfMemory->size);
				
		ib->src=(DWORD)FileBuffer;
		sgf->FreeBuffer(requestedData);
 		
 		goto Processed;
 		break;
 		
 	case AFSSIG_FACE:
		//replace the data for face
		LogWithNumber(&k_fserv,"Processing file id %x.",ib->FileID);
		FaceID=threeDWORDs->dw2;
		Skincolor=threeDWORDs->dw3;
		LogWithTwoNumbers(&k_fserv,"FaceID is %d, skincolor %d.",FaceID,Skincolor);
		strcpy(filename,GetPESInfo()->gdbDir);
		strcat(filename,"GDB\\faces\\");
		//if (g_Faces[Skincolor][FaceID]!=NULL) {
		if (g_Faces[FaceID]!=NULL) {
            //strcat(filename,g_Faces[Skincolor][FaceID]);
            strcat(filename,g_Faces[FaceID]);
            g_faceFilesBig.push_back(filename);
            g_faceFilesSmall.push_back(filename);
        } else {
			Log(&k_fserv,"FAILED! No file is assigned to this parameter combination!");
			goto Processed;
            break;
            //return;
		};
		goto LoadFileNow;
		break;
		
	case AFSSIG_HAIR:
 		//replace the data for hair
		HairID=threeDWORDs->dw2;
		LogWithNumber(&k_fserv,"Processing hair id %d.",HairID);
		strcpy(filename,GetPESInfo()->gdbDir);
		strcat(filename,"GDB\\hair\\");
		if (g_Hair[HairID]!=NULL) {
            strcat(filename,g_Hair[HairID]);
            g_hairFilesBig.push_back(filename);
            g_hairFilesSmall.push_back(filename);
        } else {
			Log(&k_fserv,"FAILED! No file is assigned to this parameter combination!");
			goto Processed;
            break;
            //return;
		};
		goto LoadFileNow;
		break;
		
	default:
		goto Processed;
		break;
	};
	
	
	LoadFileNow:
	
	//load data from a file
	LOG(&k_fserv,"Loading file %s ...",filename);
	file=CreateFile(filename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,NULL);
	if (file!=INVALID_HANDLE_VALUE) {
		FS=GetFileSize(file,NULL);
		FileBuffer=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,FS);
		g_Buffers[(DWORD)FileBuffer]=FileBuffer;
		ReadFile(file,FileBuffer,FS,&NBW,NULL);
		CloseHandle(file);
	} else {
		Log(&k_fserv,"Loading file FAILED!!!");
	};
	ib->src=(DWORD)FileBuffer;
	

	Processed:
	
	return;
};

void FreeAllBuffers()
{
	//TRACE(&k_fserv,"FreeAllBuffers: CALLED.");
	for (g_BuffersIterator=g_Buffers.begin();g_BuffersIterator!=g_Buffers.end();g_BuffersIterator++)
		HeapFree(GetProcessHeap(),0,g_BuffersIterator->second);
		
	g_Buffers.clear();			
	return;
};

bool fservFreeMemory(DWORD addr)
{
	//TRACE2(&k_fserv,"fservFreeMemory: CALLED for %x.",addr);
	bool result=true;

	if (g_Buffers[addr] != 0) {
		//TRACE(&k_fserv,"fservFreeMemory: Freeing buffer.");
		HeapFree(GetProcessHeap(),0,g_Buffers[addr]);
		g_Buffers.erase(addr);
		result=false;
	};

	return result;
};

KEXPORT void fservInitFacesAndHair()
{
	if (!Inited) {
		GetGDBFaces();
		GetGDBHair();
		
		Inited=true;
        LOG(&k_fserv, "GDB Faces and Hair loaded.");
	}
}

void fservProcessPlayerData(DWORD ESI, DWORD* PlayerNumber)
{
	//TRACE(&k_fserv,"fservProcessPlayerData: CALLED.");
    fservInitFacesAndHair();
	
	DWORD addr=**(DWORD**)(ESI+4);
	BYTE *Skincolor=(BYTE*)(addr+0x31);
	//Facesets: 0=No real face; 1=Special face; 2=Normal real face
	BYTE *Faceset=(BYTE*)(addr+0x33);
	DWORD *FaceID=(DWORD*)(addr+0x40);
	DWORD srcData=((*(BYTE*)(ESI+0x40))*32+*(BYTE*)(ESI+0x3f))*0x344+fIDs[PLAYERDATA_BASE];
	
	isInEditPlayerMode=*(BYTE*)fIDs[ISEDITPLAYERMODE];
	isInEditPlayerList=*(BYTE*)fIDs[ISEDITPLAYERLIST];
	WORD editedPlayerNumber=*(WORD*)fIDs[EDITEDPLAYER];
	
	*PlayerNumber=g_PlayersAddr[srcData];
	DWORD usedPlayerNumber=*PlayerNumber;
	
	bool hasMemoryData=false;
	DWORD newFaceID=0;
	BYTE* requestedData=NULL;
	DATAOFID* dataOfId=NULL;
	DATAOFMEMORY* dataOfMemory=NULL;

	if (isInEditPlayerList!=0 || isInEditPlayerMode!=0)
		usedPlayerNumber=editedPlayerNumber;
	

	if (usedPlayerNumber>=FIRSTREPLPLAYERID && usedPlayerNumber<(FIRSTREPLPLAYERID+64)) {
		//replay mode
		if (!savegameModPresent) goto NoSpecialData;
		
		requestedData=sgf->RequestReplayPlayerData(0,usedPlayerNumber-FIRSTREPLPLAYERID);

	} else if (usedPlayerNumber>=0x7060 && usedPlayerNumber<0x70A0) {
		//playing with ML team
		if (!savegameModPresent) goto NoSpecialData;
		
		requestedData=sgf->RequestMLPlayerData(0,usedPlayerNumber-0x7060);
		
	} else {
		goto NoSpecialData;
	};
	
	//check if the data is valid
	if (requestedData==NULL)
		goto NoSpecialData;
	
	if (requestedData[0]==0) {
		//an id is saved for this player
		dataOfId=(DATAOFID*)requestedData;
		usedPlayerNumber=dataOfId->id;
		
	} else if (requestedData[0]==1) {
		//the replay contains data
		dataOfMemory=(DATAOFMEMORY*)requestedData;
		if (dataOfMemory->size != 0)
			hasMemoryData=true;
		
	};
	
	sgf->FreeBuffer(requestedData);
	
	
	NoSpecialData:
	
	lastPlayerNumber=usedPlayerNumber;
    lastFaceID = 0;
	
	if (hasMemoryData) {
		if (isInEditPlayerMode==0) {
			TRACE2X(&k_fserv,"addr for player # %d (REPLAY MODE) is %.8x.",usedPlayerNumber,addr);
			lastFaceID=0;
			if (*Faceset==FACESET_SPECIAL)
				g_SpecialFaceHair[usedPlayerNumber]=*FaceID;
			*FaceID=GetNextSpecialAfsFileId(AFSSIG_FACEDATA,usedPlayerNumber,*Skincolor);
			*Faceset=FACESET_NORMAL;
			lastWasFromGDB=true;
			
		} else {
			TRACE2X(&k_fserv,"addr for player # %d (EDIT MODE) is %.8x.",usedPlayerNumber,addr);
			//if (*Faceset==FACESET_NORMAL && *FaceID>=NumStdFaces[*Skincolor]) {
			if (*Faceset==FACESET_NORMAL && *FaceID>=MaxNumStdFaces) {
				TRACE2(&k_fserv,"Assigning face id %d.",*FaceID);
				lastFaceID=*FaceID;
				*FaceID=GetNextSpecialAfsFileIdForFace(*FaceID,*Skincolor);
				lastWasFromGDB=true;
			};
			
			lastFaceID=0;
			lastWasFromGDB=true;
		};
		
	} else {
		//other modes
		g_PlayersIterator=g_Players.find(usedPlayerNumber);
		if (isInEditPlayerMode==0) {
            lastFaceID=0;
			TRACE2X(&k_fserv,"addr for player # %d is %.8x.",usedPlayerNumber,addr);
			if (g_PlayersIterator != g_Players.end()) {
				TRACE2(&k_fserv,"Found player in map, assigning face id %d.",g_PlayersIterator->second);
				lastFaceID=g_PlayersIterator->second;
				if (*Faceset==FACESET_SPECIAL)
					g_SpecialFaceHair[usedPlayerNumber]=*FaceID;
				*FaceID=GetNextSpecialAfsFileIdForFace(g_PlayersIterator->second,*Skincolor);
				*Faceset=FACESET_NORMAL;
				lastWasFromGDB=true;
			//} else if (*Faceset==FACESET_NORMAL && *FaceID>=NumStdFaces[*Skincolor]) {
			} else if (*Faceset==FACESET_NORMAL && *FaceID>=MaxNumStdFaces) {
				TRACE2(&k_fserv,"Assigning face id %d.",*FaceID);
				lastFaceID=*FaceID;
				*FaceID=GetNextSpecialAfsFileIdForFace(*FaceID,*Skincolor);
				lastWasFromGDB=true;
			}
			
		} else {
			TRACE2X(&k_fserv,"addr for player # %d (EDIT MODE) is %.8x.",usedPlayerNumber,addr);
			//if (*Faceset==FACESET_NORMAL && *FaceID>=NumStdFaces[*Skincolor]) {
			if (*Faceset==FACESET_NORMAL && *FaceID>=MaxNumStdFaces) {
				TRACE2(&k_fserv,"Assigning face id %d.",*FaceID);
				lastFaceID=*FaceID;
				*FaceID=GetNextSpecialAfsFileIdForFace(*FaceID,*Skincolor);
				lastWasFromGDB=true;
			};
			
			if (g_PlayersIterator != g_Players.end()) {
				lastFaceID=g_PlayersIterator->second;
				lastWasFromGDB=true;
			};
		};
	};
	
	lastSkincolor=*Skincolor;
	hasChanged=true;
	
	g_PlayersAddr2[addr]=usedPlayerNumber;
	
	TRACEX(&k_fserv,"FaceID is 0x%x, faceset %d, skincolor %d.",*FaceID,*Faceset,*Skincolor);
	return;
};


DWORD CalcHairFile(BYTE Caller)
{
	DWORD addr;
	__asm mov addr, eax
	
	bool fromGDB=false;
	BYTE Skincolor=*(BYTE*)(addr+0x31);
	BYTE Faceset=*(BYTE*)(addr+0x33);
	DWORD HairID=*(WORD*)(addr+0x34);
	WORD FaceID=*(WORD*)(addr+0x40);
	
	DWORD res=0;
	DWORD usedPlayerNumber=g_PlayersAddr2[addr];
	bool hadSpecialFace=false;
	
	if (usedPlayerNumber != 0) {
		TRACE2(&k_fserv,"CalcHairFile for %d.",usedPlayerNumber);
		g_PlayersIterator=g_PlayersHair.find(usedPlayerNumber);
		if (isInEditPlayerMode==0) {
			if (g_PlayersIterator != g_PlayersHair.end()) {
				TRACE2(&k_fserv,"Found player in map, assigning hair id %d.",g_PlayersIterator->second);
				HairID=g_PlayersIterator->second;
				fromGDB=true;
			};
			
			if (fromGDB && g_Hair[HairID]==NULL) {
				LogWithTwoNumbers(&k_fserv,"WRONG HAIR ID: %x for player %d",HairID,usedPlayerNumber);
				HairID=0;
			};
			
			lastHairID=HairID;
			lastHairWasFromGDB=fromGDB;
		} else {
			lastHairID=HairID;
			lastWasFromGDB=fromGDB;
			if (g_PlayersIterator != g_PlayersHair.end()) {
				lastHairID=g_PlayersIterator->second;
				lastHairWasFromGDB=true;
			};
		};
		
		g_PlayersIterator=g_SpecialFaceHair.find(usedPlayerNumber);
		if (g_PlayersIterator != g_SpecialFaceHair.end()) {
			FaceID=g_SpecialFaceHair[usedPlayerNumber];
			hadSpecialFace=true;
		};
	};

	if (fromGDB) {
		if (HairID==lastGDBHairID)
			res=lastGDBHairRes;
		else {
			res=GetNextSpecialAfsFileId(AFSSIG_HAIR,HairID,0);
			lastGDBHairID=HairID;
			lastGDBHairRes=res;
		};
	} else if (*(DWORD*)(&Caller-4)==fIDs[CALCHAIRID]+5 && !hadSpecialFace)
		res=((DWORD*)fIDs[HAIRSTARTARRAY])[Skincolor+4*Faceset]+HairID;
	else
		res=((DWORD*)fIDs[HAIRSTARTARRAY])[Skincolor+4]+FaceID;
		
	return res;
};

void PrintPlayerInfo(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
	isInEditPlayerMode=*(BYTE*)fIDs[ISEDITPLAYERMODE];
	isInEditPlayerList=*(BYTE*)fIDs[ISEDITPLAYERLIST];
	if (isInEditPlayerMode==0 && isInEditPlayerList==0) {
		//KDrawText(450,4,0xffff0000,16,"NOT IN EDIT MODE!");
		return;
	};
	
	char tmp[BUFLEN];
	DWORD color=0xffffffff;

	if (hasChanged) {
		sprintf(tmp,"Player ID: %d",lastPlayerNumber);
		strcpy(lastPlayerNumberString,tmp);
	};
	KDrawText(450,4,color,16,lastPlayerNumberString);
	
	if (hasChanged) {
        //LOG(&k_fserv,"hasChanged! lastFaceID:%d, lastHairID:%d", 
        //        lastFaceID, lastHairID);
		//if (g_Faces[lastSkincolor][lastFaceID]==NULL) {
		if (g_Faces[lastFaceID]==NULL) {
			lastWasFromGDB=false;
		} else {
			//sprintf(tmp,"Face file: %s",g_Faces[lastSkincolor][lastFaceID]);
			sprintf(tmp,"Face file: %s",g_Faces[lastFaceID]);
			strcpy(lastFaceFileString,tmp);
		};
		
		if (g_Hair[lastHairID]==NULL) {
			lastHairWasFromGDB=false;
		} else {
			sprintf(tmp,"Hair file: %s",g_Hair[lastHairID]);
			strcpy(lastHairFileString,tmp);
		};
	};
	hasChanged=false;
	if (lastWasFromGDB)
		KDrawText(450,34,color,16,lastFaceFileString);
		
	if (lastHairWasFromGDB)
		KDrawText(450,64,color,16,lastHairFileString);

	return;
};

void fservCopyPlayerData(DWORD p1, DWORD p2, DWORD p3)
{
	DWORD playerNumber, addr;
	__asm mov playerNumber, ebx
	__asm mov addr, edi
	
	orgCopyPlayerData(p1,p2,p3);
	
	g_PlayersAddr[addr-36]=playerNumber;
	
	return;
};

void fservReplCopyPlayerData()
{
	DWORD oldEAX, oldESI, oldEDX, oldEBP, team;
	__asm mov oldEAX, eax
	__asm mov oldESI, esi
	__asm mov oldEDX, edx
	__asm mov eax, [ebp]
	__asm mov oldEBP, eax
	
	//replace the functionality of the code we have overwritten
	ZeroMemory((BYTE*)oldESI,16);
	
	team=FIRSTREPLPLAYERID+(((oldEDX-0x128-fIDs[PLAYERDATA_BASE])<32*0x344)?0:32);
	
	g_PlayersAddr[oldEDX-0x128]=team+oldEBP;
	
	__asm mov eax, oldEAX
	__asm mov edx, oldEDX
	
	return;
};

DWORD fservEditCopyPlayerData(DWORD playerNumber, DWORD p2)
{
	DWORD result=orgEditCopyPlayerData(playerNumber,p2);
	
	DWORD srcData=fIDs[EDITPLAYERDATA_BASE]+((*(BYTE*)(result+0x35) & 0xf0)?0x520:0x290);
	DWORD addr=fIDs[PLAYERDATA_BASE]+((*(BYTE*)(srcData+0x40))*32+*(BYTE*)(srcData+0x3F))*0x344;
	
	g_PlayersAddr[addr]=playerNumber & 0xffff;
	
	return result;
};

BYTE fservGetHairTransp(DWORD addr)
{
	BYTE result=orgGetHairTransp(addr);
	
	DWORD usedPlayerNumber=g_PlayersAddr2[addr];
	if (usedPlayerNumber != 0 && isInEditPlayerMode==0)
		if (g_PlayersHair.find(usedPlayerNumber) != g_PlayersHair.end())
			result=g_HairTransp[usedPlayerNumber];
	
	return result;
};

DWORD GetNextSpecialAfsFileIdForFace(DWORD FaceID, BYTE Skincolor)
{
	DWORD result=GetNextSpecialAfsFileId(AFSSIG_FACE,FaceID,Skincolor);
	result-=StartsStdFaces[Skincolor];
	result-=0x10000;
	
	return result;
};

void fservConnectAddrToId(DWORD addr, DWORD id)
{
	g_PlayersAddr[addr]=id;
	return;
};
