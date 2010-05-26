// roster.cpp
#include <windows.h>
#include <stdio.h>
#include "curl.h"
#include "roster.h"
#include "kload_exp.h"
#include "afsreplace.h"

KMOD k_roster={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
DWORD _rosterOffset(0xffffffff);
bool _networkMode(false);
//bool _showMessages(false);
bool _downloading(false);


#define DATALEN 1
enum {
    MAIN_MENU_MODE
};

static DWORD dataArray[][DATALEN] = {
    // PES5 DEMO 2
    {
        0,
    },
    // PES5
    {
        0xfde858,
    },
    // WE9
    {
        0xfde860,
    },
    // WE9:LE
    {
        0xf187f0,
    },
};

static DWORD data[DATALEN];

enum {
    ROSTER_URL,
    ROSTER_BIN,
    ROSTER_BIN_TEMP,
    ROSTER_BIN_ETAG,
};

static char* rosterNames[][4] = {
    // PES5 DEMO 2
    {"","","",""},
    // PES5
    {
        "http://pes5-club.pesgame.net/updates/roster-pes5.bin",
        "\\GDB\\network\\roster-pes5.bin",
        "\\GDB\\network\\roster-pes5.bin.tmp",
        "\\GDB\\network\\roster-pes5.bin.etag",
    },
    // WE9
    {
        "http://pes5-club.pesgame.net/updates/roster-we9.bin",
        "\\GDB\\network\\roster-we9.bin",
        "\\GDB\\network\\roster-we9.bin.tmp",
        "\\GDB\\network\\roster-we9.bin.etag",
    },
    // WE9:LE
    {
        "http://pes5-club.pesgame.net/updates/roster-we9le.bin",
        "\\GDB\\network\\roster-we9le.bin",
        "\\GDB\\network\\roster-we9le.bin.tmp",
        "\\GDB\\network\\roster-we9le.bin.etag",
    },
};

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void initModule();
size_t HeaderFunction( void *ptr, size_t size, size_t nmemb, void *stream);
size_t WriteChunkCallback(void *ptr, size_t size, size_t nmemb, void *data);
bool rosterReadNumPages(DWORD afsId, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages);
void rosterAfterReadFile(HANDLE hFile, 
                       LPVOID lpBuffer, 
                       DWORD nNumberOfBytesToRead,
                       LPDWORD lpNumberOfBytesRead,  
                       LPOVERLAPPED lpOverlapped);
void rosterPresent(IDirect3DDevice8* self, 
        CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);

typedef struct _CHUNK_READ
{
    HANDLE hFile;
    DWORD totalFetched;
} CHUNK_READ;

typedef struct _META_INFO
{
    char etag[1024];
    DWORD contentLength;
    CURL* hCurl;
} META_INFO;

bool getRosterMetaInfo(META_INFO* pMetaInfo);
bool downloadRoster(META_INFO* pMetaInfo);
bool getLocalEtag(META_INFO* pMetaInfo);
bool setLocalEtag(META_INFO* pMetaInfo);

// global meta-info
META_INFO _metaInfo;


size_t HeaderFunction( void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t dataSize = size*nmemb;
    char* header = new char[dataSize+1];
    memset(header, 0, dataSize);
    memcpy(header, ptr, dataSize);

    char* headerValue = strchr(header,':');
    if (headerValue) {
        char* headerName = header;
        headerValue[0]='\0';
        headerValue++;
        char* crlf = strchr(headerValue,'\r');
        if (crlf) {
            crlf[0]='\0';
        }
        //LOG(&k_roster, "Header:: {%s} --> {%s}", 
        //        headerName, headerValue);

        META_INFO* pMetaInfo = (META_INFO*)stream;
        if (stricmp(headerName,"Content-Length")==0) {
            sscanf(headerValue,"%d",&pMetaInfo->contentLength);

            // check status code at this point
            long statusCode;
            curl_easy_getinfo(pMetaInfo->hCurl, 
                    CURLINFO_RESPONSE_CODE, &statusCode);
            if (statusCode >= 200 && statusCode < 300) {
                _downloading = true;
            }
        }
        else if (stricmp(headerName,"Etag")==0) {
            char* p = headerValue;
            while (*p && *p == ' ') p++; // skip space(s)
            strncpy(pMetaInfo->etag, p, 1024);
        }
    }
    delete header;
    return dataSize;
}

size_t WriteChunkCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	CHUNK_READ* pChunkRead = (CHUNK_READ*)data;
	DWORD bytesWritten = 0;
	WriteFile(pChunkRead->hFile,ptr,realsize,&bytesWritten,NULL);
	// update total fetch-count
	pChunkRead->totalFetched += bytesWritten;
	return realsize;
}

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	int i,j;
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_roster,"Attaching dll...");
		
		hInst=hInstance;
		
		int v=GetPESInfo()->GameVersion;
		switch (v) {
			case gvPES5PC:
			case gvWE9PC:
			case gvWE9LEPC:
				goto GameVersIsOK;
				break;
		};
		return false;
		
		//Everything is OK!
		GameVersIsOK:

		RegisterKModule(&k_roster);

        // initialize addresses
        memcpy(data, dataArray[v], sizeof(data));
		
		HookFunction(hk_D3D_Create,(DWORD)initModule);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_roster,"Detaching dll...");
		Log(&k_roster,"Detaching done.");
	}
	
	return true;
}

void initModule()
{
    HookFunction(hk_GetNumPages,(DWORD)rosterReadNumPages);
    HookFunction(hk_AfterReadFile,(DWORD)rosterAfterReadFile);

	curl_global_init(CURL_GLOBAL_ALL);
    Log(&k_roster, "Roster module initialized.");
}

bool rosterReadNumPages(DWORD afsId, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages)
{
    DWORD fileSize = 0;

    if (afsId == 1 && fileId == 28 &&
            *(DWORD*)data[MAIN_MENU_MODE] == 7) {
        _networkMode = !_networkMode;
        if (_networkMode) {
            Log(&k_roster,"Loading online roster ...");
            //getRosterMetaInfo(&_metaInfo);
            if (_rosterOffset == 0xffffffff)
                _rosterOffset = GetOffsetByFileId(afsId, fileId);
            *numPages = 0x200; // 1mb should always be enough
            return true;
        }
    }

    return false;
}

void rosterAfterReadFile(HANDLE hFile, 
                       LPVOID lpBuffer, 
                       DWORD nNumberOfBytesToRead,
                       LPDWORD lpNumberOfBytesRead,  
                       LPOVERLAPPED lpOverlapped)
{
    DWORD afsId = GetAfsIdByOpenHandle(hFile);

    //DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT)
    //    - (*lpNumberOfBytesRead);
    //LogWithThreeNumbers(&k_roster, 
    //        "--> reading: afsId=%d, offset:%08x, bytes:%08x <--",
    //        afsId, offset, *lpNumberOfBytesRead);

    // ensure it is 0_TEXT.afs
    if (afsId == 1) {
        DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT)
            - (*lpNumberOfBytesRead);

        if (_rosterOffset == offset &&
                *(DWORD*)data[MAIN_MENU_MODE] == 7 && _networkMode) {
            LOG(&k_roster, 
                    "--> READING ONLINE ROSTER: offset:%08x, bytes:%08x <--",
                    offset, *lpNumberOfBytesRead);

            // fetch new roster, if available
            META_INFO metaInfo;
            memset(metaInfo.etag,0,sizeof(metaInfo.etag));
            getLocalEtag(&metaInfo);
            //_showMessages = true;
            HookFunction(hk_D3D_Present, (DWORD)rosterPresent);
            downloadRoster(&metaInfo);
            UnhookFunction(hk_D3D_Present, (DWORD)rosterPresent);
            //_showMessages = false;

            // feed it to the game
            string filename(GetPESInfo()->gdbDir);
            filename += rosterNames[GetPESInfo()->GameVersion][ROSTER_BIN];
            HANDLE rosterHandle = CreateFile(
                        filename.c_str(), 
                        GENERIC_READ,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
            if (rosterHandle != INVALID_HANDLE_VALUE)
            {
                DWORD bytesRead=0;
                ReadFile(rosterHandle, lpBuffer,
                        nNumberOfBytesToRead,
                        &bytesRead, 0);
                CloseHandle(rosterHandle);
            }
        }
    }
}

bool getLocalEtag(META_INFO* pMetaInfo)
{
    string filename(GetPESInfo()->gdbDir);
    filename += rosterNames[GetPESInfo()->GameVersion][ROSTER_BIN_ETAG];

    FILE* f = fopen(filename.c_str(),"rt");
    if (f) {
        fgets(pMetaInfo->etag,sizeof(pMetaInfo->etag),f);
        char* eoln = strchr(pMetaInfo->etag,'\r');
        if (eoln) eoln[0]='\0';
        fclose(f);
        return true;
    }
    return false;
}

bool setLocalEtag(META_INFO* pMetaInfo)
{
    string filename(GetPESInfo()->gdbDir);
    filename += rosterNames[GetPESInfo()->GameVersion][ROSTER_BIN_ETAG];

    FILE* f = fopen(filename.c_str(),"wt");
    if (f) {
        fprintf(f, "%s", pMetaInfo->etag);
        fclose(f);
        return true;
    }
    return false;
}

bool downloadRoster(META_INFO* pMetaInfo)
{
    string url(rosterNames[GetPESInfo()->GameVersion][ROSTER_URL]);

    string filename(GetPESInfo()->gdbDir);
    filename += rosterNames[GetPESInfo()->GameVersion][ROSTER_BIN_TEMP];

    _downloading = false;

    HANDLE hFile = CreateFile(
                filename.c_str(), 
                GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        LOG(&k_roster, "ERROR: unable to create file: %s", 
                filename.c_str());
        return false;
    }

	// init the curl session
	CURL* hCurl = curl_easy_init();
	curl_easy_setopt(hCurl, CURLOPT_WRITEFUNCTION, WriteChunkCallback);
    curl_easy_setopt(hCurl, CURLOPT_URL, url.c_str());

    CHUNK_READ chunkRead;
    chunkRead.hFile=hFile;
    chunkRead.totalFetched=0;
    pMetaInfo->hCurl = hCurl;

    curl_easy_setopt(hCurl, CURLOPT_USERAGENT, "kitserver-agent/1.0");
    curl_easy_setopt(hCurl, CURLOPT_WRITEDATA, &chunkRead);
    curl_easy_setopt(hCurl, CURLOPT_HEADERDATA, pMetaInfo);
    curl_easy_setopt(hCurl, CURLOPT_HEADERFUNCTION, HeaderFunction);

    struct curl_slist *slist=NULL;
    string IfNoneMatchHeader("If-None-Match: ");

    pMetaInfo->contentLength=-1;
    if (pMetaInfo->etag[0]!='\0') {
        // make a conditional GET
        IfNoneMatchHeader += pMetaInfo->etag;
        LOG(&k_roster, "%s", IfNoneMatchHeader.c_str());
        slist = curl_slist_append(slist, IfNoneMatchHeader.c_str());

        curl_easy_setopt(hCurl, CURLOPT_HTTPHEADER, slist);
    }

    CURLcode code = curl_easy_perform(hCurl);
    if (code != CURLE_OK) {
        LOG(&k_roster, "downloadRoster:: curl-code: %d", code);
    }
	else
    {
        // get status code
        long statusCode;
        curl_easy_getinfo(hCurl, CURLINFO_RESPONSE_CODE, &statusCode);
        LOG(&k_roster, "status code: %d", statusCode);
        if (statusCode >= 200 && statusCode < 300) {
            LOG(&k_roster, "fetched roster: %s", filename.c_str());
            LOG(&k_roster, "Content-Length: %d", chunkRead.totalFetched);
            setLocalEtag(pMetaInfo);
            
            // copy file
            CloseHandle(hFile);
            string rosterFilename(GetPESInfo()->gdbDir);
            rosterFilename += rosterNames[
                    GetPESInfo()->GameVersion][ROSTER_BIN];
            CopyFile(filename.c_str(), rosterFilename.c_str(), false);
        }
        else if (statusCode == 304) {
            LOG(&k_roster, "roster already up-to-date.");
        }
    }

    curl_slist_free_all(slist);

    CloseHandle(hFile);
    DeleteFile(filename.c_str());
    return code == CURLE_OK;
}

void rosterPresent(IDirect3DDevice8* self, 
        CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
	if (_downloading) {
		KDrawText(59,42,0xff000000,20,"Downloading new roster...");
		KDrawText(61,44,0xff000000,20,"Downloading new roster...");
		KDrawText(59,44,0xff000000,20,"Downloading new roster...");
		KDrawText(61,42,0xff000000,20,"Downloading new roster...");
		KDrawText(60,43,0xffffffc0,20,"Downloading new roster...");
	} else {
		KDrawText(59,42,0xff000000,20,"Checking for roster update...");
		KDrawText(61,44,0xff000000,20,"Checking for roster update...");
		KDrawText(59,44,0xff000000,20,"Checking for roster update...");
		KDrawText(61,42,0xff000000,20,"Checking for roster update...");
		KDrawText(60,43,0xffcccccc,20,"Checking for roster update...");
	};
}


