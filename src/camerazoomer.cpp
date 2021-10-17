// camerazoomer.cpp
#include <windows.h>
#include <stdio.h>
#include "camerazoomer.h"
#include "kload_exp.h"

KMOD k_camera={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;

class config_t 
{
public:
    config_t() : cameraZoom(1430), fixStadiumClip(false), addStadiumRoof(false) {}
    float cameraZoom;
	bool fixStadiumClip;
	bool addStadiumRoof;
};

config_t camera_config;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitCameraZoomer();
void SetCameraData();
bool readConfig(config_t& config);

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_camera,"Attaching dll...");

		switch (GetPESInfo()->GameVersion) {
			case gvPES5PC: //support for PES5 PC...
            case gvWE9LEPC: //... and WE9:LE PC
				break;
            default:
                Log(&k_camera,"Your game version is currently not supported!");
                return false;
		}
		
		hInst=hInstance;

		RegisterKModule(&k_camera);
		
		HookFunction(hk_D3D_Create,(DWORD)InitCameraZoomer);
		HookFunction(hk_BeginUniSelect,(DWORD)SetCameraData);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_camera,"Detaching dll...");
		UnhookFunction(hk_BeginUniSelect,(DWORD)SetCameraData);
		Log(&k_camera,"Detaching done.");
	}
	
	return true;
}

void InitCameraZoomer()
{
	UnhookFunction(hk_D3D_Create,(DWORD)InitCameraZoomer);

	// read configuration
    readConfig(camera_config);
	
	Log(&k_camera, "Module initialized.");
}

void SetCameraData()
{
	int i;
	int PES5stadiumClipArray[] = {0x59c148, 0x59c1be, 0x59c20f, 0x59c226, 0x59C23B, 0x59c5a7, 0x59c5c4, 0x59c5c5, 0x59c5c5};
	int WE9LEstadiumClipArray[] = {0x5980b8, 0x59812e, 0x59817f, 0x598196, 0x5981ab, 0x598517, 0x598534, 0x598535, 0x598536};
	int stadiumClipValuesTrueArray[] = {0xeb, 0xeb, 0xeb, 0xeb, 0xeb, 0xeb, 0x90, 0x90, 0x90};
	int stadiumClipValuesFalseArray[] = {0x7a, 0x7a, 0x75, 0x7a, 0x75, 0x75, 0x83, 0xce, 0x0c};
	int addStadiumRoofValue = 0xbf800000;
	int defaultStadiumRoofValue = 0xbdd67750;
	switch (GetPESInfo()->GameVersion) {
		case gvPES5PC:
			// Big thanks to elaf who helped me get the right address for camera zoom and give some guidence to get the others values
			if (VirtualProtect((LPVOID)0x8e67a2, sizeof(0x8e67a2), newProtection, &protection)) {
				*(float*)0x8e67a2 =(float)camera_config.cameraZoom;
				LogWithDouble(&k_camera, "Camera zoom set to (%g)", (double)*(float*)0x8e67a2);
			}
			else {
				Log(&k_camera, "Problem changing camera zoom.");
			}
			// Fixing stadium clipping... many addresses sorry this is the only way i know how to do it
			for (i = 0 ; i<9; i++){
				if (VirtualProtect((LPVOID)PES5stadiumClipArray[i], sizeof(PES5stadiumClipArray[i]), newProtection, &protection)) {
					if (camera_config.fixStadiumClip){
						*(BYTE*)PES5stadiumClipArray[i] = stadiumClipValuesTrueArray[i];
						Log(&k_camera,"Stadium clipping fixed.");
					}
					else{
						*(BYTE*)PES5stadiumClipArray[i] = stadiumClipValuesFalseArray[i];
						Log(&k_camera,"Stadium clipping was not fixed, by user choice, default values were set.");
					}
				}
				else {
					Log(&k_camera, "Problem trying to fix stadium clipping.");
				}
			}
			// adding stadium roof 0xaea994
			if (VirtualProtect((LPVOID)0xaea994, sizeof(0xaea994), newProtection, &protection)) {
				if(camera_config.addStadiumRoof){
					*(int*)0xaea994 = addStadiumRoofValue;
					Log(&k_camera, "Stadium roof added.");					
				}
				else{
					*(int*)0xaea994 = defaultStadiumRoofValue;
					Log(&k_camera, "Stadium roof not added, by user choice, default values were set.");
				}
				
			}
			else {
				Log(&k_camera, "Problem adding stadium roof.");
			}
		break;
		case gvWE9LEPC:
			// Camera address for we9lek 0x8e5f42
			if (VirtualProtect((LPVOID)0x8e5f42, sizeof(0x8e5f42), newProtection, &protection)) {
				*(float*)0x8e5f42 =(float)camera_config.cameraZoom;
				LogWithDouble(&k_camera, "Camera zoom set to (%g)", (double)*(float*)0x8e5f42);
			}
			else {
				Log(&k_camera, "Problem changing camera zoom.");
			}
			// Fixing stadium clipping... many addresses sorry this is the only way i know how to do it
			for (i = 0 ; i<9; i++){
				if (VirtualProtect((LPVOID)WE9LEstadiumClipArray[i], sizeof(WE9LEstadiumClipArray[i]), newProtection, &protection)) {
					if (camera_config.fixStadiumClip){
						*(BYTE*)WE9LEstadiumClipArray[i] = stadiumClipValuesTrueArray[i];
						Log(&k_camera,"Stadium clipping fixed.");
					}
					else{
						*(BYTE*)WE9LEstadiumClipArray[i] = stadiumClipValuesFalseArray[i];
						Log(&k_camera,"Stadium clipping was not fixed, by user choice, default values were set.");
					}
				}
				else {
					Log(&k_camera, "Problem trying to fix stadium clipping.");
				}
			}
			// adding stadium roof 0xae8cf4
			if (VirtualProtect((LPVOID)0xae8cf4, sizeof(0xae8cf4), newProtection, &protection)) {
				if(camera_config.addStadiumRoof){
					*(int*)0xae8cf4 = addStadiumRoofValue;
					Log(&k_camera, "Stadium roof added.");					
				}
				else{
					*(int*)0xae8cf4 = defaultStadiumRoofValue;
					Log(&k_camera, "Stadium roof not added, by user choice, default values were set.");
				}
				
			}
			else {
				Log(&k_camera, "Problem adding stadium roof.");
			}
		break;
	}
}


bool readConfig(config_t& config)
{
	//Log(&k_camera, "enter the function.");
    string cfgFile(GetPESInfo()->mydir);
    cfgFile += "\\camerazoomer.cfg";

	FILE* cfg = fopen(cfgFile.c_str(), "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
		//Log(&k_camera, "now in the while.");
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;
		//Log(&k_camera, "before break");
		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;
		//Log(&k_camera, "before continue");
		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);
		//Log(&k_camera, "before if");
		if (strcmp(name, "zoom")==0)
		{
			//Log(&k_camera, "last if");
            float value;
			if (sscanf(pValue, "%f", &value)!=1) continue;
			LogWithDouble(&k_camera, "ReadConfig: zoom = (%g)", (double)value);
			config.cameraZoom = value;
		}
		else if (strcmp(name, "fix_stadium_clipping")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_camera, "ReadConfig: fix_stadium_clipping = (%d)", value);
			config.fixStadiumClip = (value > 0);
		}
		else if (strcmp(name, "add_stadium_roof")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			LogWithNumber(&k_camera, "ReadConfig: add_stadium_roof = (%d)", value);
			config.addStadiumRoof = (value > 0);
		}
		
	}
	fclose(cfg);
	return true;
}