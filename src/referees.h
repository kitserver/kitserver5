// referees.h
#include <windows.h>
#include <stdio.h>
#include "kload_exp.h"
#include "afsreplace.h"
#include "unordered_map"

#define MODID 987
#define NAMELONG "Referees 5.1.0"
#define NAMESHORT "REFS"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

#define DEFAULT_SELECTED_REFEREE 0
#define DEFAULT_PREVIEW_ENABLED 1
#define DEFAULT_KEY_RESET 0x43
#define DEFAULT_KEY_RANDOM 0x52
#define DEFAULT_KEY_PREV  0x56
#define DEFAULT_KEY_NEXT 0x42
#define DEFAULT_AUTO_RANDOM_MODE 0

#define MAX_ITERATIONS 1000

typedef struct _REFEREE_INFO {
    LPTSTR name;
    LPTSTR nationality = "Free Nationality";
    BYTE height = 170;
    BYTE weight = 80;
    BYTE skinColour = 0x01;
    WORD faceID = 0;
    WORD hairstyleID = 0;
    BYTE hairPattern = 0;
	BYTE facialHairType = 0;
	BYTE facialHairColour = 0;
    LPTSTR cardStrictness = "Default";
    DWORD customCardStrictness = 0;
} REFEREE_INFO;

typedef struct _REFEREES {
	LPTSTR refereeFolder;
	LPTSTR refereeDisplay;
} REFEREES;

class config_t 
{
public:
    config_t() : debug(DEFAULT_DEBUG), additionalRefsExhibitionEnabled(false), cardStrictnessEnabled(false), randomCardStrictnessEnabled(false), 
		cardstrictness_random_minimum(0), cardstrictness_random_maximum(0), card_strictness(0), selectedReferee(DEFAULT_SELECTED_REFEREE),
		previewEnabled(DEFAULT_PREVIEW_ENABLED), keyReset(DEFAULT_KEY_RESET), keyRandom(DEFAULT_KEY_RANDOM), keyPrev(DEFAULT_KEY_PREV),
		keyNext(DEFAULT_KEY_NEXT), autoRandomMode(DEFAULT_AUTO_RANDOM_MODE) {}
	BOOL debug;
	bool additionalRefsExhibitionEnabled;
	bool cardStrictnessEnabled;
	bool randomCardStrictnessEnabled;
	DWORD cardstrictness_random_minimum;
	DWORD cardstrictness_random_maximum;
	DWORD card_strictness;
    int selectedReferee;
    BOOL previewEnabled;
    BYTE keyReset;
    BYTE keyRandom;
    BYTE keyPrev;
    BYTE keyNext;
    BOOL autoRandomMode;
};

#define DATALEN 16
#define CODELEN 2

enum {
	REF_START_ADDRESS, REF_DATA_SIZE,
	SELECTED_REF, SELECT_REF_FLAG, SELECT_TOTAL_REF, EXTRA_REF_JMP,
	REF_FACE_W, REF_FACE_Y, REF_FACE_R, REF_FACE_K,
	REF_FACE_W_ID, REF_FACE_Y_ID, REF_FACE_R_ID, REF_FACE_K_ID,
	REF_HAIRFILE, REF_HAIRFILE_ID,
};

static DWORD dtaArray[][DATALEN] = {
	// PES5 DEMO 2
	{
		0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0,
	},
	// PES5
	{
		0xff6bb0, 104,
		0xfe0d6c, 0xfe0d78, 0xfe0d74, 0x52c3c1, // SELECTED_REF, SELECT_REF_FLAG are not used anymore
		2212, 2357, 1953, 1896,
		258, 138, 56, 31,
		4123, 1299, //Last dog hairstyle
	},
	// WE9
	{
		0xff6bb0, 104,
		0xfe0d6c, 0xfe0d78, 0xfe0d74, 0x52c3c1,
		2212, 2357, 1953, 1896,
		258, 138, 56, 31,
		4123, 1299, //Last dog hairstyle
	},
	// WE9:LE
	{
		0xf77650, 104,
		0, 0, 0xf1ad04, 0x527011,
		2219, 2364, 1960, 1903,
		258, 138, 56, 31,
		4130, 1299, //Last dog hairstyle
	},
};


enum {
	C_PROCESS_REFEREES_DATA_JMP, C_AFTER_PROCESS_REFEREES_DATA_JMP,
};

static DWORD codeArray[][DATALEN] = {
	// PES5 DEMO 2
	{
		0, 0,
	},
	// PES5
	{
		0x9aa4e0, 0x9aa4e8,
	},
	// WE9
	{
		0x9aa4e0, 0x9aa4e8,
	},
	// WE9:LE
	{
		0x9ad560, 0x9ad568,
	},
};

static DWORD dta[DATALEN];
static DWORD code[CODELEN];

#define FREE_NATIONALITY 108
#define TOTAL_NATIONALITIES 109

const string nationalities[] = { "Austria", "Belgium", "Bulgaria", "Croatia", "Czech Republic", "Denmark", "England", "Finland", "France", "Germany", "Greece",
"Hungary", "Ireland", "Italy", "Latvia", "Netherlands", "Northern Ireland", "Norway", "Poland", "Portugal", "Romania", "Russia", "Scotland", "Serbia and Montenegro",
"Slovakia", "Slovenia", "Spain", "Sweden", "Switzerland", "Turkey", "Ukraine", "Wales", "Cameroon", "Cote d'Ivoire", "Morocco", "Nigeria",
"Senegal", "South Africa", "Tunisia", "Costa Rica", "Mexico", "USA", "Argentina", "Brazil", "Chile", "Colombia",
"Ecuador", "Paraguay", "Peru", "Uruguay", "Venezuela", "China", "Iran", "Japan", "Saudi Arabia", "South Korea", "Australia", "Albania", "Armenia", "Belarus",
"Bosnia and Herzegovina", "Cyprus", "Georgia", "Estonia", "Faroe Islands", "Iceland", "Israel", "Lithuania", "Luxembourg", "Macedonia", "Moldova", "Algeria",
"Angola", "Burkina Faso", "Cape Verde", "Congo", "DR Congo", "Egypt", "Equatorial Guinea", "Gabon", "Gambia", "Ghana", "Guinea", "Guinea-Bissau", "Liberia",
"Libya", "Mali", "Mauritius", "Mozambique", "Namibia", "Sierra Leone", "Togo", "Zambia", "Zimbabwe", "Canada", "Grenada", "Guadeloupe", "Guatemala", "Honduras",
"Jamaica", "Martinique", "Netherlands Antilles", "Panama", "Trinidad and Tobago", "Bolivia", "Guyana", "Uzbekistan", "New Zealand", "Free Nationality" };

