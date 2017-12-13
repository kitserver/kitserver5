// detect.cpp

#include "windows.h"
#include <stdio.h>
#include "detect.h"
#include "imageutil.h"

char* GAME[] = { 
	"PES5 PC DEMO 2",
	"PES5 PC",
	"WE9 PC",
	"WE9:LE PC",
	"WE9:LE PC (NOCD)",
};
char* GAME_GUID[] = {
	"{5F982571-1E7B-4eee-BFDD-7FBA80117E59}",
	"{FAD2F3FB-BD36-4e6b-A287-A1052274BC3E}",
	"{923628C6-9F23-4041-9983-49D643B538DE}",
	"{671CB6AE-2297-4656-B61B-88FAF5FEC883}",
	"{671CB6AE-2297-4656-B61B-88FAF5FEC883}",
};
DWORD GAME_GUID_OFFSETS[] = { 
    0x41a470, 0x6ce490, 0x6ce490, 0x6ce490, 0x6cca90,
};

// Returns the game version id
int GetGameVersion(void)
{
	HMODULE hMod = GetModuleHandle(NULL);
	for (int i=0; i<sizeof(GAME_GUID)/sizeof(char*); i++)
	{
		char* guid = (char*)((DWORD)hMod + GAME_GUID_OFFSETS[i]);
		if (strncmp(guid, GAME_GUID[i], lstrlen(GAME_GUID[i]))==0)
			return i;
	}
	return -1;
}

// Returns the game version id
int GetGameVersion(char* filename)
{
	char guid[] = "{00000000-0000-0000-0000-000000000000}";

	FILE* f = fopen(filename, "rb");
	if (f == NULL)
		return -1;

	// check for regular exes
	for (int i=0; i<sizeof(GAME_GUID)/sizeof(char*); i++)
	{
		fseek(f, GAME_GUID_OFFSETS[i], SEEK_SET);
		fread(guid, lstrlen(GAME_GUID[i]), 1, f);
		if (strncmp(guid, GAME_GUID[i], lstrlen(GAME_GUID[i]))==0)
		{
			fclose(f);
			return i;
		}
	}

	// unrecognized.
	return -1;
}

