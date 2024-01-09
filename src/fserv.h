// fserv.h
#define KEXPORT EXTERN_C __declspec(dllexport)

#define MODID 103
#define NAMELONG "Faceserver 5.7.2.3"
#define NAMESHORT "FSERV"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

//AFS IDs of the faces
#define FIDSLEN 32
enum {NUM_FACES_ARRAY,STDW,STDY,STDR,STDK,STARTW,STARTY,STARTR,STARTK,
	ISEDITPLAYERLIST,ISEDITPLAYERMODE,EDITEDPLAYER,FIX_DWORDFACEID,
	CALCHAIRID,CALCSPHAIRID,HAIRSTARTARRAY,NUM_HAIR,STDHAIR,
	PLAYERDATA_BASE, EDITPLAYERDATA_BASE, C_COPYPLAYERDATA, C_COPYPLAYERDATA_CS, 
	C_REPL_COPYPLAYERDATA_CS, C_EDITCOPYPLAYERDATA, C_EDITCOPYPLAYERDATA_CS,
	C_GETHAIRTRANSP,C_GETHAIRTRANSP_CS,
	PLAYERS_LINEUP, FACES_LINEUP, HAIRS_LINEUP, PLAYERID_IN_PLAYERDATA, PLAYERDATA_SIZE,
	};

DWORD fIDsArray[][FIDSLEN] = {
	//PES5
	{
		0x393e27c,265,139,57,32,1954,2219,1897,1865,
		0x38f9ae0,0x38f9ae4,0x3937218,0x8500c1,
		0x84ef69,0x84ef34,0xdaeebc,0x393e310,274,
		0x3bd3ddc,0x3bcf880,0x4dcca0,0x4dd5c0,
		0x4502de,0x8f0e80,0x8f3b4d,
		0x84eeb0,0xaca9e9,
		0x3bcfb10,0x3be3140,0x3be31e0,0x3bd3e00,0x344,
	},
	//WE9
	{
		0x393e27c,265,139,57,32,1954,2219,1897,1865,
		0x38f9ae0,0x38f9ae4,0x3937218,0x850571,
		0x84f419,0x84f3e4,0xdaee1c,0x393e310,274,
		0x3bd3dfc,0x3bcf8a0,0x4dd0f0,0x4dda10,
		0x45067e,0x8f1060,0x8f3d2d,
		0x84f360,0xacaca9,
		0x3bcfb30,0,0,0x3bd3e20,0x344,
	},
	//WE9:LE
	{
		0x38930d4,265,139,57,32,1961,2226,1904,1872,
		0x38343f0,0x38343f4,0x388c070,0x877821,
		0x8766c9,0x876694,0xce8f74,0x3893168,274,
		0x3b5b91c,0x3b573c0,0x4db7b0,0x4dc0d0,
		0x4506ce,0x8f0620,0x8f32ed,
		0x876610,0xaca619,
		0x3b57650,0,0,0x3b5b940,0x344,
	}
};

DWORD fIDs[FIDSLEN];

typedef struct _PLAYERINFO {
	WORD id;
	//0=W; 1=Y; 2=R; 3=K
	BYTE SkinColor;
	//0 is the first face of the player's skincolor
	WORD FaceID;
	//
	BYTE FaceSet;
} PLAYERINFO;

typedef struct _ENCBUFFERHEADER {
	DWORD dwSig;
	DWORD dwEncSize;
	DWORD dwDecSize;
	BYTE other[20];
} ENCBUFFERHEADER;

typedef struct _INFOBLOCK {
	BYTE reserved1[0x54];
	DWORD FileID;
	BYTE reserved2[8];
	DWORD src;
	DWORD dest;
} INFOBLOCK;

typedef struct _DATAOFID {
	BYTE type;
	DWORD id;
} DATAOFID;

typedef struct _DATAOFMEMORY {
	BYTE type;
	DWORD size;
	BYTE data[];
} DATAOFMEMORY;

typedef struct _LINEUP_RECORD {
    DWORD ordinal;
    DWORD playerInfoAddr;
    DWORD unknown1[13];
    WORD playerId;
    BYTE unknown2;
    BYTE playerOrdinal; // in the team
    BYTE isRight; // 0-left, 1-right
    BYTE isAway;
    BYTE unknown3[0x24e];
} LINEUP_RECORD;

typedef struct _FACE_LINEUP_RECORD {
    WORD total_vertex;
    WORD unknown_1;
    DWORD face_big_texture_pointer;
    DWORD face_small_texture_pointer;
    DWORD unknown_2; //maybe related to face animations
    DWORD face_model_pointer_1;
    DWORD face_model_pointer_2; //maybe for low lod model?
    DWORD some_face_model;
    BYTE unknown_3[0x1c];
} FACE_LINEUP_RECORD;

typedef struct _HAIR_LINEUP_RECORD {
    BYTE unknown[8];
    DWORD hair_model_pointer;
    DWORD hair_big_texture_pointer;
    DWORD hair_small_texture_pointer;
} HAIR_LINEUP_RECORD;

#define FACESET_NONE 0
#define FACESET_SPECIAL 1
#define FACESET_NORMAL 2

#define HIGHEST_PLAYER_ID 0xFFFF
#define FIRSTREPLPLAYERID 0x7500

typedef void   (*COPYPLAYERDATA)(DWORD,DWORD,DWORD);
typedef BYTE   (*GETHAIRTRANSP)(DWORD);
typedef DWORD  (*EDITCOPYPLAYERDATA)(DWORD,DWORD);

KEXPORT void fservInitFacesAndHair();

