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
    config_t() : cameraZoom(1430) {}
    float cameraZoom;
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
	switch (GetPESInfo()->GameVersion) {
		case gvPES5PC:
			// Big thanks to elaf who helped me get the right address
			if (VirtualProtect((LPVOID)0x8e67a2, sizeof(0x8e67a2), newProtection, &protection)) {
				*(float*)0x8e67a2 =(float)camera_config.cameraZoom;
				LogWithDouble(&k_camera, "Camera zoom set to (%g)", (double)*(float*)0x8e67a2);
			}
			else {
				Log(&k_camera, "Problem changing camera zoom.");
			}
		break;
		// Camera address for we9lek 0x8e5f42
		case gvWE9LEPC:
			if (VirtualProtect((LPVOID)0x8e5f42, sizeof(0x8e5f42), newProtection, &protection)) {
				*(float*)0x8e5f42 =(float)camera_config.cameraZoom;
				LogWithDouble(&k_camera, "Camera zoom set to (%g)", (double)*(float*)0x8e5f42);
			}
				else {
					Log(&k_camera, "Problem changing camera zoom.");
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
	}
	fclose(cfg);
	return true;
}