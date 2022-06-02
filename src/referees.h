// referees.h

#define MODID 987
#define NAMELONG "Referees 5.0.2"
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
    LPTSTR nationality;
    BYTE height;
    BYTE weight;
    BYTE skinColour;
    LPTSTR cardStrictness;
    DWORD customCardStrictness;
} REFEREE_INFO;

typedef struct _REFEREES {
	LPTSTR referee1Folder;
	LPTSTR referee2Folder;
	LPTSTR referee3Folder;
	LPTSTR referee4Folder;
	LPTSTR referee1Display;
	LPTSTR referee2Display;
	LPTSTR referee3Display;
	LPTSTR referee4Display;
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
