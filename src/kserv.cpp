/* kserv */
/* KITSERVER ENGINE */

#include <windows.h>
#define _COMPILING_MYDLL
#include <stdio.h>
#include <limits.h>
#include <math.h>

#include "kserv.h"
#include "kload_exp.h"
#include "shared.h"
#include "kserv_config.h"
#include "gdb.h"
#include "crc32.h"
#include "afsreader.h"

#include "d3dfont.h"
#include "dxutil.h"
#include "d3d8types.h"
#include "d3d8.h"
#include "d3dx8tex.h"

#include <pngdib.h>
#include <map>
#include <string>

//#include "allocator.h"

#define VTAB_DELETESTATEBLOCK 56
#define VTAB_SETTRANSFORM 37

#define SWAPBYTES(dw) \
    (dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff)

#define SAFEFREE(ptr) if (ptr) { HeapFree(GetProcessHeap(),0,ptr); ptr=NULL; }
#define IS_CUSTOM_TEAM(id) (id == 0xffff) 

#define MAX_ITERATIONS 1000

#define MAP_FIND(map,key) (((*(map)).count(key)>0) ? (*(map))[key] : NULL)

KMOD k_mydll={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

/**************************************
Shared by all processes variables
**************************************/
#pragma data_seg(".HKT")
KSERV_CONFIG g_config = {
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    },
	DEFAULT_VKEY_HOMEKIT,
	DEFAULT_VKEY_AWAYKIT,
	DEFAULT_VKEY_GKHOMEKIT,
	DEFAULT_VKEY_GKAWAYKIT,
	DEFAULT_SHOW_KIT_INFO,
    DEFAULT_DISABLE_OVERLAYS,
};
#pragma data_seg()
#pragma comment(linker, "/section:.HKT,rws")

/************************** 
End of shared section 
**************************/

CRITICAL_SECTION g_cs;

#define TD_CHECKED 0x01
#define TD_EXISTS 0x02

BYTE g_teamDir[256];
BOOL g_restored = false;
DWORD g_ML_kitPackInfoAddr = 0;

#define KI_GKHOME 0x01
#define KI_GKAWAY 0x02
#define KI_PLHOME 0x04
#define KI_PLAWAY 0x08

#define HOME 0
#define AWAY 1

#define STRIP_PL_SHIRT    0x01
#define STRIP_PL_SHORTS   0x02
#define STRIP_PL_SOCKS    0x04
#define STRIP_PL_SHIRT_C  0xfe
#define STRIP_PL_SHORTS_C 0xfd
#define STRIP_PL_SOCKS_C  0xfb
#define STRIP_GK_SHIRT    0x08
#define STRIP_GK_SHORTS   0x10
#define STRIP_GK_SOCKS    0x20
#define STRIP_GK_SHIRT_C  0xf7
#define STRIP_GK_SHORTS_C 0xef
#define STRIP_GK_SOCKS_C  0xdf

#define MAKE_BUFFER(s) char s[BUFLEN]; ZeroMemory(s,BUFLEN)
#define CLEAR_BUFFER(s) ZeroMemory(s,BUFLEN)

DWORD g_kitID = 0;
DWORD g_follows = 0;
DWORD g_frame_tex_count = 0;
DWORD g_frame_count = 0;
BOOL g_dumpTexturesMode = FALSE;
BOOL g_display2Dkits = FALSE;
BOOL g_lastDisplay2Dkits = FALSE;
BOOL g_customTex = FALSE;

IDirect3DTexture8* g_lastMediumTex = NULL;
IDirect3DTexture8* g_lastBigTex = NULL;
IDirect3DTexture8* lastBig = NULL;
NEWKIT* boostTex=NULL;
char *g_overlayfilename = NULL;

DWORD bigCount=0;

#define MAX_STORED_NEWKITS 8
DWORD g_nextNewKitId=0;
NEWKITFILES g_NewKitFilenames[MAX_STORED_NEWKITS];
NEWKIT g_NewKits[MAX_STORED_NEWKITS];
NEWKITFILES lastKitFiles;
std::map<DWORD,bool> isGdbKit;
std::map<DWORD,DWORD> gdbKitSize;
std::map<DWORD,DWORD> gdbKitCRC;
std::map<DWORD,NEWKITFILES> NewKitFilesMap;
DWORD lastUniDecodeResult=0;
DWORD lastUsedSourceTexture=0;
vector<DWORD> splitTextures;

//#define MAX_SMALL_WIDTH 256
//#define MAX_MEDIUM_WIDTH 512
//#define MAX_LARGE_WIDTH 1024

//unlimited fun!
#define MAX_SMALL_WIDTH 0xFFFF
#define MAX_MEDIUM_WIDTH 0xFFFF
#define MAX_LARGE_WIDTH 0xFFFF

#define MIN_SMALL_WIDTH 128
#define MIN_MEDIUM_WIDTH 256
#define MIN_LARGE_WIDTH 512

RECT smallRect = {0, 0, 256, 128};
RECT mediumRect = {0, 0, 512, 256};
RECT largeRect = {0, 0, 1024, 512};

// textures for strip-selection
IDirect3DTexture8* g_home_shirt_tex = NULL;
IDirect3DTexture8* g_home_shorts_tex = NULL;
IDirect3DTexture8* g_home_socks_tex = NULL;
IDirect3DTexture8* g_away_shirt_tex = NULL;
IDirect3DTexture8* g_away_shorts_tex = NULL;
IDirect3DTexture8* g_away_socks_tex = NULL;

IDirect3DTexture8* g_gloves_left_tex = NULL;

using namespace std;
// map of textures for 2Dkits
map<string,IDirect3DTexture8*> g_kitTextureMap;
// map of palettes for 2Dkits
map<string,PALETTEENTRY*> g_kitPaletteMap;

// kit collection iterators
typedef StringKitMap::iterator KitIterator;

KitIterator g_homeShirtIterator;
KitIterator g_homeShortsIterator;
KitIterator g_homeSocksIterator;
KitIterator g_awayShirtIterator;
KitIterator g_awayShortsIterator;
KitIterator g_awaySocksIterator;

KitIterator g_homeShirtIteratorPL;
KitIterator g_homeShortsIteratorPL;
KitIterator g_homeSocksIteratorPL;
KitIterator g_awayShirtIteratorPL;
KitIterator g_awayShortsIteratorPL;
KitIterator g_awaySocksIteratorPL;
KitIterator g_homeShirtIteratorGK;
KitIterator g_homeShortsIteratorGK;
KitIterator g_homeSocksIteratorGK;
KitIterator g_awayShirtIteratorGK;
KitIterator g_awayShortsIteratorGK;
KitIterator g_awaySocksIteratorGK;

BYTE g_homeStrip = 0xff;
BYTE g_awayStrip = 0xff;

BITMAPINFO* g_MaskTex = NULL;
BYTE* g_Mask = NULL;

char* WHICH_TEAM[] = { "HOME", "AWAY" };

enum { TEXTYPE_NONE, TEXTYPE_BMP, TEXTYPE_PNG };

// KIT FILE TYPES
enum {
    GA_KIT, GA_UNKNOWN1, GA_UNKNOWN2, GA_NUMBERS, GA_FONT,
    GB_KIT, GB_UNKNOWN1, GB_UNKNOWN2, GB_NUMBERS, GB_FONT,
    PA_SHIRT, PA_SHORTS, PA_SOCKS, PA_NUMBERS, PA_FONT,
    PB_SHIRT, PB_SHORTS, PB_SOCKS, PB_NUMBERS, PB_FONT,
};

// KIT KEYS
enum {
    GA, GB, PA, PB,
};

string g_homeShirtKeyGK;
string g_homeShortsKeyGK;
string g_homeSocksKeyGK;
string g_homeShirtKeyPL;
string g_homeShortsKeyPL;
string g_homeSocksKeyPL;
string g_awayShirtKeyGK;
string g_awayShortsKeyGK;
string g_awaySocksKeyGK;
string g_awayShirtKeyPL;
string g_awayShortsKeyPL;
string g_awaySocksKeyPL;

#define GK_TYPE 0
#define PL_TYPE 1
#define GET_HOME_SHIRT_KEY(typ) ((typ==GK_TYPE)?g_homeShirtKeyGK:g_homeShirtKeyPL)
#define GET_HOME_SHORTS_KEY(typ) ((typ==GK_TYPE)?g_homeShortsKeyGK:g_homeShortsKeyPL)
#define GET_HOME_SOCKS_KEY(typ) ((typ==GK_TYPE)?g_homeSocksKeyGK:g_homeSocksKeyPL)
#define GET_AWAY_SHIRT_KEY(typ) ((typ==GK_TYPE)?g_awayShirtKeyGK:g_awayShirtKeyPL)
#define GET_AWAY_SHORTS_KEY(typ) ((typ==GK_TYPE)?g_awayShortsKeyGK:g_awayShortsKeyPL)
#define GET_AWAY_SOCKS_KEY(typ) ((typ==GK_TYPE)?g_awaySocksKeyGK:g_awaySocksKeyPL)
#define GET_KIT_ITER(typ,col) ((typ==GK_TYPE)?col->goalkeepers:col->players)

// global kit selection type
int typ = PL_TYPE;
BOOL g_modeSwitch = FALSE;
WORD g_last_home = 0xff;
WORD g_last_away = 0xff;
BYTE g_cycleDirection[8];
#define CYCLE_LEFT 1
#define CYCLE_RIGHT 0

// Kit afs-item IDs

DWORD* g_AFS_id = NULL;
MEMITEMINFO* g_AFS_memItemInfo = NULL;

#define CODELEN 8

// code array names
enum {
	C_NAVIGATELEFT,C_NAVIGATERIGHT,
	C_SPLITDONE_CS1, C_SPLITDONE_CS2, C_SPLITDONE_CS3, C_SPLITDONE2_CS1, C_SPLITDONE2_CS2,
	C_GETSPLITADDR_CS,
};

// Code addresses.
DWORD codeArray[][CODELEN] = {
    // PES5 DEMO 2
    { 0,0,
    0,0,0,0,0,
    0,
    },
    // PES5
    { 0x51ba51, 0x51ba71,
    0x84ca01,0x84ca41,0x84cbef,0x84c5ad,0x84c6bd,
    0x8d5605,
    },
    // WE9
    { 0x51bed1, 0x51bef1,
	0x84ceb1,0x84cef1,0x84d09f,0x84ca5d,0x84cb6d,
	0x8d5b65,
    },
    // WE9:LE
    { 0x5169d1, 0x5169f1,
	0x874161,0x8741a1,0x87434f,0x873d0d,0x873e1d,
	0x8d4d95,
    },
};


#define DATALEN 16

// data array names
enum {
	TEAM_IDS, TEAM_STRIPS, KIT_SLOTS, 
	ANOTHER_KIT_SLOTS, MLDATA_PTRS, TEAM_COLLARS_PTR,
	KIT_CHECK_TRIGGER, FIRST_ID, LAST_ID, 
    NATIONAL_TEAMS_ADDR, CLUB_TEAMS_ADDR, 
	ML_HOME_AREA, ML_AWAY_AREA,
	SINGLEPLAYER, KITSELECTSIDE, PLAYERSIDE,	
};

// Data addresses.
DWORD dataArray[][DATALEN] = {
    // PES5 DEMO 2
    { 0, 0, 0,
      0, 0, 0,
      0, 5810, 8109, 
      0, 0, 0, 0,
      0,0,0,
    },
    // PES5
    { 0x3be0f40, 0x3b7f2c6, 0,
      0, 0, 0,
      0, 4576, 8935, // was 8615: added All-Star teams
      0x38b77dc, 0x38b77e0, 0x38b77a4, 0x38b77a8,
      0x3be10a0, 0xfdeefc, 0xc0e4d0,
    },
    // WE9
    { 0x3be0f60, 0x3b7f2e6, 0,
      0, 0, 0,
      0, 4576, 8935, // was 8615: added All-Star teams
      0x38b77dc, 0x38b77e0, 0x38b77a4, 0x38b77a8,
      0x3be10c0, 0xfdef04, 0xc0e4d0,
    },
    // WE9:LE
    { 0x3b68a80, 0x3adb606, 0, 
      0, 0, 0, 
      0, 4583, 8942,
      0x37f20ec, 0x37f20f0, 0x37f20b4, 0x37f20b8,
      0x3b68be0, 0xf18e94, 0xb4d548,
    },
};

DWORD code[CODELEN];
DWORD data[DATALEN];

//safe_allocator<MEMITEMINFO> myAlloc;
//std::map<DWORD,MEMITEMINFO*,cmp,myAlloc> g_AFS_offsetMap;
//std::map<DWORD,MEMITEMINFO*,cmp,myAlloc> g_AFS_addressMap;
std::map<DWORD,MEMITEMINFO*> g_AFS_offsetMap;
std::map<DWORD,MEMITEMINFO*> g_AFS_addressMap;

// saves modified kitpack info
std::map<DWORD,TEAMKITINFO*> g_teamKitInfo;
std::map<DWORD,TEAMKITINFO*>::iterator g_teamKitInfoIterator;

// Actual number of teams in the game
int g_numTeams = 0; 

TEAMBUFFERINFO g_teamInfo[256];
AFSITEMINFO g_bibs;
char g_afsFileName[BUFLEN];
UINT g_triggerLoad3rdKit = 0;

WORD g_currTeams[2] = {0xffff, 0xffff};

//////////////////////////////////////

// text labels for default strips
char* DEFAULT_LABEL[] = { "1st default", "2nd default", "auto-pick" };

// Graphics Database
GDB* gdb = NULL;

///// Graphics ////////////////

struct CUSTOMVERTEX { 
	FLOAT x,y,z,w;
	DWORD color;
};

struct CUSTOMVERTEX2 { 
	FLOAT x,y,z,w;
	DWORD color;
	FLOAT tu, tv;
};

// SHIRT
IDirect3DVertexBuffer8* g_pVB_home_shirt_left = NULL;
IDirect3DVertexBuffer8* g_pVB_home_NB_shirt_left = NULL;
IDirect3DVertexBuffer8* g_pVB_home_sleeve_left = NULL;
IDirect3DVertexBuffer8* g_pVB_home_NB_sleeve_left = NULL;
IDirect3DVertexBuffer8* g_pVB_home_shirt_right = NULL;
IDirect3DVertexBuffer8* g_pVB_home_NB_shirt_right = NULL;
IDirect3DVertexBuffer8* g_pVB_home_sleeve_right = NULL;
IDirect3DVertexBuffer8* g_pVB_home_NB_sleeve_right = NULL;
IDirect3DVertexBuffer8* g_pVB_home_shirt_outline = NULL;
IDirect3DVertexBuffer8* g_pVB_home_sleeve_left_outline = NULL;
IDirect3DVertexBuffer8* g_pVB_home_sleeve_right_outline = NULL;
// SHORTS
IDirect3DVertexBuffer8* g_pVB_home_shorts = NULL;
IDirect3DVertexBuffer8* g_pVB_home_shorts_2 = NULL;
IDirect3DVertexBuffer8* g_pVB_home_shorts_outline = NULL;
IDirect3DVertexBuffer8* g_pVB_home_shorts_2_outline = NULL;
// SOCKS
IDirect3DVertexBuffer8* g_pVB_home_socks_shin_left = NULL;
IDirect3DVertexBuffer8* g_pVB_home_socks_foot_left = NULL;
IDirect3DVertexBuffer8* g_pVB_home_socks_left_outline = NULL;
IDirect3DVertexBuffer8* g_pVB_home_socks_shin_right = NULL;
IDirect3DVertexBuffer8* g_pVB_home_socks_foot_right = NULL;
IDirect3DVertexBuffer8* g_pVB_home_socks_right_outline = NULL;

// SHIRT
IDirect3DVertexBuffer8* g_pVB_away_shirt_left = NULL;
IDirect3DVertexBuffer8* g_pVB_away_NB_shirt_left = NULL;
IDirect3DVertexBuffer8* g_pVB_away_sleeve_left = NULL;
IDirect3DVertexBuffer8* g_pVB_away_NB_sleeve_left = NULL;
IDirect3DVertexBuffer8* g_pVB_away_shirt_right = NULL;
IDirect3DVertexBuffer8* g_pVB_away_NB_shirt_right = NULL;
IDirect3DVertexBuffer8* g_pVB_away_sleeve_right = NULL;
IDirect3DVertexBuffer8* g_pVB_away_NB_sleeve_right = NULL;
IDirect3DVertexBuffer8* g_pVB_away_shirt_outline = NULL;
IDirect3DVertexBuffer8* g_pVB_away_sleeve_left_outline = NULL;
IDirect3DVertexBuffer8* g_pVB_away_sleeve_right_outline = NULL;
// SHORTS
IDirect3DVertexBuffer8* g_pVB_away_shorts = NULL;
IDirect3DVertexBuffer8* g_pVB_away_shorts_2 = NULL;
IDirect3DVertexBuffer8* g_pVB_away_shorts_outline = NULL;
IDirect3DVertexBuffer8* g_pVB_away_shorts_2_outline = NULL;
// SOCKS
IDirect3DVertexBuffer8* g_pVB_away_socks_shin_left = NULL;
IDirect3DVertexBuffer8* g_pVB_away_socks_foot_left = NULL;
IDirect3DVertexBuffer8* g_pVB_away_socks_left_outline = NULL;
IDirect3DVertexBuffer8* g_pVB_away_socks_shin_right = NULL;
IDirect3DVertexBuffer8* g_pVB_away_socks_foot_right = NULL;
IDirect3DVertexBuffer8* g_pVB_away_socks_right_outline = NULL;

// GLOVES
IDirect3DVertexBuffer8* g_pVB_gloves_left = NULL;
IDirect3DVertexBuffer8* g_pVB_gloves_right = NULL;

static DWORD g_dwSavedStateBlock = 0L;
static DWORD g_dwDrawOverlayStateBlock = 0L;

// 2D-kit polygons

#define X_2DKIT_LEFT 122
#define Y_2DKIT_LEFT 325
#define SOCK_SHIFT 32
#define AWAY_KIT_SHIFT 673

// SHIRT

CUSTOMVERTEX2 g_2Dkit_home_shirt_left[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 0.0f/256}, //1
	{42.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 78.0f/512, 0.0f/256}, //2
	{21.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 12.0f/256}, //3
	{55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 12.0f/256}, //4
	{21.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 144.0f/256}, //4
	{55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 144.0f/256}, //5
};

CUSTOMVERTEX2 g_2Dkit_home_sleeve_left[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 336.0f/512, 153.0f/256}, //1
	{38.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 397.0f/512, 153.0f/256}, //2
	{-3.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 336.0f/512, 82.0f/256}, //3
	{16.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 397.0f/512, 82.0f/256}, //4
};

CUSTOMVERTEX2 g_2Dkit_home_shirt_right[] = {
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 0.0f/256}, //1
	{68.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 106.0f/512, 0.0f/256}, //2
	{89.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 12.0f/256}, //3
	{55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 12.0f/256}, //4
	{89.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 144.0f/256}, //4
	{55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 144.0f/256}, //5
};

CUSTOMVERTEX2 g_2Dkit_home_sleeve_right[] = {
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 448.0f/512, 153.0f/256}, //1
	{72.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 153.0f/256}, //2
	{113.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 448.0f/512, 82.0f/256}, //3
	{94.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 82.0f/256}, //4
};

CUSTOMVERTEX g_2Dkit_home_shirt_outline[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{42.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //2
	{55.0f, 8.0f, 0.0f, 1.0f, 0xff000000}, //3
	{68.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //4
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //5
	{89.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //6
	{21.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //7
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //8
};

CUSTOMVERTEX g_2Dkit_home_sleeve_left_outline[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{38.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
	{16.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
	{-3.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
};

CUSTOMVERTEX g_2Dkit_home_sleeve_right_outline[] = {
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{72.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
	{94.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
	{113.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
};

// SHORTS

CUSTOMVERTEX2 g_2Dkit_home_shorts[] = {
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 120.0f/512, 144.0f/256}, //1
	{84.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 172.0f/512, 144.0f/256}, //2
	{40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 120.0f/512, 256.0f/256}, //3
	{84.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 172.0f/512, 256.0f/256}, //4
};
CUSTOMVERTEX2 g_2Dkit_home_shorts_2[] = {
	{27.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 33.0f/512, 144.0f/256}, //1
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 70.0f/512, 144.0f/256}, //2
	{20.0f, 140.0f, 0.0f, 1.0f, 0xff4488ff, 33.0f/512, 256.0f/256}, //3
	{40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 87.0f/512, 256.0f/256}, //4
};
CUSTOMVERTEX g_2Dkit_home_shorts_outline[] = {
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
	{84.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
	{84.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
	{40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //3
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
};
CUSTOMVERTEX g_2Dkit_home_shorts_2_outline[] = {
	{27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
	{40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
	{20.0f, 140.0f, 0.0f, 1.0f, 0xff000000}, //3
	{27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
};

// SOCKS

CUSTOMVERTEX2 g_2Dkit_home_socks_shin_left[] = {
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 0.0f/256}, //1
	{50.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 0.0f/256}, //2
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //3
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //4
};
CUSTOMVERTEX2 g_2Dkit_home_socks_foot_left[] = {
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //1
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //2
	{52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 61.0f/256}, //3
	{52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 65.0f/256}, //4
	{33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 380.0f/512, 75.0f/256}, //5
	{12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 375.0f/512, 75.0f/256}, //6
	{8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 370.0f/512, 70.0f/256}, //7
};
CUSTOMVERTEX2 g_2Dkit_home_socks_shin_right[] = {
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 0.0f/256}, //1
	{51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 0.0f/256}, //2
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //3
	{51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //4
};
CUSTOMVERTEX2 g_2Dkit_home_socks_foot_right[] = {
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //1
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //2
	{52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 61.0f/256}, //3
	{52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 65.0f/256}, //4
	{33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 380.0f/512, 75.0f/256}, //5
	{12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 375.0f/512, 75.0f/256}, //6
	{8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 370.0f/512, 70.0f/256}, //7
};
CUSTOMVERTEX g_2Dkit_home_socks_left_outline[] = {
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
	{50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
	{52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
	{52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
	{33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
	{12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
	{8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
};
CUSTOMVERTEX g_2Dkit_home_socks_right_outline[] = {
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
	{50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
	{52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
	{52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
	{33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
	{12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
	{8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
};

// SHIRT

CUSTOMVERTEX2 g_2Dkit_away_shirt_left[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 0.0f/256}, //1
	{42.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 78.0f/512, 0.0f/256}, //2
	{21.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 12.0f/256}, //3
	{55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 12.0f/256}, //4
	{21.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 144.0f/256}, //4
	{55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 144.0f/256}, //5
};

CUSTOMVERTEX2 g_2Dkit_away_sleeve_left[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 336.0f/512, 153.0f/256}, //1
	{38.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 397.0f/512, 153.0f/256}, //2
	{-3.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 336.0f/512, 82.0f/256}, //3
	{16.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 397.0f/512, 82.0f/256}, //4
};

CUSTOMVERTEX2 g_2Dkit_away_shirt_right[] = {
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 0.0f/256}, //1
	{68.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 106.0f/512, 0.0f/256}, //2
	{89.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 12.0f/256}, //3
	{55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 12.0f/256}, //4
	{89.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 143.0f/512, 144.0f/256}, //4
	{55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 92.0f/512, 144.0f/256}, //5
};

CUSTOMVERTEX2 g_2Dkit_away_sleeve_right[] = {
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 448.0f/512, 153.0f/256}, //1
	{72.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 153.0f/256}, //2
	{113.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 448.0f/512, 82.0f/256}, //3
	{94.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 82.0f/256}, //4
};

CUSTOMVERTEX g_2Dkit_away_shirt_outline[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{42.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //2
	{55.0f, 8.0f, 0.0f, 1.0f, 0xff000000}, //3
	{68.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //4
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //5
	{89.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //6
	{21.0f, 78.0f, 0.0f, 1.0f, 0xff000000}, //7
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //8
};

CUSTOMVERTEX g_2Dkit_away_sleeve_left_outline[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{38.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
	{16.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
	{-3.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
};

CUSTOMVERTEX g_2Dkit_away_sleeve_right_outline[] = {
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
	{72.0f, 14.0f, 0.0f, 1.0f, 0xff000000}, //2
	{94.0f, 43.0f, 0.0f, 1.0f, 0xff000000}, //4
	{113.0f, 25.0f, 0.0f, 1.0f, 0xff000000}, //3
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff000000}, //1
};

// SHORTS

CUSTOMVERTEX2 g_2Dkit_away_shorts[] = {
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 120.0f/512, 144.0f/256}, //1
	{84.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 172.0f/512, 144.0f/256}, //2
	{40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 120.0f/512, 256.0f/256}, //3
	{84.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 172.0f/512, 256.0f/256}, //4
};
CUSTOMVERTEX2 g_2Dkit_away_shorts_2[] = {
	{27.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 33.0f/512, 144.0f/256}, //1
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff4488ff, 70.0f/512, 144.0f/256}, //2
	{20.0f, 140.0f, 0.0f, 1.0f, 0xff4488ff, 33.0f/512, 256.0f/256}, //3
	{40.0f, 142.0f, 0.0f, 1.0f, 0xff4488ff, 87.0f/512, 256.0f/256}, //4
};
CUSTOMVERTEX g_2Dkit_away_shorts_outline[] = {
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
	{84.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
	{84.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
	{40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //3
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
};
CUSTOMVERTEX g_2Dkit_away_shorts_2_outline[] = {
	{27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
	{40.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //2
	{40.0f, 142.0f, 0.0f, 1.0f, 0xff000000}, //4
	{20.0f, 140.0f, 0.0f, 1.0f, 0xff000000}, //3
	{27.0f, 84.0f, 0.0f, 1.0f, 0xff000000}, //1
};

// SOCKS

CUSTOMVERTEX2 g_2Dkit_away_socks_shin_left[] = {
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 0.0f/256}, //1
	{50.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 0.0f/256}, //2
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //3
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //4
};
CUSTOMVERTEX2 g_2Dkit_away_socks_foot_left[] = {
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //1
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //2
	{52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 61.0f/256}, //3
	{52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 65.0f/256}, //4
	{33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 380.0f/512, 75.0f/256}, //5
	{12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 375.0f/512, 75.0f/256}, //6
	{8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 370.0f/512, 70.0f/256}, //7
};
CUSTOMVERTEX2 g_2Dkit_away_socks_shin_right[] = {
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 0.0f/256}, //1
	{51.0f, 156.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 0.0f/256}, //2
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //3
	{51.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //4
};
CUSTOMVERTEX2 g_2Dkit_away_socks_foot_right[] = {
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000, 395.0f/512, 66.0f/256}, //1
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 433.0f/512, 61.0f/256}, //2
	{52.0f, 201.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 61.0f/256}, //3
	{52.0f, 208.0f, 0.0f, 1.0f, 0xff000000, 435.0f/512, 65.0f/256}, //4
	{33.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 380.0f/512, 75.0f/256}, //5
	{12.0f, 225.0f, 0.0f, 1.0f, 0xff000000, 375.0f/512, 75.0f/256}, //6
	{8.0f, 221.0f, 0.0f, 1.0f, 0xff000000, 370.0f/512, 70.0f/256}, //7
};
CUSTOMVERTEX g_2Dkit_away_socks_left_outline[] = {
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
	{50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
	{52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
	{52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
	{33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
	{12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
	{8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
};
CUSTOMVERTEX g_2Dkit_away_socks_right_outline[] = {
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
	{50.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //2
	{50.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //2
	{52.0f, 201.0f, 0.0f, 1.0f, 0xff000000}, //3
	{52.0f, 208.0f, 0.0f, 1.0f, 0xff000000}, //4
	{33.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //5
	{12.0f, 225.0f, 0.0f, 1.0f, 0xff000000}, //6
	{8.0f, 221.0f, 0.0f, 1.0f, 0xff000000}, //7
	{26.0f, 206.0f, 0.0f, 1.0f, 0xff000000}, //1
	{26.0f, 156.0f, 0.0f, 1.0f, 0xff000000}, //1
};

//////////////////////////

// narrow-back texture mapping corrections

CUSTOMVERTEX2 g_narrow_back_sleeve_left[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 349.0f/512, 159.0f/256}, //1
	{38.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 349.0f/512, 256.0f/256}, //2
	{-3.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 402.0f/512, 159.0f/256}, //3
	{16.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 402.0f/512, 256.0f/256}, //4
};

CUSTOMVERTEX2 g_narrow_back_sleeve_right[] = {
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 459.0f/512, 159.0f/256}, //1
	{72.0f, 14.0f, 0.0f, 1.0f, 0xff4488ff, 459.0f/512, 256.0f/256}, //2
	{113.0f, 25.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 159.0f/256}, //3
	{94.0f, 43.0f, 0.0f, 1.0f, 0xff4488ff, 512.0f/512, 256.0f/256}, //4
};

CUSTOMVERTEX2 g_narrow_back_shirt_left[] = {
	{21.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 0.0f/256}, //1
	{42.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 100.0f/512, 0.0f/256}, //2
	{21.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 12.0f/256}, //3
	{55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 115.0f/512, 12.0f/256}, //4
	{21.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 41.0f/512, 144.0f/256}, //4
	{55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 115.0f/512, 144.0f/256}, //5
};

CUSTOMVERTEX2 g_narrow_back_shirt_right[] = {
	{89.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 192.0f/512, 0.0f/256}, //1
	{68.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 128.0f/512, 0.0f/256}, //2
	{89.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 192.0f/512, 12.0f/256}, //3
	{55.0f, 8.0f, 0.0f, 1.0f, 0xff4488ff, 115.0f/512, 12.0f/256}, //4
	{89.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 192.0f/512, 144.0f/256}, //4
	{55.0f, 78.0f, 0.0f, 1.0f, 0xff4488ff, 115.0f/512, 144.0f/256}, //5
};

// GLOVES

CUSTOMVERTEX2 g_gloves_left[] = {
	{0.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 0.0f}, //1
	{0.0f, 64.0f, 0.0f, 1.0f, 0xff4488ff, 0.0f, 1.0f}, //2
	{64.0f, 0.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 0.0f}, //3
	{64.0f, 64.0f, 0.0f, 1.0f, 0xff4488ff, 1.0f, 1.0f}, //4
};

////////////////////////////////

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
#define D3DFVF_CUSTOMVERTEX2 (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

BOOL g_needsRestore = TRUE;
double stretchX=1;
double stretchY=1;

void MaskKitTextureWithFile(BITMAPINFOHEADER* tex, char* filename, DWORD id);
void MaskKitTexture(BITMAPINFOHEADER* tex, DWORD id, BITMAPINFO* maskTex);

/* IDirect3DDevice8 method-types */
typedef HRESULT (STDMETHODCALLTYPE *PFNCREATETEXTUREPROC)(IDirect3DDevice8* self, 
UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, 
IDirect3DTexture8** ppTexture);
typedef HRESULT (STDMETHODCALLTYPE *PFNSETTEXTUREPROC)(IDirect3DDevice8* self, 
DWORD stage, IDirect3DBaseTexture8* pTexture);
typedef HRESULT (STDMETHODCALLTYPE *PFNSETTRANSFORMPROC)(IDirect3DDevice8* self, 
D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX* pMatrix);
typedef HRESULT (STDMETHODCALLTYPE *PFNSETTEXTURESTAGESTATEPROC)(IDirect3DDevice8* self, 
DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
typedef HRESULT (STDMETHODCALLTYPE *PFNUPDATETEXTUREPROC)(IDirect3DDevice8* self,
IDirect3DBaseTexture8* pSrc, IDirect3DBaseTexture8* pDest);
typedef HRESULT (STDMETHODCALLTYPE *PFNCOPYRECTSPROC)(IDirect3DDevice8* self,
IDirect3DSurface8* pSourceSurface, CONST RECT* pSourceRectsArray, UINT cRects,
IDirect3DSurface8* pDestinationSurface, CONST POINT* pDestPointsArray);
typedef HRESULT (STDMETHODCALLTYPE *PFNAPPLYSTATEBLOCKPROC)(IDirect3DDevice8* self, DWORD token);
typedef HRESULT (STDMETHODCALLTYPE *PFNBEGINSCENEPROC)(IDirect3DDevice8* self);
typedef HRESULT (STDMETHODCALLTYPE *PFNENDSCENEPROC)(IDirect3DDevice8* self);
typedef HRESULT (STDMETHODCALLTYPE *PFNGETSURFACELEVELPROC)(IDirect3DTexture8* self,
UINT level, IDirect3DSurface8** ppSurfaceLevel);

void  JuceUniDecode(DWORD, DWORD, DWORD );
//void  JuceUniSplit(DWORD, DWORD, DWORD);
DWORD  JuceUniSplit(DWORD);
DWORD JuceSplitDone1(DWORD p1);
DWORD JuceSplitDone2_1(DWORD p1);
DWORD JuceSplitDone2_2(DWORD p1);
void  JuceUnpack(DWORD, DWORD, DWORD, DWORD, DWORD*, DWORD);
void  JuceGetNationalTeamInfo(DWORD,DWORD);
void  JuceGetNationalTeamInfoExitEdit(DWORD,DWORD);
void  JuceGetClubTeamInfo(DWORD,DWORD);
void  JuceGetClubTeamInfoML1(DWORD,DWORD);
void  JuceGetClubTeamInfoML2(DWORD,DWORD);
void  JuceSet2Dkits();
void  JuceClear2Dkits();

bool JuceReadFile(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);

HRESULT STDMETHODCALLTYPE JuceCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,UINT levels,
DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture, DWORD src, bool* IsProcessed);
HRESULT STDMETHODCALLTYPE JuceSetTransform(IDirect3DDevice8* self, 
D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX* pMatrix);
HRESULT STDMETHODCALLTYPE JuceSetTextureStageState(IDirect3DDevice8* self, DWORD Stage,
D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
HRESULT STDMETHODCALLTYPE JuceUpdateTexture(IDirect3DDevice8* self,
IDirect3DBaseTexture8* pSrc, IDirect3DBaseTexture8* pDest);
void JucePresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
HRESULT STDMETHODCALLTYPE JuceCopyRects(IDirect3DDevice8* self,
IDirect3DSurface8* pSourceSurface, CONST RECT* pSourceRectsArray, UINT cRects,
IDirect3DSurface8* pDestinationSurface, CONST POINT* pDestPointsArray);
HRESULT STDMETHODCALLTYPE JuceApplyStateBlock(IDirect3DDevice8* self, DWORD token);
HRESULT STDMETHODCALLTYPE JuceBeginScene(IDirect3DDevice8* self);
HRESULT STDMETHODCALLTYPE JuceEndScene(IDirect3DDevice8* self);
HRESULT STDMETHODCALLTYPE JuceGetSurfaceLevel(IDirect3DTexture8* self,
UINT level, IDirect3DSurface8** ppSurfaceLevel);
void JuceUnlockRect(IDirect3DTexture8* self,UINT Level);
void mydllKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);

DWORD LoadTexture(BITMAPINFO** tex, char* filename);
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename);
void CreateGDBTextureFromFile(char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal);
void CreateGDBTextureFromFolder(char* foldername, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal);
void FreeTexture(BITMAPINFO* tex);
void FreePNGTexture(BITMAPINFO* tex);

void DumpData(BYTE* addr, DWORD size);
void DumpData(BYTE* addr, DWORD size, char* filename);
void DumpUniforms(TEXIMGPACKHEADER* kitPackHeader);
void DumpImagePack(TEXIMGPACKHEADER* imagePackHeader);

void FindKitPartsNames(DWORD id, NEWKITFILES* files);
NEWKIT* CreateNewKits(NEWKITFILES files);
void UnloadNewKits();
void UnloadNewKits(DWORD Index);

void DrawKitInfo(IDirect3DDevice8* dev);
void GetKitName(BYTE which, KitCollection* col, char* KitName);

void ApplyGlovesDIBTexture(TEXIMGPACKHEADER* texPack, BITMAPINFO* bitmapInfo);

DWORD FindImageFileForId(DWORD id, char* suffix, char* filename);
DWORD FindImageFileForId2(DWORD id, char* suffix, char* filename);
BOOL FindImageFileForIdEx(DWORD id, char* suffix, char* filename, char* ext);
DWORD FindShortsPalImageFileForId(DWORD id, string& kitFolderKey, char* filename);

/* function pointers */
PFNCREATETEXTUREPROC g_orgCreateTexture = NULL;
PFNSETTRANSFORMPROC g_orgSetTransform = NULL;
PFNSETTEXTUREPROC g_orgSetTexture = NULL;
PFNSETTEXTURESTAGESTATEPROC g_orgSetTextureStageState = NULL;
PFNCOPYRECTSPROC g_orgCopyRects = NULL;
PFNAPPLYSTATEBLOCKPROC g_orgApplyStateBlock = NULL;
PFNBEGINSCENEPROC g_orgBeginScene = NULL;
PFNENDSCENEPROC g_orgEndScene = NULL;
PFNGETSURFACELEVELPROC g_orgGetSurfaceLevel = NULL;
PFNUPDATETEXTUREPROC g_orgUpdateTexture = NULL;

HINSTANCE hInst;

char buf[BUFLEN];

BOOL bIndicate = false;

BOOL GoodTeamId(WORD id);
void VerifyTeams(void);
void ResetCyclesAndBuffers2(void);
HRESULT SaveAsBitmap(char*, TEXIMGHEADER*);
HRESULT SaveAs8bitBMP(char*, BYTE*);
HRESULT SaveAs8bitBMP(char*, BYTE*, BYTE*, LONG, LONG);
HRESULT SaveAs4bitBMP(char*, BYTE*, BYTE*, LONG, LONG);
HRESULT SaveAsBMP(char*, BYTE*, SIZE_T, LONG, LONG, int);

//Kit* utilGetKit(WORD kitId);
TeamAttr* utilGetTeamAttr(WORD teamId);
BOOL IsEditedKit(int slot);

BOOL TeamDirExists(DWORD teamId);
BOOL IsNumOrFontTexture(DWORD id);
BOOL IsNumbersTexture(DWORD id);
BOOL IsGKTexture(DWORD id);
string GetKitFolderKey(DWORD id);
string GetNextHomeShirtKey(WORD teamId, string key);
string GetNextHomeShortsKey(WORD teamId, string key);
string GetNextHomeSocksKey(WORD teamId, string key);
string GetNextAwayShirtKey(WORD teamId, string key);
string GetNextAwayShortsKey(WORD teamId, string key);
string GetNextAwaySocksKey(WORD teamId, string key);
void ResetKitKeys();
void ResetKitKeys(BOOL all);
void ClearKitKeys();
BOOL BackToStandardStripSelection(BYTE homeStrip, BYTE awayStrip);
BOOL FileExists(char* filename);
BYTE GetTeamStrips(int which);
DWORD JuceNavigateLeft(DWORD player);
DWORD JuceNavigateRight(DWORD player);

typedef string (*CYCLEPROC)(WORD teamId, string key);

void Load2DkitTexture(WORD teamId, const char* kitFolder, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal);
void TryLoad2DkitTexture(WORD teamId, int strip, string currKey, string* key, CYCLEPROC proc, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal);
void TryLoadSamePal2DkitTexture(WORD teamId, int strip, string currKey, string* key, CYCLEPROC proc, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal, PALETTEENTRY* cmpPal);
BOOL IsSamePalette(PALETTEENTRY* a, PALETTEENTRY* b);
PALETTEENTRY* MakePaletteCopy(PALETTEENTRY* src);
void ApplyOverlay(D3DLOCKED_RECT* dest, char* overlayfilename, D3DSURFACE_DESC* desc);

// palette buffers: for shared-palette kit checks
PALETTEENTRY g_home_shirt_pal[0x100];
PALETTEENTRY g_home_shorts_pal[0x100];
PALETTEENTRY g_home_socks_pal[0x100];
PALETTEENTRY g_away_shirt_pal[0x100];
PALETTEENTRY g_away_shorts_pal[0x100];
PALETTEENTRY g_away_socks_pal[0x100];


//////////////////////////////////////////////////////////////
// FUNCTIONS /////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

// Calls IUnknown::Release() on an instance
/*
void SafeRelease(LPVOID ppObj)
{
    try {
        IUnknown** ppIUnknown = (IUnknown**)ppObj;
        if (ppIUnknown == NULL)
        {
            Log(&k_mydll,"Address of IUnknown reference is 0");
            return;
        }
        if (*ppIUnknown != NULL)
        {
            (*ppIUnknown)->Release();
            *ppIUnknown = NULL;
        }
    } catch (...) {
        // problem with a safe-release
        TRACE(&k_mydll,"Problem with safe-release");
    }
}
*/
void SafeRelease(void *ppObj)
{
    int rc;
    IUnknown **ppIUnk = (IUnknown**)ppObj;
    IUnknown *pIUnk = *ppIUnk;
    if (pIUnk) {
        *ppIUnk = NULL;
        rc = pIUnk->Release();
        TRACE2(&k_mydll, "SafeRelease: ref-count: %d", rc);
    }
}

void LogRC(void *ppObj)
{
    int rc;
    IUnknown **ppIUnk = (IUnknown**)ppObj;
    IUnknown *pIUnk = *ppIUnk;
    if (pIUnk) {
        pIUnk->AddRef();
        rc = pIUnk->Release();
        TRACE2(&k_mydll, "LogRC: ref-count: %d", rc);
    }
}

void SetPosition(CUSTOMVERTEX2* dest, CUSTOMVERTEX2* src, int n, int x, int y) 
{	
	FLOAT xratio = GetPESInfo()->stretchX;
    FLOAT yratio = GetPESInfo()->stretchY;
    for (int i=0; i<n; i++) {
        dest[i].x = (FLOAT)(int)((src[i].x + x) * xratio);
        dest[i].y = (FLOAT)(int)((src[i].y + y) * yratio);
    }
}

void SetPosition(CUSTOMVERTEX* dest, CUSTOMVERTEX* src, int n, int x, int y) 
{
	FLOAT xratio = GetPESInfo()->stretchX;
    FLOAT yratio = GetPESInfo()->stretchY;
    for (int i=0; i<n; i++) {
        dest[i].x = (FLOAT)(int)((src[i].x + x) * xratio);
        dest[i].y = (FLOAT)(int)((src[i].y + y) * yratio);
    }
}

void NarrowBackCorrection(CUSTOMVERTEX2* dest, CUSTOMVERTEX2* src, int n)
{
    for (int i=0; i<n; i++) {
        dest[i].tu = src[i].tu;
        dest[i].tv = src[i].tv;
    }
}

/* creates vertex buffers */
HRESULT InitVB(IDirect3DDevice8* dev)
{
	VOID* pVertices;

	// create vertex buffers

	// gloves-left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_gloves_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_gloves_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_gloves_left->Lock(0, sizeof(g_gloves_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_gloves_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_gloves_left, sizeof(g_gloves_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_gloves_left, sizeof(g_gloves_left)/sizeof(CUSTOMVERTEX2), 481, 328);
	g_pVB_gloves_left->Unlock();


    ///// HOME /////////////////////////////////

	// shirt-left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shirt_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_shirt_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_shirt_left->Lock(0, sizeof(g_2Dkit_home_shirt_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_shirt_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shirt_left, sizeof(g_2Dkit_home_shirt_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_shirt_left, sizeof(g_2Dkit_home_shirt_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_shirt_left->Unlock();

	// shirt-right
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shirt_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_shirt_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_shirt_right->Lock(0, sizeof(g_2Dkit_home_shirt_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_shirt_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shirt_right, sizeof(g_2Dkit_home_shirt_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_shirt_right, sizeof(g_2Dkit_home_shirt_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_shirt_right->Unlock();

	// NB shirt-left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shirt_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_NB_shirt_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_NB_shirt_left->Lock(0, sizeof(g_2Dkit_home_shirt_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_NB_shirt_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shirt_left, sizeof(g_2Dkit_home_shirt_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_shirt_left, sizeof(g_2Dkit_home_shirt_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
    NarrowBackCorrection((CUSTOMVERTEX2*)pVertices, g_narrow_back_shirt_left, sizeof(g_narrow_back_shirt_left)/sizeof(CUSTOMVERTEX2));
	g_pVB_home_NB_shirt_left->Unlock();

	// NB shirt-right
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shirt_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_NB_shirt_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_NB_shirt_right->Lock(0, sizeof(g_2Dkit_home_shirt_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_NB_shirt_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shirt_right, sizeof(g_2Dkit_home_shirt_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_shirt_right, sizeof(g_2Dkit_home_shirt_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
    NarrowBackCorrection((CUSTOMVERTEX2*)pVertices, g_narrow_back_shirt_right, sizeof(g_narrow_back_shirt_right)/sizeof(CUSTOMVERTEX2));
	g_pVB_home_NB_shirt_right->Unlock();

	// sleeve_left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_sleeve_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_sleeve_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_sleeve_left->Lock(0, sizeof(g_2Dkit_home_sleeve_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_sleeve_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_sleeve_left, sizeof(g_2Dkit_home_sleeve_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_sleeve_left, sizeof(g_2Dkit_home_sleeve_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_sleeve_left->Unlock();

	// sleeve_right
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_sleeve_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_sleeve_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_sleeve_right->Lock(0, sizeof(g_2Dkit_home_sleeve_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_sleeve_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_sleeve_right, sizeof(g_2Dkit_home_sleeve_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_sleeve_right, sizeof(g_2Dkit_home_sleeve_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_sleeve_right->Unlock();

	// NB sleeve_left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_sleeve_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_NB_sleeve_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_NB_sleeve_left->Lock(0, sizeof(g_2Dkit_home_sleeve_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_NB_sleeve_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_sleeve_left, sizeof(g_2Dkit_home_sleeve_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_sleeve_left, sizeof(g_2Dkit_home_sleeve_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
    NarrowBackCorrection((CUSTOMVERTEX2*)pVertices, g_narrow_back_sleeve_left, sizeof(g_narrow_back_sleeve_left)/sizeof(CUSTOMVERTEX2));
	g_pVB_home_NB_sleeve_left->Unlock();

	// NB sleeve_right
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_sleeve_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_NB_sleeve_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_NB_sleeve_right->Lock(0, sizeof(g_2Dkit_home_sleeve_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_NB_sleeve_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_sleeve_right, sizeof(g_2Dkit_home_sleeve_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_sleeve_right, sizeof(g_2Dkit_home_sleeve_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
    NarrowBackCorrection((CUSTOMVERTEX2*)pVertices, g_narrow_back_sleeve_right, sizeof(g_narrow_back_sleeve_right)/sizeof(CUSTOMVERTEX2));
	g_pVB_home_NB_sleeve_right->Unlock();

	// shirt_outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shirt_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_home_shirt_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_shirt_outline->Lock(0, sizeof(g_2Dkit_home_shirt_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_shirt_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shirt_outline, sizeof(g_2Dkit_home_shirt_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_home_shirt_outline, sizeof(g_2Dkit_home_shirt_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_shirt_outline->Unlock();

	// sleeve_left_outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_sleeve_left_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_home_sleeve_left_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_sleeve_left_outline->Lock(0, sizeof(g_2Dkit_home_sleeve_left_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_sleeve_left_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_sleeve_left_outline, sizeof(g_2Dkit_home_sleeve_left_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_home_sleeve_left_outline, sizeof(g_2Dkit_home_sleeve_left_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_sleeve_left_outline->Unlock();

	// sleeve_right_outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_sleeve_right_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_home_sleeve_right_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_sleeve_right_outline->Lock(0, sizeof(g_2Dkit_home_sleeve_right_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_sleeve_right_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_sleeve_right_outline, sizeof(g_2Dkit_home_sleeve_right_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_home_sleeve_right_outline, sizeof(g_2Dkit_home_sleeve_right_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_sleeve_right_outline->Unlock();

    // SHORTS
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shorts), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_shorts)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_shorts->Lock(0, sizeof(g_2Dkit_home_shorts), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_shorts->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shorts, sizeof(g_2Dkit_home_shorts));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_shorts, sizeof(g_2Dkit_home_shorts)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_shorts->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shorts_2), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_shorts_2)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_shorts_2->Lock(0, sizeof(g_2Dkit_home_shorts_2), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_shorts_2->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shorts_2, sizeof(g_2Dkit_home_shorts_2));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_shorts_2, sizeof(g_2Dkit_home_shorts_2)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_shorts_2->Unlock();

    // shorts outlines
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shorts_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_home_shorts_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_shorts_outline->Lock(0, sizeof(g_2Dkit_home_shorts_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_shorts_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shorts_outline, sizeof(g_2Dkit_home_shorts_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_home_shorts_outline, sizeof(g_2Dkit_home_shorts_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_shorts_outline->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_shorts_2_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_home_shorts_2_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_shorts_2_outline->Lock(0, sizeof(g_2Dkit_home_shorts_2_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_shorts_2_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_shorts_2_outline, sizeof(g_2Dkit_home_shorts_2_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_home_shorts_2_outline, sizeof(g_2Dkit_home_shorts_2_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_shorts_2_outline->Unlock();

    // SOCKS
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_socks_shin_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_socks_shin_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_socks_shin_left->Lock(0, sizeof(g_2Dkit_home_socks_shin_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_socks_shin_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_socks_shin_left, sizeof(g_2Dkit_home_socks_shin_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_socks_shin_left, sizeof(g_2Dkit_home_socks_shin_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_socks_shin_left->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_socks_foot_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_socks_foot_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_socks_foot_left->Lock(0, sizeof(g_2Dkit_home_socks_foot_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_socks_foot_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_socks_foot_left, sizeof(g_2Dkit_home_socks_foot_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_socks_foot_left, sizeof(g_2Dkit_home_socks_foot_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_socks_foot_left->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_socks_left_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_home_socks_left_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_socks_left_outline->Lock(0, sizeof(g_2Dkit_home_socks_left_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_socks_left_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_socks_left_outline, sizeof(g_2Dkit_home_socks_left_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_home_socks_left_outline, sizeof(g_2Dkit_home_socks_left_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT, Y_2DKIT_LEFT);
	g_pVB_home_socks_left_outline->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_socks_shin_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_socks_shin_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_socks_shin_right->Lock(0, sizeof(g_2Dkit_home_socks_shin_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_socks_shin_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_socks_shin_right, sizeof(g_2Dkit_home_socks_shin_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_socks_shin_right, sizeof(g_2Dkit_home_socks_shin_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + SOCK_SHIFT, Y_2DKIT_LEFT);
	g_pVB_home_socks_shin_right->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_socks_foot_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_home_socks_foot_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_socks_foot_right->Lock(0, sizeof(g_2Dkit_home_socks_foot_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_socks_foot_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_socks_foot_right, sizeof(g_2Dkit_home_socks_foot_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_home_socks_foot_right, sizeof(g_2Dkit_home_socks_foot_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + SOCK_SHIFT, Y_2DKIT_LEFT);
	g_pVB_home_socks_foot_right->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_home_socks_right_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_home_socks_right_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_home_socks_right_outline->Lock(0, sizeof(g_2Dkit_home_socks_right_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_home_socks_right_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_home_socks_right_outline, sizeof(g_2Dkit_home_socks_right_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_home_socks_right_outline, sizeof(g_2Dkit_home_socks_right_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT + SOCK_SHIFT, Y_2DKIT_LEFT);
	g_pVB_home_socks_right_outline->Unlock();

    ///// AWAY /////////////////////////////////

	// shirt-left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shirt_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_shirt_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_shirt_left->Lock(0, sizeof(g_2Dkit_away_shirt_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_shirt_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shirt_left, sizeof(g_2Dkit_away_shirt_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_shirt_left, sizeof(g_2Dkit_away_shirt_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_shirt_left->Unlock();

	// shirt-right
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shirt_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_shirt_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_shirt_right->Lock(0, sizeof(g_2Dkit_away_shirt_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_shirt_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shirt_right, sizeof(g_2Dkit_away_shirt_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_shirt_right, sizeof(g_2Dkit_away_shirt_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_shirt_right->Unlock();

	// NB shirt-left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shirt_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_NB_shirt_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_NB_shirt_left->Lock(0, sizeof(g_2Dkit_away_shirt_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_NB_shirt_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shirt_left, sizeof(g_2Dkit_away_shirt_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_shirt_left, sizeof(g_2Dkit_away_shirt_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
    NarrowBackCorrection((CUSTOMVERTEX2*)pVertices, g_narrow_back_shirt_left, sizeof(g_narrow_back_shirt_left)/sizeof(CUSTOMVERTEX2));
	g_pVB_away_NB_shirt_left->Unlock();

	// NB shirt-right
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shirt_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_NB_shirt_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_NB_shirt_right->Lock(0, sizeof(g_2Dkit_away_shirt_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_NB_shirt_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shirt_right, sizeof(g_2Dkit_away_shirt_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_shirt_right, sizeof(g_2Dkit_away_shirt_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
    NarrowBackCorrection((CUSTOMVERTEX2*)pVertices, g_narrow_back_shirt_right, sizeof(g_narrow_back_shirt_right)/sizeof(CUSTOMVERTEX2));
	g_pVB_away_NB_shirt_right->Unlock();

	// sleeve_left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_sleeve_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_sleeve_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_sleeve_left->Lock(0, sizeof(g_2Dkit_away_sleeve_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_sleeve_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_sleeve_left, sizeof(g_2Dkit_away_sleeve_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_sleeve_left, sizeof(g_2Dkit_away_sleeve_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_sleeve_left->Unlock();

	// sleeve_right
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_sleeve_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_sleeve_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_sleeve_right->Lock(0, sizeof(g_2Dkit_away_sleeve_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_sleeve_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_sleeve_right, sizeof(g_2Dkit_away_sleeve_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_sleeve_right, sizeof(g_2Dkit_away_sleeve_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_sleeve_right->Unlock();

	// NB sleeve_left
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_sleeve_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_NB_sleeve_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_NB_sleeve_left->Lock(0, sizeof(g_2Dkit_away_sleeve_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_NB_sleeve_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_sleeve_left, sizeof(g_2Dkit_away_sleeve_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_sleeve_left, sizeof(g_2Dkit_away_sleeve_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
    NarrowBackCorrection((CUSTOMVERTEX2*)pVertices, g_narrow_back_sleeve_left, sizeof(g_narrow_back_sleeve_left)/sizeof(CUSTOMVERTEX2));
	g_pVB_away_NB_sleeve_left->Unlock();

	// NB sleeve_right
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_sleeve_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_NB_sleeve_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_NB_sleeve_right->Lock(0, sizeof(g_2Dkit_away_sleeve_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_NB_sleeve_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_sleeve_right, sizeof(g_2Dkit_away_sleeve_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_sleeve_right, sizeof(g_2Dkit_away_sleeve_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
    NarrowBackCorrection((CUSTOMVERTEX2*)pVertices, g_narrow_back_sleeve_right, sizeof(g_narrow_back_sleeve_right)/sizeof(CUSTOMVERTEX2));
	g_pVB_away_NB_sleeve_right->Unlock();

	// shirt_outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shirt_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_away_shirt_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_shirt_outline->Lock(0, sizeof(g_2Dkit_away_shirt_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_shirt_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shirt_outline, sizeof(g_2Dkit_away_shirt_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_away_shirt_outline, sizeof(g_2Dkit_away_shirt_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_shirt_outline->Unlock();

	// sleeve_left_outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_sleeve_left_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_away_sleeve_left_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_sleeve_left_outline->Lock(0, sizeof(g_2Dkit_away_sleeve_left_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_sleeve_left_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_sleeve_left_outline, sizeof(g_2Dkit_away_sleeve_left_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_away_sleeve_left_outline, sizeof(g_2Dkit_away_sleeve_left_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_sleeve_left_outline->Unlock();

	// sleeve_right_outline
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_sleeve_right_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_away_sleeve_right_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_sleeve_right_outline->Lock(0, sizeof(g_2Dkit_away_sleeve_right_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_sleeve_right_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_sleeve_right_outline, sizeof(g_2Dkit_away_sleeve_right_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_away_sleeve_right_outline, sizeof(g_2Dkit_away_sleeve_right_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_sleeve_right_outline->Unlock();

    // SHORTS
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shorts), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_shorts)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_shorts->Lock(0, sizeof(g_2Dkit_away_shorts), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_shorts->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shorts, sizeof(g_2Dkit_away_shorts));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_shorts, sizeof(g_2Dkit_away_shorts)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_shorts->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shorts_2), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_shorts_2)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_shorts_2->Lock(0, sizeof(g_2Dkit_away_shorts_2), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_shorts_2->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shorts_2, sizeof(g_2Dkit_away_shorts_2));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_shorts_2, sizeof(g_2Dkit_away_shorts_2)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_shorts_2->Unlock();

    // shorts outlines
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shorts_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_away_shorts_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_shorts_outline->Lock(0, sizeof(g_2Dkit_away_shorts_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_shorts_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shorts_outline, sizeof(g_2Dkit_away_shorts_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_away_shorts_outline, sizeof(g_2Dkit_away_shorts_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_shorts_outline->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_shorts_2_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_away_shorts_2_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_shorts_2_outline->Lock(0, sizeof(g_2Dkit_away_shorts_2_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_shorts_2_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_shorts_2_outline, sizeof(g_2Dkit_away_shorts_2_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_away_shorts_2_outline, sizeof(g_2Dkit_away_shorts_2_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_shorts_2_outline->Unlock();

    // SOCKS
	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_socks_shin_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_socks_shin_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_socks_shin_left->Lock(0, sizeof(g_2Dkit_away_socks_shin_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_socks_shin_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_socks_shin_left, sizeof(g_2Dkit_away_socks_shin_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_socks_shin_left, sizeof(g_2Dkit_away_socks_shin_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_socks_shin_left->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_socks_foot_left), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_socks_foot_left)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_socks_foot_left->Lock(0, sizeof(g_2Dkit_away_socks_foot_left), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_socks_foot_left->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_socks_foot_left, sizeof(g_2Dkit_away_socks_foot_left));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_socks_foot_left, sizeof(g_2Dkit_away_socks_foot_left)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_socks_foot_left->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_socks_left_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_away_socks_left_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_socks_left_outline->Lock(0, sizeof(g_2Dkit_away_socks_left_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_socks_left_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_socks_left_outline, sizeof(g_2Dkit_away_socks_left_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_away_socks_left_outline, sizeof(g_2Dkit_away_socks_left_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_socks_left_outline->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_socks_shin_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_socks_shin_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_socks_shin_right->Lock(0, sizeof(g_2Dkit_away_socks_shin_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_socks_shin_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_socks_shin_right, sizeof(g_2Dkit_away_socks_shin_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_socks_shin_right, sizeof(g_2Dkit_away_socks_shin_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + SOCK_SHIFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_socks_shin_right->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_socks_foot_right), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX2, D3DPOOL_MANAGED, &g_pVB_away_socks_foot_right)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_socks_foot_right->Lock(0, sizeof(g_2Dkit_away_socks_foot_right), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_socks_foot_right->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_socks_foot_right, sizeof(g_2Dkit_away_socks_foot_right));
	SetPosition((CUSTOMVERTEX2*)pVertices, g_2Dkit_away_socks_foot_right, sizeof(g_2Dkit_away_socks_foot_right)/sizeof(CUSTOMVERTEX2), X_2DKIT_LEFT + SOCK_SHIFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_socks_foot_right->Unlock();

	if (FAILED(dev->CreateVertexBuffer(sizeof(g_2Dkit_away_socks_right_outline), D3DUSAGE_WRITEONLY, 
					D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &g_pVB_away_socks_right_outline)))
	{
		Log(&k_mydll,"CreateVertexBuffer() failed.");
		return E_FAIL;
	}
	Log(&k_mydll,"CreateVertexBuffer() done.");

	if (FAILED(g_pVB_away_socks_right_outline->Lock(0, sizeof(g_2Dkit_away_socks_right_outline), (BYTE**)&pVertices, 0)))
	{
		Log(&k_mydll,"g_pVB_away_socks_right_outline->Lock() failed.");
		return E_FAIL;
	}
	memcpy(pVertices, g_2Dkit_away_socks_right_outline, sizeof(g_2Dkit_away_socks_right_outline));
	SetPosition((CUSTOMVERTEX*)pVertices, g_2Dkit_away_socks_right_outline, sizeof(g_2Dkit_away_socks_right_outline)/sizeof(CUSTOMVERTEX), X_2DKIT_LEFT + SOCK_SHIFT + AWAY_KIT_SHIFT, Y_2DKIT_LEFT);
	g_pVB_away_socks_right_outline->Unlock();


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
		Log(&k_mydll,"InitVB() failed.");
        return hr;
    }
	Log(&k_mydll,"InitVB() done.");

	// Create the state blocks for rendering overlay graphics
	for( UINT which=0; which<2; which++ )
	{
		dev->BeginStateBlock();

        dev->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        dev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
        dev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        dev->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
        dev->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture( 0, g_home_shirt_tex );

        // Set diffuse blending for alpha set in vertices.
        dev->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );
        
		if( which==0 )
			dev->EndStateBlock( &g_dwSavedStateBlock );
		else
			dev->EndStateBlock( &g_dwDrawOverlayStateBlock );
	}

	g_needsRestore = FALSE;
    return S_OK;
}

void DeleteStateBlocks(IDirect3DDevice8* dev)
{
	// Delete the state blocks
	try
	{
        //LogWithNumber(&k_mydll,"dev = %08x", (DWORD)dev);
        DWORD* vtab = (DWORD*)(*(DWORD*)dev);
        //LogWithNumber(&k_mydll,"vtab = %08x", (DWORD)vtab);
        if (vtab && vtab[VTAB_DELETESTATEBLOCK]) {
            //LogWithNumber(&k_mydll,"vtab[VTAB_DELETESTATEBLOCK] = %08x", (DWORD)vtab[VTAB_DELETESTATEBLOCK]);
            if (g_dwSavedStateBlock) {
                dev->DeleteStateBlock( g_dwSavedStateBlock );
                Log(&k_mydll,"g_dwSavedStateBlock deleted.");
            }
            if (g_dwDrawOverlayStateBlock) {
                dev->DeleteStateBlock( g_dwDrawOverlayStateBlock );
                Log(&k_mydll,"g_dwDrawOverlayStateBlock deleted.");
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
	TRACE(&k_mydll,"InvalidateDeviceObjects called.");
	if (dev == NULL)
	{
		TRACE(&k_mydll,"InvalidateDeviceObjects: nothing to invalidate.");
		return S_OK;
	}

    // home
	SafeRelease( &g_pVB_home_shirt_left );
	SafeRelease( &g_pVB_home_NB_shirt_left );
	SafeRelease( &g_pVB_home_sleeve_left );
	SafeRelease( &g_pVB_home_NB_sleeve_left );
	SafeRelease( &g_pVB_home_shirt_right );
	SafeRelease( &g_pVB_home_NB_shirt_right );
	SafeRelease( &g_pVB_home_sleeve_right );
	SafeRelease( &g_pVB_home_NB_sleeve_right );
	SafeRelease( &g_pVB_home_shirt_outline );
	SafeRelease( &g_pVB_home_sleeve_left_outline );
	SafeRelease( &g_pVB_home_sleeve_right_outline );
	SafeRelease( &g_pVB_home_shorts );
	SafeRelease( &g_pVB_home_shorts_2 );
	SafeRelease( &g_pVB_home_shorts_outline );
	SafeRelease( &g_pVB_home_shorts_2_outline );
	SafeRelease( &g_pVB_home_socks_shin_left );
	SafeRelease( &g_pVB_home_socks_foot_left );
	SafeRelease( &g_pVB_home_socks_left_outline );
	SafeRelease( &g_pVB_home_socks_shin_right );
	SafeRelease( &g_pVB_home_socks_foot_right );
	SafeRelease( &g_pVB_home_socks_right_outline );

    // away
	SafeRelease( &g_pVB_away_shirt_left );
	SafeRelease( &g_pVB_away_NB_shirt_left );
	SafeRelease( &g_pVB_away_sleeve_left );
	SafeRelease( &g_pVB_away_NB_sleeve_left );
	SafeRelease( &g_pVB_away_shirt_right );
	SafeRelease( &g_pVB_away_NB_shirt_right );
	SafeRelease( &g_pVB_away_sleeve_right );
	SafeRelease( &g_pVB_away_NB_sleeve_right );
	SafeRelease( &g_pVB_away_shirt_outline );
	SafeRelease( &g_pVB_away_sleeve_left_outline );
	SafeRelease( &g_pVB_away_sleeve_right_outline );
	SafeRelease( &g_pVB_away_shorts );
	SafeRelease( &g_pVB_away_shorts_2 );
	SafeRelease( &g_pVB_away_shorts_outline );
	SafeRelease( &g_pVB_away_shorts_2_outline );
	SafeRelease( &g_pVB_away_socks_shin_left );
	SafeRelease( &g_pVB_away_socks_foot_left );
	SafeRelease( &g_pVB_away_socks_left_outline );
	SafeRelease( &g_pVB_away_socks_shin_right );
	SafeRelease( &g_pVB_away_socks_foot_right );
	SafeRelease( &g_pVB_away_socks_right_outline );

    // gloves indicator
	SafeRelease( &g_pVB_gloves_left );

	Log(&k_mydll,"InvalidateDeviceObjects: SafeRelease(s) done.");

    DeleteStateBlocks(dev);
    Log(&k_mydll,"InvalidateDeviceObjects: DeleteStateBlock(s) done.");

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

/**
 * Reset kit keys back to original position, and reset iterators.
 */
void ResetKitKeys()
{
    Log(&k_mydll,"ResetKitKeys: CALLED.");

    BYTE homeStrip = GetTeamStrips(HOME);
    BYTE awayStrip = GetTeamStrips(AWAY);

    switch (typ) {
        case PL_TYPE:
            g_homeShirtKeyPL = (homeStrip & STRIP_PL_SHIRT)?"pb":"pa";
            g_homeShortsKeyPL = (homeStrip & STRIP_PL_SHORTS)?"pb":"pa";
            g_homeSocksKeyPL = (homeStrip & STRIP_PL_SOCKS)?"pb":"pa";
            g_awayShirtKeyPL = (awayStrip & STRIP_PL_SHIRT)?"pb":"pa";
            g_awayShortsKeyPL = (awayStrip & STRIP_PL_SHORTS)?"pb":"pa";
            g_awaySocksKeyPL = (awayStrip & STRIP_PL_SOCKS)?"pb":"pa";
            break;

        case GK_TYPE:
            g_homeShirtKeyGK = (homeStrip & STRIP_GK_SHIRT)?"gb":"ga";
            g_homeShortsKeyGK = (homeStrip & STRIP_GK_SHORTS)?"gb":"ga";
            g_homeSocksKeyGK = (homeStrip & STRIP_GK_SOCKS)?"gb":"ga";
            g_awayShirtKeyGK = (awayStrip & STRIP_GK_SHIRT)?"gb":"ga";
            g_awayShortsKeyGK = (awayStrip & STRIP_GK_SHORTS)?"gb":"ga";
            g_awaySocksKeyGK = (awayStrip & STRIP_GK_SOCKS)?"gb":"ga";
            break;
    }
}

/**
 * Reset all kit keys back to original position, and reset iterators.
 */
void ResetKitKeys(BOOL all)
{
    Log(&k_mydll,"ResetKitKeys(BOOL): CALLED.");

    BYTE homeStrip = GetTeamStrips(HOME);
    BYTE awayStrip = GetTeamStrips(AWAY);

    if (typ == PL_TYPE || all) {
        g_homeShirtKeyPL = (homeStrip & STRIP_PL_SHIRT)?"pb":"pa";
        g_homeShortsKeyPL = (homeStrip & STRIP_PL_SHORTS)?"pb":"pa";
        g_homeSocksKeyPL = (homeStrip & STRIP_PL_SOCKS)?"pb":"pa";
        g_awayShirtKeyPL = (awayStrip & STRIP_PL_SHIRT)?"pb":"pa";
        g_awayShortsKeyPL = (awayStrip & STRIP_PL_SHORTS)?"pb":"pa";
        g_awaySocksKeyPL = (awayStrip & STRIP_PL_SOCKS)?"pb":"pa";
    } 
    if (typ == GK_TYPE || all) {
        g_homeShirtKeyGK = (homeStrip & STRIP_GK_SHIRT)?"gb":"ga";
        g_homeShortsKeyGK = (homeStrip & STRIP_GK_SHORTS)?"gb":"ga";
        g_homeSocksKeyGK = (homeStrip & STRIP_GK_SOCKS)?"gb":"ga";
        g_awayShirtKeyGK = (awayStrip & STRIP_GK_SHIRT)?"gb":"ga";
        g_awayShortsKeyGK = (awayStrip & STRIP_GK_SHORTS)?"gb":"ga";
        g_awaySocksKeyGK = (awayStrip & STRIP_GK_SOCKS)?"gb":"ga";
    }
}

/**
 * Clear kit keys back to original position.
 */
void ClearKitKeys()
{
    Log(&k_mydll,"ClearKitKeys: CALLED.");

    g_homeShirtKeyPL = "";
    g_homeShortsKeyPL = "";
    g_homeSocksKeyPL = "";
    g_awayShirtKeyPL = "";
    g_awayShortsKeyPL = "";
    g_awaySocksKeyPL = "";

    g_homeShirtKeyGK = "";
    g_homeShortsKeyGK = "";
    g_homeSocksKeyGK = "";
    g_awayShirtKeyGK = "";
    g_awayShortsKeyGK = "";
    g_awaySocksKeyGK = "";
}

/**
 * Checks if we got back to changing the kits via 1st/2nd strip selection.
 * (as opposed to find-tuning the kits)
 */
BOOL BackToStandardStripSelection(BYTE homeStrip, BYTE awayStrip)
{
    //LogWithTwoNumbers(&k_mydll,"BackToStandard: home=%02x, g_home=%02x", homeStrip, g_homeStrip);
    //LogWithTwoNumbers(&k_mydll,"BackToStandard: away=%02x, g_away=%02x", awayStrip, g_awayStrip);
    return 
        ((homeStrip & STRIP_PL_SHIRT)!=(g_homeStrip & STRIP_PL_SHIRT) &&
        (homeStrip & STRIP_PL_SHORTS)!=(g_homeStrip & STRIP_PL_SHORTS) &&
        (homeStrip & STRIP_PL_SOCKS)!=(g_homeStrip & STRIP_PL_SOCKS)) 
        ||
        ((awayStrip & STRIP_PL_SHIRT)!=(g_awayStrip & STRIP_PL_SHIRT) &&
        (awayStrip & STRIP_PL_SHORTS)!=(g_awayStrip & STRIP_PL_SHORTS) &&
        (awayStrip & STRIP_PL_SOCKS)!=(g_awayStrip & STRIP_PL_SOCKS));
}

/**
 * Return currently selected (home or away) team ID.
 */
WORD GetTeamId(int which)
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

/**
 * Return currently selected (home or away) team strips configuration.
 */
BYTE GetTeamStrips(int which)
{
    if (data[TEAM_STRIPS]==0) return 0xff;
    return ((BYTE*)data[TEAM_STRIPS])[which];
}

/**
 * Load a texture from a specific kit folder.
 */
void Load2DkitTexture(WORD teamId, const char* kitFolder, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal) 
{
	MAKE_BUFFER(folder);
	KitCollection* col = MAP_FIND(gdb->uni,teamId);
	sprintf(folder, "uni\\%s\\%s", (col)?col->foldername:"(null)", kitFolder);
	
	MAKE_BUFFER(name);
	sprintf(name, "%sGDB\\%s\\%s", GetPESInfo()->gdbDir, folder, filename);

    MAKE_BUFFER(overlayfilename);
    bool loadOverlay(false);
    if (col && lstrcmpi(filename, "shirt.png")==0) {
        if (kitFolder[0]=='p') {
            StringKitMap::iterator it = col->players->find(kitFolder);
            if (it != col->players->end()) {
                if (it->second->overlayFile && it->second->overlayFile[0]!='\0') {
                    sprintf(overlayfilename, "%sGDB\\%s\\%s", GetPESInfo()->gdbDir, folder, it->second->overlayFile);
                    loadOverlay = true;
                }
            }
        }
        else if (kitFolder[0]=='g') {
            StringKitMap::iterator it = col->goalkeepers->find(kitFolder);
            if (it != col->goalkeepers->end()) {
                if (it->second->overlayFile && it->second->overlayFile[0]!='\0') {
                    sprintf(overlayfilename, "%sGDB\\%s\\%s", GetPESInfo()->gdbDir, folder, it->second->overlayFile);
                    loadOverlay = true;
                }
            }
        }
    }

	if (lstrcmpi(name + lstrlen(name)-4, ".png")==0) {
        if (!FileExists(name))
            lstrcpy(name + lstrlen(name)-4, ".bmp");
		else
			goto FileFound;

    } else if (lstrcmpi(name + lstrlen(name)-4, ".bmp")==0) {
        if (!FileExists(name))
            lstrcpy(name + lstrlen(name)-4, ".png");
		else
			goto FileFound;
    }
	if (!FileExists(name)) {
		CLEAR_BUFFER(name);
        sprintf(name, "%sGDB\\%s\\all.png", GetPESInfo()->gdbDir, folder);
        if (lstrcmpi(name + lstrlen(name)-4, ".png")==0) {
        if (!FileExists(name))
            lstrcpy(name + lstrlen(name)-4, ".bmp");
		else
			goto FileFound;

	    } else if (lstrcmpi(name + lstrlen(name)-4, ".bmp")==0) {
	        if (!FileExists(name))
	            lstrcpy(name + lstrlen(name)-4, ".png");
			else
				goto FileFound;
	    }
	} else
		goto FileFound;
		
	if (!FileExists(name)) {
		Log(&k_mydll,"Load2DkitTexture: Finding right file FAILED.");
		*ppTex = NULL;
		return;
	};
    
    FileFound:
	MAKE_BUFFER(keyBuf);
    sprintf(keyBuf, "%d|%s", teamId, name);
    string key = keyBuf;
	//LogWithString(&k_mydll,"keyBuf = %s", keyBuf);
    //LogWithNumber(&k_mydll,"g_kitTextureMap[key] = %08x", (DWORD)g_kitTextureMap[key]);
    
    if (!g_kitTextureMap[key]) {
        CreateGDBTextureFromFolder(folder, filename, ppTex, pPal);
        if (*ppTex) {
            // store texture in the texture cache
            g_kitTextureMap[key] = *ppTex;
            // store palette in the palette cache
            g_kitPaletteMap[key] = MakePaletteCopy(pPal);

            // apply overlay to shirt
            if (!g_config.disableOverlays && loadOverlay) {
                D3DLOCKED_RECT rect;
	            D3DSURFACE_DESC desc;
                if (SUCCEEDED((*ppTex)->LockRect(0, &rect, NULL, 0))) {
                    (*ppTex)->GetLevelDesc(0, &desc);
                    ApplyOverlay(&rect, overlayfilename, &desc);
                    (*ppTex)->UnlockRect(0);
                    TRACE4(&k_mydll,"2D kit: overlay applied: %s",overlayfilename);
                }
            }
        }
    } else {
        // texture already loaded: get from cache
        *ppTex = g_kitTextureMap[key];
        // copy palette
        memcpy(pPal, g_kitPaletteMap[key], 0x100*sizeof(PALETTEENTRY));
    }
}

void PrintHomeKitCollection()
{
    KitCollection* col = MAP_FIND(gdb->uni,GetTeamId(HOME));
    if (col) {
        for (StringKitMap::iterator it = (*(col->players)).begin(); 
                it != (*(col->players)).end(); it++) {
            string key = it->first;
            LogWithString(&k_mydll,"home kit collection key: {%s}", (char*)key.c_str());
        }
    }
}

void PrintAwayKitCollection()
{
    KitCollection* col = MAP_FIND(gdb->uni,GetTeamId(AWAY));
    if (col) {
        for (StringKitMap::iterator it = (*(col->players)).begin(); 
                it != (*(col->players)).end(); it++) {
            string key = it->first;
            LogWithString(&k_mydll,"away kit collection key: {%s}", (char*)key.c_str());
        }
    }
}

void TryLoad2DkitTexture(WORD teamId, string oldKey, string& key, CYCLEPROC proc, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal)
{
    // check for "(null)"
    if (key == "(null)") {
        return; // no textures available
    }
    // make sure we try all existing textures
    Load2DkitTexture(teamId, key.c_str(), filename, ppTex, pPal);
    string seenKey = key;
    while (!*ppTex) {
        key = proc(teamId, oldKey);
        Load2DkitTexture(teamId, key.c_str(), filename, ppTex, pPal);
        if (key == seenKey) break; // full cycle

    }
}

void TryLoadSamePal2DkitTexture(WORD teamId, string oldKey, string& key, CYCLEPROC proc, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal, PALETTEENTRY* cmpPal)
{
    // check for "(null)"
    if (key == "(null)") {
        return; // no textures available
    }
    // make sure we try all existing textures
    key = proc(teamId, oldKey);
    Load2DkitTexture(teamId, key.c_str(), filename, ppTex, pPal);
    string seenKey = key;
    while (!*ppTex || !IsSamePalette(pPal, cmpPal)) {
        key = proc(teamId, oldKey);
        Load2DkitTexture(teamId, key.c_str(), filename, ppTex, pPal);
        if (key == seenKey) break; // full cycle

    }
}

BOOL IsSamePalette(PALETTEENTRY* a, PALETTEENTRY* b)
{
    return memcmp(a, b, 0x100*sizeof(PALETTEENTRY))==0;
}

PALETTEENTRY* MakePaletteCopy(PALETTEENTRY* src)
{
    PALETTEENTRY* dest = (PALETTEENTRY*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x100*sizeof(PALETTEENTRY));
    if (dest) {
        memcpy(dest, src, 0x100*sizeof(PALETTEENTRY));
        return dest;
    }
    return NULL;
}

void PrintPal(PALETTEENTRY* pal)
{
    FILE* f = fopen("c:\\pal.log","ab");
    for (int i=0; i<0x100; i++) {
        fprintf(f,"%c%c%c%c",pal[i].peRed,pal[i].peGreen,pal[i].peBlue,pal[i].peFlags);
    }
    fclose(f);
}

#define CHANGED_SHIRT(oldS, newS) ((oldS & STRIP_PL_SHIRT)!=(newS & STRIP_PL_SHIRT))
#define CHANGED_SHORTS(oldS, newS) ((oldS & STRIP_PL_SHORTS)!=(newS & STRIP_PL_SHORTS))
#define CHANGED_SOCKS(oldS, newS) ((oldS & STRIP_PL_SOCKS)!=(newS & STRIP_PL_SOCKS))
#define UNDEFINED(s1, s2) ((s1 == 0xff && s2 == 0xff))

/**
 * Load the needed textures from GDB
 */
void Load2Dkits()
{
    // Get current strip selection
    BYTE homeStrip = GetTeamStrips(HOME);
    BYTE awayStrip = GetTeamStrips(AWAY);

    // safety check for early exit
    if (UNDEFINED(homeStrip, awayStrip)) {
        return;
    }

    // initialize g_homeStrip and g_awayStrip, if this is a new session
    if (UNDEFINED(g_homeStrip, g_awayStrip)) { 
        g_homeStrip = homeStrip;
        g_awayStrip = awayStrip;
        ResetKitKeys();
    }

    // check if we got back to "1st/2nd" selection mode
    BOOL needsReset = BackToStandardStripSelection(homeStrip, awayStrip);

    // Compare with previously saved last strip selection
    if (g_lastDisplay2Dkits != g_display2Dkits || homeStrip != g_homeStrip || needsReset || g_modeSwitch) {
        // home strip selection changed
        WORD id = GetTeamId(HOME);

        // reset kit keys, if needed
        if (needsReset || g_lastDisplay2Dkits != g_display2Dkits) {
            ResetKitKeys();
        } else if (TeamDirExists(id)) {
            if (CHANGED_SHIRT(g_homeStrip, homeStrip) &&
                    !(CHANGED_SHORTS(g_homeStrip, homeStrip)) &&
                    !(CHANGED_SOCKS(g_homeStrip, homeStrip))) {
                // cycle home shirt
                string oldKey = GET_HOME_SHIRT_KEY(typ);
                GET_HOME_SHIRT_KEY(typ) = GetNextHomeShirtKey(id, oldKey);
                LogWithString(&k_mydll,"New home shirt: {%s}", (char*)GET_HOME_SHIRT_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_HOME_SHIRT_KEY(typ), (CYCLEPROC)GetNextHomeShirtKey, "shirt.png", &g_home_shirt_tex, g_home_shirt_pal);
            }
            else if (CHANGED_SHORTS(g_homeStrip, homeStrip) &&
                    !(CHANGED_SHIRT(g_homeStrip, homeStrip)) &&
                    !(CHANGED_SOCKS(g_homeStrip, homeStrip))) {
                // cycle home shorts
                string oldKey = GET_HOME_SHORTS_KEY(typ);
                GET_HOME_SHORTS_KEY(typ) = GetNextHomeShortsKey(id, oldKey);
                LogWithString(&k_mydll,"New home shorts: {%s}", (char*)GET_HOME_SHORTS_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_HOME_SHORTS_KEY(typ), (CYCLEPROC)GetNextHomeShortsKey, "shorts.png", &g_home_shorts_tex, g_home_shorts_pal);
            }
            else if (CHANGED_SOCKS(g_homeStrip, homeStrip) &&
                    !(CHANGED_SHIRT(g_homeStrip, homeStrip)) &&
                    !(CHANGED_SHORTS(g_homeStrip, homeStrip))) {
                // cycle home socks
                string oldKey = GET_HOME_SOCKS_KEY(typ);
                GET_HOME_SOCKS_KEY(typ) = GetNextHomeSocksKey(id, oldKey);
                LogWithString(&k_mydll,"New home socks: {%s}", (char*)GET_HOME_SOCKS_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_HOME_SOCKS_KEY(typ), (CYCLEPROC)GetNextHomeSocksKey, "socks.png", &g_home_socks_tex, g_home_socks_pal);
            }
        }

        Load2DkitTexture(id, GET_HOME_SHIRT_KEY(typ).c_str(), "shirt.png", &g_home_shirt_tex, g_home_shirt_pal);
        Load2DkitTexture(id, GET_HOME_SHORTS_KEY(typ).c_str(), "shorts.png", &g_home_shorts_tex, g_home_shorts_pal);
        Load2DkitTexture(id, GET_HOME_SOCKS_KEY(typ).c_str(), "socks.png", &g_home_socks_tex, g_home_socks_pal);

        // update home strip
        g_homeStrip = homeStrip;
    }

    if (g_lastDisplay2Dkits != g_display2Dkits || awayStrip != g_awayStrip || needsReset || g_modeSwitch) {
        // away strip selection chagned
        WORD id = GetTeamId(AWAY);

        // reset kit keys, if needed
        if (needsReset || g_lastDisplay2Dkits != g_display2Dkits) {
            ResetKitKeys();
        } else if (TeamDirExists(id)) {
            if (CHANGED_SHIRT(g_awayStrip, awayStrip) &&
                    !(CHANGED_SHORTS(g_awayStrip, awayStrip)) &&
                    !(CHANGED_SOCKS(g_awayStrip, awayStrip))) {
                // cycle away shirt
                string oldKey = GET_AWAY_SHIRT_KEY(typ);
                GET_AWAY_SHIRT_KEY(typ) = GetNextAwayShirtKey(id, oldKey);
                LogWithString(&k_mydll,"New away shirt: {%s}", (char*)GET_AWAY_SHIRT_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_AWAY_SHIRT_KEY(typ), (CYCLEPROC)GetNextAwayShirtKey, "shirt.png", &g_away_shirt_tex, g_away_shirt_pal);
            }
            else if (CHANGED_SHORTS(g_awayStrip, awayStrip) &&
                    !(CHANGED_SHIRT(g_awayStrip, awayStrip)) &&
                    !(CHANGED_SOCKS(g_awayStrip, awayStrip))) {
                // cycle away shorts
                string oldKey = GET_AWAY_SHORTS_KEY(typ);
                GET_AWAY_SHORTS_KEY(typ) = GetNextAwayShortsKey(id, oldKey);
                LogWithString(&k_mydll,"New away shorts: {%s}", (char*)GET_AWAY_SHORTS_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_AWAY_SHORTS_KEY(typ), (CYCLEPROC)GetNextAwayShortsKey, "shorts.png", &g_away_shorts_tex, g_away_shorts_pal);
            }
            else if (CHANGED_SOCKS(g_awayStrip, awayStrip) &&
                    !(CHANGED_SHIRT(g_awayStrip, awayStrip)) &&
                    !(CHANGED_SHORTS(g_awayStrip, awayStrip))) {
                // cycle away socks
                string oldKey = GET_AWAY_SOCKS_KEY(typ);
                GET_AWAY_SOCKS_KEY(typ) = GetNextAwaySocksKey(id, oldKey);
                LogWithString(&k_mydll,"New away socks: {%s}", (char*)GET_AWAY_SOCKS_KEY(typ).c_str());

                TryLoad2DkitTexture(id, oldKey, GET_AWAY_SOCKS_KEY(typ), (CYCLEPROC)GetNextAwaySocksKey, "socks.png", &g_away_socks_tex, g_away_socks_pal);
            }
        }

        Load2DkitTexture(id, GET_AWAY_SHIRT_KEY(typ).c_str(), "shirt.png", &g_away_shirt_tex, g_away_shirt_pal);
        Load2DkitTexture(id, GET_AWAY_SHORTS_KEY(typ).c_str(), "shorts.png", &g_away_shorts_tex, g_away_shorts_pal);
        Load2DkitTexture(id, GET_AWAY_SOCKS_KEY(typ).c_str(), "socks.png", &g_away_socks_tex, g_away_socks_pal);

        // update away strip
        g_awayStrip = awayStrip;
        g_modeSwitch = FALSE;
    }
}

BOOL IsNarrowBackModel(BYTE model)
{
    if (model>=0 && model<sizeof(g_config.narrowBackModels)) {
        return g_config.narrowBackModels[model]==1;
    }
    return FALSE;
}

BOOL IsNarrowBack(int which )
{
    string currKey;
    Kit* kit = NULL;
    WORD teamId = GetTeamId(which);
    if (teamId != 0xffff) {
        KitCollection* col = MAP_FIND(gdb->uni,teamId);
        if (!col) return FALSE;
        switch (which) {
            case HOME:
                currKey = GET_HOME_SHIRT_KEY(typ);
                if (typ == GK_TYPE) {
                    kit = MAP_FIND(col->goalkeepers,currKey);
                }
                else {
                    kit = MAP_FIND(col->players,currKey);
                }
                break;
            case AWAY:
                currKey = GET_AWAY_SHIRT_KEY(typ);
                if (typ == GK_TYPE) {
                    kit = MAP_FIND(col->goalkeepers,currKey);
                }
                else {
                    kit = MAP_FIND(col->players,currKey);
                }
                break;
        }
        // check the model
        if (kit && (kit->attDefined & MODEL)) {
            return IsNarrowBackModel(kit->model);
        }
    }
    return FALSE;
}

void RenderHomeShirt(IDirect3DDevice8* dev)
{
    if (g_home_shirt_tex) {
        BOOL isNB = IsNarrowBack(HOME);
        IDirect3DVertexBuffer8* home_sleeve_left = (isNB) ? g_pVB_home_NB_sleeve_left : g_pVB_home_sleeve_left;
        IDirect3DVertexBuffer8* home_sleeve_right = (isNB) ? g_pVB_home_NB_sleeve_right : g_pVB_home_sleeve_right;
        IDirect3DVertexBuffer8* home_shirt_left = (isNB) ? g_pVB_home_NB_shirt_left : g_pVB_home_shirt_left;
        IDirect3DVertexBuffer8* home_shirt_right = (isNB) ? g_pVB_home_NB_shirt_right : g_pVB_home_shirt_right;

        // shirt texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        // shirt
        dev->SetTexture(0, g_home_shirt_tex);
        // left sleeve
        dev->SetStreamSource( 0, home_sleeve_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        // right sleeve
        dev->SetStreamSource( 0, home_sleeve_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

        // sleeve outlines
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_home_sleeve_left_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4);
        dev->SetStreamSource( 0, g_pVB_home_sleeve_right_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4);

        // shirt texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        // shirt-left
        dev->SetTexture(0, g_home_shirt_tex);
        dev->SetStreamSource( 0, home_shirt_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 4);
        // shirt-right
        dev->SetStreamSource( 0, home_shirt_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 4);

        // shirt outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_home_shirt_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 7);
    }
}

void RenderAwayShirt(IDirect3DDevice8* dev)
{
    if (g_away_shirt_tex) {
        BOOL isNB = IsNarrowBack(AWAY);
        IDirect3DVertexBuffer8* away_sleeve_left = (isNB) ? g_pVB_away_NB_sleeve_left : g_pVB_away_sleeve_left;
        IDirect3DVertexBuffer8* away_sleeve_right = (isNB) ? g_pVB_away_NB_sleeve_right : g_pVB_away_sleeve_right;
        IDirect3DVertexBuffer8* away_shirt_left = (isNB) ? g_pVB_away_NB_shirt_left : g_pVB_away_shirt_left;
        IDirect3DVertexBuffer8* away_shirt_right = (isNB) ? g_pVB_away_NB_shirt_right : g_pVB_away_shirt_right;

        // shirt texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        // shirt
        dev->SetTexture(0, g_away_shirt_tex);
        // left sleeve
        dev->SetStreamSource( 0, away_sleeve_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        // right sleeve
        dev->SetStreamSource( 0, away_sleeve_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

        // sleeve outlines
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_away_sleeve_left_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4);
        dev->SetStreamSource( 0, g_pVB_away_sleeve_right_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4);

        // shirt texture
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        // shirt-left
        dev->SetTexture(0, g_away_shirt_tex);
        dev->SetStreamSource( 0, away_shirt_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 4);
        // shirt-right
        dev->SetStreamSource( 0, away_shirt_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 4);
        
        // shirt outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_away_shirt_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 7);
    }
}

void Draw2Dkits(IDirect3DDevice8* dev)
{
	if (g_needsRestore) 
	{
		if (FAILED(RestoreDeviceObjects(dev)))
		{
			Log(&k_mydll,"Draw2Dkits: RestoreDeviceObjects() failed.");
            return;
		}
		Log(&k_mydll,"Draw2Dkits: RestoreDeviceObjects() done.");
	}

    // load textures from GDB (if needed)
    Load2Dkits();

	// render
	dev->BeginScene();

	// setup renderstate
	dev->CaptureStateBlock( g_dwSavedStateBlock );
	dev->ApplyStateBlock( g_dwDrawOverlayStateBlock );

    ////////// HOME 2DKIT /////////////////////////////
    
    dev->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE ); 

    RenderHomeShirt(dev);

    if (g_home_shorts_tex) {
        // shorts
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_home_shorts_tex);
        dev->SetStreamSource( 0, g_pVB_home_shorts, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        dev->SetStreamSource( 0, g_pVB_home_shorts_2, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_home_shorts_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4);
        dev->SetStreamSource( 0, g_pVB_home_shorts_2_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4);
    }

    if (g_home_socks_tex) {
        // socks
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_home_socks_tex);
        dev->SetStreamSource( 0, g_pVB_home_socks_shin_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        dev->SetStreamSource( 0, g_pVB_home_socks_foot_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 5);
        dev->SetStreamSource( 0, g_pVB_home_socks_shin_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        dev->SetStreamSource( 0, g_pVB_home_socks_foot_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 5);

        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_home_socks_left_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 9);
        dev->SetStreamSource( 0, g_pVB_home_socks_right_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 9);
    }

    ////////// AWAY 2DKIT /////////////////////////////
    
    RenderAwayShirt(dev);

    if (g_away_shorts_tex) {
        // shorts
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_away_shorts_tex);
        dev->SetStreamSource( 0, g_pVB_away_shorts, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        dev->SetStreamSource( 0, g_pVB_away_shorts_2, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_away_shorts_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4);
        dev->SetStreamSource( 0, g_pVB_away_shorts_2_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 4);
    }

    if (g_away_socks_tex) {
        // socks
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
        dev->SetTexture(0, g_away_socks_tex);
        dev->SetStreamSource( 0, g_pVB_away_socks_shin_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        dev->SetStreamSource( 0, g_pVB_away_socks_foot_left, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 5);
        dev->SetStreamSource( 0, g_pVB_away_socks_shin_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        dev->SetStreamSource( 0, g_pVB_away_socks_foot_right, sizeof(CUSTOMVERTEX2));
        dev->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 5);

        // outline
        dev->SetVertexShader( D3DFVF_CUSTOMVERTEX );
        dev->SetTexture(0, NULL);
        dev->SetStreamSource( 0, g_pVB_away_socks_left_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 9);
        dev->SetStreamSource( 0, g_pVB_away_socks_right_outline, sizeof(CUSTOMVERTEX));
        dev->DrawPrimitive( D3DPT_LINESTRIP, 0, 9);
    }

    // gloves indicators
    if (typ == GK_TYPE) {
        if (!g_gloves_left_tex) {
            if (FAILED(D3DXCreateTextureFromFileEx(dev, 
                            (string(GetPESInfo()->mydir) 
                                + "\\igloves.png").c_str(),
                        0, 0, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                        D3DX_FILTER_NONE, D3DX_FILTER_NONE,
                        0xffffffff, NULL, NULL, &g_gloves_left_tex))) {
                Log(&k_mydll,"FAILED to load image for gloves indicator.");
            }
        }
        if (g_gloves_left_tex) {
            // Set diffuse blending for alpha set in vertices.
            dev->SetRenderState( D3DRS_ALPHABLENDENABLE,   TRUE );

            // left
            dev->SetVertexShader( D3DFVF_CUSTOMVERTEX2 );
            dev->SetTexture(0, g_gloves_left_tex);
            dev->SetStreamSource( 0, g_pVB_gloves_left, sizeof(CUSTOMVERTEX2));
            dev->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
        }
    }

	// restore the modified renderstates
	dev->ApplyStateBlock( g_dwSavedStateBlock );

	dev->EndScene();
}

void BeginDrawKitInfo()
{
	dksiSetMenuTitle("Kit information");
	return;
};

void DrawKitInfo(IDirect3DDevice8* dev)
{
	//Print description
	DWORD color=0xffffffff;
	char tmp[BUFLEN], KitName[BUFLEN];

    KitCollection* hcol = MAP_FIND(gdb->uni,GetTeamId(HOME));
    KitCollection* acol = MAP_FIND(gdb->uni,GetTeamId(AWAY));

	//HOME TEAM
	GetKitName(0,hcol,KitName);
	if (strlen(KitName)>0) {
		sprintf(tmp,"Shirt: %s",KitName);
		KDrawText(24,576,color,16,tmp);
	};
	
	GetKitName(1,hcol,KitName);
	if (strlen(KitName)>0) {
		sprintf(tmp,"Shorts: %s",KitName);
		KDrawText(24,616,color,16,tmp);
	};
	
	GetKitName(2,hcol,KitName);
	if (strlen(KitName)>0) {
		sprintf(tmp,"Socks: %s",KitName);
		KDrawText(24,656,color,16,tmp);
	};
	
	//AWAY TEAM
	GetKitName(3,acol,KitName);
	if (strlen(KitName)>0) {
		sprintf(tmp,"Shirt: %s",KitName);
		KDrawText(536,576,color,16,tmp);
	};
	
	GetKitName(4,acol,KitName);
	if (strlen(KitName)>0) {
		sprintf(tmp,"Shorts: %s",KitName);
		KDrawText(536,616,color,16,tmp);
	};
	
	GetKitName(5,acol,KitName);
	if (strlen(KitName)>0) {
		sprintf(tmp,"Socks: %s",KitName);
		KDrawText(536,656,color,16,tmp);
	};

	return;
};

void GetKitName(BYTE which, KitCollection* col, char* KitName)
{
    if (!col) {
        KitName[0]='\0';
        return;
    }

	string key;
	switch (which) {
        case 0:
            key=GET_HOME_SHIRT_KEY(typ);
            break;
        case 1:
            key=GET_HOME_SHORTS_KEY(typ);
            break;
        case 2:
            key=GET_HOME_SOCKS_KEY(typ);
            break;
        case 3:
            key=GET_AWAY_SHIRT_KEY(typ);
            break;
        case 4:
            key=GET_AWAY_SHORTS_KEY(typ);
            break;
        case 5:
            key=GET_AWAY_SOCKS_KEY(typ);
            break;
	}

    KitIterator ki = GET_KIT_ITER(typ,col)->find(key);
    if (ki != GET_KIT_ITER(typ,col)->end()) {
        if (ki->second->attDefined & KITDESCRIPTION) {
            strcpy(KitName,ki->second->description);
        }
        else {
            strcpy(KitName,key.c_str());
        }
        return;
    }

    KitName[0]='\0';
}

// sanity-check for team id
BOOL GoodTeamId(WORD id)
{
	//TRACE2(&k_mydll,"GoodTeamId: checking id = %04x", id);
	return (id>=0 && id<256);
}

/* New Reset function */
void JuceReset(IDirect3DDevice8* self, LPVOID params)
{
	Log(&k_mydll,"JuceReset: cleaning-up.");

	InvalidateDeviceObjects(self);
	DeleteDeviceObjects(self);

    g_needsRestore = TRUE;

	return;
}

/** 
 * Finds slot for given kitId.
 * Returns -1, if slot not found.
 */
int GetKitSlot(int kitId)
{
	int slot = -1;
	for (int i=0; i<4; i++) 
	{
		WORD id = *((WORD*)(data[KIT_SLOTS] + i*0x38 + 0x0a));
		if (kitId == id) { slot = i; break; }
	}
	return slot;
}

/* New Present function */
void JucePresent(IDirect3DDevice8* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
    ////// 2D-kits //////////////////////////

    if (g_display2Dkits) Draw2Dkits(self);
    g_lastDisplay2Dkits = g_display2Dkits;

	/*
	// draw the uniform labels
	if (bIndicate) DrawKitLabels(self);

	*/

	/*
	if (g_triggerLoad3rdKit > 0)
	{
		int kitId = 0, slot = -1;
		BYTE* strips = (BYTE*)data[TEAM_STRIPS];

		switch (g_triggerLoad3rdKit)
		{
			case 1:
				g_triggerLoad3rdKit = 0;
				// clear kit slot to force uni-file reload
				kitId = (strips[0] & 0x01) ? g_currTeams[0] * 5 + 3 : g_currTeams[0] * 5 + 2;
				slot = GetKitSlot(kitId);
				if (slot != -1)
				{
					ZeroMemory((BYTE*)data[KIT_SLOTS] + slot * 0x38, 0x38);
					// set uni-reload flag
					*((DWORD*)data[KIT_CHECK_TRIGGER]) = 1;
				}
				break;

			case 2:
				g_triggerLoad3rdKit = 0;
				// clear kit info to force uni-file reload
				kitId = (strips[1] & 0x01) ? g_currTeams[1] * 5 + 3 : g_currTeams[1] * 5 + 2;
				slot = GetKitSlot(kitId);
				if (slot != -1)
				{
					ZeroMemory((BYTE*)data[KIT_SLOTS] + slot * 0x38, 0x38);
					// set uni-reload flag
					*((DWORD*)data[KIT_CHECK_TRIGGER]) = 1;
				}
				break;
		}
	}
	*/

	// reset texture count
	g_frame_tex_count = 0;
	g_frame_count++;

	/*
	if (g_dumpTexturesMode && g_frame_count >= 2) {
		g_dumpTexturesMode = FALSE;
		g_frame_count = 0;
	}
	*/

    /*
	DWORD* vtable = (DWORD*)(*((DWORD*)GetActiveDevice()));
	DWORD protection = 0;
	DWORD newProtection = PAGE_EXECUTE_READWRITE;

    // hook SetTransform method, if needed
    if (vtable[VTAB_SETTRANSFORM] != (DWORD)JuceSetTransform) {
        Log(&k_mydll,"Hooking SetTransform...");
        if (!g_orgSetTransform) {
            g_orgSetTransform = (PFNSETTRANSFORMPROC)vtable[VTAB_SETTRANSFORM];
        }
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (VirtualProtect(vtable+VTAB_SETTRANSFORM, 4, newProtection, &protection))
        {
            vtable[VTAB_SETTRANSFORM] = (DWORD)JuceSetTransform;
            Log(&k_mydll,"SetTransform hooked.");
        }
    }
    */

	//Log(&k_mydll,"=========================frame============================");
	return;
}

/* CopyRects tracker. */
HRESULT STDMETHODCALLTYPE JuceCopyRects(IDirect3DDevice8* self,
IDirect3DSurface8* pSourceSurface, CONST RECT* pSourceRectsArray, UINT cRects,
IDirect3DSurface8* pDestinationSurface, CONST POINT* pDestPointsArray)
{
	TRACEX(&k_mydll,"JuceCopyRects: rect(%dx%d), numRects = %d", pSourceRectsArray[0].right,
			pSourceRectsArray[0].bottom, cRects);
	return g_orgCopyRects(self, pSourceSurface, pSourceRectsArray, cRects,
			pDestinationSurface, pDestPointsArray);
}

/* ApplyStateBlock tracker. */
HRESULT STDMETHODCALLTYPE JuceApplyStateBlock(IDirect3DDevice8* self, DWORD token)
{
	TRACE2(&k_mydll,"JuceApplyStateBlock: token = %d", token);
	return g_orgApplyStateBlock(self, token);
}

/* BeginScene tracker. */
HRESULT STDMETHODCALLTYPE JuceBeginScene(IDirect3DDevice8* self)
{
	TRACE(&k_mydll,"JuceBeginScene: CALLED.");
	return g_orgBeginScene(self);
}

/* EndScene tracker. */
HRESULT STDMETHODCALLTYPE JuceEndScene(IDirect3DDevice8* self)
{
	TRACE(&k_mydll,"JuceEndScene: CALLED.");
	return g_orgEndScene(self);
}

/**
 * Initialize array of MEMITEMINFO structures.
 */
BOOL InitMemItemInfo(DWORD** ppIds, MEMITEMINFO** ppMemItemInfoArray, DWORD n)
{
	ZeroMemory(g_afsFileName, sizeof(BUFLEN));
	lstrcpy(g_afsFileName, GetPESInfo()->pesdir);
	lstrcat(g_afsFileName, "dat\\0_text.afs");
    LOG(&k_mydll, "g_afsFileName = {%s}", g_afsFileName);

	FILE* f = fopen(g_afsFileName, "rb");
	if (f == NULL) {
		LOG(&k_mydll,"FATAL ERROR: failed to open 0_text.afs for reading.");
		return FALSE;
	}

    *ppIds = (DWORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n*sizeof(DWORD));
    *ppMemItemInfoArray = (MEMITEMINFO*)HeapAlloc(GetProcessHeap(), 
            HEAP_ZERO_MEMORY, n*sizeof(MEMITEMINFO));
    DWORD* ids = *ppIds;
    MEMITEMINFO* memItemInfo = *ppMemItemInfoArray;

    if (!ids || !memItemInfo) {
        Log(&k_mydll,"FATAL ERROR: unable to allocate memory for data structures");
        fclose(f);
        return FALSE;
    }

	int i;
    for (i=0; i<n; i++) {
        ids[i] = data[FIRST_ID] + i;
    }
	for (i=0; i<n; i++) {
		memItemInfo[i].id = ids[i];
		ReadItemInfoById(f, ids[i], &memItemInfo[i].afsItemInfo, 0);
		TRACE2(&k_mydll,"id = %d", memItemInfo[i].id);
		TRACE2(&k_mydll,"offset = %08x", memItemInfo[i].afsItemInfo.dwOffset);

        // store pointer in offset-map
        g_AFS_offsetMap[memItemInfo[i].afsItemInfo.dwOffset] = memItemInfo + i;
	}

	fclose(f);
    return TRUE;
}

void FreeMemItemInfo(DWORD** ppIds, MEMITEMINFO** ppMemItemInfoArray) 
{
    if (ppIds != NULL) {
        HeapFree(GetProcessHeap(), 0, *ppIds);
        *ppIds = NULL;
    }
    if (ppMemItemInfoArray != NULL) {
        HeapFree(GetProcessHeap(), 0, *ppMemItemInfoArray);
        *ppMemItemInfoArray = NULL;
    }
}

void LoadKitMask()
{
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
	lstrcpy(filename, GetPESInfo()->gdbDir);
	lstrcat(filename, "GDB\\uni\\masks\\mask.png");
	if (LoadPNGTexture(&g_MaskTex, filename) > 0) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)g_MaskTex;
		g_Mask = (BYTE*)g_MaskTex + bih->biSize + 0x400;
	}
}

void UnloadKitMask()
{
	if (g_MaskTex != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)g_MaskTex);
	}
}


DWORD CalculateNewKitsSize(RECT* rect)
{
	return rect->right*rect->bottom*4;
};

void Convert8To32Bit(BITMAPINFO* dest,BITMAPINFO* src)
{
	DWORD* destpix=(DWORD*)((DWORD)dest+sizeof(BITMAPINFOHEADER));
	BITMAPINFOHEADER* srcbih=(BITMAPINFOHEADER*)src;
	BYTE *srcpix=(BYTE*)((DWORD)src+sizeof(BITMAPINFOHEADER)+0x400);
	
	RGBQUAD* srcpal=(RGBQUAD*)&(src->bmiColors);
	RGBQUAD* srccol;
	
	for (int x=0;x<(srcbih->biWidth);x++)
	for (int y=0;y<(srcbih->biHeight);y++) {
		srccol=&(srcpal[*srcpix]);
		*destpix=srccol->rgbBlue + 
            0x100 * srccol->rgbGreen + 
            0x10000 * srccol->rgbRed + 
            0x1000000 * srccol->rgbReserved;
		srcpix++;
		destpix++;
	};
	(dest->bmiHeader).biBitCount=32;
	//DumpData((BYTE*)dest,srcbih->biWidth*srcbih->biHeight*4+sizeof(BITMAPINFOHEADER));
}

void Convert24To32Bit(BITMAPINFO* dest,BITMAPINFO* src,BYTE* alpha)
{
	DWORD srccol=0;
	DWORD *destpix=(DWORD*)((DWORD)dest+sizeof(BITMAPINFOHEADER));
	BITMAPINFOHEADER* srcbih=(BITMAPINFOHEADER*)src;
	BYTE *srcpix=(BYTE*)((DWORD)src+sizeof(BITMAPINFOHEADER));
	BYTE *a=alpha;
	
	for (int x=0;x<(srcbih->biWidth);x++)
	for (int y=0;y<(srcbih->biHeight);y++) {
		srccol=*srcpix;
		srcpix++;
		srccol+=(*srcpix)*0x100;
		srcpix++;
		srccol+=(*srcpix)*0x10000;
		srcpix++;
		if (alpha!=NULL)
			srccol+=(*a)*0x1000000;
		else
			srccol+=0xFF*0x1000000; //Alpha
		*destpix=srccol;
		a++;
		destpix++;
	};
	
	(dest->bmiHeader).biBitCount=32;
	return;
};

void CopyImageData(BYTE** dest,BITMAPINFO* src,BITMAPINFO* alphaTex)
{
	BITMAPINFOHEADER* bih=(BITMAPINFOHEADER*)src;
	DWORD pixelsSize=bih->biWidth * bih->biHeight * 4;
	
	*dest=(BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,pixelsSize+sizeof(BITMAPINFOHEADER));
	
	if (bih->biBitCount==32) {
		//Copy header
		memcpy(*dest,(BYTE*)src,sizeof(BITMAPINFOHEADER));
		
		//The three DWORD are not copied
		if (bih->biCompression==BI_BITFIELDS)
			memcpy(*dest+sizeof(BITMAPINFOHEADER),(BYTE*)src+sizeof(BITMAPINFOHEADER)+
						3*sizeof(DWORD),pixelsSize);
		else
			memcpy(*dest+sizeof(BITMAPINFOHEADER),(BYTE*)src+sizeof(BITMAPINFOHEADER),pixelsSize);
		
		Log(&k_mydll,"32 bit image copied.");
	} else if (bih->biBitCount==24) {
		BYTE* alpha=NULL;
		if (alphaTex!=NULL)
			alpha=(BYTE*)alphaTex+((BITMAPINFOHEADER*)alphaTex)->biSize+0x400;
		Convert24To32Bit((BITMAPINFO*)*dest,src,alpha);
	} else
		Convert8To32Bit((BITMAPINFO*)*dest,src);

	BITMAPINFOHEADER* destbih=(BITMAPINFOHEADER*)*dest;
	destbih->biSize=sizeof(BITMAPINFOHEADER);
	destbih->biWidth=bih->biWidth;
	destbih->biHeight=bih->biHeight;
	destbih->biPlanes=1;
	destbih->biBitCount=32;
	destbih->biCompression=BI_RGB;
	return;
};

// color 1 is alpha-blended over color 2
DWORD BlendColors(DWORD color1, DWORD color2)
{
	BYTE alpha1 =  (color1 & 0xff000000) >> 24;

	if (alpha1 == 0) return color2;		// transparent overlay
	if (alpha1 == 255) return ((color1 & 0xffffff) | (color2 & 0xff000000));	// opaque overlay
		
	BYTE redValue = ((color1 & 0xff0000) * alpha1 + (color2 & 0xff0000) * (255 - alpha1)) / 0xff0000;
	BYTE greenValue = ((color1 & 0xff00) * alpha1 + (color2 & 0xff00) * (255 - alpha1)) / 0xff00;
	BYTE blueValue = ((color1 & 0xff) * alpha1 + (color2 & 0xff) * (255 - alpha1)) / 0xff;

	DWORD alpha2 =  color2 & 0xff000000;	// keep the original alpha
		
	return (alpha2 | (redValue << 16) | (greenValue << 8) | blueValue);
}

void ApplyOverlay(D3DLOCKED_RECT* dest, char* overlayfilename, D3DSURFACE_DESC* desc)
{
	UINT width = desc->Width;
	UINT height = desc->Height;

	IDirect3DTexture8* pOvTexture = NULL;
	D3DLOCKED_RECT rectOv;
	
	// create overlay texture
	if (!SUCCEEDED(D3DXCreateTextureFromFileEx(
            GetActiveDevice(), overlayfilename, 
            width, height,
            1, desc->Usage, D3DFMT_A8R8G8B8, desc->Pool,
            D3DX_DEFAULT, D3DX_DEFAULT,
            0, NULL, NULL, &pOvTexture))) {
        LOG(&k_mydll, "Unable to create overlay texture from file: %s", overlayfilename);
		return;
	}
	
	if (!SUCCEEDED(pOvTexture->LockRect(0, &rectOv, NULL, 0))) {
		pOvTexture->Release();
        LOG(&k_mydll, "LockRect failed for overlay texture");
		return;
	}

	BYTE* pDestRow = (BYTE*)dest->pBits;
	BYTE* pSrcRow = (BYTE*)rectOv.pBits;
	for (int y = 0; y < height; y++) {
		DWORD* pDestPixel = (DWORD*)pDestRow;
		DWORD* pSrcPixel = (DWORD*)pSrcRow;
		for (int x = 0; x < width; x++) {
			*pDestPixel = BlendColors(*pSrcPixel, *pDestPixel);
			pDestPixel++;
			pSrcPixel++;
		}
		pDestRow += dest->Pitch;
		pSrcRow += rectOv.Pitch;
	}
		
	pOvTexture->UnlockRect(0);
	pOvTexture->Release();
}

void ResampleImage(BYTE* dest,BYTE* src,RECT* destSize,RECT* srcSize)
{
	DWORD UnitX, UnitY;
	double fUnitX=0.0, fUnitY=0.0;
	double fTmp1=0.0, fTmp2=0.0;
	int x,y,zx,zy;
	RGBQUAD *destPix=NULL,*srcPix=NULL;
	DWORD medianR,medianG,medianB,medianA;
	DWORD srcCol=0;
	DWORD *dwSrc,*dwDest;
	
	if (destSize->right == srcSize->right) {
		memcpy(dest,src,CalculateNewKitsSize(srcSize));
	} else if (destSize->right < srcSize->right) {
		//resample to smaller image
		fUnitX=(double)srcSize->right / (double)destSize->right;
		fUnitY=(double)srcSize->bottom / (double)destSize->bottom;
		if (modf(fUnitX,&fTmp1)!=0.0 || modf(fUnitY,&fTmp2)!=0.0) {
			LogWithTwoNumbers(&k_mydll,"Downsampling is not possible! (%f x %f)",fUnitX,fUnitY);
			return;
		};
		UnitX=fTmp1;
		UnitY=fTmp2;
		for (x=0;x<(destSize->right);x++)
		for (y=0;y<(destSize->bottom);y++) {
			if (true) {
				//resize, faster
				dwDest=(DWORD*)(dest + y*destSize->right*4 + x*4);
				dwSrc=(DWORD*)(src + (y*UnitY)*srcSize->right*4 + (x*UnitX)*4);
				*dwDest=*dwSrc;
			} else {
				//resample, more quality
				destPix=(RGBQUAD*)(dest + y*destSize->right*4 + x*4);
				medianR=0; medianG=0; medianB=0; medianA=0;
				for (zx=0;zx<UnitX;zx++)
				for (zy=0;zy<UnitY;zy++) {
					srcPix=(RGBQUAD*)(src + (y*UnitY+zy)*srcSize->right*4 + (x*UnitX+zx)*4);
					medianR+=srcPix->rgbRed;
					medianG+=srcPix->rgbGreen;
					medianB+=srcPix->rgbBlue;
					medianA+=srcPix->rgbReserved;
				};
				//LogWithThreeNumbers(&k_mydll,"medianR=%d, medianG=%d, medianB=%d",medianR,medianG,medianB);
				destPix->rgbRed=floor((float)medianR / (UnitX*UnitY));
				destPix->rgbGreen=floor((float)medianG / (UnitX*UnitY));
				destPix->rgbBlue=floor((float)medianB / (UnitX*UnitY));
				destPix->rgbReserved=floor((float)medianA / (UnitX*UnitY));
			};
		};
		//DumpData(dest,destSize->right*destSize->bottom*4);
	} else {
		//resample to bigger image
		fTmp1=srcSize->right;
		fTmp2=srcSize->bottom;
		fUnitX=fTmp1 / destSize->right;
		fUnitY=fTmp2 / destSize->bottom;
		dwSrc=(DWORD*)src;
		dwDest=(DWORD*)dest;
		for (x=0;x<(destSize->right);x++)
		for (y=0;y<(destSize->bottom);y++) {
			srcCol=dwSrc[(DWORD)(floor(fUnitY*y)*srcSize->right + floor(fUnitX*x))];
			//LogWithNumber(&k_mydll,"%d",(DWORD)(floor(fUnitY*y)*srcSize->right + floor(fUnitX*x)));
			dwDest[y*destSize->right + x]=srcCol;
		};
				
		
		//DumpData(dest,destSize->right*destSize->bottom*4);
	};
	return;
};

void ResampleImage(BITMAPINFOHEADER** src,RECT* destSize)
{
	BYTE* tmp=(BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
							CalculateNewKitsSize(destSize)+sizeof(BITMAPINFOHEADER));
							
	RECT srcSize={0,0,(*src)->biWidth,(*src)->biHeight};
	
	ResampleImage(tmp+sizeof(BITMAPINFOHEADER),(BYTE*)*src+sizeof(BITMAPINFOHEADER),
					destSize,&srcSize);
	
	memcpy(tmp,*src,sizeof(BITMAPINFOHEADER));
	
	HeapFree(GetProcessHeap(),0,(LPVOID)*src);
	*src=(BITMAPINFOHEADER*)tmp;
	
	(*src)->biWidth=destSize->right;
	(*src)->biHeight=destSize->bottom;
	return;
};

void CreateAlphaTableName(char* alphaName,char* filename)
{
	strcpy(alphaName,filename);
	strcpy(alphaName+lstrlen(filename)-4,"-alpha");
	strcpy(alphaName+lstrlen(filename)+2,filename+lstrlen(filename)-4);
	return;
};

NEWKIT* CreateNewKits(NEWKITFILES files)
{
	int i,j,lastValidFactor;
	BITMAPINFO *tex = NULL,*alpha = NULL;
    BYTE *tmpShirt=NULL,*tmpShorts=NULL,*tmpSocks=NULL;
    BITMAPINFOHEADER* bih=NULL;
    DWORD texSize=0,resTexSize=0,pixelsSize=0;
    char tmp[BUFLEN], alphaName[BUFLEN];
    HANDLE pHeap=GetProcessHeap();
    bool IsAllInOneKit=false;
    int line=0;
    BYTE *tmpLine;
    DWORD *pixStartShorts, *pixStartSocks;
	BYTE* g_newLargeKitTex = NULL;
	NEWKIT* res=NULL;
	char* shirtName=files.shirtName;
	char* shortsName=files.shortsName;
	char* socksName=files.socksName;
	char* maskName=files.maskName;

	if (lstrlen(shirtName)==0) {
		return NULL;
	};
	
	if (lstrcmp(shirtName,shortsName)==0 && lstrcmp(shirtName,socksName)==0)
		IsAllInOneKit=true;

	for (i=0;i<MAX_STORED_NEWKITS;i++)
		if ((lstrcmp(g_NewKitFilenames[i].shirtName,shirtName)==0) &&
			(lstrcmp(g_NewKitFilenames[i].shortsName,shortsName)==0) &&
			(lstrcmp(g_NewKitFilenames[i].socksName,socksName)==0)) {
				res=&(g_NewKits[i]);
				return res;
		};
			

    //Load the shirt
	if (lstrcmpi(shirtName+lstrlen(shirtName)-4,".png")==0) {
	    texSize = LoadPNGTexture(&tex,shirtName);
	    if (texSize<=0)
	    	goto CreateNewKitsFailed;
	    CreateAlphaTableName(alphaName,shirtName);
	    if (LoadPNGTexture(&alpha,alphaName)<=0)
	    	alpha=NULL;
		CopyImageData(&tmpShirt,tex,alpha);
		FreePNGTexture(tex);
		FreePNGTexture(alpha);
		
	} else {
		texSize = LoadTexture(&tex,shirtName);
		if (texSize<=0)
	    	goto CreateNewKitsFailed;
		CreateAlphaTableName(alphaName,shirtName);
	    if (LoadTexture(&alpha,alphaName)<=0)
	    	alpha=NULL;
		CopyImageData(&tmpShirt,tex,alpha);
		FreeTexture(tex);
		FreeTexture(alpha);
	};
	
	if (IsAllInOneKit) goto FindNewKitSize;
	//Only mask if it is not an All-In-One kit
	MaskKitTextureWithFile((BITMAPINFOHEADER*)tmpShirt,maskName,0);
		
	//Load shorts
	if (lstrcmpi(shortsName+lstrlen(shortsName)-4,".png")==0) {
	    texSize = LoadPNGTexture(&tex,shortsName);
	    if (texSize<=0)
	    	goto CreateNewKitsFailed;
		CreateAlphaTableName(alphaName,shortsName);
	    if (LoadPNGTexture(&alpha,alphaName)<=0)
	    	alpha=NULL;
		CopyImageData(&tmpShorts,tex,alpha);
		FreePNGTexture(tex);
		FreePNGTexture(alpha);
	} else {
		texSize = LoadTexture(&tex,shortsName);
		if (texSize<=0)
	    	goto CreateNewKitsFailed;
	    CreateAlphaTableName(alphaName,shirtName);
	    if (LoadTexture(&alpha,alphaName)<=0)
	    	alpha=NULL;
		CopyImageData(&tmpShorts,tex,alpha);
		FreeTexture(tex);
		FreeTexture(alpha);
	};
	MaskKitTextureWithFile((BITMAPINFOHEADER*)tmpShorts,maskName,1);
	
	//Load socks
	if (lstrcmpi(socksName+lstrlen(socksName)-4,".png")==0) {
	    texSize = LoadPNGTexture(&tex,socksName);
	    if (texSize<=0)
	    	goto CreateNewKitsFailed;
		CreateAlphaTableName(alphaName,socksName);
	    if (LoadPNGTexture(&alpha,alphaName)<=0)
	    	alpha=NULL;
	    CopyImageData(&tmpSocks,tex,alpha);
		FreePNGTexture(tex);
		FreePNGTexture(alpha);
	} else {
		texSize = LoadTexture(&tex,socksName);
		if (texSize<=0)
	    	goto CreateNewKitsFailed;
	    CreateAlphaTableName(alphaName,shirtName);
	    if (LoadTexture(&alpha,alphaName)<=0)
	    	alpha=NULL;
		CopyImageData(&tmpSocks,tex,alpha);
		FreeTexture(tex);
		FreeTexture(alpha);
	};
	MaskKitTextureWithFile((BITMAPINFOHEADER*)tmpSocks,maskName,2);
	
	
	
	FindNewKitSize:
	//Find out the right destination size
	bih=(BITMAPINFOHEADER*)tmpShirt;
	largeRect.right=bih->biWidth; largeRect.bottom=bih->biHeight;
	
	if (IsAllInOneKit) goto FindNewKitSize2;
	
	bih=(BITMAPINFOHEADER*)tmpShorts;
	if (bih->biWidth>largeRect.right) {
		largeRect.right=bih->biWidth; largeRect.bottom=bih->biHeight;
	};
	
	bih=(BITMAPINFOHEADER*)tmpSocks;
	if (bih->biWidth>largeRect.right) {
		largeRect.right=bih->biWidth; largeRect.bottom=bih->biHeight;
	};
	

	FindNewKitSize2:
	lastValidFactor=1;
	for (i=1;i<1024;i++) {
		if (largeRect.right % i != 0) continue;
		//if too small, use the last size
		if ((largeRect.right / i) < MIN_LARGE_WIDTH) {
			if (i>0)
				largeRect.right=largeRect.right / lastValidFactor;
			break;
		};
		//size is OK, break
		if ((largeRect.right / i) < MAX_LARGE_WIDTH) {
			largeRect.right=largeRect.right/i;
			break;
		};
		//too big, go on
		lastValidFactor=i;
	};
	largeRect.bottom=largeRect.right/2;
	
	
	//the medium texture is at maximum half the large size
	mediumRect.right=largeRect.right;
	for (i=2;i<1024;i++) {
		if (mediumRect.right % i != 0) continue;
		//if too small, use the last size
		if ((mediumRect.right / i) < MIN_MEDIUM_WIDTH) {
			if (i>0)
				mediumRect.right=mediumRect.right / lastValidFactor;
			break;
		};
		//size is OK, break
		if ((mediumRect.right / i) < MAX_MEDIUM_WIDTH) {
			mediumRect.right=mediumRect.right/i;
			break;
		};
		//too big, go on
		lastValidFactor=i;
	};
	mediumRect.bottom=mediumRect.right/2;
	
	
	//the small texture is at maximum half the medium size
	smallRect.right=mediumRect.right;
	for (i=2;i<1024;i++) {
		if (smallRect.right % i != 0) continue;
		//if too small, use the last size
		if ((smallRect.right / i) < MIN_SMALL_WIDTH) {
			if (i>0)
				smallRect.right=smallRect.right / lastValidFactor;
			break;
		};
		//size is OK, break
		if ((smallRect.right / i) < MAX_SMALL_WIDTH) {
			smallRect.right=smallRect.right/i;
			break;
		};
		//too big, go on
		lastValidFactor=i;
	};
	smallRect.bottom=smallRect.right/2;
	

	LOG(&k_mydll,"CreateNewKits: Large texture is sized %dx%d.",
				largeRect.right,largeRect.bottom);
	LOG(&k_mydll,"CreateNewKits: Medium texture is sized %dx%d.",
				mediumRect.right,mediumRect.bottom);
	LOG(&k_mydll,"CreateNewKits: Small texture is sized %dx%d.",
				smallRect.right,smallRect.bottom);
	
	//Resample the images if needed
	bih=(BITMAPINFOHEADER*)tmpShirt;
	if (bih->biWidth != largeRect.right || bih->biHeight != largeRect.bottom)
		ResampleImage((BITMAPINFOHEADER**)&tmpShirt,&largeRect);
	if (IsAllInOneKit) goto AllocNewKitMem;
	
	bih=(BITMAPINFOHEADER*)tmpShorts;
	if (bih->biWidth != largeRect.right || bih->biHeight != largeRect.bottom)
		ResampleImage((BITMAPINFOHEADER**)&tmpShorts,&largeRect);
		
	bih=(BITMAPINFOHEADER*)tmpSocks;
	if (bih->biWidth != largeRect.right || bih->biHeight != largeRect.bottom)
		ResampleImage((BITMAPINFOHEADER**)&tmpSocks,&largeRect);
	
	AllocNewKitMem:
	//Allocate memory for resulting image
	pixelsSize=CalculateNewKitsSize(&largeRect);
	g_newLargeKitTex = (BYTE*)HeapAlloc(pHeap,HEAP_ZERO_MEMORY,pixelsSize);

	//Put the parts together
	memcpy(g_newLargeKitTex,tmpShirt+sizeof(BITMAPINFOHEADER),pixelsSize);
	if (IsAllInOneKit) goto ReverseNewKitsLines;
	
	pixStartShorts=(DWORD*)(tmpShorts+sizeof(BITMAPINFOHEADER));
	pixStartSocks=(DWORD*)(tmpSocks+sizeof(BITMAPINFOHEADER));
	for (i=0;i<(pixelsSize/sizeof(DWORD));i++) {
		((DWORD*)g_newLargeKitTex)[i] |= pixStartShorts[i];
		((DWORD*)g_newLargeKitTex)[i] |= pixStartSocks[i];
	};
	
	ReverseNewKitsLines:
	//DumpData(g_newLargeKitTex,CalculateNewKitsSize(&largeRect),"LargeKit.bmp");

	//reverse lines
	tmpLine=new BYTE[largeRect.right*4];
	for (line=0; line<0.5*largeRect.bottom; line++) {
        memcpy(tmpLine, g_newLargeKitTex + line*largeRect.right*4, largeRect.right*4);
        memcpy(g_newLargeKitTex + line*largeRect.right*4, g_newLargeKitTex + (largeRect.bottom-line-1)*largeRect.right*4, largeRect.right*4);
        memcpy(g_newLargeKitTex + (largeRect.bottom-line-1)*largeRect.right*4, tmpLine, largeRect.right*4);
    }
    delete tmpLine;
    tmpLine=NULL;

	//Large kit is finished now
	Log(&k_mydll,"Successfully created large new kit!");


	strcpy(g_NewKitFilenames[g_nextNewKitId].shirtName,shirtName);
	strcpy(g_NewKitFilenames[g_nextNewKitId].shortsName,shortsName);
	strcpy(g_NewKitFilenames[g_nextNewKitId].socksName,socksName);
	UnloadNewKits(g_nextNewKitId);
	g_NewKits[g_nextNewKitId].tex=g_newLargeKitTex;
	g_NewKits[g_nextNewKitId].sizeBig=largeRect;
	g_NewKits[g_nextNewKitId].sizeMedium=mediumRect;
	g_NewKits[g_nextNewKitId].sizeSmall=smallRect;
	
	res=&(g_NewKits[g_nextNewKitId]);
	
	g_nextNewKitId++;
	g_nextNewKitId%=MAX_STORED_NEWKITS;

	//LogWithNumber(&k_mydll,"g_nextNewKitId = %d",g_nextNewKitId);

	CreateNewKitsFailed:
	//if (g_newSmallKitTex == NULL || g_newMediumKitTex == NULL || g_newLargeKitTex == NULL) {
	if (g_newLargeKitTex == NULL) {
		Log(&k_mydll,"FAILED to create new kits!");
		return NULL;
	};

	//unload the parts
	if (tmpShirt!=NULL)
		HeapFree(pHeap,0,(LPVOID)tmpShirt);
	if (tmpShorts!=NULL)
		HeapFree(pHeap,0,(LPVOID)tmpShorts);
	if (tmpSocks!=NULL)
		HeapFree(pHeap,0,(LPVOID)tmpSocks);

	return res;
};

void UnloadNewKits()
{
	for (int i=0;i<MAX_STORED_NEWKITS;i++)
		UnloadNewKits(i);
	g_nextNewKitId=0;
	ZeroMemory(g_NewKitFilenames,MAX_STORED_NEWKITS*sizeof(NEWKITFILES));
	ZeroMemory(g_NewKits,MAX_STORED_NEWKITS*sizeof(NEWKIT));
	
	isGdbKit.clear();
	gdbKitSize.clear();
	gdbKitCRC.clear();
	NewKitFilesMap.clear();
	
	return;
};

void UnloadNewKits(DWORD Index)
{
	HANDLE pHeap=GetProcessHeap();
	if (g_NewKits[Index].tex != NULL) {
		HeapFree(pHeap,0,g_NewKits[Index].tex);
		g_NewKits[Index].tex=NULL;
	};
	return;
};

/************
 * This function initializes kitserver.
 ************/
void InitKserv()
{
	ZeroMemory(g_teamDir, sizeof(g_teamDir)*sizeof(BYTE));

    // Determine the game version
    int v = GetPESInfo()->GameVersion;
    if (v != -1)
    {
        memcpy(code, codeArray[v], sizeof(code));
        memcpy(data, dataArray[v], sizeof(data));

        if (!InitMemItemInfo(&g_AFS_id, &g_AFS_memItemInfo, data[LAST_ID]-data[FIRST_ID]+1)) {
            LOG(&k_mydll,"InitMemItemInfo failed.");
            return;
        }

        // Load GDB database
        gdb = gdbLoad(GetPESInfo()->gdbDir);
        if (!gdb) {
            Log(&k_mydll,"Unable to load GDB.");
            return;
        }
        LogWithTwoNumbers(&k_mydll,"GDB loaded: gdb = %08x, teams: %d", (DWORD)gdb, (*(gdb->uni)).size());
        //LogWithString(&k_mydll,"77: %s", MAP_FIND(gdb->uni,77)->foldername);
        //LogWithString(&k_mydll,"88: %s", MAP_FIND(gdb->uni,88)->foldername);
        //LogWithNumber(&k_mydll,"99: %08x", (DWORD)MAP_FIND(gdb->uni,99));

        // load kit mask
        LoadKitMask();
		
		HookFunction(hk_ReadFile,(DWORD)JuceReadFile);

        // hook BeginUniSelect
		HookFunction(hk_BeginUniSelect,(DWORD)JuceSet2Dkits);

        // hook EndUniSelect
		HookFunction(hk_EndUniSelect,(DWORD)JuceClear2Dkits);

        // hook UniDecode
		HookFunction(hk_UniDecode,(DWORD)JuceUniDecode);
		
		// hook UniSplit
		//HookFunction(hk_UniSplit,(DWORD)JuceUniSplit);

		// hook GetNationalTeamInfo
		HookFunction(hk_GetNationalTeamInfo,(DWORD)JuceGetNationalTeamInfo);

		// hook GetClubTeamInfo
		HookFunction(hk_GetClubTeamInfo,(DWORD)JuceGetClubTeamInfo);
		HookFunction(hk_GetClubTeamInfoML1,(DWORD)JuceGetClubTeamInfoML1);
		HookFunction(hk_GetClubTeamInfoML2,(DWORD)JuceGetClubTeamInfoML2);
		
		// hook GetNationalTeamInfoExitEdit
		HookFunction(hk_GetNationalTeamInfoExitEdit,(DWORD)JuceGetNationalTeamInfoExitEdit);
		
        // hook Unpack
        HookFunction(hk_Unpack,(DWORD)JuceUnpack);
        
        // hook CreateTexture
        HookFunction(hk_D3D_CreateTexture,(DWORD)JuceCreateTexture);
        
        HookFunction(hk_D3D_UnlockRect,(DWORD)JuceUnlockRect);
        
        // install keyboard hook
		HookFunction(hk_Input,(DWORD)mydllKeyboardProc);
		
		if (g_config.ShowKitInfo) {
			// add a menu with information about the selected kit
			HookFunction(hk_DrawKitSelectInfo,(DWORD)DrawKitInfo);
			HookFunction(hk_OnShowMenu,(DWORD)BeginDrawKitInfo);
		};
		
		DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    BYTE* bptr;
	    DWORD* ptr;
	    
		if (code[C_NAVIGATELEFT] != 0) {
		    bptr = (BYTE*)code[C_NAVIGATELEFT];
		    ptr = (DWORD*)(code[C_NAVIGATELEFT] + 1);
		    if (VirtualProtect(bptr, 5, newProtection, &protection)) {
		        bptr[0] = 0xe9;
		        ptr[0] = (DWORD)JuceNavigateLeft - (DWORD)(code[C_NAVIGATELEFT] + 5);
		        Log(&k_mydll,"NavigateLeft HOOKED at code[C_NAVIGATELEFT]");
			};
	    };
		
		if (code[C_NAVIGATERIGHT] != 0) {
		    bptr = (BYTE*)code[C_NAVIGATERIGHT];
		    ptr = (DWORD*)(code[C_NAVIGATERIGHT] + 1);
		    if (VirtualProtect(bptr, 5, newProtection, &protection)) {
		        bptr[0] = 0xe9;
		        ptr[0] = (DWORD)JuceNavigateRight - (DWORD)(code[C_NAVIGATERIGHT] + 5);
		        Log(&k_mydll,"NavigateRight HOOKED at code[C_NAVIGATERIGHT]");
			};
		};
		
		MasterHookFunction(code[C_SPLITDONE_CS1],1,JuceSplitDone1);
		MasterHookFunction(code[C_SPLITDONE_CS2],1,JuceSplitDone1);
		MasterHookFunction(code[C_SPLITDONE_CS3],1,JuceSplitDone1);
		MasterHookFunction(code[C_SPLITDONE2_CS1],1,JuceSplitDone2_1);
		MasterHookFunction(code[C_SPLITDONE2_CS2],1,JuceSplitDone2_2);
		
		MasterHookFunction(code[C_GETSPLITADDR_CS],1,JuceUniSplit);
		
	return;
	
    }
    else
    {
        Log(&k_mydll,"Game version not recognized. Nothing is hooked");
    }
}

/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	DWORD wbytes, procId; 

	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;
        InitializeCriticalSection(&g_cs);
        
        RegisterKModule(&k_mydll);

		//texs.clear();

		// read configuration
		char cfgFile[BUFLEN];
		ZeroMemory(cfgFile, BUFLEN);
		lstrcpy(cfgFile, GetPESInfo()->mydir); 
		lstrcat(cfgFile, CONFIG_FILE);

		ReadConfig(&g_config, cfgFile);

		LogWithNumber(&k_mydll,"k_mydll.debug = %d", k_mydll.debug);
		
		HookFunction(hk_D3D_CreateDevice,(DWORD)InitKserv);
		HookFunction(hk_D3D_Reset,(DWORD)JuceReset);
	}

	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_mydll,"DLL detaching...");
		
		UnhookFunction(hk_ReadFile,(DWORD)JuceReadFile);

        // unhook BeginUniSelect
		UnhookFunction(hk_BeginUniSelect,(DWORD)JuceSet2Dkits);

        // unhook EndUniSelect
		UnhookFunction(hk_EndUniSelect,(DWORD)JuceClear2Dkits);

        // unhook UniDecode
		UnhookFunction(hk_UniDecode,(DWORD)JuceUniDecode);
		
		// unhook UniSplit
		//UnhookFunction(hk_UniSplit,(DWORD)JuceUniSplit);

		// unhook GetNationalTeamInfo
		UnhookFunction(hk_GetNationalTeamInfo,(DWORD)JuceGetNationalTeamInfo);

		// unhook GetClubTeamInfo
		UnhookFunction(hk_GetClubTeamInfo,(DWORD)JuceGetClubTeamInfo);
		UnhookFunction(hk_GetClubTeamInfoML1,(DWORD)JuceGetClubTeamInfoML1);
		UnhookFunction(hk_GetClubTeamInfoML2,(DWORD)JuceGetClubTeamInfoML2);
		
		// unhook GetNationalTeamInfoExitEdit
		UnhookFunction(hk_GetNationalTeamInfoExitEdit,(DWORD)JuceGetNationalTeamInfoExitEdit);
		
        // unhook Unpack
        UnhookFunction(hk_Unpack,(DWORD)JuceUnpack);
        
		// unhook Direct3D functions
        UnhookFunction(hk_D3D_CreateTexture,(DWORD)JuceCreateTexture);
        UnhookFunction(hk_D3D_Reset,(DWORD)JuceReset);
		UnhookFunction(hk_D3D_Present,(DWORD)JucePresent);
		UnhookFunction(hk_D3D_UnlockRect,(DWORD)JuceUnlockRect);

		// uninstall keyboard hook
		UnhookFunction(hk_Input,(DWORD)mydllKeyboardProc);
		
		if (g_config.ShowKitInfo) {
			// Remove the menu
			UnhookFunction(hk_DrawKitSelectInfo,(DWORD)DrawKitInfo);
		};	
		
		MasterUnhookFunction(code[C_SPLITDONE_CS1],JuceSplitDone1);
		MasterUnhookFunction(code[C_SPLITDONE_CS2],JuceSplitDone1);
		MasterUnhookFunction(code[C_SPLITDONE_CS3],JuceSplitDone1);
		MasterUnhookFunction(code[C_SPLITDONE2_CS1],JuceSplitDone2_1);
		MasterUnhookFunction(code[C_SPLITDONE2_CS2],JuceSplitDone2_2);
		MasterUnhookFunction(code[C_GETSPLITADDR_CS],JuceUniSplit);

		InvalidateDeviceObjects(GetActiveDevice());
		Log(&k_mydll,"InvalideDeviceObjects done.");
		DeleteDeviceObjects(GetActiveDevice());
		Log(&k_mydll,"DeleteDeviceObjects done.");

		// unload kit mask
		UnloadKitMask();
		Log(&k_mydll,"Kit mask unloaded.");
		
		// unload new kits (boost)
		UnloadNewKits();
		Log(&k_mydll,"New kits unloaded.");

		// unload GDB
		TRACE(&k_mydll,"Unloading GDB...");
		gdbUnload(gdb);
		Log(&k_mydll,"GDB unloaded.");

		FreeMemItemInfo(&g_AFS_id, &g_AFS_memItemInfo);

        DeleteCriticalSection(&g_cs);
        
        Log(&k_mydll,"Detaching done.");
	}

	return TRUE;    /* ok */
}

// helper function
// returns TRUE if specified slot contains edited kit.
BOOL IsEditedKit(int slot) 
{
	KitSlot* kitSlot = (KitSlot*)(data[ANOTHER_KIT_SLOTS] + slot*sizeof(KitSlot));
	return kitSlot->isEdited;
}

HRESULT STDMETHODCALLTYPE JuceSetTransform(IDirect3DDevice8* dev, D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX* pMatrix)
{
    //LogWithNumber(&k_mydll, "JuceSetTransform: state=%d", (int)state);
    if (state == D3DTS_PROJECTION) {
        Log(&k_mydll, "matrix:");
        /*
        for (int i=0; i<4; i++) {
            LogWithFourFloats(&k_mydll, "%0.5f %0.5f %0.5f %0.5f", 
                    pMatrix->m[i][0], pMatrix->m[i][1], pMatrix->m[i][2], pMatrix->m[i][3]);
        }
        */
        // modify the matrix
        D3DMATRIX* pM = (D3DMATRIX*)pMatrix;
        pM->m[0][0] *= 0.6;
        pM->m[1][1] *= 0.8;
        //Log(&k_mydll, "JuceSetTransform: projection matrix adjusted.");
        LogWithNumber(&k_mydll, "address of pMatrix = %08x", (DWORD)pMatrix);
    }
    return g_orgSetTransform(dev, state, pMatrix);
}

HRESULT JuceCreateTextureFromFile(IDirect3DDevice8* dev, char* name, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal)
{
    // initialize ppTex
    if (ppTex) {
        *ppTex = NULL;
    }

    DWORD texType = TEXTYPE_NONE; 
    if (lstrcmpi(name + lstrlen(name)-4, ".png")==0) {
        if (FileExists(name)) {
            texType = TEXTYPE_PNG;
        } else {
            lstrcpy(name + lstrlen(name)-4, ".bmp");
            if (FileExists(name)) {
                texType = TEXTYPE_BMP;
            }
        }
    } else if (lstrcmpi(name + lstrlen(name)-4, ".bmp")==0) {
        if (FileExists(name)) {
            texType = TEXTYPE_BMP;
        } else {
            lstrcpy(name + lstrlen(name)-4, ".png");
            if (FileExists(name)) {
                texType = TEXTYPE_PNG;
            }
        }
    }

    switch (texType) {
        case TEXTYPE_PNG:
        case TEXTYPE_BMP:
            if (SUCCEEDED(D3DXCreateTextureFromFile(dev, name, ppTex))) {
                return S_OK;
            }
            else {
                Log(&k_mydll,"JuceCreateTextureFromFile: D3DXCreateTextureFromFile FAILED.");
            }
            break;
    }
    
    return E_FAIL;
}

void CreateGDBTextureFromFile(char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal)
{
	char name[BUFLEN];
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%sGDB\\%s", GetPESInfo()->gdbDir, filename);
	LogWithString(&k_mydll,"CreateGDBTextureFromFile: Loading texture: %s", filename);
	if (SUCCEEDED(JuceCreateTextureFromFile(GetActiveDevice(), name, ppTex, pPal))) {
		Log(&k_mydll,"CreateGDBTextureFromFile: JuceCreateTextureFromFile succeeded.");
		//D3DXSaveTextureToFile("G:\\texs\\test.bmp", D3DXIFF_BMP, g_home_kit_tex, NULL);
	} else {
		Log(&k_mydll,"CreateGDBTextureFromFile: JuceCreateTextureFromFile failed.");
        *ppTex = NULL;
	}
}

void CreateGDBTextureFromFolder(char* foldername, char* filename, IDirect3DTexture8** ppTex, PALETTEENTRY* pPal)
{
    MAKE_BUFFER(name);
    string altFilename(filename);
    altFilename.replace(altFilename.size()-4,4,".bmp");

    vector<string> namesToTry;
    namesToTry.push_back(string(filename));
    namesToTry.push_back("all.png");
    namesToTry.push_back(altFilename);
    namesToTry.push_back("all.bmp");

    for (vector<string>::iterator vit=namesToTry.begin(); vit!=namesToTry.end(); vit++) {
        sprintf(name, "%sGDB\\%s\\%s", GetPESInfo()->gdbDir, foldername, vit->c_str());
        LogWithString(&k_mydll,"CreateGDBTextureFromFolder: Loading texture: %s", name);
        if (SUCCEEDED(JuceCreateTextureFromFile(GetActiveDevice(), name, ppTex, pPal))) {
            Log(&k_mydll,"CreateGDBTextureFromFolder: SUCCEEDED.");
            return;
        }
    }
    Log(&k_mydll,"CreateGDBTextureFromFolder: FAILED.");
    *ppTex = NULL;
}

DWORD JuceNavigateLeft(DWORD player)
{
	DWORD oldEAX;
	__asm mov oldEAX, eax
	
	g_cycleDirection[player]=CYCLE_LEFT;
	
	return oldEAX;
};

DWORD JuceNavigateRight(DWORD player)
{
	DWORD oldEAX;
	__asm mov oldEAX, eax
	
	g_cycleDirection[player]=CYCLE_RIGHT;
	
	return oldEAX;
};

void CycleIterator(BYTE which,KitIterator* ki,StringKitMap* skm)
{
	KitIterator tmp=*ki;
	bool forward=true;
	bool SinglePlayer=*(BYTE*)(data[SINGLEPLAYER]) != 3;
	DWORD KitSelectSide=*(DWORD*)(data[KITSELECTSIDE]);
	BYTE* PlayerSide=(BYTE*)(data[PLAYERSIDE]);
	
	if (SinglePlayer)
		forward=(g_cycleDirection[PlayerSide[0]] == CYCLE_RIGHT) && (KitSelectSide==which);
	else
		forward=(g_cycleDirection[PlayerSide[which]] == CYCLE_RIGHT);
	
	if (forward) {
		tmp++;
		if (tmp == (*skm).end())
			tmp = (*skm).begin();
	} else {
		if (tmp == (*skm).begin())
			tmp = (*skm).end();
		tmp--;
	};
	
	*ki=tmp;

	return;
};

string GetNextHomeShirtKey(WORD teamId, string currKey)
{
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(0,&g_homeShirtIterator,col->goalkeepers);
                    string nextKey = g_homeShirtIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(0,&g_homeShirtIterator,col->goalkeepers);
                        nextKey = g_homeShirtIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(0,&g_homeShirtIterator,col->players);
                    string nextKey = g_homeShirtIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(0,&g_homeShirtIterator,col->players);
                        nextKey = g_homeShirtIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextHomeShortsKey(WORD teamId, string currKey)
{
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(0,&g_homeShortsIterator,col->goalkeepers);
                    string nextKey = g_homeShortsIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(0,&g_homeShortsIterator,col->goalkeepers);
                        nextKey = g_homeShortsIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(0,&g_homeShortsIterator,col->players);
                    string nextKey = g_homeShortsIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(0,&g_homeShortsIterator,col->players);
                        nextKey = g_homeShortsIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextHomeSocksKey(WORD teamId, string currKey)
{
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(0,&g_homeSocksIterator,col->goalkeepers);
                    string nextKey = g_homeSocksIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(0,&g_homeSocksIterator,col->goalkeepers);
                        nextKey = g_homeSocksIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(0,&g_homeSocksIterator,col->players);
                    string nextKey = g_homeSocksIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(0,&g_homeSocksIterator,col->players);
                        nextKey = g_homeSocksIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextAwayShirtKey(WORD teamId, string currKey)
{
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(1,&g_awayShirtIterator,col->goalkeepers);
                    string nextKey = g_awayShirtIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(1,&g_awayShirtIterator,col->goalkeepers);
                        nextKey = g_awayShirtIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(1,&g_awayShirtIterator,col->players);
                    string nextKey = g_awayShirtIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(1,&g_awayShirtIterator,col->players);
                        nextKey = g_awayShirtIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextAwayShortsKey(WORD teamId, string currKey)
{
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(1,&g_awayShortsIterator,col->goalkeepers);
                    string nextKey = g_awayShortsIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(1,&g_awayShortsIterator,col->goalkeepers);
                        nextKey = g_awayShortsIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(1,&g_awayShortsIterator,col->players);
                    string nextKey = g_awayShortsIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(1,&g_awayShortsIterator,col->players);
                        nextKey = g_awayShortsIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

string GetNextAwaySocksKey(WORD teamId, string currKey)
{
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        switch (typ) {
            case GK_TYPE:
                if ((*(col->goalkeepers)).end() == (*(col->goalkeepers)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(1,&g_awaySocksIterator,col->goalkeepers);
                    string nextKey = g_awaySocksIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(1,&g_awaySocksIterator,col->goalkeepers);
                        nextKey = g_awaySocksIterator->first;
                    }
                    return nextKey;
                }
            case PL_TYPE:
                if ((*(col->players)).end() == (*(col->players)).begin()) {
                    return "(null)";
                } else {
					CycleIterator(1,&g_awaySocksIterator,col->players);
                    string nextKey = g_awaySocksIterator->first;
                    if (currKey == nextKey) {
                        // same key -> skip to next
						CycleIterator(1,&g_awaySocksIterator,col->players);
                        nextKey = g_awaySocksIterator->first;
                    }
                    return nextKey;
                }
        }
    }
    return "(null)";
}

/**************************************************************** 
 * WH_KEYBOARD hook procedure                                   *
 ****************************************************************/ 

void mydllKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	//TRACE2(&k_mydll,"KeyboardProc CALLED. code1 = %d", code1);

    if (code1 < 0) // do not process message 
        return; 

	switch (code1)
	{
		case HC_ACTION:
			/* process the key events */
			if (lParam & 0x80000000)
			{
				if (wParam == VK_TAB) {
                    // switch kit selection mode: Players/Goalkeepers
                    typ = (typ + 1) % 2;
                    g_modeSwitch = TRUE;

                    // switch iterators
                    g_homeShirtIterator = (typ==PL_TYPE) ?
                        g_homeShirtIteratorPL : g_homeShirtIteratorGK;
                    g_homeShortsIterator = (typ==PL_TYPE) ?
                        g_homeShortsIteratorPL : g_homeShortsIteratorGK;
                    g_homeSocksIterator = (typ==PL_TYPE) ?
                        g_homeSocksIteratorPL : g_homeSocksIteratorGK;
                    g_awayShirtIterator = (typ==PL_TYPE) ?
                        g_awayShirtIteratorPL : g_awayShirtIteratorGK;
                    g_awayShortsIterator = (typ==PL_TYPE) ?
                        g_awayShortsIteratorPL : g_awayShortsIteratorGK;
                    g_awaySocksIterator = (typ==PL_TYPE) ?
                        g_awaySocksIteratorPL : g_awaySocksIteratorGK;
                }
                /*
				if (wParam == g_config.vKeyHomeKit) {
					//g_display2Dkits = (g_display2Dkits + 1) % 2;
					//LogWithNumber(&k_mydll,"KeyboardProc: g_display2Dkits = %d", g_display2Dkits);

					// hook SetTexture method, if not yet done so
					if (GetActiveDevice() && !g_orgSetTexture) {
						DWORD* vtable = (DWORD*)(*((DWORD*)GetActiveDevice()));

						g_orgSetTexture = (PFNSETTEXTUREPROC)vtable[VTAB_SETTEXTURE];
						DWORD protection = 0;
						DWORD newProtection = PAGE_EXECUTE_READWRITE;
						if (VirtualProtect(vtable+VTAB_SETTEXTURE, 4, newProtection, &protection))
						{
							vtable[VTAB_SETTEXTURE] = (DWORD)JuceSetTexture;
							Log(&k_mydll,"SetTexture hooked.");
						}
					} else if (GetActiveDevice() && g_orgSetTexture) {
						DWORD* vtable = (DWORD*)(*((DWORD*)GetActiveDevice()));

						DWORD protection = 0;
						DWORD newProtection = PAGE_EXECUTE_READWRITE;
						if (VirtualProtect(vtable+VTAB_SETTEXTURE, 4, newProtection, &protection))
						{
							vtable[VTAB_SETTEXTURE] = (DWORD)g_orgSetTexture;
							Log(&k_mydll,"SetTexture unhooked.");
						}
                        g_orgSetTexture = NULL;
                    }

                    if (!g_display2Dkits) {
                        // clear the kit texture map
                        Log(&k_mydll,"Clearing 2Dkit textures and g_kitTextureMap.");
                        g_kitTextureMap.clear();
                        g_home_shirt_tex = NULL;
                        g_home_shorts_tex = NULL;
                        g_home_socks_tex = NULL;
                        g_away_shirt_tex = NULL;
                        g_away_shorts_tex = NULL;
                        g_away_socks_tex = NULL;
                    }

                    if (g_display2Dkits) {
                        // initialize home iterators
                        WORD teamId = GetTeamId(HOME);
                        if (teamId != 0xffff) {
                            KitCollection* col = MAP_FIND(gdb->uni,teamId);
                            if (col) {
                                g_homeShirtIterator = (*(col->players)).begin();
                                g_homeShortsIterator = (*(col->players)).begin();
                                g_homeSocksIterator = (*(col->players)).begin();
                            }
                        }
                        // initialize away iterators
                        teamId = GetTeamId(AWAY);
                        if (teamId != 0xffff) {
                            KitCollection* col = MAP_FIND(gdb->uni,teamId);
                            if (col) {
                                g_awayShirtIterator = (*(col->players)).begin();
                                g_awayShortsIterator = (*(col->players)).begin();
                                g_awaySocksIterator = (*(col->players)).begin();
                            }
                        }
                    }

					//bIndicate = g_display2Dkits;
				}
				else if (wParam == g_config.vKeyAwayKit)
				{
				}
				else if (wParam == g_config.vKeyGKHomeKit)
				{
				}
				else if (wParam == g_config.vKeyGKAwayKit)
				{
				}
				*/
			}
			break;
	}

	// We must pass the all messages on to CallNextHookEx.
	return;
}

/* Set debug flag */
EXTERN_C LIBSPEC void SetDebug(DWORD flag)
{
	//LogWithNumber(&k_mydll,"Setting g_config.debug flag to %d", flag);
	k_mydll.debug = flag;
}

EXTERN_C LIBSPEC DWORD GetDebug(void)
{
	return k_mydll.debug;
}

/* Set key */
EXTERN_C LIBSPEC void SetKey(int whichKey, int code1)
{
	switch (whichKey)
	{
		case VKEY_HOMEKIT: g_config.vKeyHomeKit = code1; break;
		case VKEY_AWAYKIT: g_config.vKeyAwayKit = code1; break;
		case VKEY_GKHOMEKIT: g_config.vKeyGKHomeKit = code1; break;
		case VKEY_GKAWAYKIT: g_config.vKeyGKAwayKit = code1; break;
	}
}

EXTERN_C LIBSPEC int GetKey(int whichKey)
{
	switch (whichKey)
	{
		case VKEY_HOMEKIT: return g_config.vKeyHomeKit;
		case VKEY_AWAYKIT: return g_config.vKeyAwayKit;
		case VKEY_GKHOMEKIT: return g_config.vKeyGKHomeKit;
		case VKEY_GKAWAYKIT: return g_config.vKeyGKAwayKit;
	}
	return -1;
}

/* Set gdb dir */
EXTERN_C LIBSPEC void SetKdbDir(char* gdbDir)
{
	if (gdbDir == NULL) return;
	ZeroMemory(GetPESInfo()->gdbDir, BUFLEN);
	lstrcpy(GetPESInfo()->gdbDir, gdbDir);
}

EXTERN_C LIBSPEC void GetKdbDir(char* gdbDir)
{
	if (gdbDir == NULL) return;
	ZeroMemory(gdbDir, BUFLEN);
	lstrcpy(gdbDir, GetPESInfo()->gdbDir);
}

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
		LogWithString(&k_mydll,"Dumped to file %s.",name);
	}
}

/**
 * Utility function to dump any data to a named file.
 */
void DumpData(BYTE* addr, DWORD size, char* filename)
{
	// prepare filename
	char name[BUFLEN];
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s", GetPESInfo()->mydir, filename);

	FILE* f = fopen(name, "wb");
	if (f != NULL)
	{
		fwrite(addr, size, 1, f);
		fclose(f);
		LogWithString(&k_mydll,"Dumped to file %s.",name);
	}
}

/**
 * This function saves the textures from decoded
 * uniform buffer into two BMP files: 512x256x8 and 256x128x8
 * (with palette).
 */
void DumpUniforms(TEXIMGPACKHEADER* kitPackHeader)
{
	static int count = 0;
	DWORD* kitPackToc = (DWORD*)((BYTE*)kitPackHeader + kitPackHeader->tocOffset);
	BYTE* buffer = (BYTE*)kitPackHeader + kitPackToc[0];

	// prepare 1st filename
	char name[BUFLEN];
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s%02d.bmp", GetPESInfo()->mydir, "du", count++);

	SaveAs8bitBMP(name, buffer + 0x480, buffer + 0x80, 512, 256);

	/*
	// prepare 2nd filename
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s%02d.bmp", GetPESInfo()->mydir, "du", count++);

	SaveAs8bitBMP(name, buffer + 0x20480, buffer + 0x80, 256, 128);

	// prepare 3rd filename
	ZeroMemory(name, BUFLEN);
	sprintf(name, "%s%s%02d.bmp", GetPESInfo()->mydir, "du", count++);

	SaveAs8bitBMP(name, buffer + 0x28480, buffer + 0x80, 128, 64);
	*/
}

/**
 * This function saves the textures from decoded
 * image-pack into BMP files.
 */
void DumpImagePack(TEXIMGPACKHEADER* imagePackHeader)
{
	static int count = 0;
	int i;
	for (i=0; i<imagePackHeader->numFiles; i++) {
		TEXIMGHEADER* imageHeader = (TEXIMGHEADER*)((BYTE*)imagePackHeader + imagePackHeader->toc[i]);

		// prepare 1st filename
		char name[BUFLEN];
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s%02d.bmp", GetPESInfo()->mydir, "du", count++);

		SaveAsBitmap(name, imageHeader);
	}
}

// Read a file into a memory block.
static int read_file_to_mem(char *fn,unsigned char **ppfiledata, int *pfilesize)
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
					(*ppfiledata)  = fbuf;
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
}

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
                //palette[i].rgbReserved = (alphaValues[i]==0xff) ? 0x80 : alphaValues[i]/2;
                palette[i].rgbReserved = alphaValues[i];
            }
        }
        // move on to next chunk
        offset += sizeof(chunk->dwSize) + sizeof(chunk->dwName) + 
            SWAPBYTES(chunk->dwSize) + sizeof(DWORD); // last one is CRC
    }
}

// Load texture from PNG file. Returns the size of loaded texture
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename)
{
	TRACE4(&k_mydll,"LoadPNGTexture: loading %s", filename);
    DWORD size = 0;

    PNGDIB *pngdib;
    LPBITMAPINFOHEADER* ppDIB = (LPBITMAPINFOHEADER*)tex;

    pngdib = pngdib_p2d_init();
	//TRACE(&k_mydll,"LoadPNGTexture: structure initialized");

    BYTE* memblk;
    int memblksize;
    
    if(read_file_to_mem(filename,&memblk, &memblksize)) {
        TRACE(&k_mydll,"LoadPNGTexture: unable to read PNG file");
        return 0;
    }

    //TRACE(&k_mydll,"LoadPNGTexture: file read into memory");

    pngdib_p2d_set_png_memblk(pngdib,memblk,memblksize);
	pngdib_p2d_set_use_file_bg(pngdib,1);
	pngdib_p2d_run(pngdib);
	
	if (pngdib_p2d_get_colortype(pngdib)==6) { // RGBA+alpha
        BITMAPINFOHEADER *pDIB = NULL;
        pngdib_p2d_get_dib(pngdib, &pDIB, (int*)&size);
        pngdib_p2d_free_dib(pngdib, pDIB);
		pngdib_set_dibalpha32(pngdib,1);
        pngdib_p2d_run(pngdib);
    }

	//TRACE(&k_mydll,"LoadPNGTexture: run done");
    pngdib_p2d_get_dib(pngdib, ppDIB, (int*)&size);
	//TRACE(&k_mydll,"LoadPNGTexture: get_dib done");

    pngdib_done(pngdib);
	TRACE(&k_mydll,"LoadPNGTexture: done done");

	TRACE2(&k_mydll,"LoadPNGTexture: *ppDIB = %08x", (DWORD)*ppDIB);
    if (*ppDIB == NULL) {
		TRACE(&k_mydll,"LoadPNGTexture: ERROR - unable to load PNG image.");
        return 0;
    }

    // initialize palette colors to all non-transparent
    BITMAPINFO* pBMI = (BITMAPINFO*)*ppDIB;
    int colors = pBMI->bmiHeader.biClrUsed;
    //LogWithNumber(&k_mydll, "biClrUsed = %d", colors);
    if (!colors) {
        colors = 1 << (pBMI->bmiHeader.biBitCount);
    }
    //LogWithNumber(&k_mydll, "colors = %d", colors);
    if (colors<=256) {
        for (int i=0; i<colors; i++) {
            pBMI->bmiColors[i].rgbReserved = 0xff;
        }
    }

    // read transparency values from tRNS chunk
    // and put them into DIB's RGBQUAD.rgbReserved fields
    ApplyAlphaChunk((RGBQUAD*)&((BITMAPINFO*)*ppDIB)->bmiColors, memblk, memblksize);

    HeapFree(GetProcessHeap(), 0, memblk);

	TRACE(&k_mydll,"LoadPNGTexture: done");
	return size;
}

// Load texture. Returns the size of loaded texture
DWORD LoadTexture(BITMAPINFO** tex, char* filename)
{
	TRACE4(&k_mydll,"LoadTexture: loading %s", filename);

	FILE* f = fopen(filename, "rb");
	if (f == NULL)
	{
		TRACE(&k_mydll,"LoadTexture: ERROR - unable to open file.");
		return 0;
	}
	BITMAPFILEHEADER bfh;
	fread(&bfh, sizeof(BITMAPFILEHEADER), 1, f);
	DWORD size = bfh.bfSize - sizeof(BITMAPFILEHEADER);

	TRACE2(&k_mydll,"LoadTexture: filesize = %08x", bfh.bfSize);

	*tex = (BITMAPINFO*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	fread(*tex, size, 1, f);
	fclose(f);

	TRACE(&k_mydll,"LoadTexture: done");
	return size;
}

// Substiture kit textures with data from DIB
void ApplyDIBTexture(TEXIMGPACKHEADER* orgKit, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"Applying DIB texture");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)orgKit + orgKit->toc[0]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    BYTE* largeKit = (BYTE*)orgKit + orgKit->toc[0];

	// copy palette from tex0
	TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
	TRACE2(&k_mydll,"palOff  = %08x", palOff);

	for (int bank=0; bank<8; bank++)
	{
		memcpy(largeKit + 0x80 + bank*32*4,        srcTex + palOff + bank*32*4,        8*4);
		memcpy(largeKit + 0x80 + bank*32*4 + 16*4, srcTex + palOff + bank*32*4 + 8*4,  8*4);
		memcpy(largeKit + 0x80 + bank*32*4 + 8*4,  srcTex + palOff + bank*32*4 + 16*4, 8*4);
		memcpy(largeKit + 0x80 + bank*32*4 + 24*4, srcTex + palOff + bank*32*4 + 24*4, 8*4);
	}
	// swap R and B
	for (int i=0; i<0x100; i++) 
	{
		BYTE blue = largeKit[0x80 + i*4];
		BYTE red = largeKit[0x80 + i*4 + 2];
		largeKit[0x80 + i*4] = red;
		largeKit[0x80 + i*4 + 2] = blue;
	}
	TRACE(&k_mydll,"Palette copied.");

	int k, m, j, w;
	int height, width;
	int imageWidth;

	// copy tex0
	width = imageWidth = 512; height = 256;
	for (k=0, m=bih->biHeight-1; k<height, m>=bih->biHeight - height; k++, m--)
	{
		memcpy(largeKit + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
	}
	TRACE(&k_mydll,"Texture 512x256 replaced.");

	if (bih->biHeight == 384) // MipMap kit 
	{
		BYTE* mediumAndSmallKits = (BYTE*)orgKit + orgKit->toc[1];

		// copy palette from largeKit
		memcpy(mediumAndSmallKits + 0x80, largeKit + 0x80, 0x400);

		// copy tex1
		width = 256; height = 128;
		for (k=0, m=127; k<height, m>=0; k++, m--)
		{
			memcpy(mediumAndSmallKits + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
		}
		TRACE(&k_mydll,"Texture 256x128 replaced.");

		// copy tex2
		width = 128; height = 64;
		for (k=0, m=127; k<height, m>=64; k++, m--)
		{
			memcpy(mediumAndSmallKits + 0x8480 + k*width, srcTex + bitsOff + 256 + m*imageWidth, width);
		}
		TRACE(&k_mydll,"Texture 128x64 replaced.");
	}
	else // Single-texture kit
	{
		BYTE* mediumAndSmallKits = (BYTE*)orgKit + orgKit->toc[1];

		// copy palette from largeKit
		memcpy(mediumAndSmallKits + 0x80, largeKit + 0x80, 0x400);

		// resample the texture at half-size in each dimension
		int W = width/2, H = height/2;
		for (k=0, m=height-2; k<H, m>=0; k++, m-=2)
		{
			for (w=0, j=0; w<W, j<width-1; w++, j+=2)
			{
				mediumAndSmallKits[0x480 + k*W + w] = srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 512x256 resampled to 256x128.");
		TRACE(&k_mydll,"Texture 256x128 replaced.");

		// resample the texture at 1/4-size in each dimension
		W = width/4, H = height/4;
		for (k=0, m=height-4; k<H, m>=0; k++, m-=4)
		{
			for (w=0, j=0; w<W, j<width-3; w++, j+=4)
			{
				mediumAndSmallKits[0x8480 + k*W + w] = srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 256x128 resampled to 128x64.");
		TRACE(&k_mydll,"Texture 128x64 replaced.");
	}
}

// OR kit textures with data from DIB
void OrDIBTexture(TEXIMGPACKHEADER* orgKit, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"ORing DIB texture");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)orgKit + orgKit->toc[0]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    BYTE* largeKit = (BYTE*)orgKit + orgKit->toc[0];

	int k, m, j, w;
	int height, width;
	int imageWidth;

	// OR tex0
	width = imageWidth = 512; height = 256;
	for (k=0, m=bih->biHeight-1; k<height, m>=bih->biHeight - height; k++, m--)
	{
		//memcpy(largeKit + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
        for (w=0; w<width; w++) {
            largeKit[0x480 + k*width + w] |= srcTex[bitsOff + m*imageWidth + w];
        }
	}
	TRACE(&k_mydll,"Texture 512x256 OR-ed.");

	if (bih->biHeight == 384) // MipMap kit 
	{
		BYTE* mediumAndSmallKits = (BYTE*)orgKit + orgKit->toc[1];

		// copy tex1
		width = 256; height = 128;
		for (k=0, m=127; k<height, m>=0; k++, m--)
		{
			//memcpy(mediumAndSmallKits + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
            for (w=0; w<width; w++) {
                mediumAndSmallKits[0x480 + k*width + w] |= srcTex[bitsOff + m*imageWidth + w];
            }
		}
		TRACE(&k_mydll,"Texture 256x128 OR-ed.");

		// copy tex2
		width = 128; height = 64;
		for (k=0, m=127; k<height, m>=64; k++, m--)
		{
			//memcpy(mediumAndSmallKits + 0x8480 + k*width, srcTex + bitsOff + 256 + m*imageWidth, width);
            for (w=0; w<width; w++) {
                mediumAndSmallKits[0x8480 + k*width + w] |= srcTex[bitsOff + 256 + m*imageWidth + w];
            }
		}
		TRACE(&k_mydll,"Texture 128x64 OR-ed.");
	}
	else // Single-texture kit
	{
		BYTE* mediumAndSmallKits = (BYTE*)orgKit + orgKit->toc[1];

		// resample the texture at half-size in each dimension
		int W = width/2, H = height/2;
		for (k=0, m=height-2; k<H, m>=0; k++, m-=2)
		{
			for (w=0, j=0; w<W, j<width-1; w++, j+=2)
			{
				mediumAndSmallKits[0x480 + k*W + w] |= srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 512x256 resampled to 256x128.");
		TRACE(&k_mydll,"Texture 256x128 OR-ed.");

		// resample the texture at 1/4-size in each dimension
		W = width/4, H = height/4;
		for (k=0, m=height-4; k<H, m>=0; k++, m-=4)
		{
			for (w=0, j=0; w<W, j<width-3; w++, j+=4)
			{
				mediumAndSmallKits[0x8480 + k*W + w] |= srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 256x128 resampled to 128x64.");
		TRACE(&k_mydll,"Texture 128x64 OR-ed.");
	}
}

// Substiture kit textures with data from DIB
void ApplyDIBTextureMipMap(TEXIMGPACKHEADER* orgKit, int ordinal, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"Applying DIB texture (mipmap)");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)orgKit + orgKit->toc[0]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    BYTE* destData = (BYTE*)orgKit + orgKit->toc[1];

	TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
	TRACE2(&k_mydll,"palOff  = %08x", palOff);

	for (int bank=0; bank<8; bank++)
	{
		memcpy(destData + 0x80 + bank*32*4,        srcTex + palOff + bank*32*4,        8*4);
		memcpy(destData + 0x80 + bank*32*4 + 16*4, srcTex + palOff + bank*32*4 + 8*4,  8*4);
		memcpy(destData + 0x80 + bank*32*4 + 8*4,  srcTex + palOff + bank*32*4 + 16*4, 8*4);
		memcpy(destData + 0x80 + bank*32*4 + 24*4, srcTex + palOff + bank*32*4 + 24*4, 8*4);
	}
	// swap R and B
	for (int i=0; i<0x100; i++) 
	{
		BYTE blue = destData[0x80 + i*4];
		BYTE red = destData[0x80 + i*4 + 2];
		destData[0x80 + i*4] = red;
		destData[0x80 + i*4 + 2] = blue;
	}
	TRACE(&k_mydll,"Palette copied.");

	int k, m, j, w;
    int width = 128*(2-ordinal);
	int height = 64*(2-ordinal);

    DWORD pixelsOff = 0x480 + 0x8000*ordinal;

    for (k=0, m=height-1; k<height, m>=0; k++, m--)
    {
        memcpy(destData + pixelsOff + k*width, srcTex + bitsOff + m*width, width);
    }
    TRACE2(&k_mydll,"Mipmap (%d) texture replaced.", ordinal);
}

// Substiture palette with palette from DIB.
void Apply4BitDIBPalette(TEXIMGHEADER* tex, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"Applying palette from DIB");

	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    // copy palette from image
    TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
    TRACE2(&k_mydll,"palOff  = %08x", palOff);
    memcpy(pal, srcTex + palOff, 16*4); // take only 1st 16 colors

    // swap R and B
    for (int i=0; i<16; i++) 
    {
        BYTE blue = pal[i*4];
        BYTE red = pal[i*4 + 2];
        pal[i*4] = red;
        pal[i*4 + 2] = blue;
    }
    TRACE(&k_mydll,"Palette copied.");
}

// Substiture kit textures with data from DIB.
void Apply4BitDIBTexture(TEXIMGPACKHEADER* pack, BITMAPINFO* bitmap, BOOL usePalette)
{
    TRACE(&k_mydll,"Applying DIB image to 4-bit texture");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)pack + pack->toc[0]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

	if (usePalette) {
		// copy palette from image
		TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
		TRACE2(&k_mydll,"palOff  = %08x", palOff);
		memcpy(pal, srcTex + palOff, 16*4); // take only 1st 16 colors

		// swap R and B
		for (int i=0; i<16; i++) 
		{
			BYTE blue = pal[i*4];
			BYTE red = pal[i*4 + 2];
			pal[i*4] = red;
			pal[i*4 + 2] = blue;
		}
		TRACE(&k_mydll,"Palette copied.");
	}

    if (bih->biBitCount == 4) {
        // copy data as is
        int k, m, j;
        for (k=0, m=bih->biHeight-1; k<tex->height, m>=0; k++, m--)
        {
            memcpy(buf + k*(tex->width/2), srcTex + bitsOff + m*(bih->biWidth/2), tex->width/2);
            // shuffle pixel data
            for (j=0; j<tex->width/2; j++) {
                BYTE src = buf[k*(tex->width/2)+j];
                buf[k*(tex->width/2)+j] = (src << 4 & 0xf0) | (src >> 4 & 0x0f);
            }
        }

    } else if (bih->biBitCount == 8) {
        // need to convert 8-bit data to 4-bit
        int k, m, j;
        for (k=0, m=bih->biHeight-1; k<tex->height, m>=0; k++, m--)
        {
            for (j=0; j<bih->biWidth; j+=2) {
                // combine two bytes of data into one
                // (shuffling it as well)
                buf[k*(tex->width/2)+j/2] = 
                    (srcTex[bitsOff + m*(bih->biWidth) + j] & 0x0f) |
                    (srcTex[bitsOff + m*(bih->biWidth) + j + 1] << 4 & 0xf0);
            }
        }
    }

	TRACE(&k_mydll,"4-bit DIB Texture replaced.");
}

void FreeTexture(BITMAPINFO* bitmap) 
{
	if (bitmap != NULL) {
		HeapFree(GetProcessHeap(), 0, bitmap);
	}
}

void FreePNGTexture(BITMAPINFO* bitmap) 
{
	if (bitmap != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)bitmap);
	}
}

void ApplyGlovesDIBTexture(TEXIMGPACKHEADER* orgKit, BITMAPINFO* bitmap)
{
    TRACE(&k_mydll,"Applying PNG glove texture");

	TEXIMGHEADER* tex = (TEXIMGHEADER*)((BYTE*)orgKit + orgKit->toc[2]);
	BYTE* buf = (BYTE*)tex + tex->dataOffset;
	BYTE* pal = (BYTE*)tex + tex->paletteOffset;

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    BYTE* largeGloves = (BYTE*)orgKit + orgKit->toc[2];

	TRACE2(&k_mydll,"bitsOff = %08x", bitsOff);
	TRACE2(&k_mydll,"palOff  = %08x", palOff);

	for (int bank=0; bank<8; bank++)
	{
		memcpy(largeGloves + 0x80 + bank*32*4,        srcTex + palOff + bank*32*4,        8*4);
		memcpy(largeGloves + 0x80 + bank*32*4 + 16*4, srcTex + palOff + bank*32*4 + 8*4,  8*4);
		memcpy(largeGloves + 0x80 + bank*32*4 + 8*4,  srcTex + palOff + bank*32*4 + 16*4, 8*4);
		memcpy(largeGloves + 0x80 + bank*32*4 + 24*4, srcTex + palOff + bank*32*4 + 24*4, 8*4);
	}
	// swap R and B
	for (int i=0; i<0x100; i++) 
	{
		BYTE blue = largeGloves[0x80 + i*4];
		BYTE red = largeGloves[0x80 + i*4 + 2];
		largeGloves[0x80 + i*4] = red;
		largeGloves[0x80 + i*4 + 2] = blue;
		largeGloves[0x80 + i*4 + 3] = 0x80; // always 100% opacity
	}
	TRACE(&k_mydll,"Palette copied.");

	int k, m, j, w;
	int height, width;
	int imageWidth;

	// copy tex0
	width = imageWidth = 64; height = 128;
	for (k=0, m=bih->biHeight-1; k<height, m>=bih->biHeight - height; k++, m--)
	{
		memcpy(largeGloves + 0x480 + k*width, srcTex + bitsOff + m*imageWidth, width);
	}
	TRACE(&k_mydll,"Texture 64x128 replaced.");

	if (bih->biHeight == 192) // MipMap kit 
	{
		// copy tex1
		width = 32; height = 64;
		for (k=0, m=63; k<height, m>=0; k++, m--)
		{
			memcpy(largeGloves + 0x2480 + k*width, srcTex + bitsOff + m*imageWidth, width);
		}
		TRACE(&k_mydll,"Texture 32x64 replaced.");
	}
	else // Single-texture kit
	{
		// resample the texture at half-size in each dimension
		int W = width/2, H = height/2;
		for (k=0, m=height-2; k<H, m>=0; k++, m-=2)
		{
			for (w=0, j=0; w<W, j<width-1; w++, j+=2)
			{
				largeGloves[0x2480 + k*W + w] = srcTex[bitsOff + m*width + j];
			}
		}
		TRACE(&k_mydll,"Texture 64x128 resampled to 32x64.");
		TRACE(&k_mydll,"Texture 32x64 replaced.");
	}
}

/**
 * Tracker for IDirect3DTexture8::GetSurfaceLevel method.
 */
HRESULT STDMETHODCALLTYPE JuceGetSurfaceLevel(IDirect3DTexture8* self, UINT level,
IDirect3DSurface8** ppSurfaceLevel)
{
	TRACE(&k_mydll,"JuceGetSurfaceLevel: CALLED.");
	TRACE2(&k_mydll,"JuceGetSurfaceLevel: texture(self) = %08x", (DWORD)self);
	TRACE2(&k_mydll,"JuceGetSurfaceLevel: level = %d", level);

	return g_orgGetSurfaceLevel(self, level, ppSurfaceLevel);
}

/**
 * Saves surface into a auto-named file.
 */
void DumpSurface(int count, IDirect3DSurface8* surf)
{
	char filename[BUFLEN];
	ZeroMemory(filename, BUFLEN);
	sprintf(filename, "%ssurf-%03d.bmp", GetPESInfo()->mydir, count);
	D3DXSaveSurfaceToFile(filename, D3DXIFF_BMP, surf, NULL, NULL);
	LogWithString(&k_mydll,"Dumped to %s",filename);
}

/**
 * Tracker for IDirect3DDevice8::CreateTexture method.
 */
HRESULT STDMETHODCALLTYPE JuceCreateTexture(IDirect3DDevice8* self, UINT width, UINT height,UINT levels,
DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture8** ppTexture, DWORD src, bool* IsProcessed)
{
	HRESULT res=D3D_OK;

	//TRACEX(&k_mydll,"JuceCreateTexture: (%dx%d), levels = %d", width, height, levels);
	
	if (*IsProcessed==true)
		return res;
	
	// reset the pointer
	g_lastMediumTex = NULL;
	g_lastBigTex = NULL;

	// Kit Texture Booster
	bool gdbKit=false;
	if (isGdbKit.find(src) != isGdbKit.end())
		gdbKit=isGdbKit[src];
		
	/*
	CRC check doesn't work at the moment as unfortunatly the kit texture isn't
	split in UniSplit but actually a bit later so a wrong CRC is calculated and
	this check always fails.
	*/
	if (gdbKit) {
		if (0){//gdbKitCRC[src]!=GetCRC((BYTE*)src,gdbKitSize[src])) {
			//wrong CRC, delete related information
			NewKitFilesMap.erase(src);
			isGdbKit.erase(src);
			gdbKitSize.erase(src);
			gdbKitCRC.erase(src);
			gdbKit=false;
		};
	};

    if (width==256 && height==128 && levels==2) {
        LOG(&k_mydll, "CreateTexture:: (256x128x2), src=%p, gdbKit=%d",
                src, gdbKit);
    }
	
	
	if (!IsBadReadPtr((LPVOID)src,sizeof(DECBUFFERHEADER)) && gdbKit==true) {
		//Create the new kits (or get them from memory if they are already there)
		boostTex=CreateNewKits(NewKitFilesMap[src]);
		if (boostTex==NULL) goto NoReplacingTexture;
		
		DECBUFFERHEADER* d=(DECBUFFERHEADER*)src;

        g_overlayfilename = NULL;
        std::map<DWORD,NEWKITFILES>::iterator it = NewKitFilesMap.find(src);
        if (it != NewKitFilesMap.end()) {
            g_overlayfilename = it->second.overlayName;
        }

		//DumpData((BYTE*)src,d->dwDecSize);

		TRACE2(&k_mydll,"width  = %d", (DWORD)width);
		TRACE2(&k_mydll,"height = %d", (DWORD)height);
		TRACE2(&k_mydll,"levels = %d", (DWORD)levels);
		TRACE2(&k_mydll,"usage  = %d", (DWORD)usage);
		TRACE2(&k_mydll,"format = %d", (DWORD)format);
		TRACE2(&k_mydll,"pool   = %d", (DWORD)pool);
		
		if (d->texWidth==512 && d->texHeight==256) {
			LogWithNumber(&k_mydll,"CreateTexture (512x256) of %x",src);
			TRACE(&k_mydll,"JuceCreateTexture: Using LARGER big texture for the kit.");
			res = OrgCreateTexture(self, boostTex->sizeBig.right, boostTex->sizeBig.bottom,
					levels,usage,D3DFMT_A8R8G8B8,pool,ppTexture);
			g_lastBigTex = *ppTexture;
			TRACE2(&k_mydll,"big tex = %08x", (DWORD)g_lastBigTex);
			
		} else if (d->texWidth==256 && d->texHeight==128) {
			LogWithNumber(&k_mydll,"CreateTexture (256x128) of %x",src);
			TRACE(&k_mydll,"JuceCreateTexture: Using LARGER middle texture for the kit.");
			res = OrgCreateTexture(self, boostTex->sizeMedium.right, boostTex->sizeMedium.bottom,
					levels,usage,D3DFMT_A8R8G8B8,pool,ppTexture);
			g_lastMediumTex = *ppTexture;
			TRACE2(&k_mydll,"med tex = %08x", (DWORD)g_lastMediumTex);
			
		};
	};

	NoReplacingTexture:

	if (width == 512 && height == 256)
		TRACE2(&k_mydll,"BIG-tex (512x256): %08x", (DWORD)*ppTexture);
/*
	if (lastBig != NULL)
	{
		IDirect3DSurface8* s = NULL;
		lastBig->GetSurfaceLevel(0, &s);
		DumpSurface(bigCount++, s);
		lastBig = NULL;
	}
	if (texToBoostType==BOOST_NONE && width == 512 && height == 256)
		lastBig = *ppTexture;
*/

	if ((g_lastMediumTex != NULL) || (g_lastBigTex != NULL))
		*IsProcessed=true;

	return res;
}

void JuceUnlockRect(IDirect3DTexture8* self,UINT Level)
{
	//LogWithTwoNumbers(&k_mydll,"JuceUnlockRect: Processing texture %x, level %d",(DWORD)self,Level);
	
	if (boostTex==NULL)
		return;
		
	IDirect3DTexture8* texToBoost = NULL;
	IDirect3DSurface8* g_med0 = NULL;
	IDirect3DSurface8* g_med1 = NULL;
	HANDLE pHeap=GetProcessHeap();
	BYTE* g_newSmallKitTex = NULL;
	BYTE* g_newMediumKitTex = NULL;
	HRESULT r1, r2;
	
	#define BOOST_NONE 0
	#define BOOST_MED 1
	#define BOOST_BIG 2
	int texToBoostType = BOOST_NONE;
	
	// Boost the kit texture

	if (g_lastMediumTex != NULL) {
		texToBoost = g_lastMediumTex;
		texToBoostType=BOOST_MED;
	} else if (g_lastBigTex != NULL) {
		texToBoost = g_lastBigTex;
		texToBoostType=BOOST_BIG;
	} else {
		texToBoost=NULL;
		texToBoostType=BOOST_NONE;
	};
		
	switch (texToBoostType) {
	//Big texture boosting
	case BOOST_BIG:
		// shift the surfaces.
		// MED,level 1 <- MED,level 0
		// MED,level 0 <- big texture buffer data

		r1 = texToBoost->GetSurfaceLevel(0, &g_med0);
		TRACE2(&k_mydll,"BIG: g_med0 = %08x", (DWORD)g_med0);
		//DumpSurface(bigCount++, g_med0);

		if (SUCCEEDED(r1))
		{
			//DumpSurface(bigCount++, g_med0);
			// copy pixels
			/*D3DLOCKED_RECT lr;
			g_med0->LockRect(&lr,NULL,0);
			DumpData((BYTE*)lr.pBits,512*256*4);
			g_med0->UnlockRect();*/
			
			if (SUCCEEDED(D3DXLoadSurfaceFromMemory(
							g_med0, NULL, NULL,
							boostTex->tex, D3DFMT_A8R8G8B8,
							boostTex->sizeBig.right*4,NULL,
							&(boostTex->sizeBig), D3DX_FILTER_NONE, 0)))
			{ 
				TRACE(&k_mydll,"BIG: g_med0 <- buffer COMPLETE");

                // apply overlay
                D3DLOCKED_RECT rect;
	            D3DSURFACE_DESC desc;
                if (!g_config.disableOverlays && g_overlayfilename && g_overlayfilename[0]!='\0') {
                    if (SUCCEEDED(g_med0->LockRect(&rect, NULL, 0))) {
                        texToBoost->GetLevelDesc(0, &desc);
                        ApplyOverlay(&rect, g_overlayfilename, &desc);
                        g_med0->UnlockRect();
                        TRACE(&k_mydll,"BIG: g_med0: overlay applied");
                    }
                }
			}
			else
			{
				TRACE(&k_mydll,"BIG: g_med0 <- buffer FAILED");
			}
			/*g_med0->LockRect(&lr,NULL,0);
			DumpData((BYTE*)lr.pBits,512*256*4);
			g_med0->UnlockRect();
			DumpSurface(bigCount++, g_med0);*/
		}

		if (SUCCEEDED(r1)) g_med0->Release();
		g_lastBigTex=NULL;
		
		break;
	
	//Medium texture boosting
	case BOOST_MED:
		// shift the surfaces.
		// MED,level 1 <- MED,level 0
		// MED,level 0 <- big texture buffer data
		
		//The smallest image will be overwritten by PES if it hasn't been processed yet
		if (Level!=1)
			return;
        //texToBoost = self;
		HRESULT r1 = texToBoost->GetSurfaceLevel(0, &g_med0);
		TRACE2(&k_mydll,"MED: g_med0 = %08x", (DWORD)g_med0);
		//DumpSurface(bigCount++, g_med0);
		HRESULT r2 = texToBoost->GetSurfaceLevel(1, &g_med1);
		TRACE2(&k_mydll,"MED: g_med1 = %08x", (DWORD)g_med1);
		//DumpSurface(bigCount++, g_med1);
		if (SUCCEEDED(r1) && SUCCEEDED(r2))
		{
			//Resample the low-res images from the high-res one
			g_newMediumKitTex = (BYTE*)HeapAlloc(pHeap, HEAP_ZERO_MEMORY,
						CalculateNewKitsSize(&(boostTex->sizeMedium)));
			ResampleImage(g_newMediumKitTex,boostTex->tex,&(boostTex->sizeMedium),
						&(boostTex->sizeBig));
			
			g_newSmallKitTex = (BYTE*)HeapAlloc(pHeap, HEAP_ZERO_MEMORY,
						CalculateNewKitsSize(&(boostTex->sizeSmall)));
			ResampleImage(g_newSmallKitTex,boostTex->tex,&(boostTex->sizeSmall),
						&(boostTex->sizeBig));
			// copy pixels
			if (g_newSmallKitTex != NULL) {
				//DumpSurface(bigCount++, g_med1);
				if (SUCCEEDED(D3DXLoadSurfaceFromMemory(
								g_med1, NULL, NULL,
								g_newSmallKitTex, D3DFMT_A8R8G8B8,
								boostTex->sizeSmall.right*4,NULL,
								&(boostTex->sizeSmall), D3DX_FILTER_NONE, 0)))
				{
					TRACE(&k_mydll,"MED: g_med1 <- g_med0 COMPLETE");

                    // apply overlay
                    D3DLOCKED_RECT rect;
                    D3DSURFACE_DESC desc;
                    if (!g_config.disableOverlays && g_overlayfilename && g_overlayfilename[0]!='\0') {
                        if (SUCCEEDED(g_med1->LockRect(&rect, NULL, 0))) {
                            texToBoost->GetLevelDesc(1, &desc);
                            ApplyOverlay(&rect, g_overlayfilename, &desc);
                            g_med1->UnlockRect();
                            TRACE(&k_mydll,"MED: g_med1: overlay applied");
                        }
                    }
				}
				else
				{
					TRACE(&k_mydll,"MED: g_med1 <- g_med0 FAILED");
				}
				//DumpSurface(bigCount++, g_med1);
			};
			if (g_newMediumKitTex != NULL) {
				//DumpSurface(bigCount++, g_med0);
				if (SUCCEEDED(D3DXLoadSurfaceFromMemory(
								g_med0, NULL, NULL,
								g_newMediumKitTex, D3DFMT_A8R8G8B8,
								boostTex->sizeMedium.right*4,NULL,
								&(boostTex->sizeMedium), D3DX_FILTER_NONE, 0)))
				{
					TRACE(&k_mydll,"MED: g_med0 <- buffer COMPLETE");

                    // apply overlay
                    D3DLOCKED_RECT rect;
                    D3DSURFACE_DESC desc;
                    if (!g_config.disableOverlays && g_overlayfilename && g_overlayfilename[0]!='\0') {
                        if (SUCCEEDED(g_med0->LockRect(&rect, NULL, 0))) {
                            texToBoost->GetLevelDesc(0, &desc);
                            ApplyOverlay(&rect, g_overlayfilename, &desc);
                            g_med0->UnlockRect();
                            TRACE(&k_mydll,"MED: g_med0: overlay applied");
                        }
                    }
				}
				else
				{
					TRACE(&k_mydll,"MED: g_med0 <- buffer FAILED");
				}
				//DumpSurface(bigCount++, g_med0);
			};
			
			HeapFree(pHeap,0,g_newMediumKitTex);
			g_newMediumKitTex=NULL;
			HeapFree(pHeap,0,g_newSmallKitTex);
			g_newSmallKitTex=NULL;
		}

		if (SUCCEEDED(r1)) g_med0->Release();
		if (SUCCEEDED(r2)) g_med1->Release();
		g_lastMediumTex=NULL;

        self->AddDirtyRect(NULL);
		break;
	};

	return;
};

/***
 * Tracker for IDirect3DDevice8::SetTextureStageState method
 */
HRESULT STDMETHODCALLTYPE JuceSetTextureStageState(IDirect3DDevice8* self, DWORD Stage,
D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	TRACE(&k_mydll,"JuceSetTextureStageState: CALLED.");
	HRESULT res = g_orgSetTextureStageState(self, Stage, Type, Value);
	return res;
}

/***
 * Tracker for IDirect3DDevice8::UpdateTexture method
 */
HRESULT STDMETHODCALLTYPE JuceUpdateTexture(IDirect3DDevice8* self,
IDirect3DBaseTexture8* pSrc, IDirect3DBaseTexture8* pDest)
{
	TRACE(&k_mydll,"JuceUpdateTexture: CALLED.");
	HRESULT res = g_orgUpdateTexture(self, pSrc, pDest);
	return res;
}

/**
 * Checks if offset matches any of the kits offsets.
 */
BOOL IsKitOffset(DWORD offset)
{
	BOOL result = false;

	// first check: bibs
	if (g_bibs.dwOffset == offset)
		return true;

	// rough approximation (boundary) check:
	if (offset < g_teamInfo[0].ga.dwOffset || g_teamInfo[g_numTeams-1].vg.dwOffset < offset)
		return false;

	// precise check: check every kit
	for (int i=0; i<g_numTeams; i++) 
	{
		if (g_teamInfo[i].ga.dwOffset == offset) return true;
		if (g_teamInfo[i].gb.dwOffset == offset) return true;
		if (g_teamInfo[i].pa.dwOffset == offset) return true;
		if (g_teamInfo[i].pb.dwOffset == offset) return true;
		if (g_teamInfo[i].vg.dwOffset == offset) return true;
	}

	return false;
}

// determines item id, based on offset.
MEMITEMINFO* FindMemItemInfoByOffset(DWORD offset)
{
    EnterCriticalSection(&g_cs);
    MEMITEMINFO* memItemInfo = g_AFS_offsetMap[offset];
    LeaveCriticalSection(&g_cs);
    return memItemInfo;
}

// determines item id, based on offset.
MEMITEMINFO* FindMemItemInfoByAddress(DWORD address)
{
    EnterCriticalSection(&g_cs);
    MEMITEMINFO* memItemInfo = g_AFS_addressMap[address];
    g_AFS_addressMap.erase(address);
    LeaveCriticalSection(&g_cs);
    return memItemInfo;
}

/**
 * Monitors the file pointer.
 */
bool JuceReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped)
{
	// get current file pointer
	DWORD dwOffset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);  

    DWORD afsId = GetAfsIdByOpenHandle(hFile);
    if (afsId != 1) {
        return false; // not 0_text.afs
    }

	// search for such offset
	MEMITEMINFO* memItemInfo = FindMemItemInfoByOffset(dwOffset);
	if (memItemInfo != NULL) {
		// there is an itemInfo with such offset
		// associate a memory location with it - for future use
		memItemInfo->address = (DWORD)lpBuffer;
        g_AFS_addressMap[memItemInfo->address] = memItemInfo;

        TRACE2(&k_mydll,"JuceReadFile: memItemInfo->id = %d", memItemInfo->id);
        TRACE2(&k_mydll,"JuceReadFile: memItemInfo->afsItemInfo.dwOffset = %08x", memItemInfo->afsItemInfo.dwOffset);
        TRACE2(&k_mydll,"JuceReadFile: lpBuffer = %08x", (DWORD)lpBuffer);
        return true;
	}

	return false;
}

/**
 * This function calls the unpack function for non-kits.
 * Parameters:
 *   addr1   - address of the encoded buffer (without header)
 *   addr2   - address of the decoded buffer
 *   size1   - size of the encoded buffer (minus header)
  *  zero    - always zero
 *   size2   - pointer to size of the decoded buffer
 */
void JuceUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2, DWORD result)
{
    TRACE(&k_mydll,"JuceUnpack: CALLED.");
	/*
	TRACE2(&k_mydll,"JuceUnpack: addr1 = %08x", addr1);
	TRACE2(&k_mydll,"JuceUnpack: addr2 = %08x", addr2);
	TRACE2(&k_mydll,"JuceUnpack: size1 = %08x", size1);
	TRACE2(&k_mydll,"JuceUnpack: size2 = %08x", size2);
	*/
    TRACE2(&k_mydll,"JuceUnpack: addr = %08x", addr1 - 0x20);
    bool replaced = FALSE;

    //BYTE* strips = (BYTE*)data[TEAM_STRIPS];
    //LogWithTwoNumbers(&k_mydll,"strip = %02x %02x", strips[0], strips[1]);

    MEMITEMINFO* memItemInfo = FindMemItemInfoByAddress(addr1 - 0x20);
    if (memItemInfo) {
        LogWithNumber(&k_mydll,"JuceUnpack: Loading id = %d", memItemInfo->id);

        if (!IsNumOrFontTexture(memItemInfo->id)) {
            TRACE(&k_mydll,"JuceUnpack: SEVERE WARNING!: this is NOT a num/font texture.");
            return;
        };

		// find corresponding file
		BITMAPINFO* tex = NULL;
		DWORD texSize = 0;

		char filename[BUFLEN];
		ZeroMemory(filename, BUFLEN);
        DWORD texType = IsNumbersTexture(memItemInfo->id) ?
            FindImageFileForId2(memItemInfo->id, "", filename) :
            FindImageFileForId(memItemInfo->id, "", filename);

        switch (texType) {
            case TEXTYPE_PNG:
                LogWithString(&k_mydll,"JuceUnpack: PNG Image file = %s", filename);
                texSize = LoadPNGTexture(&tex, filename);
                if (texSize > 0) {
                    Apply4BitDIBTexture((TEXIMGPACKHEADER*)result, tex, TRUE);
                    FreePNGTexture(tex);
                    replaced = TRUE;
                }
                break;
            case TEXTYPE_BMP:
                LogWithString(&k_mydll,"JuceUnpack: BMP Image file = %s", filename);
                texSize = LoadTexture(&tex, filename);
                if (texSize > 0) {
                    Apply4BitDIBTexture((TEXIMGPACKHEADER*)result, tex, TRUE);
                    FreeTexture(tex);
                    replaced = TRUE;
                }
                break;
        }

        TEXIMGPACKHEADER* pack = (TEXIMGPACKHEADER*)result;
        TEXIMGHEADER* top = (TEXIMGHEADER*)((BYTE*)pack + pack->toc[0]);
        if (replaced && pack->numFiles == 3 && top->width == 256 && top->height == 128) {
            // looks like an numbers image.
            // In this case we need to load 2 more images
            // to substitute palettes for shorts

            // for home shorts
            TEXIMGHEADER* palAtex = (TEXIMGHEADER*)((BYTE*)pack + pack->toc[1]);

            int id1 = memItemInfo->id;
            int fileType = (memItemInfo->id - data[FIRST_ID]) % 20;
            switch (fileType) {
                case GA_NUMBERS: id1 = memItemInfo->id - GA_NUMBERS + GA_UNKNOWN1; break;
                case GB_NUMBERS: id1 = memItemInfo->id - GB_NUMBERS + GA_UNKNOWN1; break;
                case PA_NUMBERS: id1 = memItemInfo->id - PA_NUMBERS + PA_SHORTS; break;
                case PB_NUMBERS: id1 = memItemInfo->id - PB_NUMBERS + PA_SHORTS; break;
            }
            string kitFolderKey1 = GetKitFolderKey(id1);

            ZeroMemory(filename, BUFLEN);
            texType = FindShortsPalImageFileForId(memItemInfo->id, kitFolderKey1, filename);
            switch (texType) {
                case TEXTYPE_PNG:
                    LogWithString(&k_mydll,"JuceUnpack: PNG Image file = %s", filename);
                    texSize = LoadPNGTexture(&tex, filename);
                    if (texSize > 0) {
                        Apply4BitDIBPalette(palAtex, tex);
                        FreePNGTexture(tex);
                    }
                    break;
                case TEXTYPE_BMP:
                    LogWithString(&k_mydll,"JuceUnpack: BMP Image file = %s", filename);
                    texSize = LoadTexture(&tex, filename);
                    if (texSize > 0) {
                        Apply4BitDIBPalette(palAtex, tex);
                        FreeTexture(tex);
                    }
                    break;
            }

            // for away shorts
            TEXIMGHEADER* palBtex = (TEXIMGHEADER*)((BYTE*)pack + pack->toc[2]);

            int id2 = memItemInfo->id;
            switch (fileType) {
                case GA_NUMBERS: id2 = memItemInfo->id - GA_NUMBERS + GB_UNKNOWN1; break;
                case GB_NUMBERS: id2 = memItemInfo->id - GB_NUMBERS + GB_UNKNOWN1; break;
                case PA_NUMBERS: id2 = memItemInfo->id - PA_NUMBERS + PB_SHORTS; break;
                case PB_NUMBERS: id2 = memItemInfo->id - PB_NUMBERS + PB_SHORTS; break;
            }
            string kitFolderKey2 = GetKitFolderKey(id2);

            ZeroMemory(filename, BUFLEN);
            texType = FindShortsPalImageFileForId(memItemInfo->id, kitFolderKey2, filename);
            switch (texType) {
                case TEXTYPE_PNG:
                    LogWithString(&k_mydll,"JuceUnpack: PNG Image file = %s", filename);
                    texSize = LoadPNGTexture(&tex, filename);
                    if (texSize > 0) {
                        Apply4BitDIBPalette(palBtex, tex);
                        FreePNGTexture(tex);
                    }
                    break;
                case TEXTYPE_BMP:
                    LogWithString(&k_mydll,"JuceUnpack: BMP Image file = %s", filename);
                    texSize = LoadTexture(&tex, filename);
                    if (texSize > 0) {
                        Apply4BitDIBPalette(palBtex, tex);
                        FreeTexture(tex);
                    }
                    break;
            }
        }

		//ENCBUFFERHEADER* header = (ENCBUFFERHEADER*)(addr1 - 0x20);
		//DumpData((BYTE*)result, header->dwDecSize);
		//DumpImagePack((TEXIMGPACKHEADER*)result);
    }

    TRACE(&k_mydll,"JuceUnpack: done.");

	return;
}

/**
 * Simple file-check routine.
 */
BOOL FileExists(char* filename)
{
    TRACE4(&k_mydll,"FileExists: Checking file: %s", filename);
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

string GetKitFolderKey(DWORD id)
{
    WORD teamId = (id - data[FIRST_ID]) / 20;
    int fileType = (id - data[FIRST_ID]) % 20;
    //LogWithNumber(&k_mydll,"GetKitFolderKey: fileType = %d", fileType);

    WORD home = GetTeamId(HOME);
    WORD away = GetTeamId(AWAY);

    // Important check: if teams changed, then we need to clear kit keys
    if (home != g_last_home || away != g_last_away) {
        ClearKitKeys();
        g_last_home = home;
        g_last_away = away;
    }

    BOOL isHome = TRUE;
    if (home == away) {
        BYTE* strips = (BYTE*)data[TEAM_STRIPS];
        // special case. We need to find out then
        // which team this file is being loaded for: away or home
        switch (fileType) {
            case GA_KIT:
            case GA_UNKNOWN1:
            case GA_UNKNOWN2:
            case GA_NUMBERS:
            case GA_FONT:
                isHome = (strips[0] & STRIP_GK_SHIRT) == 0;
                break;
            case GB_KIT:
            case GB_UNKNOWN1:
            case GB_UNKNOWN2:
            case GB_NUMBERS:
            case GB_FONT:
                isHome = (strips[1] & STRIP_GK_SHIRT) == 0;
                break;
            case PA_SHORTS:
                isHome = (strips[0] & STRIP_PL_SHORTS) == 0;
                break;
            case PA_SOCKS:
                isHome = (strips[0] & STRIP_PL_SOCKS) == 0;
                break;
            case PA_SHIRT:
            case PA_NUMBERS:
            case PA_FONT:
                isHome = (strips[0] & STRIP_PL_SHIRT) == 0;
                break;
            case PB_SHORTS:
                isHome = (strips[1] & STRIP_PL_SHORTS) == 0;
                break;
            case PB_SOCKS:
                isHome = (strips[1] & STRIP_PL_SOCKS) == 0;
                break;
            case PB_SHIRT:
            case PB_NUMBERS:
            case PB_FONT:
            default:
                isHome = (strips[1] & STRIP_PL_SHIRT) == 0;
        }
    } else {
        // normal case: two different teams
        isHome = (home == teamId);
    }

    switch (fileType) {
        case GA_KIT:


        case GA_NUMBERS:
        case GA_FONT: {
            string& key = (isHome) ? GET_HOME_SHIRT_KEY(GK_TYPE) : GET_AWAY_SHIRT_KEY(GK_TYPE);
            return (key.length()>0) ? key : "ga";
                      }
        case GA_UNKNOWN1: {
            string& key = (isHome) ? GET_HOME_SHORTS_KEY(GK_TYPE) : GET_AWAY_SHORTS_KEY(GK_TYPE);
            return (key.length()>0) ? key : "ga";
                      }
        case GA_UNKNOWN2: {
            string& key = (isHome) ? GET_HOME_SOCKS_KEY(GK_TYPE) : GET_AWAY_SOCKS_KEY(GK_TYPE);
            return (key.length()>0) ? key : "ga";
                      }
        case GB_KIT:


        case GB_NUMBERS:
        case GB_FONT: {
            string& key = (isHome) ? GET_HOME_SHIRT_KEY(GK_TYPE) : GET_AWAY_SHIRT_KEY(GK_TYPE);
            return (key.length()>0) ? key : "gb";
                      }
        case GB_UNKNOWN1: {
            string& key = (isHome) ? GET_HOME_SHORTS_KEY(GK_TYPE) : GET_AWAY_SHORTS_KEY(GK_TYPE);
            return (key.length()>0) ? key : "gb";
                      }
        case GB_UNKNOWN2: {
            string& key = (isHome) ? GET_HOME_SOCKS_KEY(GK_TYPE) : GET_AWAY_SOCKS_KEY(GK_TYPE);
            return (key.length()>0) ? key : "gb";
                      }
        case PA_SHIRT:
        case PA_NUMBERS:
        case PA_FONT: {
            string& key = (isHome) ? GET_HOME_SHIRT_KEY(PL_TYPE) : GET_AWAY_SHIRT_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pa";
                      }
        case PA_SHORTS: {
            string& key = (isHome) ? GET_HOME_SHORTS_KEY(PL_TYPE) : GET_AWAY_SHORTS_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pa";
                        }
        case PA_SOCKS: {
            string& key = (isHome) ? GET_HOME_SOCKS_KEY(PL_TYPE) : GET_AWAY_SOCKS_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pa";
                       }
        case PB_SHIRT:
        case PB_NUMBERS:
        case PB_FONT: {
            string& key = (isHome) ? GET_HOME_SHIRT_KEY(PL_TYPE) : GET_AWAY_SHIRT_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pb";
                      }
        case PB_SHORTS: {
            string& key = (isHome) ? GET_HOME_SHORTS_KEY(PL_TYPE) : GET_AWAY_SHORTS_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pb";
                        }
        case PB_SOCKS: {
            string& key = (isHome) ? GET_HOME_SOCKS_KEY(PL_TYPE) : GET_AWAY_SOCKS_KEY(PL_TYPE);
            return (key.length()>0) ? key : "pb";
                       }
    }

    return "(null)";
}

BOOL FindImageFileForIdEx(DWORD id, char* suffix, char* filename, char* ext)
{
    WORD teamId = (id - data[FIRST_ID]) / 20;
    int fileType = (id - data[FIRST_ID]) % 20;

    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (!col) {
        // no kit collection for this team 
        return FALSE;
    }
    Kit* kit = NULL;

    char fileClue1[BUFLEN];
    ZeroMemory(fileClue1, BUFLEN);

    string kitKey = GetKitFolderKey(id);

    switch (fileType) {
        case GA_KIT:
            sprintf(fileClue1,"%s\\shirt", kitKey.c_str());
            break;
        case GA_UNKNOWN1:
            sprintf(fileClue1,"%s\\shorts", kitKey.c_str());
            break;
        case GA_UNKNOWN2:
            sprintf(fileClue1,"%s\\socks", kitKey.c_str());
            break;
        case GA_NUMBERS:
            kit = MAP_FIND(col->goalkeepers,kitKey);
            sprintf(fileClue1,"%s\\%s", kitKey.c_str(), (kit)?kit->numbersFile:"");
            break;
        case GA_FONT:
            sprintf(fileClue1,"%s\\font", kitKey.c_str());
            break;
        case GB_KIT:
            sprintf(fileClue1,"%s\\shirt", kitKey.c_str());
            break;
        case GB_UNKNOWN1:
            sprintf(fileClue1,"%s\\shorts", kitKey.c_str());
            break;
        case GB_UNKNOWN2:
            sprintf(fileClue1,"%s\\socks", kitKey.c_str());
            break;
        case GB_NUMBERS:
            kit = MAP_FIND(col->goalkeepers,kitKey);
            sprintf(fileClue1,"%s\\%s", kitKey.c_str(), (kit)?kit->numbersFile:"");
            break;
        case GB_FONT:
            sprintf(fileClue1,"%s\\font", kitKey.c_str());
            break;
        case PA_SHIRT:
            sprintf(fileClue1,"%s\\shirt", kitKey.c_str());
            break;
        case PA_SHORTS:
            sprintf(fileClue1,"%s\\shorts", kitKey.c_str());
            break;
        case PA_SOCKS:
            sprintf(fileClue1,"%s\\socks", kitKey.c_str());
            break;
        case PA_NUMBERS:
            kit = MAP_FIND(col->players,kitKey);
            sprintf(fileClue1,"%s\\%s", kitKey.c_str(), (kit)?kit->numbersFile:"");
            break;
        case PA_FONT:
            sprintf(fileClue1,"%s\\font", kitKey.c_str());
            break;
        case PB_SHIRT:
            sprintf(fileClue1,"%s\\shirt", kitKey.c_str());
            break;
        case PB_SHORTS:
            sprintf(fileClue1,"%s\\shorts", kitKey.c_str());
            break;
        case PB_SOCKS:
            sprintf(fileClue1,"%s\\socks", kitKey.c_str());
            break;
        case PB_NUMBERS:
            kit = MAP_FIND(col->players,kitKey);
            sprintf(fileClue1,"%s\\%s", kitKey.c_str(), (kit)?kit->numbersFile:"");
            break;
        case PB_FONT:
            sprintf(fileClue1,"%s\\font", kitKey.c_str());
            break;
    }

    char filename1[BUFLEN];
    ZeroMemory(filename1, BUFLEN);
    sprintf(filename1, "%sGDB\\uni\\%s\\%s%s%s", GetPESInfo()->gdbDir, col->foldername, fileClue1, suffix, ext);

    if (FileExists(filename1)) {
        lstrcpy(filename, filename1);
        return TRUE;
    } else {
        // fallback: in case of shirt/shorts/socks - check for "all" filename
        char fileClue2[BUFLEN];
        ZeroMemory(fileClue2, BUFLEN);

        switch (fileType) {
            case GA_KIT:
            case GA_UNKNOWN1:
            case GA_UNKNOWN2:
                sprintf(fileClue2,"%s\\all", kitKey.c_str());
                break;
            case GB_KIT:
            case GB_UNKNOWN1:
            case GB_UNKNOWN2:
                sprintf(fileClue2,"%s\\all", kitKey.c_str());
                break;
            case PA_SHIRT:
            case PA_SHORTS:
            case PA_SOCKS:
                sprintf(fileClue2,"%s\\all", kitKey.c_str());
                break;
            case PB_SHIRT:
            case PB_SHORTS:
            case PB_SOCKS:
                sprintf(fileClue2,"%s\\all", kitKey.c_str());
                break;
        }

        char filename2[BUFLEN];
        ZeroMemory(filename2, BUFLEN);
        sprintf(filename2, "%sGDB\\uni\\%s\\%s%s%s", GetPESInfo()->gdbDir, col->foldername, fileClue2, suffix, ext);

        if (FileExists(filename2)) {
            lstrcpy(filename, filename2);
            return TRUE;
        }
    }
    
    filename[0] = '\0';
    return FALSE;
}

DWORD FindImageFileForId(DWORD id, char* suffix, char* filename)
{
    if (FindImageFileForIdEx(id, suffix, filename, ".png")) {
        return TEXTYPE_PNG;
    } else if (FindImageFileForIdEx(id, suffix, filename, ".bmp")) {
        return TEXTYPE_BMP;
    }
    return TEXTYPE_NONE;
}

DWORD FindImageFileForId2(DWORD id, char* suffix, char* filename)
{
    if (FindImageFileForIdEx(id, suffix, filename, "")) {
        int n = lstrlen(filename);
        char* ext = (n>3)?filename+n-4:"";
        if (lstrcmpi(ext, ".png")==0) {
            return TEXTYPE_PNG;
        } else if (lstrcmpi(ext, ".bmp")==0) {
            return TEXTYPE_BMP;
        }
    }
    return TEXTYPE_NONE;
}

DWORD FindShortsPalImageFileForId(DWORD id, string& kitFolderKey, char* filename)
{
    WORD teamId = (id - data[FIRST_ID]) / 20;
    int fileType = (id - data[FIRST_ID]) % 20;

    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    if (col) {
        Kit* kit = (fileType < PA_SHIRT) ?
            MAP_FIND(col->goalkeepers,GetKitFolderKey(id)) :
            MAP_FIND(col->players,GetKitFolderKey(id));
        if (!kit) {
            return TEXTYPE_NONE;
        }
        LogWithString(&k_mydll,"FindShortsPal: kitFolderKey = {%s}", (char*)kitFolderKey.c_str());
        char* palFile = MAP_FIND(kit->shortsPaletteFiles,kitFolderKey);
        char filename1[BUFLEN];
        ZeroMemory(filename1, BUFLEN);
        sprintf(filename1, "%s%s\\%s", GetPESInfo()->gdbDir, kit->foldername, palFile);
        LogWithString(&k_mydll,"FindShortsPal: filename1 = {%s}", filename1);

        if (FileExists(filename1)) {
            lstrcpy(filename, filename1);
            int n = lstrlen(filename);
            char* ext = (n>3)?filename+n-4:"";
            if (lstrcmpi(ext, ".png")==0) {
                return TEXTYPE_PNG;
            } else if (lstrcmpi(ext, ".bmp")==0) {
                return TEXTYPE_BMP;
            }
        }
    }
    return TEXTYPE_NONE;
}

BOOL IsNumOrFontTexture(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % 20;
	switch (ordinal) {
		case GA_NUMBERS:
		case GA_FONT:
		case GB_NUMBERS:
		case GB_FONT:
		case PA_NUMBERS:
		case PA_FONT:
		case PB_NUMBERS:
		case PB_FONT:
			return TRUE;
	}
	return FALSE;
}

BOOL IsNumbersTexture(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % 20;
	switch (ordinal) {
		case GA_NUMBERS:
		case GB_NUMBERS:
		case PA_NUMBERS:
		case PB_NUMBERS:
			return TRUE;
	}
	return FALSE;
}

BOOL IsGK(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % 20;
	switch (ordinal) {
		case GA_KIT:
		case GB_KIT:
			return TRUE;
	}
	return FALSE;
}

BOOL IsGKTexture(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % 20;
	switch (ordinal) {
		case GA_KIT:
		case GA_UNKNOWN1:
		case GA_UNKNOWN2:
		case GA_NUMBERS:
		case GA_FONT:
		case GB_KIT:
		case GB_UNKNOWN1:
		case GB_UNKNOWN2:
		case GB_NUMBERS:
		case GB_FONT:
			return TRUE;
	}
	return FALSE;
}

BOOL IsShirt(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % 20;
	switch (ordinal) {
		case GA_KIT:
		case GB_KIT:
		case PA_SHIRT:
		case PB_SHIRT:
			return TRUE;
	}
	return FALSE;
}

BOOL IsShortsOrSocks(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % 20;
	switch (ordinal) {
		case GA_UNKNOWN1:
		case GA_UNKNOWN2:
		case GB_UNKNOWN1:
		case GB_UNKNOWN2:
		case PA_SHORTS:
		case PA_SOCKS:
		case PB_SHORTS:
		case PB_SOCKS:
			return TRUE;
	}
	return FALSE;
}

BOOL IsKitTexture(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % 20;
	switch (ordinal) {
		case GA_KIT:
		case GB_KIT:
		case PA_SHIRT:
		case PA_SHORTS:
		case PA_SOCKS:
		case PB_SHIRT:
		case PB_SHORTS:
		case PB_SOCKS:
			return TRUE;
	}
	return FALSE;
}

DWORD GetSplitKitOffset(DWORD id)
{
	DWORD ordinal = (id - data[FIRST_ID]) % 20;
	switch (ordinal) {
		case PA_SHIRT:
		case PB_SHIRT:
            return 0;
		case PA_SHORTS:
		case PB_SHORTS:
			return 1;
		case PA_SOCKS:
		case PB_SOCKS:
			return 2;
	}
	return 0xffffffff;
}

void MaskKitTextureWithFile(BITMAPINFOHEADER* tex,char* filename,DWORD id)
{
	BITMAPINFO* maskTex = NULL;
	
	if (strlen(filename)==0 || FileExists(filename)==FALSE) {
			maskTex=g_MaskTex;
			goto MaskNotFound;
	};	
	
	LogWithString(&k_mydll,"Using mask %s.",filename);
	
	LoadPNGTexture(&maskTex,filename);
	
	MaskNotFound:
	MaskKitTexture(tex,id,maskTex);
	
	if (maskTex != NULL && maskTex!=g_MaskTex)
		pngdib_p2d_free_dib(NULL,(BITMAPINFOHEADER*)maskTex);
	
	return;
};

void MaskKitTexture(BITMAPINFOHEADER* tex, DWORD id, BITMAPINFO* maskTex)
{
	int x,y,zx,zy;
	DWORD* pixels = (DWORD*)((BYTE*)tex + tex->biSize);
	DWORD UnitX=tex->biWidth/512;
	DWORD UnitY=tex->biHeight/256;

	if (maskTex == NULL) return;
	
	BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)maskTex;
	BYTE* mask = (BYTE*)maskTex + bih->biSize + 0x400;

	DWORD partColor=0;
	switch (id) {
	case 0:
		//Red for shirt
		partColor=0xFF0000; break;
	case 1:
		//Blue for shorts
		partColor=0x0000FF; break;
	case 2:
		//Yellow for socks
		partColor=0xFFFF00; break;
	};

	BYTE Index=255;
	for (int i=0;i<256;i++)
		if ((((DWORD*)((BYTE*)maskTex + bih->biSize))[i] & 0xFFFFFF) == partColor) {
			Index=i;
			break;
		};
	//LogWithNumber(&k_mydll,"splitting color is %d.",Index);
	for (x=0; x<512; x++) {
        for (y=0; y<256; y++) {
            if (mask[y*512+x]!=Index) {
                int zyto = (y+1)/256.0*tex->biHeight;
                int zxto = (x+1)/512.0*tex->biWidth;
                for (zy=y/256.0*tex->biHeight; zy<zyto; zy++)
                for (zx=x/512.0*tex->biWidth; zx<zxto; zx++)
                    pixels[zy*tex->biWidth + zx] = 0;
            }
        }
    }
}

BOOL TeamDirExists(DWORD id) 
{
    KitCollection* col = MAP_FIND(gdb->uni,id);
    return (col)?TRUE:FALSE;
}

void SetKitInfo(Kit* kit, KITINFO* kitInfo, BOOL editable)
{
	// set kit type
    switch (kitInfo->kitType) {
        case 1:
            if (editable) {
                kitInfo->kitType = 2;
            }
            break;
        default:
            kitInfo->kitType = 2;
            break;
    }
	// set collar
	if (kit && (kit->attDefined & COLLAR)) {
		kitInfo->collar = kit->collar;
	}
	// set model
	if (kit && (kit->attDefined & MODEL)) {
		kitInfo->model = kit->model;
	}
	// set name location
	if (kit && (kit->attDefined & NAME_LOCATION)) {
		kitInfo->nameLocation = kit->nameLocation;
	}
	// set logo location
	if (kit && (kit->attDefined & LOGO_LOCATION)) {
		kitInfo->logoLocation = kit->logoLocation;
	}
   	// set name shape
	if (kit && (kit->attDefined & NAME_SHAPE)) {
		kitInfo->nameShape = kit->nameShape;
	}
	
	// set radar color
	if (kit && (kit->attDefined & RADAR_COLOR)) {
		kitInfo->shirtColors[0] = 0x8000
			+((kit->radarColor.r>>3) & 31)
			+0x20*((kit->radarColor.g>>3) & 31)
			+0x400*((kit->radarColor.b>>3) & 31);
	}

    /*
	// set name type
	if (kit && (kit->attDefined & NAME_TYPE)) {
		kitInfo->nameType = kit->nameType;
	}
	// set number type
	if (kit && (kit->attDefined & NUMBER_TYPE)) {
		kitInfo->numberType = kit->numberType;
	}
    */
}

void SetShortsInfo(Kit* kit, KITINFO* kitInfo, BOOL editable)
{
    // set shorts number position
	if (kit && (kit->attDefined & SHORTS_NUMBER_LOCATION)) {
		kitInfo->shortsNumberLocation = kit->shortsNumberLocation;
	}

	// set shorts main color (for under-shorts)
	if (kit && (kit->attDefined & SHORTS_COLOR)) {
		kitInfo->shortsColors[0] = 0x8000
			+((kit->shortsColor.r>>3) & 31)
			+0x20*((kit->shortsColor.g>>3) & 31)
			+0x400*((kit->shortsColor.b>>3) & 31);
	}
}

void ResetKitPackInfo(KITPACKINFO* kitPackInfo, KITPACKINFO* saved)
{
	kitPackInfo->gkHome.kitType = saved->gkHome.kitType;
	kitPackInfo->gkHome.collar = saved->gkHome.collar;
	kitPackInfo->gkHome.model = saved->gkHome.model;
	kitPackInfo->gkHome.shortsNumberLocation = saved->gkHome.shortsNumberLocation;
	kitPackInfo->gkHome.nameLocation = saved->gkHome.nameLocation;
	kitPackInfo->gkHome.logoLocation = saved->gkHome.logoLocation;
	kitPackInfo->gkHome.nameShape = saved->gkHome.nameShape;
	kitPackInfo->gkHome.shirtColors[0] = saved->gkHome.shirtColors[0];
	//kitPackInfo->gkHome.nameType = saved->gkHome.nameType;
	//kitPackInfo->gkHome.numberType = saved->gkHome.numberType;

	kitPackInfo->gkAway.kitType = saved->gkAway.kitType;
	kitPackInfo->gkAway.collar = saved->gkAway.collar;
	kitPackInfo->gkAway.model = saved->gkAway.model;
	kitPackInfo->gkAway.shortsNumberLocation = saved->gkAway.shortsNumberLocation;
	kitPackInfo->gkAway.nameLocation = saved->gkAway.nameLocation;
	kitPackInfo->gkAway.logoLocation = saved->gkAway.logoLocation;
	kitPackInfo->gkAway.nameShape = saved->gkAway.nameShape;
	kitPackInfo->gkAway.shirtColors[0] = saved->gkAway.shirtColors[0];
	//kitPackInfo->gkAway.nameType = saved->gkAway.nameType;
	//kitPackInfo->gkAway.numberType = saved->gkAway.numberType;

	kitPackInfo->plHome.kitType = saved->plHome.kitType;
	kitPackInfo->plHome.collar = saved->plHome.collar;
	kitPackInfo->plHome.model = saved->plHome.model;
	kitPackInfo->plHome.shortsNumberLocation = saved->plHome.shortsNumberLocation;
	kitPackInfo->plHome.nameLocation = saved->plHome.nameLocation;
	kitPackInfo->plHome.logoLocation = saved->plHome.logoLocation;
	kitPackInfo->plHome.nameShape = saved->plHome.nameShape;
	kitPackInfo->plHome.shirtColors[0] = saved->plHome.shirtColors[0];
	//kitPackInfo->plHome.nameType = saved->plHome.nameType;
	//kitPackInfo->plHome.numberType = saved->plHome.numberType;

	kitPackInfo->plAway.kitType = saved->plAway.kitType;
	kitPackInfo->plAway.collar = saved->plAway.collar;
	kitPackInfo->plAway.model = saved->plAway.model;
	kitPackInfo->plAway.shortsNumberLocation = saved->plAway.shortsNumberLocation;
	kitPackInfo->plAway.nameLocation = saved->plAway.nameLocation;
	kitPackInfo->plAway.logoLocation = saved->plAway.logoLocation;
	kitPackInfo->plAway.nameShape = saved->plAway.nameShape;
	kitPackInfo->plAway.shirtColors[0] = saved->plAway.shirtColors[0];
	//kitPackInfo->plAway.nameType = saved->plAway.nameType;
	//kitPackInfo->plAway.numberType = saved->plAway.numberType;
}

/**
 * This function calls GetClubTeamInfo() function
 * Parameter:
 *   id   - team id
 * Return value:
 *   address of the KITPACKINFO structure
 */
void JuceGetClubTeamInfo(DWORD id,DWORD result)
{
	//TRACE2(&k_mydll,"JuceGetClubTeamInfo: CALLED for id = %003d.", id);

	if (id == 0xf2) {
		// master league team (home)
		BYTE* mlData = *((BYTE**)data[ML_HOME_AREA]);
		id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
	} else if (id == 0xf3) {
		// master league team (away)
		BYTE* mlData = *((BYTE**)data[ML_AWAY_AREA]);
		id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
	}

	if (id >= 64 && id < 255 && TeamDirExists(id)) {
		// check if we need to store it in the hash-map
		if (g_teamKitInfo[result] == NULL) {
			// not yet saved: save it
			TEAMKITINFO* teamKitInfo = (TEAMKITINFO*)HeapAlloc(GetProcessHeap(),
					0, sizeof(TEAMKITINFO));
			if (teamKitInfo) {
				memcpy(&teamKitInfo->kits, (KITPACKINFO*)result, sizeof(KITPACKINFO));
				teamKitInfo->teamId = id;
				g_teamKitInfo[result] = teamKitInfo;
				//DumpData((BYTE*)&teamKitInfo->kits, sizeof(KITPACKINFO));
			} else {
				Log(&k_mydll,"JuceGetClubTeamInfo: SEVERE PROBLEM: unable to allocate memory for kitPackInfo");
			}
            LogWithNumber(&k_mydll,"JuceGetClubTeamInfo: stored id = %003d.", id);
		}

		KITPACKINFO* kitPackInfo = (KITPACKINFO*)result;

        KitCollection* col = MAP_FIND(gdb->uni,id);
        if (col) {
            Kit* ga = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GA_KIT));
            Kit* gb = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GB_KIT));
            Kit* gaShorts = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GA_UNKNOWN1));
            Kit* gbShorts = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GB_UNKNOWN1));
            Kit* pa = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PA_SHIRT));
            Kit* pb = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PB_SHIRT));
            Kit* paShorts = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PA_SHORTS));
            Kit* pbShorts = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PB_SHORTS));
          
            //BOOL editable = kDB->editable[id];
            BOOL editable = TRUE;

            // set attributes
            SetKitInfo(ga, &kitPackInfo->gkHome, editable);
            SetKitInfo(gb, &kitPackInfo->gkAway, editable);
            SetShortsInfo(gaShorts, &kitPackInfo->gkHome, editable);
            SetShortsInfo(gbShorts, &kitPackInfo->gkAway, editable);
            SetKitInfo(pa, &kitPackInfo->plHome, editable);
            SetKitInfo(pb, &kitPackInfo->plAway, editable);
            SetShortsInfo(paShorts, &kitPackInfo->plHome, editable);
            SetShortsInfo(pbShorts, &kitPackInfo->plAway, editable);
        }
	}
	return;
}

void clearTeamKitInfo()
{
	if (g_teamKitInfo.size() == 0) {
		return;
	}

	Log(&k_mydll,"clearTeamKitInfo: restoring kitinfo edits");

	HANDLE pHeap = GetProcessHeap();
	for(g_teamKitInfoIterator = g_teamKitInfo.begin(); 
			g_teamKitInfoIterator != g_teamKitInfo.end();
			g_teamKitInfoIterator++)
	{
		KITPACKINFO* kitPackInfo = (KITPACKINFO*)g_teamKitInfoIterator->first;
		TEAMKITINFO* savedTeamInfo = g_teamKitInfoIterator->second;

		LogWithThreeNumbers(&k_mydll,"clearTeamKitInfo: restoring info for %03d (%00005d) at %08x", 
				savedTeamInfo->teamId, data[FIRST_ID]+savedTeamInfo->teamId*20, (DWORD)kitPackInfo);

		// reset licensed flags
        ResetKitPackInfo(kitPackInfo, &savedTeamInfo->kits);

		// free the memory for saved pack
		HeapFree(pHeap, 0, savedTeamInfo);
	}

	// clear the hash-map
	g_teamKitInfo.clear();
}

/**
 * This function calls GetClubTeamInfo() function
 * Parameter:
 *   id   - team id
 * Return value:
 *   address of the KITPACKINFO structure
 */
void JuceGetClubTeamInfoML1(DWORD id,DWORD result)
{
	// clear the hash-map
	clearTeamKitInfo();

	g_ML_kitPackInfoAddr = result;

	return;
}

/**
 * This function calls GetClubTeamInfo() function
 * Parameter:
 *   id   - team id
 * Return value:
 *   address of the KITPACKINFO structure
 */
void JuceGetClubTeamInfoML2(DWORD id,DWORD result)
{
	//TRACE2(&k_mydll,"JuceGetClubTeamInfoML2: CALLED for id = %003d.", id);

	if (id >= 64 && id < 255 && TeamDirExists(id)) {
		// check if we need to store it in the hash-map
		if (g_teamKitInfo[result] == NULL) {
			// not yet saved: save it
			TEAMKITINFO* teamKitInfo = (TEAMKITINFO*)HeapAlloc(GetProcessHeap(),
					0, sizeof(TEAMKITINFO));
			if (teamKitInfo) {
				memcpy(&teamKitInfo->kits, (KITPACKINFO*)result, sizeof(KITPACKINFO));
				teamKitInfo->teamId = id;
				g_teamKitInfo[result] = teamKitInfo;
				//DumpData((BYTE*)&teamKitInfo->kits, sizeof(KITPACKINFO));
			} else {
				Log(&k_mydll,"JuceGetClubTeamInfoML2: SEVERE PROBLEM: unable to allocate memory for kitPackInfo");
			}
		}

		// check if need to save ML copy
		if (g_teamKitInfo[g_ML_kitPackInfoAddr] == NULL) {
			// not yet saved: save it
			TEAMKITINFO* teamKitInfo = (TEAMKITINFO*)HeapAlloc(GetProcessHeap(),
					0, sizeof(TEAMKITINFO));
			if (teamKitInfo) {
				memcpy(&teamKitInfo->kits, (KITPACKINFO*)result, sizeof(KITPACKINFO));
				teamKitInfo->teamId = id;
				g_teamKitInfo[g_ML_kitPackInfoAddr] = teamKitInfo;
				//DumpData((BYTE*)&teamKitInfo->kits, sizeof(KITPACKINFO));
			} else {
				Log(&k_mydll,"JuceGetClubTeamInfoML2: SEVERE PROBLEM: unable to allocate memory for kitPackInfo");
			}
		}

		KITPACKINFO* kitPackInfo = (KITPACKINFO*)result;

        KitCollection* col = MAP_FIND(gdb->uni,id);
        if (col) {
            Kit* ga = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GA_KIT));
            Kit* gb = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GB_KIT));
            Kit* gaShorts = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GA_UNKNOWN1));
            Kit* gbShorts = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GB_UNKNOWN1));
            Kit* pa = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PA_SHIRT));
            Kit* pb = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PB_SHIRT));
            Kit* paShorts = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PA_SHORTS));
            Kit* pbShorts = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PB_SHORTS));
            
            //BOOL editable = kDB->editable[id];
            BOOL editable = TRUE;

            // set attributes
            SetKitInfo(ga, &kitPackInfo->gkHome, editable);
            SetKitInfo(gb, &kitPackInfo->gkAway, editable);
            SetShortsInfo(gaShorts, &kitPackInfo->gkHome, editable);
            SetShortsInfo(gbShorts, &kitPackInfo->gkAway, editable);
            SetKitInfo(pa, &kitPackInfo->plHome, editable);
            SetKitInfo(pb, &kitPackInfo->plAway, editable);
            SetShortsInfo(paShorts, &kitPackInfo->plHome, editable);
            SetShortsInfo(pbShorts, &kitPackInfo->plAway, editable);
        }
	}
	return;
}


/**
 * This function calls GetNationalTeamInfo() function
 * Parameter:
 *   id   - team id
 * Return value:
 *   address of the KITPACKINFO structure
 */
void JuceGetNationalTeamInfo(DWORD id,DWORD result)
{
	//TRACE2(&k_mydll,"JuceGetNationalTeamInfo: CALLED for id = %003d.", id);

	if (id >= 0 && id < 64 && TeamDirExists(id)) {
		// check if we need to store it in the hash-map
		if (g_teamKitInfo[result] == NULL) {
			// not yet saved: save it
			TEAMKITINFO* teamKitInfo = (TEAMKITINFO*)HeapAlloc(GetProcessHeap(),
					0, sizeof(TEAMKITINFO));
			if (teamKitInfo) {
				memcpy(&teamKitInfo->kits, (KITPACKINFO*)result, sizeof(KITPACKINFO));
				teamKitInfo->teamId = id;
				g_teamKitInfo[result] = teamKitInfo;
				//DumpData((BYTE*)&teamKitInfo->kits, sizeof(KITPACKINFO));
			} else {
				Log(&k_mydll,"JuceGetNationalTeamInfo: SEVERE PROBLEM: unable to allocate memory for kitPackInfo");
			}
            LogWithNumber(&k_mydll,"JuceGetNationalTeamInfo: stored id = %003d.", id);
		}

		KITPACKINFO* kitPackInfo = (KITPACKINFO*)result;

        KitCollection* col = MAP_FIND(gdb->uni,id);
        if (col) {
            Kit* ga = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GA_KIT));
            Kit* gb = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GB_KIT));
            Kit* gaShorts = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GA_UNKNOWN1));
            Kit* gbShorts = MAP_FIND(col->goalkeepers,GetKitFolderKey(data[FIRST_ID]+id*20+GB_UNKNOWN1));
            Kit* pa = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PA_SHIRT));
            Kit* pb = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PB_SHIRT));
            Kit* paShorts = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PA_SHORTS));
            Kit* pbShorts = MAP_FIND(col->players,GetKitFolderKey(data[FIRST_ID]+id*20+PB_SHORTS));
            
            //BOOL editable = kDB->editable[id];
            BOOL editable = TRUE;

            // set attributes
            SetKitInfo(ga, &kitPackInfo->gkHome, editable);
            SetKitInfo(gb, &kitPackInfo->gkAway, editable);
            SetShortsInfo(gaShorts, &kitPackInfo->gkHome, editable);
            SetShortsInfo(gbShorts, &kitPackInfo->gkAway, editable);
            SetKitInfo(pa, &kitPackInfo->plHome, editable);
            SetKitInfo(pb, &kitPackInfo->plAway, editable);
            SetShortsInfo(paShorts, &kitPackInfo->plHome, editable);
            SetShortsInfo(pbShorts, &kitPackInfo->plAway, editable);
        }
	}
	return;
}

/**
 * This function gets called upon exit of edit mode.
 * We need to remove all our changes from KITINFO structures.
 * This function calls GetNationalTeamInfo() function
 * Parameter:
 *   id   - team id
 * Return value:
 *   address of the KITPACKINFO structure
 */
void JuceGetNationalTeamInfoExitEdit(DWORD id,DWORD result)
{
	TRACE2(&k_mydll,"JuceGetNationalTeamInfoExitEdit: CALLED for id = %003d.", id);

	// clear the hash-map
	clearTeamKitInfo();
	
	UnloadNewKits();

	return;
}

void JuceSet2Dkits()
{
    g_display2Dkits = TRUE;

    // initialize home iterators
    WORD teamId = GetTeamId(HOME);
    if (teamId != 0xffff) {
        KitCollection* col = MAP_FIND(gdb->uni,teamId);
        if (col) {
            g_homeShirtIteratorPL = (*(col->players)).begin();
            g_homeShirtIteratorGK = (*(col->goalkeepers)).begin();
            g_homeShortsIteratorPL = (*(col->players)).begin();
            g_homeShortsIteratorGK = (*(col->goalkeepers)).begin();
            g_homeSocksIteratorPL = (*(col->players)).begin();
            g_homeSocksIteratorGK = (*(col->goalkeepers)).begin();

            g_homeShirtIterator = (typ == PL_TYPE) ? 
                g_homeShirtIteratorPL : g_homeShirtIteratorGK;
            g_homeShortsIterator = (typ == PL_TYPE) ? 
                g_homeShirtIteratorPL : g_homeShirtIteratorGK;
            g_homeSocksIterator = (typ == PL_TYPE) ? 
                g_homeShirtIteratorPL : g_homeShirtIteratorGK;
        }
    }
    // initialize away iterators
    teamId = GetTeamId(AWAY);
    if (teamId != 0xffff) {
        KitCollection* col = MAP_FIND(gdb->uni,teamId);
        if (col) {
            g_awayShirtIteratorPL = (*(col->players)).begin();
            g_awayShirtIteratorGK = (*(col->goalkeepers)).begin();
            g_awayShortsIteratorPL = (*(col->players)).begin();
            g_awayShortsIteratorGK = (*(col->goalkeepers)).begin();
            g_awaySocksIteratorPL = (*(col->players)).begin();
            g_awaySocksIteratorGK = (*(col->goalkeepers)).begin();

            g_awayShirtIterator = (typ == PL_TYPE) ? 
                g_awayShirtIteratorPL : g_awayShirtIteratorGK;
            g_awayShortsIterator = (typ == PL_TYPE) ? 
                g_awayShirtIteratorPL : g_awayShirtIteratorGK;
            g_awaySocksIterator = (typ == PL_TYPE) ? 
                g_awayShirtIteratorPL : g_awayShirtIteratorGK;
        }
    }

    // hook Present method
    HookFunction(hk_D3D_Present,(DWORD)JucePresent);
    Log(&k_mydll,"Present hooked.");
	
    // reset the keys to defaults
    ResetKitKeys(TRUE);

    // clear the kit palette map
    g_kitPaletteMap.clear();
    ZeroMemory(g_home_shirt_pal, 0x100*sizeof(PALETTEENTRY));
    ZeroMemory(g_home_shorts_pal, 0x100*sizeof(PALETTEENTRY));
    ZeroMemory(g_home_socks_pal, 0x100*sizeof(PALETTEENTRY));
    ZeroMemory(g_away_shirt_pal, 0x100*sizeof(PALETTEENTRY));
    ZeroMemory(g_away_shorts_pal, 0x100*sizeof(PALETTEENTRY));
    ZeroMemory(g_away_socks_pal, 0x100*sizeof(PALETTEENTRY));

	UnloadNewKits();

    g_last_home = GetTeamId(HOME);
    g_last_away = GetTeamId(AWAY);

    Log(&k_mydll,"Show 2D kits");
    return;
}

void JuceClear2Dkits()
{
    g_display2Dkits = FALSE;
    g_lastDisplay2Dkits = FALSE;

    // unhook Present method
	UnhookFunction(hk_D3D_Present,(DWORD)JucePresent);
    Log(&k_mydll,"Present unhooked.");

    // clear the kit texture map
    map<string,IDirect3DTexture8*>::iterator it;
    for (it = g_kitTextureMap.begin(); it != g_kitTextureMap.end(); it++) {
        LogWithString(&k_mydll,"Releasing 2Dkit texture for key: %s", (char*)it->first.c_str());
        IDirect3DTexture8* pTex = it->second;
        SafeRelease(&it->second);
    }
    Log(&k_mydll,"Clearing 2Dkit textures and g_kitTextureMap.");
    g_kitTextureMap.clear();

    g_home_shirt_tex = g_home_shorts_tex = g_home_socks_tex = NULL;
    g_away_shirt_tex = g_away_shorts_tex = g_away_socks_tex = NULL;
    
    // Special logic for a case when team is playing against itself
    WORD id = GetTeamId(HOME); 
    if (id == GetTeamId(AWAY) && id != 0xffff & TeamDirExists(id)) {
        BYTE* strips = (BYTE*)data[TEAM_STRIPS];

        // home: set bits to 0,0,0,0,0,0
        strips[0] = (((strips[0] & STRIP_PL_SHIRT_C) & STRIP_PL_SHORTS_C) & STRIP_PL_SOCKS_C);
        strips[0] = (((strips[0] & STRIP_GK_SHIRT_C) & STRIP_GK_SHORTS_C) & STRIP_GK_SOCKS_C);

        // away: set bits to 1,1,1,1,1,1
        strips[1] = (((strips[1] | STRIP_PL_SHIRT) | STRIP_PL_SHORTS) | STRIP_PL_SOCKS);
        strips[1] = (((strips[1] | STRIP_GK_SHIRT) | STRIP_GK_SHORTS) | STRIP_GK_SOCKS);

        //LogWithTwoNumbers(&k_mydll,"strip = %02x %02x", strips[0], strips[1]);
    }

    g_homeStrip = 0xff;
    g_awayStrip = 0xff;

    // reset kit selection mode
    typ = PL_TYPE;
    
    UnloadNewKits();

    Log(&k_mydll,"Hide 2D kits");
    return;
}

void FindKitPartsNames(DWORD id, NEWKITFILES* files)
{
	char filename[BUFLEN];
	DWORD baseId=id - ((id-data[FIRST_ID]) % 5);
	
    DWORD texType=TEXTYPE_NONE;
    
    //Shirt
	texType = FindImageFileForId(baseId,"",filename);
    if (texType==TEXTYPE_NONE)
    	strcpy(files->shirtName,"\0");
    else
    	strcpy(files->shirtName,filename);
    
    //Shorts
	texType = FindImageFileForId(baseId+1,"",filename);
    if (texType==TEXTYPE_NONE)
    	strcpy(files->shortsName,"\0");
    else
    	strcpy(files->shortsName,filename);
    
    //Socks
    texType = FindImageFileForId(baseId+2,"",filename);
    if (texType==TEXTYPE_NONE)
    	strcpy(files->socksName,"\0");
    else
    	strcpy(files->socksName,filename);
        
    strcpy(files->maskName,"\0");
    strcpy(files->overlayName,"\0");
	
	//Mask
	WORD teamId = (id - data[FIRST_ID]) / 20;
    KitCollection* col = MAP_FIND(gdb->uni,teamId);
    Kit* kit=NULL;
    string kitKey = GetKitFolderKey(id);
    if (!col)
    	 goto NoCollection;
    
	kit=((id - data[FIRST_ID]) % 20 < PA_SHIRT) ? MAP_FIND(col->goalkeepers,kitKey):
												  MAP_FIND(col->players,kitKey);
	
    if (kit != NULL || (kit->attDefined & MASK_FILE)) {
        //First try to find it in the team's folder
        ZeroMemory(filename, BUFLEN);
        sprintf(filename,"%s%s\\%s",GetPESInfo()->gdbDir,kit->foldername,kit->maskFile);
        if (FileExists(filename)==FALSE) {
            //If not successful, try masks folder
            ZeroMemory(filename, BUFLEN);
            sprintf(filename,"%sGDB\\uni\\masks\\%s",GetPESInfo()->gdbDir,kit->maskFile);
            if (FileExists(filename)==FALSE)
                goto CheckOverlay;
        }
        strcpy(files->maskName,filename);
    }

    //Overlay
    CheckOverlay:
    kitKey = GetKitFolderKey(baseId);
    kit=((id - data[FIRST_ID]) % 20 < PA_SHIRT) ? MAP_FIND(col->goalkeepers,kitKey):
                                                  MAP_FIND(col->players,kitKey);

    if (kit != NULL || (kit->attDefined & OVERLAY_FILE)) {
        ZeroMemory(filename, BUFLEN);
        sprintf(filename,"%s%s\\%s",GetPESInfo()->gdbDir,kit->foldername,kit->overlayFile);
        LOG(&k_mydll,"Checking file: {%s}",filename);
        if (FileExists(filename)) {
            strcpy(files->overlayName,filename);
        }
    }

    NoCollection:
    return;
}

bool HasKitFile(DWORD id, NEWKITFILES* newKitFiles)
{
    int fileType = (id - data[FIRST_ID]) % 20;
    switch (fileType) {
        case PA_SHIRT:
        case PB_SHIRT:
            return strlen(newKitFiles->shirtName)>0;
        case PA_SHORTS:
        case PB_SHORTS:
            return strlen(newKitFiles->shortsName)>0;
        case PA_SOCKS:
        case PB_SOCKS:
            return strlen(newKitFiles->socksName)>0;
        case PA_NUMBERS:
        case PB_NUMBERS:
        case PA_FONT:
        case PB_FONT:
            break;
    }
    return false;
}

/**
 * This function calls kit BIN decode function
 * Parameters:
 *   addr   - address of the encoded buffer header
 *   size   - size of the encoded buffer
 * Return value:
 *   address of the decoded buffer
 */
void JuceUniDecode(DWORD addr, DWORD size, DWORD result)
{
	TRACE(&k_mydll,"JuceUniDecode: CALLED.");
	TRACE2(&k_mydll,"JuceUniDecode: addr = %08x", addr);

	TEXIMGPACKHEADER* orgKit = (TEXIMGPACKHEADER*)result;

	/*
	// save encoded buffer as raw data
	ENCBUFFERHEADER* header = (ENCBUFFERHEADER*)addr1;
	DumpData((BYTE*)addr1, header->dwEncSize + sizeof(ENCBUFFERHEADER));
	*/

	// save decoded buffer as raw data
	//ENCBUFFERHEADER* header = (ENCBUFFERHEADER*)addr;
	//DumpData((BYTE*)result, header->dwDecSize);

	MEMITEMINFO* memItemInfo = FindMemItemInfoByAddress(addr);
	if (memItemInfo != NULL) {
        LogWithNumber(&k_mydll,"JuceUniDecode: Loading id = %d", memItemInfo->id);
		g_kitID = memItemInfo->id;

        if (!IsKitTexture(memItemInfo->id)) {
            TRACE(&k_mydll,"JuceUniDecode: SEVERE WARNING!: this is NOT a kit texture.");
            return;
		};
        
		FindKitPartsNames(memItemInfo->id, &lastKitFiles);
		if (strlen(lastKitFiles.shirtName)>0)
        	LogWithString(&k_mydll,"Using shirt: %s.",lastKitFiles.shirtName);
        if (strlen(lastKitFiles.shortsName)>0)
        	LogWithString(&k_mydll,"Using shorts: %s.",lastKitFiles.shortsName);
        if (strlen(lastKitFiles.socksName)>0)
        	LogWithString(&k_mydll,"Using socks: %s.",lastKitFiles.socksName);
        	
        if (HasKitFile(memItemInfo->id, &lastKitFiles)) {
            //Save the palettes. Transparency manipulation is needed as PES seems to check
            //if the palettes need more than just two transparency levels (invisible/visible)
            
            BYTE* pOffset=NULL;
            
            TEXIMGPACKHEADER* tiph=(TEXIMGPACKHEADER*)result;
            DECBUFFERHEADER* dbh1=(DECBUFFERHEADER*)((DWORD)tiph+tiph->toc[0+(tiph->tocOffset-8)/4]);
            if (dbh1->dwSig==DEC_SIG_IMAGE) {
                //make palette transparent
                pOffset=(BYTE*)dbh1+dbh1->paletteOffset;
                for (int i=3;i<1024;i+=4) {
                    //0==0;//pOffset[i]=(i+1)/4-1;		
                    pOffset[i]=(i+1)/4-1;		
                    for (int k=i-1; k>i-4; k--) pOffset[k] = 0;
                }
            };
            if (tiph->numFiles>1) {
                DECBUFFERHEADER* dbh2=(DECBUFFERHEADER*)((DWORD)tiph+tiph->toc[1+(tiph->tocOffset-8)/4]);
                if (dbh2->dwSig==DEC_SIG_IMAGE) {
                    //make palette transparent
                    pOffset=(BYTE*)dbh2+dbh2->paletteOffset;
                    for (int i=3;i<1024;i+=4) {
                        //0==0;//pOffset[i]=(i+1)/4-1;
                        pOffset[i]=(i+1)/4-1;
                        for (int k=i-1; k>i-4; k--) pOffset[k] = 0;
                    }
                };
            };
        } // otherwise, don't modify the palette
		
        // if goalkeeper, extra steps needed:
        if (IsGK(memItemInfo->id)) {
            // check for gloves
			char filename[BUFLEN];
			BITMAPINFO* tex = NULL;
			DWORD texSize = 0;
            DWORD texType = FindImageFileForId(memItemInfo->id, "-gloves", filename);
            switch (texType) {
                case TEXTYPE_PNG:
                    LogWithString(&k_mydll,"JuceUniDecode: Gloves file = %s", filename);
                    texSize = LoadPNGTexture(&tex, filename);
                    if (texSize > 0) {
                        ApplyGlovesDIBTexture((TEXIMGPACKHEADER*)result, tex);
                        FreePNGTexture(tex);
                    }
                    break;
                case TEXTYPE_BMP:
                    LogWithString(&k_mydll,"JuceUniDecode: Gloves file = %s", filename);
                    texSize = LoadTexture(&tex, filename);
                    if (texSize > 0) {
                        ApplyGlovesDIBTexture((TEXIMGPACKHEADER*)result, tex);
                        FreeTexture(tex);
                    }
                    break;
            };
        };
        
        lastUniDecodeResult=result;
	} else {
		//for self-created teams from ML or others without AFS kit
		strcpy(lastKitFiles.shirtName,"\0");
		strcpy(lastKitFiles.shortsName,"\0");
		strcpy(lastKitFiles.socksName,"\0");
		strcpy(lastKitFiles.maskName,"\0");
		strcpy(lastKitFiles.overlayName,"\0");
		lastUniDecodeResult=result;
	};

	// save decoded uniforms as BMPs
	//DumpUniforms(orgKit);

	// dump palette
	//DumpData((BYTE*)g_palette, 0x400);
	
	return;
	
    TRACE(&k_mydll,"JuceUniDecode done");
	
	return;
}

/*
void JuceUniSplit(DWORD id, DWORD addr, DWORD ESI)
{
	//addr belongs always to the last kit in UniDecode and
	//will later be the src parameter of CreateTexture
	
	if ((isGdbKit.find(addr) != isGdbKit.end()) && isGdbKit[addr]) {
		NewKitFilesMap.erase(addr);
		isGdbKit.erase(addr);
		gdbKitSize.erase(addr);
		gdbKitCRC.erase(addr);
	};
	
	//ESI is a pointer to the buffer with the kit (result of UniDecode)
	if (ESI !=0 && addr!=0 && ESI==lastUniDecodeResult) {
		//LogWithNumberAndString(&k_mydll,"JuceUniSplit: %x = %s",addr,lastKitFiles.shirtName);
		if (lstrlen(lastKitFiles.shirtName)>0) {
			strcpy(NewKitFilesMap[addr].shirtName,lastKitFiles.shirtName);
			strcpy(NewKitFilesMap[addr].shortsName,lastKitFiles.shortsName);
			strcpy(NewKitFilesMap[addr].socksName,lastKitFiles.socksName);
			strcpy(NewKitFilesMap[addr].maskName,lastKitFiles.maskName);
			
			isGdbKit[addr]=true;
			DWORD size=((DECBUFFERHEADER*)addr)->dwDecSize;
			gdbKitSize[addr]=size;
			//checksum is calculated a bit later
			//gdbKitCRC[addr]=GetCRC((BYTE*)addr,size);
			splitTextures.push_back(addr);
		};
	};
	
	return;
};*/

DWORD JuceUniSplit(DWORD id)
{
	DWORD result=MasterCallNext(id);

	if (*((BYTE*)result+0x16)!=0) {
		return result;
	};
	
	DWORD addr=*(DWORD*)(result+0x18);
	
	//addr belongs always to the last kit in UniDecode and
	//will later be the src parameter of CreateTexture
	
    /*
	if ((isGdbKit.find(addr) != isGdbKit.end()) && isGdbKit[addr]) {
		NewKitFilesMap.erase(addr);
		isGdbKit.erase(addr);
		gdbKitSize.erase(addr);
		gdbKitCRC.erase(addr);
	};
    */
	
	//lastUsedSourceTexture is a pointer to the buffer with the kit (result of UniDecode)
	if (lastUsedSourceTexture !=0 && addr!=0 && lastUsedSourceTexture==lastUniDecodeResult) {
		if (lstrlen(lastKitFiles.shirtName)>0) {
			strcpy(NewKitFilesMap[addr].shirtName,lastKitFiles.shirtName);
			strcpy(NewKitFilesMap[addr].shortsName,lastKitFiles.shortsName);
			strcpy(NewKitFilesMap[addr].socksName,lastKitFiles.socksName);
			strcpy(NewKitFilesMap[addr].maskName,lastKitFiles.maskName);
			strcpy(NewKitFilesMap[addr].overlayName,lastKitFiles.overlayName);
			
			isGdbKit[addr]=true;
			DWORD size=((DECBUFFERHEADER*)addr)->dwDecSize;
			gdbKitSize[addr]=size;
			splitTextures.push_back(addr);
			//checksum is calculated a bit later with the help of the preceding attributes
        }
        else {
            if ((isGdbKit.find(addr) != isGdbKit.end()) && isGdbKit[addr]) {
                NewKitFilesMap.erase(addr);
                isGdbKit.erase(addr);
                gdbKitSize.erase(addr);
                gdbKitCRC.erase(addr);
            }
        }
	} else {
        //LOG(&k_mydll, "CONDITION fails: %p!=0 && %p!=0 && %p==%p",
        //        lastUsedSourceTexture, addr, lastUsedSourceTexture,
        //        lastUniDecodeResult);
    }
	
	return result;
};


DWORD JuceSplitDone1(DWORD p1)
{
	DWORD oldEBX;
	__asm mov oldEBX, ebx
	
	splitTextures.clear();
	lastUsedSourceTexture=oldEBX;
	
	DWORD result=MasterCallNext(p1);
	
	vector<DWORD>::iterator it=splitTextures.begin();
	while (it != splitTextures.end()) {
		DWORD addr=*it;
		if (addr != 0) {
			gdbKitCRC[addr]=GetCRC((BYTE*)addr,gdbKitSize[addr]);
		};
		it++;
	};
	
	lastUsedSourceTexture=0;
	
	return result;
};

DWORD JuceSplitDone2_1(DWORD p1)
{
	splitTextures.clear();
	lastUsedSourceTexture=p1;
	
	DWORD result=MasterCallNext(p1);
	return result;
};

DWORD JuceSplitDone2_2(DWORD p1)
{
	DWORD result=MasterCallNext(p1);
	
	vector<DWORD>::iterator it=splitTextures.begin();
	while (it != splitTextures.end()) {
		DWORD addr=*it;
		if (addr != 0) {
			gdbKitCRC[addr]=GetCRC((BYTE*)addr,gdbKitSize[addr]);
		};
		it++;
	};
	
	lastUsedSourceTexture=0;
	
	return result;
};

/* Writes an indexed BMP file (with palette) */
HRESULT SaveAsBitmap(char* filename, TEXIMGHEADER* header)
{
	BYTE* buf = (BYTE*)header + header->dataOffset;
	if (header->dataOffset == header->dwSize) {
		buf = NULL; // palette-only file
	}
	BYTE* pal = (BYTE*)header + header->paletteOffset;
	LONG width = header->width;
	LONG height = header->height;

	if (header->bps == 4) {
		return SaveAs4bitBMP(filename, buf, pal, width, height);
	} else if (header->bps == 8) {
		return SaveAs8bitBMP(filename, buf, pal, width, height);
	}

	Log(&k_mydll,"SaveAsBitmap: unsupported bits-per-pixel format.");
	return E_FAIL;
}

/* Writes an indexed 4-bit BMP file (with palette) */
HRESULT SaveAs4bitBMP(char* filename, BYTE* buf, BYTE* pal, LONG width, LONG height)
{
	BITMAPFILEHEADER fhdr;
	BITMAPINFOHEADER infoheader;
	SIZE_T size = (buf)? width * height : 8; // size of data in bytes

	// fill in the headers
	fhdr.bfType = 0x4D42; // "BM"
	fhdr.bfSize = sizeof(fhdr) + sizeof(infoheader) + 0x40 + size;
	fhdr.bfReserved1 = 0;
	fhdr.bfReserved2 = 0;
	fhdr.bfOffBits = sizeof(fhdr) + sizeof(infoheader) + 0x40;

	infoheader.biSize = sizeof(infoheader);
	infoheader.biWidth = (buf)? width : 16;
	infoheader.biHeight = (buf) ? height : 1;
	infoheader.biPlanes = 1;
	infoheader.biBitCount = 4;
	infoheader.biCompression = BI_RGB;
	infoheader.biSizeImage = 0;
	infoheader.biXPelsPerMeter = 0;
	infoheader.biYPelsPerMeter = 0;
	infoheader.biClrUsed = 16;
	infoheader.biClrImportant = 16;

	// prepare filename
	char name[BUFLEN];
	if (filename == NULL)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s-%d%02d%02d-%02d%02d%02d%03d.bmp", GetPESInfo()->mydir, "kserv", 
				time.wYear, time.wMonth, time.wDay,
				time.wHour, time.wMinute, time.wSecond, time.wMilliseconds); 
		filename = name;
	}

	// save to file
	DWORD wbytes;
	HANDLE hFile = CreateFile(filename,            // file to create 
					 GENERIC_WRITE,                // open for writing 
					 0,                            // do not share 
					 NULL,                         // default security 
					 OPEN_ALWAYS,                  // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,        // normal file 
					 NULL);                        // no attr. template 

	if (hFile != INVALID_HANDLE_VALUE) 
	{
		WriteFile(hFile, &fhdr, sizeof(fhdr), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, &infoheader, sizeof(infoheader), (LPDWORD)&wbytes, NULL);

		// write palette
		for (int i=0; i<16; i++)
		{
			WriteFile(hFile, pal + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
			WriteFile(hFile, pal + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
			WriteFile(hFile, pal + i*4, 1, (LPDWORD)&wbytes, NULL);
			WriteFile(hFile, pal + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
		}

		// write pixel data
		if (buf) {
			for (int k=height-1; k>=0; k--)
			{
				int j;
				// unshuffle pixel data
				for (j=0;j<width/2;j++) {
					BYTE src = buf[k*(width/2)+j];
					buf[k*(width/2)+j] = (src << 4 & 0xf0) | (src >> 4 & 0x0f);
				}
				WriteFile(hFile, buf + k*(width/2), width/2, (LPDWORD)&wbytes, NULL);
				// reshuffle pixel data
				for (j=0;j<width/2;j++) {
					BYTE src = buf[k*(width/2)+j];
					buf[k*(width/2)+j] = (src << 4 & 0xf0) | (src >> 4 & 0x0f);
				}
			}
		} else {
			// palette-only data: write sample stuff
			BYTE sample[] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
			WriteFile(hFile, sample, sizeof(sample), (LPDWORD)&wbytes, NULL);
		}
		CloseHandle(hFile);
	}
	else 
	{
		Log(&k_mydll,"SaveAs4bitBMP: failed to save to file.");
		return E_FAIL;
	}
	return S_OK;
}

/* Writes an indexed 8-bit BMP file (with palette) */
HRESULT SaveAs8bitBMP(char* filename, BYTE* buffer)
{
	DECBUFFERHEADER* header = (DECBUFFERHEADER*)buffer;
	BYTE* buf = (BYTE*)header + header->bitsOffset;
	BYTE* pal = (BYTE*)header + header->paletteOffset;
	LONG width = header->texWidth;
	LONG height = header->texHeight;

	return SaveAs8bitBMP(filename, buf, pal, width, height);
}

/* Writes an indexed 8-bit BMP file (with palette) */
HRESULT SaveAs8bitBMP(char* filename, BYTE* buf, BYTE* pal, LONG width, LONG height)
{
	BITMAPFILEHEADER fhdr;
	BITMAPINFOHEADER infoheader;
	SIZE_T size = width * height; // size of data in bytes

	// fill in the headers
	fhdr.bfType = 0x4D42; // "BM"
	fhdr.bfSize = sizeof(fhdr) + sizeof(infoheader) + 0x400 + size;
	fhdr.bfReserved1 = 0;
	fhdr.bfReserved2 = 0;
	fhdr.bfOffBits = sizeof(fhdr) + sizeof(infoheader) + 0x400;

	infoheader.biSize = sizeof(infoheader);
	infoheader.biWidth = width;
	infoheader.biHeight = height;
	infoheader.biPlanes = 1;
	infoheader.biBitCount = 8;
	infoheader.biCompression = BI_RGB;
	infoheader.biSizeImage = 0;
	infoheader.biXPelsPerMeter = 0;
	infoheader.biYPelsPerMeter = 0;
	infoheader.biClrUsed = 256;
	infoheader.biClrImportant = 0;

	// prepare filename
	char name[BUFLEN];
	if (filename == NULL)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s-%d%02d%02d-%02d%02d%02d%03d.bmp", GetPESInfo()->mydir, "kserv", 
				time.wYear, time.wMonth, time.wDay,
				time.wHour, time.wMinute, time.wSecond, time.wMilliseconds); 
		filename = name;
	}

	// save to file
	DWORD wbytes;
	HANDLE hFile = CreateFile(filename,            // file to create 
					 GENERIC_WRITE,                // open for writing 
					 0,                            // do not share 
					 NULL,                         // default security 
					 OPEN_ALWAYS,                  // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,        // normal file 
					 NULL);                        // no attr. template 

	if (hFile != INVALID_HANDLE_VALUE) 
	{
		WriteFile(hFile, &fhdr, sizeof(fhdr), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, &infoheader, sizeof(infoheader), (LPDWORD)&wbytes, NULL);

		// write palette
		for (int bank=0; bank<8; bank++)
		{
            int i;
			for (i=0; i<8; i++)
			{
				WriteFile(hFile, pal + bank*32*4 + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
			}
			for (i=16; i<24; i++)
			{
				WriteFile(hFile, pal + bank*32*4 + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
			}
			for (i=8; i<16; i++)
			{
				WriteFile(hFile, pal + bank*32*4 + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
			}
			for (i=24; i<32; i++)
			{
				WriteFile(hFile, pal + bank*32*4 + i*4 + 2, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 1, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4, 1, (LPDWORD)&wbytes, NULL);
				WriteFile(hFile, pal + bank*32*4 + i*4 + 3, 1, (LPDWORD)&wbytes, NULL);
			}
		}
		// write pixel data
		for (int k=height-1; k>=0; k--)
		{
			WriteFile(hFile, buf + k*width, width, (LPDWORD)&wbytes, NULL);
		}
		CloseHandle(hFile);
	}
	else 
	{
		Log(&k_mydll,"SaveAs8bitBMP: failed to save to file.");
		return E_FAIL;
	}
	return S_OK;
}

/* Writes a BMP file */
HRESULT SaveAsBMP(char* filename, BYTE* rgbBuf, SIZE_T size, LONG width, LONG height, int bpp)
{
	BITMAPFILEHEADER fhdr;
	BITMAPINFOHEADER infoheader;

	// fill in the headers
	fhdr.bfType = 0x4D42; // "BM"
	fhdr.bfSize = sizeof(fhdr) + sizeof(infoheader) + size;
	fhdr.bfReserved1 = 0;
	fhdr.bfReserved2 = 0;
	fhdr.bfOffBits = sizeof(fhdr) + sizeof(infoheader);

	infoheader.biSize = sizeof(infoheader);
	infoheader.biWidth = width;
	infoheader.biHeight = height;
	infoheader.biPlanes = 1;
	infoheader.biBitCount = bpp*8;
	infoheader.biCompression = BI_RGB;
	infoheader.biSizeImage = 0;
	infoheader.biXPelsPerMeter = 0;
	infoheader.biYPelsPerMeter = 0;
	infoheader.biClrUsed = 0;
	infoheader.biClrImportant = 0;

	// prepare filename
	char name[BUFLEN];
	if (filename == NULL)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		ZeroMemory(name, BUFLEN);
		sprintf(name, "%s%s-%d%02d%02d-%02d%02d%02d%03d.bmp", GetPESInfo()->mydir, "kserv", 
				time.wYear, time.wMonth, time.wDay,
				time.wHour, time.wMinute, time.wSecond, time.wMilliseconds); 
		filename = name;
	}

	// save to file
	DWORD wbytes;
	HANDLE hFile = CreateFile(filename,            // file to create 
					 GENERIC_WRITE,                // open for writing 
					 0,                            // do not share 
					 NULL,                         // default security 
					 OPEN_ALWAYS,                  // overwrite existing 
					 FILE_ATTRIBUTE_NORMAL,        // normal file 
					 NULL);                        // no attr. template 

	if (hFile != INVALID_HANDLE_VALUE) 
	{
		WriteFile(hFile, &fhdr, sizeof(fhdr), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, &infoheader, sizeof(infoheader), (LPDWORD)&wbytes, NULL);
		WriteFile(hFile, rgbBuf, size, (LPDWORD)&wbytes, NULL);
		CloseHandle(hFile);
	}
	else 
	{
		Log(&k_mydll,"SaveAsBMP: failed to save to file.");
		return E_FAIL;
	}
	return S_OK;
}

/*
// helper function
Kit* utilGetKit(WORD kitId)
{
	int id = kitId / 5; int idRem = kitId % 5;

	// sanity check
	if (id < 0 || id > 255)
		return NULL;

	// select corresponding kit
	Kit* kit = NULL;
	if (g_kitExtras[kitId] != NULL) 
		kit = g_kitExtras[kitId]->kit;
	if (kit == NULL)
	{
		switch (idRem)
		{
			case 0: kit = kDB->goalkeepers[id].a; break;
			case 1: kit = kDB->goalkeepers[id].b; break;
			case 2: kit = kDB->players[id].a; break;
			case 3: kit = kDB->players[id].b; break;
		}
	}

	return kit;
}
*/

// helper function
TeamAttr* utilGetTeamAttr(WORD id)
{
	// sanity check
	if (id < 0 || id > 255)
		return NULL;

	DWORD teamAttrBase = *((DWORD*)data[TEAM_COLLARS_PTR]);
	return (TeamAttr*)(teamAttrBase + id*sizeof(TeamAttr));
}

// this function makes sure we have the most up-to-date information
// about what teams are currently chosen.
void VerifyTeams()
{
	g_currTeams[0] = g_currTeams[1] = 0xffff;
	WORD* teams = (WORD*)data[TEAM_IDS];

	// try to define both teams.
	if (teams[0] >=0 && teams[0] <= g_numTeams) g_currTeams[0] = teams[0];
	if (teams[1] >=0 && teams[1] <= g_numTeams) g_currTeams[1] = teams[1];

	// check if home team is still undefined.
	if (g_currTeams[0] == 0xffff)
	{
		int which = (teams[0] == 0x0122) ? 1 : 0;

		// looks like it's an ML team.
		DWORD* mlPtrs = (DWORD*)data[MLDATA_PTRS];
		if (mlPtrs[which] != NULL)
		{
			WORD* mlData = (WORD*)(mlPtrs[which] + 0x278);
			WORD teamId = mlData[0];
			BOOL originalTeam = mlData[1];
			if (!originalTeam) g_currTeams[0] = teamId;
		}
	}

	// check if away team is still undefined.
	if (g_currTeams[1] == 0xffff)
	{
		int which = (teams[1] == 0x0122) ? 1 : 0;

		// looks like it's an ML team.
		DWORD* mlPtrs = (DWORD*)data[MLDATA_PTRS];
		if (mlPtrs[which] != NULL)
		{
			WORD* mlData = (WORD*)(mlPtrs[which] + 0x278);
			WORD teamId = mlData[0];
			BOOL originalTeam = mlData[1];
			if (!originalTeam) g_currTeams[1] = teamId;
		}
	}
}

