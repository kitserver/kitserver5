// camerazoomer.h
#include <windows.h>
#define MODID 110
#define NAMELONG "Camera Zoomer 5.0.0"
#define NAMESHORT "CAMERA"

#define DEFAULT_DEBUG 1

#define BUFLEN 4096

class config_t 
{
public:
    config_t() : cameraZoom(1430.0), fixStadiumClip(false), addStadiumRoof(false) {}
    float cameraZoom;
	bool fixStadiumClip;
	bool addStadiumRoof;
};

#define DATALEN 11

enum {
	CAMERA_ZOOM, STADIUM_ROOF, 
	STADIUM_CLIP_1, STADIUM_CLIP_2, STADIUM_CLIP_3,
	STADIUM_CLIP_4, STADIUM_CLIP_5, STADIUM_CLIP_6,
	STADIUM_CLIP_7, STADIUM_CLIP_8, STADIUM_CLIP_9,
	
};

static DWORD dtaArray [][DATALEN] = {
	// pes5demo2
	{
		0, 0,
		0, 0, 0,
		0, 0, 0,
		0, 0, 0,
	},
	//pes5
	{
		0x8e67a2, 0xaea994,
		0x59c148, 0x59c1be, 0x59c20f, 
		0x59c226, 0x59C23B, 0x59c5a7, 
		0x59c5c4, 0x59c5c5, 0x59c5c6,
	},
	//we9
	{
		0x8e67a2, 0xaea994,
		0x59c148, 0x59c1be, 0x59c20f, 
		0x59c226, 0x59C23B, 0x59c5a7, 
		0x59c5c4, 0x59c5c5, 0x59c5c6,

	},
	//we9le
	{
		0x8e5f42, 0xae8cf4,
		0x5980b8, 0x59812e, 0x59817f, 
		0x598196, 0x5981ab, 0x598517, 
		0x598534, 0x598535, 0x598536,
	},

};

static DWORD dta[DATALEN];

BYTE stadiumClipValuesTrueArray[] = {0xeb, 0xeb, 0xeb, 0xeb, 0xeb, 0xeb, 0x90, 0x90, 0x90};
BYTE stadiumClipValuesFalseArray[] = {0x7a, 0x7a, 0x75, 0x7a, 0x75, 0x75, 0x83, 0xce, 0x0c};
DWORD addStadiumRoofValue = 0xbf800000;
DWORD defaultStadiumRoofValue = 0xbdd67750;


