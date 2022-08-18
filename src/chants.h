// chants.h

#define MODID 122
#define NAMELONG "Chants server 5.0.0"
#define NAMESHORT "CHANTS"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

#define DATALEN 5

#define AFS_FILE 0

#define HOME_CHANT_ID 25

#define AWAY_CHANT_ID 30

#define TOTAL_CHANTS 5

#define HOME 0

#define AWAY 1

enum {
    CHANTS_ADDRESS, TOTAL_TEAMS,
    TEAM_IDS, ML_HOME_AREA, ML_AWAY_AREA,
};

static DWORD dtaArray[][DATALEN] = {
	// PES5 Demo 2
	{
	 0, 0
     },
	// PES5
	{
	 0xadf128, 221,
     0x3be0f40, 0x38b77a4, 0x38b77a8,
     },
	// WE9
	{
     0xadf128, 221,
     0x3be0f60, 0x38b77a4, 0x38b77a8,
	},
    // WE9:LE
	{
     0xadced8, 221,
     0x3b68a80, 0x37f20b4, 0x37f20b8,
	},
};

static char* FILE_NAMES[] = {
    "chant_0.adx",
    "chant_1.adx",
	"chant_2.adx",
	"chant_3.adx",
	"chant_4.adx",
};


static DWORD dta[DATALEN];

