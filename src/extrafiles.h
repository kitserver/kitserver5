//the data format for extrafiles of savegames

enum {
	PESTYPE_OPTIONFILE,PESTYPE_REPLAY,PESTYPE_FORMATIONS,PESTYPE_CUP,PESTYPE_LEAGUE,
	PESTYPE_ML,PESTYPE_UNKNOWN6,PESTYPE_CHALLENGETR,PESTYPE_DRIBBLING,PESTYPE_UNKNOWN9,
	PESTYPE_ML_BESTTEAM,PESTYPE_UNKNOWN11,PESTYPE_UNKNOWN12
};

typedef struct _SAVEGAMEDATA {
	char sig[4];
	DWORD type;	//SGTYPE_???
	DWORD globalDataStart;
	DWORD team1DataStart;
	DWORD team2DataStart;
	DWORD playersDataStart;
} SAVEGAMEDATA;

enum {
	SGTYPE_REPLAY,
	SGTYPE_MLBESTTEAM,
};

typedef struct _SG_REPLAYPLAYERSDATA {
	DWORD playersStarts[64];
} SG_REPLAYPLAYERSDATA;

typedef struct _SG_ML_BT_PLAYERSDATA {
	DWORD playersStarts[64];
} SG_ML_BT_PLAYERSDATA;

typedef struct _SG_PLAYERDATA {
	DWORD numSects;
	DWORD sectStarts[];
} SG_PLAYERDATA;

//possible section types
enum {
	PDSECT_ID,
	PDSECT_FACE,
	PDSECT_HAIR,
	//PDSECT_BOOTS,
	PDSECT_NONE,
};

typedef struct _SG_PLAYERSECT_ID {
	DWORD type;	//PDSECT_???
	DWORD id;
} SG_PLAYERSECT_ID;

typedef struct _SG_PLAYERSECT_DATA {
	DWORD type;	//PDSECT_???
	DWORD size;
	BYTE data[];
} SG_PLAYERSECT_DATA;


typedef struct _DATAOFID {
	BYTE type;
	DWORD id;
} DATAOFID;

typedef struct _DATAOFMEMORY {
	BYTE type;
	DWORD size;
	BYTE data[];
} DATAOFMEMORY;
