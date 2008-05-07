// savegames.h

#define MODID 107
#define NAMELONG "Savegame Manager 5.2.3.1"
#define NAMESHORT "SAVEGAMES"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

#define CODELEN 12
enum {
	C_COPYPLAYERDATA, C_COPYPLAYERDATA_JMP,C_COPYPLAYERDATA_CS,
	C_ASYNC_WRITEFILE_CS1, C_ASYNC_WRITEFILE_CS2, C_ASYNC_READFILE_CS, C_READSELECTEDFILE,
	C_ASYNC_OP_FINISHED_JMP, RESERVEDFILEMEMORY, RESERVEDMEMORY1,
	RESERVEDMEMORY2, C_MLSETPLAYERID_CS,
	
};

DWORD codeArray[][CODELEN] = {
	//PES5
	{
		0x4dd240, 0x4dd6c5, 0x577a12,
		0x4434e1, 0x443584, 0x443459, 0x438c43,
		0x830be7, 0x404121, 0x8316fd,
		0x831729, 0xa2ab02,
        //0x82f230
	},
	//WE9
	{
		0xdc690, 0xdcb15,0x176ea2,
		0,0,0, 0x37ff3,
		0x4300a7, 0, 0x430bbd,
		0x430be9, 0x629f22,
        //0x42e6f0 --> 0x82f6f0
	},
	//WE9:LE
	{
		0xdad50, 0xdb1d5, 0x172862,
		0,0,0, 0x37f53,
		0x457347, 0, 0x457e5d,
		0x457e89, 0x62cde2,
        //0x455990 --> 0x856990
	}
};

#define DATALEN 5
enum {
	PLAYERDATA_BASE, CPD_PLAYERIDS, CURRENT_ASYNCOP, SAVEDDATAFOLDER,
	CURR_MLFILE,
};

DWORD dataArray[][DATALEN] = {
	//PES5
	{
		0x3bd3ddc, 0xfd93e8, 0x3be24e0, 0xb0e170,
		0xf77f21,
	},
	//WE9
	{
		0x3bd3dfc, 0xfd93f0, 0x3be2500, 0xb0e170,
		0xf77f21,
	},
	//WE9:LE
	{
		0x3b5b91c, 0xf133a8, 0x3b6a020, 0xb0c170,
		0xeb1f21,
	}
};

DWORD code[CODELEN];
DWORD data[DATALEN];

//reserve 10 instead of 1.3 MB as buffer for saved files (like replays)
#define NEWRESERVEDFILEMEMORY 10000000
#define NEWRESERVEDMEMORY 0x3b7d000

typedef DWORD  (*STARTASYNCREAD)(DWORD,DWORD,DWORD,DWORD,DWORD,DWORD);
typedef DWORD  (*STARTASYNCWRITE)(DWORD,DWORD,DWORD,DWORD);
typedef DWORD  (*GETPESFILETYPE)(DWORD);
typedef void   (*COPYPLAYERDATA)();
