// skins.h

#define MODID 111
#define NAMELONG "Skinserver 5.0.0"
#define NAMESHORT "SKINS"

#define DEFAULT_DEBUG 1
#define BUFLEN 4096

#include <windows.h>
#include <stdio.h>
#include <unordered_map>


#define DATALEN 14
enum {
    EDITMODE_CURRPLAYER_MOD, EDITMODE_CURRPLAYER_ORG,
    EDITMODE_FLAG, EDITPLAYERMODE_FLAG, EDITPLAYER_ID,
    PLAYERS_LINEUP, PLAYERID_IN_PLAYERDATA,
    LINEUP_RECORD_SIZE, LINEUP_BLOCKSIZE, PLAYERDATA_SIZE, STACK_SHIFT,
    EDITPLAYERBOOT_FLAG, EDITBOOTS_FLAG, PLAYERDATA_BASE,
};
DWORD dtaArray[][DATALEN] = {
    // PES5
  {
      0, 0,
      0, 0, 0,
      0, 0,
      0, 0, 0, 0,
      0, 0, 0 ,
  },
  // PES5
{
    0, 0,
    0x38f97f8, 0x38f9ae4, 0x3937218,
    0x3BCF880, 0x3bd3e00,
    0x290, 0x60, 0x344, 0,
    0, 0x38f9be8, 0x3BD3DD8,
},
// WE9
{
    0, 0,
    0x38f97f8, 0x38f9ae4, 0x3937218,
    0x3bcfb30, 0x3bd3e20,
    0x290, 0x60, 0x344, 0,
    0, 0x38f9be8, 0x3bd3dfc,
},
// WE9LE
{
    0, 0,
    0x3834108, 0x38343f4, 0x388c070,
    0x3b57650, 0x3b5b940,
    0x290, 0x60, 0x344, 0,
    0, 0x38344f8, 0x3b5b91c,
},
};
DWORD dta[DATALEN];

#define CODELEN 2
enum {
    C_COPYPLAYERDATA_CS,
    C_RESETFLAG2_CS,
};
DWORD codeArray[][CODELEN] = {
    // PES5 DEMO 2
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
};
DWORD code[CODELEN];
