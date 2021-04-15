// bootserv.h

#define MODID 107
#define NAMELONG "Bootserver 5.6.0"
#define NAMESHORT "BOOTSERV"

#define DEFAULT_DEBUG 1
#define BUFLEN 4096

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
    
