// scoreboard.h

#define MODID 121
#define NAMELONG "Scoreboard Server 5.0.0"
#define NAMESHORT "SCRSRV"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

#define DEFAULT_SELECTED_SCR 0
#define DEFAULT_PREVIEW_ENABLED 1
#define DEFAULT_KEY_RESET 0x43
#define DEFAULT_KEY_RANDOM 0x52
#define DEFAULT_KEY_PREV  0x56
#define DEFAULT_KEY_NEXT 0x42
#define DEFAULT_AUTO_RANDOM_MODE 0

#define MAX_ITERATIONS 1000

typedef struct _SCRSRV_CFG {
    int selectedScr;
    BOOL previewEnabled;
    BYTE keyReset;
    BYTE keyRandom;
    BYTE keyPrev;
    BYTE keyNext;
    BOOL autoRandomMode;
} SCRSRV_CFG;

