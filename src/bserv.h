// bserv.h

#define MODID 101
#define NAMELONG "Ballserver 5.5.9"
#define NAMESHORT "BSERV"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

#define CODELEN 2
enum {
    C_GETFILEFROMAFS_CS, C_SETBALLNAME_CS,
};

static DWORD codeArray[][CODELEN] = {
	// PES5 Demo 2
	{0, 0,
     },
	// PES5
	{0x5b7ea2, 0x9b1078,
     },
	// WE9
	{0x5b82b2, 0x9b1358,
	},
	// WE9:LE
	{0x5df692, 0x9b3cf8,
	},
};

#define DATALEN 5
enum {
    AFS_PAGELEN_TABLE, NUM_BALL_FILES,
    TEAM_IDS, ML_HOME_AREA, ML_AWAY_AREA,
};

static DWORD dataArray[][DATALEN] = {
	// PES5 Demo 2
	{
	 0, 0
     },
	// PES5
	{
	 0x3bfff00, 16,
     0x3be0f40, 0x38b77a4, 0x38b77a8,
     },
	// WE9
	{
     0x3bfff20, 16,
     0x3be0f60, 0x38b77a4, 0x38b77a8,
	},
    // WE9:LE
	{
     0x3adef40, 22,
     0x3b68a80, 0x37f20b4, 0x37f20b8,
	},
};


static DWORD code[CODELEN];
static DWORD data[DATALEN];
	

#define SWAPBYTES(dw) \
    (dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff)

#define MAX_ITERATIONS 1000

typedef struct _AFSENTRY {
	DWORD FileNumber;
	DWORD AFSAddr;
	DWORD FileSize;
	DWORD Buffer;
} AFSENTRY;

typedef struct _BALLS {
	LPTSTR display;
	LPTSTR model;
	LPTSTR texture;
} BALLS;

typedef struct _ENCBUFFERHEADER {
	DWORD dwSig;
	DWORD dwEncSize;
	DWORD dwDecSize;
	BYTE other[20];
} ENCBUFFERHEADER;

typedef struct _DECBUFFERHEADER {
	DWORD dwSig;
	DWORD numTexs;
	DWORD dwDecSize;
	BYTE reserved1[4];
	WORD bitsOffset;
	WORD paletteOffset;
	WORD texWidth;
	WORD texHeight;
	BYTE reserved2[2];
	BYTE bitsPerPixel;
	BYTE reserved3[13];
	WORD blockWidth;
	WORD blockHeight;
	BYTE other[84];
} DECBUFFERHEADER;

typedef struct _PNG_CHUNK_HEADER {
    DWORD dwSize;
    DWORD dwName;
} PNG_CHUNK_HEADER;

#define DEFAULT_SELECTED_BALL 0
#define DEFAULT_PREVIEW_ENABLED 1
#define DEFAULT_KEY_RESET_BALL 0x43
#define DEFAULT_KEY_RANDOM_BALL 0x52
#define DEFAULT_KEY_PREV_BALL  0x56
#define DEFAULT_KEY_NEXT_BALL 0x42
#define DEFAULT_HOME_TEAM_CHOICE 0
#define DEFAULT_AUTO_RANDOM_MODE 0

typedef struct _BSERV_CFG {
    int selectedBall;
    BOOL previewEnabled;
    BYTE keyResetBall;
    BYTE keyRandomBall;
    BYTE keyPrevBall;
    BYTE keyNextBall;
    BOOL homeTeamChoice;
    BOOL autoRandomMode;
} BSERV_CFG;

