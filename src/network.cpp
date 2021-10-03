// network.cpp
#include <windows.h>
#include <winreg.h>
#include <stdio.h>
#include <list>
#include "archive.h"
#include "archive_entry.h"
#include "curl.h"
#include "network.h"
#include "kload_exp.h"
#include "afsreplace.h"
#include "md5.h"

KMOD k_network={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
bool _networkMode(false);
bool _downloading(false);
bool _checking(false);
bool _success(false);
bool _gdb_preloaded(false);
char _checkingText[128];
char _downloadingText[128];
char _preloadingText[128];
HANDLE _preloadEvent = NULL;
string _error;
LARGE_INTEGER _nonOnlineFrequency = {0,0};
char _versionString[17] = "1.66-FakeVersion";
md5_state_t state;
md5_byte_t digest[16];
int _login_credentials = 0;
char _cred_key[31] = "djkj93rajf8123bvdfg9475hpok43k";

#define DATALEN 17
enum {
    MAIN_MENU_MODE, CLOCK_FREQUENCY,
    STUN_SERVER, STUN_SERVER_BUFLEN,
    GAME_SERVER, GAME_SERVER_BUFLEN,
    DB_FILE1, DB_FILE2, DB_FILE3,
    ROSTER_FILEID, 
    DB_FILE5, DB_FILE6, DB_FILE7,
    CODE_READ_VERSION_STRING, CREDENTIALS,
    CODE_READ_CRED_FLAG, CODE_WRITE_CRED_FLAG,
};

static DWORD dtaArray[][DATALEN] = {
    // PES5 DEMO 2
    {
        0, 0,
        0, 0,
        0, 0,
        0,0,0,0,0,0,0,
        0, 0,
        0, 0,
    },
    // PES5
    {
        0xfde858, 0xe953f0,
        0xadadd8, 28,
        0xada608, 32,
        17,18,19,22,23,24,25,
        0x83d67f, 0x3b7a1b8,
        0x48eb18, 0x48eb78,
    },
    // WE9
    {
        0xfde860, 0xe953f0,
        0xadadf4, 28,
        0xada620, 32,
        17,18,19,22,23,24,25,
        0x83dbaf, 0x3b7a1d8,
        0x48edd8, 0x48ee38,
    },
    // WE9:LE
    {
        0xf187f0, 0xdcf3f0,
        0xadb0a8, 28,
        0xada920, 32,
        23,24,25,28,29,30,31,
        0x864e9f, 0x3ad64f8,
        0x48e9d8, 0x48ea38,
    },
};

static DWORD dta[DATALEN];

enum {
    ROSTER_URL,
    ROSTER_BIN,
    ROSTER_BIN_TEMP,
    ROSTER_BIN_ETAG,
};

static char* urlDirNames[] = {
    // PES5 DEMO 2
    "pes5demo2/",
    // PES5
    "pes5/",
    // WE9
    "we9/",
    // WE9:LE
    "we9le/",
};

static char* dirNames[] = {
    // PES5 DEMO 2
    "pes5demo2\\",
    // PES5
    "pes5\\",
    // WE9
    "we9\\",
    // WE9:LE
    "we9le\\",
};

class network_config_t
{
public:
    network_config_t() : 
        debug(false),
        updateEnabled(false),
        useNetworkOption(false),
        doRosterHash(false),
        rememberLogin(true)
    {}

    bool debug;
    bool updateEnabled;
    bool useNetworkOption;
    bool doRosterHash;
    bool rememberLogin;
    string updateBaseURI;
    string stunServer;
    string server;
};

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void initModule();
bool readConfig(network_config_t& config);
size_t HeaderFunction( void *ptr, size_t size, size_t nmemb, void *stream);
size_t WriteChunkCallback(void *ptr, size_t size, size_t nmemb, void *dta);
bool rosterReadNumPages(DWORD afsId, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages);
bool rosterAfterReadFile(HANDLE hFile, 
                       LPVOID lpBuffer, 
                       DWORD nNumberOfBytesToRead,
                       LPDWORD lpNumberOfBytesRead,  
                       LPOVERLAPPED lpOverlapped);
void rosterPresent(IDirect3DDevice8* self, 
        CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void rosterPresentPreloadGDB(IDirect3DDevice8* self, 
        CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
HANDLE rosterCreateOption(
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile);
void rosterVersionReadCallPoint();
void rosterVersionRead(char* version);
void loginReadCallPoint();
void loginRead();
void loginWriteCallPoint();
void loginWrite();

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


bool initGDB();
bool getRosterMetaInfo(META_INFO* pMetaInfo, const char* shortName);
bool downloadArchive(META_INFO* pMetaInfo, const char* shortName);
bool eraseAllDbFiles(const string& shortName);
bool extractArchive(const string& shortName);
bool getLocalEtag(META_INFO* pMetaInfo, const char* shortName);
bool setLocalEtag(META_INFO* pMetaInfo, const char* shortName);

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
                _checking = false;
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

size_t WriteChunkCallback(void *ptr, size_t size, size_t nmemb, void *dta)
{
	size_t realsize = size * nmemb;
	CHUNK_READ* pChunkRead = (CHUNK_READ*)dta;
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
        memcpy(dta, dtaArray[v], sizeof(dta));
		
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
        if (feof(cfg)) 
            break;
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);

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
        else if (strcmp(name, "network.option.enabled")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.useNetworkOption = (value > 0);
		}
        else if (strcmp(name, "network.roster.hash")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.doRosterHash = (value > 0);
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
            config.stunServer = value.substr(0, dta[STUN_SERVER_BUFLEN]-1);
		}
		else if (strcmp(name, "network.server")==0)
		{
            string value(pValue);
            string_strip(value);
            config.server = value.substr(0, dta[GAME_SERVER_BUFLEN]-1);
		}
        else if (strcmp(name, "network.remember.login")==0)
		{
			if (sscanf(pValue, "%d", &value)!=1) continue;
			config.rememberLogin = (value > 0);
		} 
	}
	fclose(cfg);
	return true;
}

void initModule()
{
    HookFunction(hk_GetNumPages,(DWORD)rosterReadNumPages);
    HookFunction(hk_AfterReadFile,(DWORD)rosterAfterReadFile);
    HookFunction(hk_CreateOption,(DWORD)rosterCreateOption);
    HookCallPoint(dta[CODE_READ_VERSION_STRING], 
            rosterVersionReadCallPoint, 6, 3, false);
    HookCallPoint(dta[CODE_READ_CRED_FLAG], 
            loginReadCallPoint, 6, 2, false);
    HookCallPoint(dta[CODE_WRITE_CRED_FLAG], 
            loginWriteCallPoint, 6, 2, false);

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
    LOG(&k_network, "_config.useNetworkOption = %d", 
            _config.useNetworkOption);
    LOG(&k_network, "_config.rememberLogin = %d", 
            _config.rememberLogin);

    // overwrite host-names
    DWORD protection = 0;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;

    if (!_config.server.empty()) {
        char* gameServerString = (char*)dta[GAME_SERVER];
	    if (VirtualProtect(gameServerString, dta[GAME_SERVER_BUFLEN], 
                    newProtection, &protection)) {
            strncpy(gameServerString, 
                    _config.server.c_str(), dta[GAME_SERVER_BUFLEN]);
        }
        else {
            LOG(&k_network, "ERROR: cannot set game server host");
        }

    }
    if (!_config.stunServer.empty()) {
        char* stunServerString = (char*)dta[STUN_SERVER];
        if (VirtualProtect(stunServerString, dta[STUN_SERVER_BUFLEN],
                    newProtection, &protection)) {
            strncpy(stunServerString, 
                    _config.stunServer.c_str(), dta[STUN_SERVER_BUFLEN]);
        }
        else {
            LOG(&k_network, "ERROR: cannot set stun server host");
        }
    }

    _login_credentials = dta[CREDENTIALS];
}

void setPerformanceFrequency(float factor)
{
    LARGE_INTEGER* freq = (LARGE_INTEGER*)dta[CLOCK_FREQUENCY];
    _nonOnlineFrequency.LowPart = freq->LowPart;
    _nonOnlineFrequency.HighPart = freq->HighPart;

    LARGE_INTEGER orgFreq;
    if (!GetOriginalFrequency(&orgFreq)) {
        LOG(&k_network, "Problem getting original frequency.");
        return;
    }

    freq->LowPart = orgFreq.LowPart / factor;
    freq->HighPart = orgFreq.HighPart / factor;
    LOG(&k_network, "new: hi=%08x, lo=%08x", 
            freq->HighPart, freq->LowPart);
}

void restorePerformanceFrequency()
{
    if (_nonOnlineFrequency.LowPart || _nonOnlineFrequency.HighPart) {
        LARGE_INTEGER* freq = (LARGE_INTEGER*)dta[CLOCK_FREQUENCY];
        freq->LowPart = _nonOnlineFrequency.LowPart;
        freq->HighPart = _nonOnlineFrequency.HighPart;
        LOG(&k_network, "restored: hi=%08x, lo=%08x", 
                freq->HighPart, freq->LowPart);
    }
}

bool rosterReadNumPages(DWORD afsId, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages)
{
    DWORD fileSize = 0;

    if (afsId == 1 
            && ((fileId >= dta[DB_FILE1] && fileId <= dta[DB_FILE3])
                || (fileId >= dta[ROSTER_FILEID] && fileId <= dta[DB_FILE7]))
            && *(DWORD*)dta[MAIN_MENU_MODE] == 7) {

        if (fileId == dta[DB_FILE3]) {
            _networkMode = !_networkMode;
            if (_networkMode) {
                LOG(&k_network,"Loading online db ...");
            } else {
                // restore game speed
                restorePerformanceFrequency();
            }
        }

        if (_networkMode) {
            *numPages = 0x200; // 1mb should always be enough
            return true;
        }
    }

    return false;
}

HANDLE rosterCreateOption(
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile)
{
    if (!_config.useNetworkOption)
        return INVALID_HANDLE_VALUE;

    LOG(&k_network, "Attempting to use network option file...");

    string filename(GetPESInfo()->gdbDir);
    filename += "\\GDB\\network\\";
    filename += dirNames[GetPESInfo()->GameVersion];
    filename += "option.bin";

    // check for existence of the file first.
    // If the option is not there, we don't want to modify
    // the path, but instead let the game use the normal one
    HANDLE handle = CreateFile(
            filename.c_str(), 
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);

        HANDLE optionHandle = CreateFile(
                filename.c_str(), 
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile);
        LOG(&k_network, "Using option file: {%s}", filename.c_str());
        return optionHandle;
    }

    LOG(&k_network, "Network option file does not exist. Using default.");
    return INVALID_HANDLE_VALUE;
}

char* read_db_cfg(size_t& size)
{
    string filename(GetPESInfo()->gdbDir);
    filename += "\\GDB\\network\\";
    filename += dirNames[GetPESInfo()->GameVersion];
    filename += "db.cfg";

    HANDLE handle = CreateFile(
            filename.c_str(), 
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    char* buffer = NULL;
    size = 0;
    if (handle != INVALID_HANDLE_VALUE)
    {
        size = GetFileSize(handle, NULL);
        buffer = (char*)malloc(size);
        if (buffer) {
            DWORD bytesRead=0;
            ReadFile(handle, buffer, size, &bytesRead, 0);
            CloseHandle(handle);
        }
    }
    return buffer;
}

bool rosterAfterReadFile(HANDLE hFile, 
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

    /*
    if (IsOptionHandle(hFile) && _config.useNetworkOption) {
        DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT)
            - (*lpNumberOfBytesRead);
        LOG(&k_network, 
                "==> OPTION FILE read: offset:%08x, bytes:%08x <--",
                offset, *lpNumberOfBytesRead);

        // feed it to the game
        string filename(GetPESInfo()->gdbDir);
        filename += "\\GDB\\network\\";
        filename += dirNames[GetPESInfo()->GameVersion];
        filename += "option.bin";
        HANDLE optionHandle = CreateFile(
                    filename.c_str(), 
                    GENERIC_READ,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);
        if (optionHandle != INVALID_HANDLE_VALUE)
        {
            DWORD bytesRead=0;
            SetFilePointer(optionHandle, offset, NULL, FILE_BEGIN);
            ReadFile(optionHandle, lpBuffer,
                    *lpNumberOfBytesRead,
                    &bytesRead, 0);
            CloseHandle(optionHandle);
        }

        return;
    }
    */

    // ensure it is 0_TEXT.afs
    if (afsId == 1) {
        DWORD offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT)
            - (*lpNumberOfBytesRead);
        DWORD fileId = GetProbableFileIdForHandle(afsId, offset, hFile);

        if ((fileId >= dta[DB_FILE1] && fileId <= dta[DB_FILE3] 
                    || (fileId >= dta[ROSTER_FILEID] 
                        && fileId <= dta[DB_FILE7]))
                && *(DWORD*)dta[MAIN_MENU_MODE] == 7 && _networkMode) {
            LOG(&k_network, 
                    "--> READING ONLINE DB (%d): offset:%08x, bytes:%08x <--",
                    fileId, offset, *lpNumberOfBytesRead);

            char shortName[30];
            memset(shortName,0,sizeof(shortName));
            sprintf(shortName,"db_%d.bin",fileId);

            // if enabled, check for db-update
            if (_config.updateEnabled && !_config.updateBaseURI.empty()) {
                if (fileId == dta[DB_FILE3]) {
                    META_INFO metaInfo;
                    memset(metaInfo.etag,0,sizeof(metaInfo.etag));

                    HookFunction(hk_D3D_Present, (DWORD)rosterPresent);
                    getLocalEtag(&metaInfo, "db.zip");
                    downloadArchive(&metaInfo, "db.zip");
                    UnhookFunction(hk_D3D_Present, (DWORD)rosterPresent);

                    // set game speed
                    string fname(GetPESInfo()->gdbDir);
                    fname += "\\GDB\\network\\";
                    fname += dirNames[GetPESInfo()->GameVersion];
                    fname += "db.cfg";
                    FILE* f = fopen(fname.c_str(),"rt");
                    if (f) {
                        float factor=1.0;
                        if (fscanf(f,"speed.factor = %f",&factor)==1) {
                            // set new game speed, if DB specifies it
                            LOG(&k_network,
                                    "setting game speed (factor: %0.3f)", 
                                    factor);
                            setPerformanceFrequency(factor);
                        }
                        fclose(f);
                    }
                }
            }

            // feed it to the game
            string filename(GetPESInfo()->gdbDir);
            filename += "\\GDB\\network\\";
            filename += dirNames[GetPESInfo()->GameVersion];
            filename += shortName;
            HANDLE handle = CreateFile(
                        filename.c_str(), 
                        GENERIC_READ,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
            bool result = false;
            if (handle!=INVALID_HANDLE_VALUE) {
                DWORD filesize = GetFileSize(handle,NULL);
                DWORD curr_offset = offset - GetOffsetByFileId(afsId, fileId);
                LOG(&k_network, 
                        "offset=%08x, GetOffsetByFileId()=%08x, curr_offset=%08x",
                        offset, GetOffsetByFileId(afsId, fileId), curr_offset);
                DWORD bytesToRead = *lpNumberOfBytesRead;
                if (filesize < curr_offset + *lpNumberOfBytesRead) {
                    bytesToRead = filesize - curr_offset;
                }

                DWORD bytesRead = 0;
                SetFilePointer(handle, curr_offset, NULL, FILE_BEGIN);
                LOG(&k_network, "reading 0x%x bytes (0x%x) from {%s}", 
                        bytesToRead, *lpNumberOfBytesRead, filename.c_str());
                ReadFile(handle, lpBuffer, bytesToRead, &bytesRead, lpOverlapped);
                LOG(&k_network, "read 0x%x bytes from {%s}", bytesRead, filename.c_str());
                if (*lpNumberOfBytesRead - bytesRead > 0) {
                    DEBUGLOG(&k_network, "zeroing out 0x%0x bytes",
                            *lpNumberOfBytesRead - bytesRead);
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

                // calculate roster hash
                if (_config.doRosterHash) {
                    if (fileId == dta[ROSTER_FILEID]) {
                        md5_init(&state);
                        md5_append(&state, 
                                (const md5_byte_t *)lpBuffer, 
                                bytesRead);
                        // also get db.cfg
                        size_t size;
                        char* cfgBuffer = read_db_cfg(size);
                        if (cfgBuffer && size>0) {
                            md5_append(&state,
                                    (const md5_byte_t *)cfgBuffer,
                                    size);
                            free(cfgBuffer);
                        }
                        md5_finish(&state, 
                                (md5_byte_t*)_versionString); //digest);
                        LOG(&k_network, "Roster-HASH CALCULATED.");
                    }
                }
            }

            // preload GDB to avoid disconnects when starting
            // the first online game
            if (!_gdb_preloaded) {
                _gdb_preloaded = true;
                HookFunction(hk_D3D_Present, (DWORD)rosterPresentPreloadGDB);
                initGDB();
                UnhookFunction(hk_D3D_Present, (DWORD)rosterPresentPreloadGDB);
            }

            return result;
        }
    }
    return false;
}

bool getLocalEtag(META_INFO* pMetaInfo, const char* shortName)
{
    string filename(GetPESInfo()->gdbDir);
    filename += "\\GDB\\network\\";
    filename += dirNames[GetPESInfo()->GameVersion];
    filename += shortName;
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

bool setLocalEtag(META_INFO* pMetaInfo, const char* shortName)
{
    string filename(GetPESInfo()->gdbDir);
    filename += "\\GDB\\network\\";
    filename += dirNames[GetPESInfo()->GameVersion];
    filename += shortName;
    filename += ".etag";

    FILE* f = fopen(filename.c_str(),"wt");
    if (f) {
        fprintf(f, "%s", pMetaInfo->etag);
        fclose(f);
        return true;
    }
    return false;
}

bool eraseAllDbFiles(const string& filename)
{
	WIN32_FIND_DATA fData;
    string dir(GetPESInfo()->gdbDir);
    dir += "\\GDB\\network\\";
    dir += dirNames[GetPESInfo()->GameVersion];
    string pattern(dir);
    pattern += "\\*.*";

    string filenameEtag(filename);
    filenameEtag += ".etag";

    list<string> toDelete;
	HANDLE hff = FindFirstFile(pattern.c_str(), &fData);
	if (hff != INVALID_HANDLE_VALUE) {
        while(true)
        {
            // check if this is a directory
            if (!(fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                bool skip = _strnicmp(fData.cFileName, 
                        filename.c_str(), filename.size())==0;
                skip = skip || _strnicmp(fData.cFileName, 
                        filenameEtag.c_str(), filenameEtag.size())==0;
                if (!skip) {
                    string fullname(dir);
                    fullname += fData.cFileName;
                    toDelete.push_back(fullname);
                }
            }
            // proceed to next file
            if (!FindNextFile(hff, &fData)) 
                break;
        }
        FindClose(hff);
    }

    for (list<string>::iterator it = toDelete.begin();
            it != toDelete.end(); it++) {
        if (!DeleteFile(it->c_str())) {
            LOG(&k_network, 
                    "ERROR: unable to delete old file: %s", 
                    it->c_str());
        }
    }
    return true;
}

bool extractArchive(const string& filename)
{
    struct archive* x = archive_read_new();
    if (!x) {
        LOG(&k_network,"ERROR: archive_read_new FAILED.");
        return false;
    }

    archive_read_support_compression_all(x);
    archive_read_support_format_all(x);

    if (archive_read_open_filename(x, filename.c_str(), filename.size())
            == ARCHIVE_OK) {
        struct archive_entry* e;
        while (archive_read_next_header(x, &e) != ARCHIVE_EOF) {
            int code;
            if ((code = archive_read_extract(x, e, 0)) == ARCHIVE_OK) {
                LOG(&k_network, "Extracted: %s", archive_entry_pathname(e));
            }
            else {
                LOG(&k_network, "Extraction of entry FAILED: %d", code);
            }
        }
        archive_read_close(x);
        archive_read_finish(x);
    }
    else {
        LOG(&k_network,"ERROR: archive_read_open_filename FAILED.");
        archive_read_finish(x);
        return false;
    }

    return true;
}

typedef void (*FPROC)();

bool initGDBinThread()
{
    sprintf(_preloadingText, "Preloading GDB content...");

    HINSTANCE fservModule = GetModuleHandle("fserv");
    if (!fservModule)
        fservModule = GetModuleHandle("fserv-new");

    if (fservModule) {
        LOG(&k_network, "Preloading GDB...");
        FPROC proc = (FPROC)GetProcAddress(
                fservModule, "fservInitFacesAndHair");
        if (proc) {
            proc();
            LOG(&k_network, "GDB preloaded OK.");
        }
        else {
            LOG(&k_network, "WARN: unable to preload GDB: function not found");
        }
    }
    Sleep(2000);
    SetEvent(_preloadEvent);
    return true;
}

bool initGDB()
{
    _preloadEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
    DWORD threadId;
    HANDLE initThread = CreateThread( 
        NULL,                   // default security attributes
        0,                      // use default stack size  
        (LPTHREAD_START_ROUTINE)initGDBinThread, // thread function name
        NULL,                   // argument to thread function 
        0,                      // use default creation flags 
        &threadId);             // returns the thread identifier 
    WaitForSingleObject(_preloadEvent, INFINITE);
    return true;
}

bool downloadArchive(META_INFO* pMetaInfo, const char* shortName)
{
    string url(_config.updateBaseURI);
    url += urlDirNames[GetPESInfo()->GameVersion];
    url += shortName;

    string dbDir(GetPESInfo()->gdbDir);
    dbDir += "\\GDB\\network\\";
    dbDir += dirNames[GetPESInfo()->GameVersion];
    // TODO: auto-create dirs
    
    string filename(dbDir);
    filename += shortName;
    filename += ".tmp";

    _checking = true;
    _downloading = false;
    _success = false;
    _error = "";

    sprintf(_checkingText,"Checking for updates: %s", shortName);
    sprintf(_downloadingText,"Downloading: %s", shortName);

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
        _checking = false;
        _error = "ERROR: unable to create file: ";
        _error += shortName;
        _error += ".tmp (check log)";

        LOG(&k_network, "ERROR: unable to create file: %s", 
                filename.c_str());
        LOG(&k_network, "Make sure this directory exists: %s", 
                dbDir.c_str());

        // let the error message show up on the screen
        // for a couple of seconds
        Sleep(2000);
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
        LOG(&k_network, "downloadArchive:: curl-code: %d", code);
        char buf[128];
        _snprintf(buf,128,"Communication error: %d", code);
        _error = buf;
    }
	else
    {
        // get status code
        long statusCode;
        curl_easy_getinfo(hCurl, CURLINFO_RESPONSE_CODE, &statusCode);
        LOG(&k_network, "status code: %d", statusCode);
        if (statusCode >= 200 && statusCode < 300) {
            LOG(&k_network, "fetched db file: %s", filename.c_str());
            LOG(&k_network, "Content-Length: %d", chunkRead.totalFetched);
            setLocalEtag(pMetaInfo, shortName);
            
            // copy file
            CloseHandle(hFile);
            string dbDir(GetPESInfo()->gdbDir);
            dbDir += "\\GDB\\network\\";
            dbDir += dirNames[GetPESInfo()->GameVersion];
            string dbFile(dbDir);
            dbFile += shortName;
            CopyFile(filename.c_str(), dbFile.c_str(), false);

            // delete all existing files, except for
            // the newly downloaded one
            eraseAllDbFiles(shortName);

            // unpack db archive
            char currDir[1024];
            memset(currDir,0,sizeof(currDir));
            GetCurrentDirectory(sizeof(currDir), currDir);
            SetCurrentDirectory(dbDir.c_str());
            extractArchive(dbFile);
            SetCurrentDirectory(currDir);

            DeleteFile(dbFile.c_str());
            _success = true;
        }
        else if (statusCode == 304) {
            LOG(&k_network, "DB already up-to-date.");
            _success = true;
        }
        else if (statusCode == 404) {
            char buf[128];
            _error = "DB update unavailable.";
        }
        else {
            char buf[128];
            _snprintf(buf,128,"Unexpected HTTP status: %d", statusCode);
            _error = buf;
        }
    }
    _downloading = false;
    _checking = false;

    curl_slist_free_all(slist);

    CloseHandle(hFile);
    DeleteFile(filename.c_str());

    // let the status message show up on the screen
    // for a couple of seconds
    Sleep(2000);
    return code == CURLE_OK;
}

void rosterPresent(IDirect3DDevice8* self, 
        CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
	if (_downloading) {
		//KDrawText(59,42,0xff000000,20,"Downloading new DB...");
		//KDrawText(61,44,0xff000000,20,"Downloading new DB...");
		//KDrawText(59,44,0xff000000,20,"Downloading new DB...");
		//KDrawText(61,42,0xff000000,20,"Downloading new DB...");
		//KDrawText(60,43,0xffffffc0,20,"Downloading new DB...");
		KDrawText(59,42,0xff000000,20,_downloadingText);
		KDrawText(61,44,0xff000000,20,_downloadingText);
		KDrawText(59,44,0xff000000,20,_downloadingText);
		KDrawText(61,42,0xff000000,20,_downloadingText);
		KDrawText(60,43,0xffffffc0,20,_downloadingText);
	} 
    else if (_checking) {
		//KDrawText(59,42,0xff000000,20,"Checking for DB update...");
		//KDrawText(61,44,0xff000000,20,"Checking for DB update...");
		//KDrawText(59,44,0xff000000,20,"Checking for DB update...");
		//KDrawText(61,42,0xff000000,20,"Checking for DB update...");
		//KDrawText(60,43,0xffcccccc,20,"Checking for DB update...");
		KDrawText(59,42,0xff000000,20,_checkingText);
		KDrawText(61,44,0xff000000,20,_checkingText);
		KDrawText(59,44,0xff000000,20,_checkingText);
		KDrawText(61,42,0xff000000,20,_checkingText);
		KDrawText(60,43,0xffcccccc,20,_checkingText);
	}
    else if (_success) {
		KDrawText(59,42,0xff000000,20,"DB is up-to-date");
		KDrawText(61,44,0xff000000,20,"DB is up-to-date");
		KDrawText(59,44,0xff000000,20,"DB is up-to-date");
		KDrawText(61,42,0xff000000,20,"DB is up-to-date");
		KDrawText(60,43,0xffccffcc,20,"DB is up-to-date");
    }
    else {
		KDrawText(59,42,0xff000000,20,(char*)_error.c_str());
		KDrawText(61,44,0xff000000,20,(char*)_error.c_str());
		KDrawText(59,44,0xff000000,20,(char*)_error.c_str());
		KDrawText(61,42,0xff000000,20,(char*)_error.c_str());
		KDrawText(60,43,0xffffcccc,20,(char*)_error.c_str());
    }
}

void rosterPresentPreloadGDB(IDirect3DDevice8* self, 
        CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused)
{
    KDrawText(59,42,0xff000000,20,_preloadingText);
    KDrawText(61,44,0xff000000,20,_preloadingText);
    KDrawText(59,44,0xff000000,20,_preloadingText);
    KDrawText(61,42,0xff000000,20,_preloadingText);
    KDrawText(60,43,0xffc0c0ff,20,_preloadingText);
}

void rosterVersionReadCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov ecx, esp
        add ecx, 8
        add ecx, 0x24  // adjust stack
        push ecx
        call rosterVersionRead
        add esp, 4  // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void rosterVersionRead(char* version)
{
    if (_config.doRosterHash) {
        memcpy(version, _versionString, 16);
    }
}

void loginReadCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        call loginRead
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        push eax
        mov eax, _login_credentials
        cmp byte ptr ds:[eax],1  // original code
        pop eax
        retn
    }
}

void loginWriteCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        call loginWrite
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        push eax
        mov eax, _login_credentials
        mov byte ptr ds:[eax],1  // original code
        pop eax
        retn
    }
}

void loginRead()
{
    LOGIN_CREDENTIALS* lc = (LOGIN_CREDENTIALS*)dta[CREDENTIALS];
    if (_config.rememberLogin && lc) {
        // read registry key
        HKEY handle;
        if (RegCreateKeyEx(HKEY_CURRENT_USER, 
                "Software\\Kitserver", 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_READ|KEY_WRITE, 
                NULL, &handle, NULL) != ERROR_SUCCESS) {
            LOG(&k_network,"ERROR: Unable to open/create registry key.");
            return;
        }
        DWORD lcSize = sizeof(LOGIN_CREDENTIALS);
        if (RegQueryValueEx(handle,_config.server.c_str(),
                NULL, NULL, (unsigned char *)lc, 
                &lcSize) == ERROR_SUCCESS) {
            // decode password
            for (int i=0; i<sizeof((*lc).password); i++) {
                lc->password[i] = lc->password[i] ^ _cred_key[i];
            }
        }
        RegCloseKey(handle);
    }
}

void loginWrite()
{
    LOGIN_CREDENTIALS* lc = (LOGIN_CREDENTIALS*)dta[CREDENTIALS];
    if (_config.rememberLogin && lc) {
        lc->initialized = 1;

        // open/create registry key
        HKEY handle;
        if (RegCreateKeyEx(HKEY_CURRENT_USER, 
                "Software\\Kitserver", 0, NULL,
                REG_OPTION_NON_VOLATILE, KEY_READ|KEY_WRITE, 
                NULL, &handle, NULL) != ERROR_SUCCESS) {
            LOG(&k_network,"ERROR: Unable to open/create registry key.");
            LOG(&k_network,"ERROR: Network login cannot be stored.");
            return;
        }
        // encode password for storing in registry
        int i;
        for (i=0; i<sizeof((*lc).password); i++) {
            lc->password[i] = lc->password[i] ^ _cred_key[i];
        }
        DWORD lcSize = sizeof(LOGIN_CREDENTIALS);
        if (RegSetValueEx(handle,_config.server.c_str(),
                NULL, REG_BINARY,
                (const unsigned char*)lc, lcSize) != ERROR_SUCCESS) {
            LOG(&k_network,"ERROR: Unable to write 'login' value");
        }
        // decode password
        for (i=0; i<sizeof((*lc).password); i++) {
            lc->password[i] = lc->password[i] ^ _cred_key[i];
        }
        RegCloseKey(handle);
    }
}

