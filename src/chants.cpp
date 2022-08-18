// chants.cpp
#include <windows.h>
#include <stdio.h>
#include "chants.h"
#include "kload_exp.h"
#include "afsreplace.h"
#include <map>


KMOD k_chants={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};
DWORD protection;
DWORD newProtection = PAGE_EXECUTE_READWRITE;


static std::map<WORD,std::string*> g_chantsMap;

bool homeHasChants = false;
bool awayHasChants = false;

#define MAP_FIND(map,key) map[key]
#define MAP_CONTAINS(map,key) (map.find(key)!=map.end())

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitChants();
bool chantsGetNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages);
bool chantsAfterReadFile(HANDLE hFile, LPVOID lpBuffer, 
        DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,  
        LPOVERLAPPED lpOverlapped);

void chantsBeginUniSelect();
void ReadMap();
WORD GetTeamId(int which);
void relinkChants();

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{	
		Log(&k_chants,"Attaching dll...");
		
		switch (GetPESInfo()->GameVersion) {
			case gvPES5PC: //support for PES5 PC...
			case gvWE9PC: //... and WE9 PC
            case gvWE9LEPC: //... and WE9:LE PC
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_chants,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:
		
		RegisterKModule(&k_chants);
		
		memcpy(dta, dtaArray[GetPESInfo()->GameVersion], sizeof(dta));
		
		
		
		HookFunction(hk_D3D_CreateDevice,(DWORD)InitChants);
		HookFunction(hk_BeginUniSelect, (DWORD)chantsBeginUniSelect);
        HookFunction(hk_GetNumPages,(DWORD)chantsGetNumPages);
		HookFunction(hk_AfterReadFile,(DWORD)chantsAfterReadFile);
		

	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_chants,"Detaching dll...");
		UnhookFunction(hk_BeginUniSelect, (DWORD)chantsBeginUniSelect);

		UnhookFunction(hk_AfterReadFile,(DWORD)chantsAfterReadFile);
		
		g_chantsMap.clear();
		
		Log(&k_chants,"Detaching done.");
	};

	return true;
};

void InitChants()
{
    
    ReadMap();
	UnhookFunction(hk_D3D_CreateDevice,(DWORD)InitChants);
}


bool chantsGetNumPages(DWORD afsId, DWORD fileId, DWORD orgNumPages, DWORD *numPages)
{
    DWORD fileSize = 0;
    char filename[512] = {0};

    if (afsId == X_TEXT) { // x_text.afs
		if (fileId >= HOME_CHANT_ID && fileId < HOME_CHANT_ID + TOTAL_CHANTS && homeHasChants){
			WORD homeTeamId = GetTeamId(HOME);
			std::string* homeFolderString = MAP_FIND(g_chantsMap,homeTeamId);
			if (homeFolderString != NULL) {
				/*strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\chants\\");
				strcat(filename,(char*)homeFolderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[fileId - HOME_CHANT_ID]);
				*/
				sprintf(filename,"%sGDB\\chants\\%s\\%s", GetPESInfo()->gdbDir, (char*)homeFolderString->c_str(), FILE_NAMES[fileId - HOME_CHANT_ID]);
				
				
			}
			else{
				return false;
			}
		}
		else if (fileId >= AWAY_CHANT_ID && fileId < AWAY_CHANT_ID + TOTAL_CHANTS && awayHasChants) {
			WORD awayTeamId = GetTeamId(AWAY);
			std::string* awayFolderString = MAP_FIND(g_chantsMap,awayTeamId);
			if (awayFolderString != NULL) {
				/*
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\chants\\");
				strcat(filename,(char*)awayFolderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[fileId - AWAY_CHANT_ID]);
				*/
				sprintf(filename,"%sGDB\\chants\\%s\\%s", GetPESInfo()->gdbDir, (char*)awayFolderString->c_str(), FILE_NAMES[fileId - AWAY_CHANT_ID]);

			}
			else{
				return false;
			}
		}
		else{
			return false;
		}
		HANDLE TempHandle=CreateFile(filename,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		if (TempHandle!=INVALID_HANDLE_VALUE) {
			fileSize=GetFileSize(TempHandle,NULL);
			CloseHandle(TempHandle);
		} else {
			fileSize=0;
		};

		if (fileSize > 0) {
			/*LogWithString(&k_chants, "chantsReadNumPages: found file: %s", filename);
			LogWithTwoNumbers(&k_chants,"chantsReadNumPages: had size: %08x pages (%08x bytes)", 
					orgNumPages, orgNumPages*0x800);*/

			// adjust buffer size to fit file
			*numPages = ((fileSize-1)>>0x0b)+1;
			/*LOG(&k_chants,
					"chantsReadNumPages: new size: %08x pages (%08x bytes)", 
					*numPages, (*numPages)*0x800);*/
			return true;
		}
    }
    return false;
}


bool chantsAfterReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped){
	DWORD afsId = GetAfsIdByOpenHandle(hFile);
    DWORD filesize = 0;
    char filename[512] = {0};

	// ensure it is x_TEXT.afs
	if (afsId == X_TEXT) {
		DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - (*lpNumberOfBytesRead);
		DWORD fileId = GetProbableFileIdForHandle(afsId, offset, hFile);

		if (fileId >= HOME_CHANT_ID && fileId < HOME_CHANT_ID + TOTAL_CHANTS && homeHasChants){
			WORD homeTeamId = GetTeamId(HOME);
			std::string* homeFolderString = MAP_FIND(g_chantsMap,homeTeamId);

			if (homeFolderString != NULL) {
				/*
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\chants\\");
				strcat(filename,(char*)homeFolderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[fileId - HOME_CHANT_ID]);
				*/
				sprintf(filename,"%sGDB\\chants\\%s\\%s", GetPESInfo()->gdbDir, (char*)homeFolderString->c_str(), FILE_NAMES[fileId - HOME_CHANT_ID]);
			}
			else{
				return false;
			}
		}
		else if (fileId >= AWAY_CHANT_ID && fileId < AWAY_CHANT_ID + TOTAL_CHANTS && awayHasChants) {
			WORD awayTeamId = GetTeamId(AWAY);
			std::string* awayFolderString = MAP_FIND(g_chantsMap,awayTeamId);
			if (awayFolderString != NULL) {
				/*
				strcpy(filename,GetPESInfo()->gdbDir);
				strcat(filename,"GDB\\chants\\");
				strcat(filename,(char*)awayFolderString->c_str());
				strcat(filename,"\\");
				strcat(filename,FILE_NAMES[fileId - AWAY_CHANT_ID]);
				*/
				sprintf(filename,"%sGDB\\chants\\%s\\%s", GetPESInfo()->gdbDir, (char*)awayFolderString->c_str(), FILE_NAMES[fileId - AWAY_CHANT_ID]);

			}
			else{
				return false;
			}
		}
		else{
			return false;
		}


		//LOG(&k_chants, "loading (%s)", filename);
		HANDLE handle = CreateFile(
		filename, 
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
		bool result = false;
		if (handle!=INVALID_HANDLE_VALUE) {
			filesize = GetFileSize(handle,NULL);
			DWORD curr_offset = offset - GetOffsetByFileId(afsId, fileId);
			/*LOG(&k_chants, 
			"offset=%08x, GetOffsetByFileId()=%08x, curr_offset=%08x",
			offset, GetOffsetByFileId(afsId, fileId), curr_offset);*/
			DWORD bytesToRead = *lpNumberOfBytesRead;
			if (filesize < curr_offset + *lpNumberOfBytesRead) {
				bytesToRead = filesize - curr_offset;
			}
			DWORD bytesRead = 0;
			SetFilePointer(handle, curr_offset, NULL, FILE_BEGIN);
			/*LOG(&k_chants, "reading 0x%x bytes (0x%x) from {%s}", 
			bytesToRead, *lpNumberOfBytesRead, filename);*/
			ReadFile(handle, lpBuffer, bytesToRead, &bytesRead, lpOverlapped);
			//LOG(&k_chants, "read 0x%x bytes from {%s}", bytesRead, filename);
			if (*lpNumberOfBytesRead - bytesRead > 0) {
				/*DEBUGLOG(&k_chants, "zeroing out 0x%0x bytes",
				*lpNumberOfBytesRead - bytesRead);*/
				memset(
				(BYTE*)lpBuffer+bytesRead, 0, *lpNumberOfBytesRead-bytesRead);
			}
			CloseHandle(handle);
			// set next likely read
			if (filesize > curr_offset + bytesRead) {
				SetNextProbableReadForHandle(
				afsId, offset+bytesRead, fileId, hFile);
			}
			result = true;
		}
		else {
			LOG(&k_chants, "Unable to read file {%s} must be protected or opened", filename);
		}
		return result;
	}
	return false;
}


void chantsBeginUniSelect()
{
    Log(&k_chants, "chantsBeginUniSelect");

    // check if home and away team has a chants designated in map.txt
	WORD homeTeamId = GetTeamId(HOME);
	WORD awayTeamId = GetTeamId(AWAY);
	std::string* homeFolderString = MAP_FIND(g_chantsMap,homeTeamId);
	std::string* awayFolderString = MAP_FIND(g_chantsMap,awayTeamId);
	if (homeFolderString != NULL) {
		homeHasChants = true;
	}
	else{
		homeHasChants = false;
	}
	if (awayFolderString != NULL) {
		awayHasChants = true;
	}
	else{
		awayHasChants = false;
	}
	relinkChants();
    return;
}

void relinkChants()
{
	if (VirtualProtect((LPVOID)dta[CHANTS_ADDRESS], dta[TOTAL_TEAMS] * 4, newProtection, &protection)) {
		
		if (homeHasChants){
			WORD homeTeamId = GetTeamId(HOME);
			*(WORD*)(dta[CHANTS_ADDRESS] + homeTeamId * 4) = HOME_CHANT_ID;
			*(WORD*)(dta[CHANTS_ADDRESS] + homeTeamId * 4 + 2) = X_TEXT;
		}
		if (awayHasChants){
			WORD awayTeamId = GetTeamId(AWAY);
			*(WORD*)(dta[CHANTS_ADDRESS] + awayTeamId * 4) = AWAY_CHANT_ID;
			*(WORD*)(dta[CHANTS_ADDRESS] + awayTeamId * 4 + 2) = X_TEXT;
		}
	};

}

void ReadMap()
{
	//map.txt
	
    char mapFile[BUFLEN];
    ZeroMemory(mapFile,BUFLEN);

    sprintf(mapFile, "%sGDB\\chants\\map.txt", GetPESInfo()->gdbDir);
    FILE* map = fopen(mapFile, "rt");
    if (map == NULL) {
        LOG(&k_chants, "Unable to find chants map: %s", mapFile);
        return;
    }

	// go line by line
    char buf[BUFLEN];
	while (!feof(map))
	{
		ZeroMemory(buf, BUFLEN);
		fgets(buf, BUFLEN, map);
		if (lstrlen(buf) == 0) break;

		// strip off comments
		char* comm = strstr(buf, "#");
		if (comm != NULL) comm[0] = '\0';

        // find team id
        WORD teamId = 0xffff;
        if (sscanf(buf, "%d", &teamId)==1) {
            LogWithNumber(&k_chants, "teamId = %d", teamId);
            char* foldername = NULL;
            // look for comma
            char* pComma = strstr(buf,",");
            if (pComma) {
                // what follows is the filename.
                // It can be contained within double quotes, so 
                // strip those off, if found.
                char* start = NULL;
                char* end = NULL;
                start = strstr(pComma + 1,"\"");
                if (start) end = strstr(start + 1,"\"");
                if (start && end) {
                    // take what inside the quotes
                    end[0]='\0';
                    foldername = start + 1;
                } else {
                    // just take the rest of the line
                    foldername = pComma + 1;
                }

                LOG(&k_chants, "foldername = {%s}", foldername);

                // store in the home-ball map
				g_chantsMap[teamId] = new std::string(foldername);
            }
        }
    }
    fclose(map);
	
	return;
};


WORD GetTeamId(int which)
{
    BYTE* mlData;
    if (dta[TEAM_IDS]==0) return 0xffff;
    WORD id = ((WORD*)dta[TEAM_IDS])[which];
    if (id==0xf2 || id==0xf3) {
        switch (id) {
            case 0xf2:
                // master league team (home)
                mlData = *((BYTE**)dta[ML_HOME_AREA]);
                id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
                break;
            case 0xf3:
                // master league team (away)
                mlData = *((BYTE**)dta[ML_AWAY_AREA]);
                id = *((DWORD*)(mlData + 0x6c)) & 0xffff; // 3rd byte is a flag of "edited" kit
                break;
        }
    }
    return id;
}

