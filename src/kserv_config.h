#ifndef _JUCE_CONFIG
#define _JUCE_CONFIG

#include <windows.h>

#define BUFLEN 4096

#define MODID 100
#define NAMELONG "KitServer 5.5.7.0"
#define NAMESHORT "KSERV"
#define CONFIG_FILE "kserv.cfg"

#define DEFAULT_DEBUG 0
#define DEFAULT_VKEY_HOMEKIT 0x31
#define DEFAULT_VKEY_AWAYKIT 0x32
#define DEFAULT_VKEY_GKHOMEKIT 0x33
#define DEFAULT_VKEY_GKAWAYKIT 0x34
#define DEFAULT_SHOW_KIT_INFO true
#define DEFAULT_DISABLE_OVERLAYS false

typedef struct _KSERV_CONFIG_STRUCT {
    BYTE   narrowBackModels[256];
	WORD   vKeyHomeKit;
	WORD   vKeyAwayKit;
	WORD   vKeyGKHomeKit;
	WORD   vKeyGKAwayKit;
	bool   ShowKitInfo;
    bool   disableOverlays;
} KSERV_CONFIG;

BOOL ReadConfig(KSERV_CONFIG* config, char* cfgFile);
BOOL WriteConfig(KSERV_CONFIG* config, char* cfgFile);

#endif
