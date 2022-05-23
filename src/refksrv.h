// refksrv.h

#define MODID 120
#define NAMELONG "Referee Kit Server 5.1.0"
#define NAMESHORT "REFKSRV"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

#define DEFAULT_SELECTED_REF_KIT 0
#define DEFAULT_PREVIEW_ENABLED 1
#define DEFAULT_KEY_RESET 0x43
#define DEFAULT_KEY_RANDOM 0x52
#define DEFAULT_KEY_PREV  0x56
#define DEFAULT_KEY_NEXT 0x42
#define DEFAULT_AUTO_RANDOM_MODE 0

#define MAX_ITERATIONS 1000

typedef struct _REF_KITS {
	LPTSTR display;
	LPTSTR model;
	LPTSTR folder;
} REF_KITS;

typedef struct _REFKSRV_CFG {
    int selectedRefKit;
    BOOL previewEnabled;
    BYTE keyReset;
    BYTE keyRandom;
    BYTE keyPrev;
    BYTE keyNext;
    BOOL autoRandomMode;
} REFKSRV_CFG;

