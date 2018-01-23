#ifndef _FSERV_CONFIG
#define _FSEV_CONFIG

#include <windows.h>

#define BUFLEN 4096

#define CONFIG_FILE "fserv.cfg"

#define DEFAULT_DUMP_FACE_TEXTURES false
#define DEFAULT_HD_FACE_WIDTH 256
#define DEFAULT_HD_FACE_HEIGHT 512
#define MIN_HD_FACE_WIDTH 64
#define MIN_HD_FACE_HEIGHT 128

typedef struct _FSERV_CONFIG_STRUCT {
    bool   dump_face_textures;
    UINT   hd_face_width;
    UINT   hd_face_height;
} FSERV_CONFIG;

BOOL ReadConfig(FSERV_CONFIG* config, char* cfgFile);

#endif
