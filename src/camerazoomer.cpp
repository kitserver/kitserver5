// camerazoomer.cpp
#include <windows.h>
#include <stdio.h>
#include "camerazoomer.h"
#include "kload_exp.h"

KMOD k_camera={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;

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

		memcpy(dta, dtaArray[GetPESInfo()->GameVersion], sizeof(dta));

		RegisterKModule(&k_camera);
		
		HookFunction(hk_D3D_Create,(DWORD)InitCameraZoomer);
		//HookFunction(hk_BeginUniSelect,(DWORD)SetCameraData);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_camera,"Detaching dll...");
		//UnhookFunction(hk_D3D_Create,(DWORD)SetCameraData);
		Log(&k_camera,"Detaching done.");
	}
	
	return true;
}

void InitCameraZoomer()
{
	UnhookFunction(hk_D3D_Create,(DWORD)InitCameraZoomer);

	// read configuration
    readConfig(camera_config);
	SetCameraData();
	Log(&k_camera, "Module initialized.");
}

void SetCameraData(){
	if (VirtualProtect((LPVOID)dta[CAMERA_ZOOM], sizeof(float), newProtection, &protection)) {
		*(float*)dta[CAMERA_ZOOM] = (float)camera_config.cameraZoom;
		LogWithDouble(&k_camera, "Camera zoom set to (%g)", (double)*(float*)dta[CAMERA_ZOOM]);
		//restore old protection
		VirtualProtect((LPVOID)dta[CAMERA_ZOOM], sizeof(float), protection, &protection);
	}
	else {
		Log(&k_camera, "Problem changing camera zoom.");
	}
	// Fixing stadium clipping... many addresses sorry this is the only way i know how to do it
	for (int i = 0 ; i<sizeof(stadiumClipValuesTrueArray) / sizeof(stadiumClipValuesTrueArray[0]); i++){
		if (VirtualProtect((LPVOID)dta[STADIUM_CLIP_1 + i], sizeof(BYTE), newProtection, &protection)) {
			if (camera_config.fixStadiumClip){
				*(BYTE*)dta[STADIUM_CLIP_1 + i] = stadiumClipValuesTrueArray[i];
				Log(&k_camera,"Stadium clipping fixed.");
			}
			else{
				*(BYTE*)dta[STADIUM_CLIP_1 + i] = stadiumClipValuesFalseArray[i];
				Log(&k_camera,"Stadium clipping was not fixed, by user choice, default values were set.");
			}
			//restore old protection
			VirtualProtect((LPVOID)dta[STADIUM_CLIP_1 + i], sizeof(BYTE), protection, &protection);
		}
		else {
			Log(&k_camera, "Problem trying to fix stadium clipping.");
		}
	}
	// adding stadium roof 0xaea994
	if (VirtualProtect((LPVOID)dta[STADIUM_ROOF], sizeof(DWORD), newProtection, &protection)) {
		if(camera_config.addStadiumRoof){
			*(DWORD*)dta[STADIUM_ROOF] = addStadiumRoofValue;
			Log(&k_camera, "Stadium roof added.");					
		}
		else{
			*(DWORD*)dta[STADIUM_ROOF] = defaultStadiumRoofValue;
			Log(&k_camera, "Stadium roof not added, by user choice, default values were set.");
		}
		//restore old protection
		VirtualProtect((LPVOID)dta[STADIUM_ROOF], sizeof(DWORD), protection, &protection);
	}
	else {
		Log(&k_camera, "Problem adding stadium roof.");
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