// network.cpp
#include <windows.h>
#include <stdio.h>
#include "curl.h"
#include "network.h"
#include "kload_exp.h"
#include "afsreplace.h"

KMOD k_network={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
DWORD _rosterOffset(0xffffffff);
bool _networkMode(false);
bool _downloading(false);


#define DATALEN 5
enum {
    MAIN_MENU_MODE,
    STUN_SERVER, STUN_SERVER_BUFLEN,
    GAME_SERVER, GAME_SERVER_BUFLEN,
};

static DWORD dataArray[][DATALEN] = {
    // PES5 DEMO 2
    {
        0,
        0, 0,
        0, 0,
    },
    // PES5
    {
        0xfde858,
        0xadadd8, 28,
        0xada608, 32,
    },
    // WE9
    {
        0xfde860,
        0xadadf4, 28,
        0xada620, 32,
    },
    // WE9:LE
    {
        0xf187f0,
        0xadb0a8, 28,
        0xada920, 32,
    },
};

static DWORD data[DATALEN];

enum {
    ROSTER_URL,
    ROSTER_BIN,
    ROSTER_BIN_TEMP,
    ROSTER_BIN_ETAG,
};

static char* rosterNames[] = {
    // PES5 DEMO 2
    "",
    // PES5
    "roster-pes5.bin",
    // WE9
    "roster-we9.bin",
    // WE9:LE
    "roster-we9le.bin",
};

class network_config_t
{
public:
    network_config_t() : 
        debug(false),
        updateEnabled(false)
    {}

    bool debug;
    bool updateEnabled;
    string updateBaseURI;
    string stunServer;
    string server;
};

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void initModule();
bool readConfig(network_config_t& config);
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

// global config
network_config_t _config;


static void string_strip(string& s)
{
    static const char* empties = " \t\n\r\"";
    int e = s.find_last_not_of(empties);
    s.erase(e + 1);
    int b = s.find_first_not_of(empties);
    s.erase(0,b);
}

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
        //LOG(&k_network, "Header:: {%s} --> {%s}", 
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
		Log(&k_network,"Attaching dll...");
		
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

		RegisterKModule(&k_network);

        // initialize addresses
        memcpy(data, dataArray[v], sizeof(data));
		
		HookFunction(hk_D3D_Create,(DWORD)initModule);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_network,"Detaching dll...");
		Log(&k_network,"Detaching done.");
	}
	
	return true;
}

/**
 * Returns true if successful.
 */
bool readConfig(network_config_t& config)
{
    string cfgFile(GetPESInfo()->mydir);
    cfgFile += "\\network.cfg";

	FILE* cfg = fopen(cfgFile.c_str(), "rt");
	if (cfg == NULL) return false;

	char str[BUFLEN];
	char name[BUFLEN];
	int value = 0;
	float dvalue = 0.0f;

	char *pName = NULL, *pValue = NULL, *comment = NULL;
	while (true)
	{
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;

		// skip comments
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';

		// parse the line
		pName = pValue = NULL;
		ZeroMemory(name, BUFLEN); value = 0;
		char* eq = strstr(str, "=");
		if (eq == NULL || eq[1] == '\0') continue;

		eq[0] = '\0';
		pName = str; pValue = eq + 1;

		ZeroMemory(name, NULL); 
		sscanf(pName, "%s", name);

		if (strcmp(name, "debug")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.debug = value;
		}
        else if (strcmp(name, "network.roster.update")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.updateEnabled = (value > 0);
		}
        else if (strcmp(name, "network.roster.update.baseurl")==0)
        {
            string value(pValue);
            string_strip(value);
            config.updateBaseURI = value;
        }
		else if (strcmp(name, "network.stun-server")==0)
		{
            string value(pValue);
            string_strip(value);
            config.stunServer = value.substr(0, data[STUN_SERVER_BUFLEN]-1);
		}
		else if (strcmp(name, "network.server")==0)
		{
            string value(pValue);
            string_strip(value);
            config.server = value.substr(0, data[GAME_SERVER_BUFLEN]-1);
		}
	}
	fclose(cfg);
	return true;
}

void initModule()
{
    HookFunction(hk_GetNumPages,(DWORD)rosterReadNumPages);
    HookFunction(hk_AfterReadFile,(DWORD)rosterAfterReadFile);

	curl_global_init(CURL_GLOBAL_ALL);
    Log(&k_network, "Network module initialized.");

    // read configuration
    readConfig(_config);

    LOG(&k_network, "_config.debug = %d", _config.debug);
    LOG(&k_network, "_config.updateEnabled = %d", _config.updateEnabled);
    LOG(&k_network, "_config.updateBaseURI = {%s}", 
            _config.updateBaseURI.c_str());
    LOG(&k_network, "_config.stunServer = {%s}", 
            _config.stunServer.c_str());
    LOG(&k_network, "_config.server = {%s}", 
            _config.server.c_str());

    // overwrite host-names
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;

    if (!_config.server.empty()) {
        char* gameServerString = (char*)data[GAME_SERVER];
	    if (VirtualProtect(gameServerString, data[GAME_SERVER_BUFLEN], 
                    newProtection, &protection)) {
            strncpy(gameServerString, 
                    _config.server.c_str(), data[GAME_SERVER_BUFLEN]);
        }
        else {
            LOG(&k_network, "ERROR: cannot set game server host");
        }

    }
    if (!_config.stunServer.empty()) {
        char* stunServerString = (char*)data[STUN_SERVER];
        if (VirtualProtect(stunServerString, data[STUN_SERVER_BUFLEN],
                    newProtection, &protection)) {
            strncpy(stunServerString, 
                    _config.stunServer.c_str(), data[STUN_SERVER_BUFLEN]);
        }
        else {
            LOG(&k_network, "ERROR: cannot set stun server host");
        }
    }
}

bool rosterReadNumPages(DWORD afsId, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages)
{
    DWORD fileSize = 0;

    if (afsId == 1 && fileId == 28 &&
            *(DWORD*)data[MAIN_MENU_MODE] == 7) {
        _networkMode = !_networkMode;
        if (_networkMode) {
            Log(&k_network,"Loading online roster ...");
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
    //LogWithThreeNumbers(&k_network, 
    //        "--> reading: afsId=%d, offset:%08x, bytes:%08x <--",
    //        afsId, offset, *lpNumberOfBytesRead);

    // ensure it is 0_TEXT.afs
    if (afsId == 1) {
        DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT)
            - (*lpNumberOfBytesRead);

        if (_rosterOffset == offset &&
                *(DWORD*)data[MAIN_MENU_MODE] == 7 && _networkMode) {
            LOG(&k_network, 
                    "--> READING ONLINE ROSTER: offset:%08x, bytes:%08x <--",
                    offset, *lpNumberOfBytesRead);

            // fetch new roster, if available
            META_INFO metaInfo;
            memset(metaInfo.etag,0,sizeof(metaInfo.etag));

            HookFunction(hk_D3D_Present, (DWORD)rosterPresent);
            getLocalEtag(&metaInfo);
            downloadRoster(&metaInfo);
            UnhookFunction(hk_D3D_Present, (DWORD)rosterPresent);

            // feed it to the game
            string filename(GetPESInfo()->gdbDir);
            filename += "\\GDB\\network\\";
            filename += rosterNames[GetPESInfo()->GameVersion];
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
    filename += "\\GDB\\network\\";
    filename += rosterNames[GetPESInfo()->GameVersion];
    filename += ".etag";

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
    filename += "\\GDB\\network\\";
    filename += rosterNames[GetPESInfo()->GameVersion];
    filename += ".etag";

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
    string url(_config.updateBaseURI);
    url += rosterNames[GetPESInfo()->GameVersion];

    string filename(GetPESInfo()->gdbDir);
    filename += "\\GDB\\network\\";
    filename += rosterNames[GetPESInfo()->GameVersion];
    filename += ".temp";

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
        LOG(&k_network, "ERROR: unable to create file: %s", 
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
        LOG(&k_network, "%s", IfNoneMatchHeader.c_str());
        slist = curl_slist_append(slist, IfNoneMatchHeader.c_str());

        curl_easy_setopt(hCurl, CURLOPT_HTTPHEADER, slist);
    }

    CURLcode code = curl_easy_perform(hCurl);
    if (code != CURLE_OK) {
        LOG(&k_network, "downloadRoster:: curl-code: %d", code);
    }
	else
    {
        // get status code
        long statusCode;
        curl_easy_getinfo(hCurl, CURLINFO_RESPONSE_CODE, &statusCode);
        LOG(&k_network, "status code: %d", statusCode);
        if (statusCode >= 200 && statusCode < 300) {
            LOG(&k_network, "fetched roster: %s", filename.c_str());
            LOG(&k_network, "Content-Length: %d", chunkRead.totalFetched);
            setLocalEtag(pMetaInfo);
            
            // copy file
            CloseHandle(hFile);
            string rosterFilename(GetPESInfo()->gdbDir);
            rosterFilename += "\\GDB\\network\\";
            rosterFilename += rosterNames[GetPESInfo()->GameVersion];
            CopyFile(filename.c_str(), rosterFilename.c_str(), false);
        }
        else if (statusCode == 304) {
            LOG(&k_network, "roster already up-to-date.");
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


