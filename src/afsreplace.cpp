// afsreplace.cpp

#include "windows.h"
#include "kload_exp.h"
#include "afsreplace.h"

//#include <hash_map>
#include <map>

extern KMOD k_kload;


#define DATALEN 1
enum {
    AFS_PAGELEN_TABLE
};

static DWORD dtaArray[][DATALEN] = {
    // PES5 DEMO 2
    {
        0,
    },
    // PES5
    {
        0x3bfff00,
    },
    // WE9
    {
        0x3bfff20,
    },
    // WE9:LE
    {
        0x3adef40,
    },
};

static DWORD dta[DATALEN];

CALLLINE l_GetNumPages={0,NULL};

DWORD newReadNumPages(DWORD base, DWORD fileId, DWORD orgNumPages);

struct NEXT_LIKELY_READ
{
    DWORD afsId;
    DWORD offset;
    DWORD fileId;
};

map<HANDLE,struct NEXT_LIKELY_READ> _next_likely_reads;


KEXPORT DWORD GetAfsIdByBase(DWORD base)
{
    //LogWithNumber(&k_kload, "GetAfsIdByBase:: AFS_PAGELEN_TABLE = %d",
    //        AFS_PAGELEN_TABLE);
    DWORD* pageLenTable = (DWORD*)dta[AFS_PAGELEN_TABLE];
    //LogWithNumber(&k_kload, "GetAfsIdByBase:: pageLenTable = %08x",
    //        (DWORD)pageLenTable);
    for (DWORD i=0; i<8; i++)
        if (pageLenTable[i]==base)
            return i;
    return 0xffffffff;
}

KEXPORT DWORD GetFileIdByOffset(DWORD afsId, DWORD offset)
{
    DWORD* pageLenTable = (DWORD*)dta[AFS_PAGELEN_TABLE];
    AFS_INFO* afsInfo = (AFS_INFO*)pageLenTable[afsId];
    DWORD prevFileId = 0, fileId = 0;
    DWORD offsetSoFar = 0x800 * afsInfo->pages[0];
    for (; fileId<afsInfo->numItems; fileId++) {
        if (offsetSoFar == offset) {
            return fileId;
        }
        DWORD numPages = afsInfo->pages[fileId+1];
        prevFileId = (numPages > 0) ? fileId : prevFileId;
        offsetSoFar += 0x800 * numPages;
        if (offsetSoFar > offset) {
            return prevFileId;
        }
    }
    return 0xffffffff;
}

KEXPORT DWORD GetProbableFileIdForHandle(DWORD afsId, 
        DWORD offset, HANDLE hFile)
{
    map<HANDLE,struct NEXT_LIKELY_READ>::iterator nit;
    nit = _next_likely_reads.find(hFile);
    if (nit != _next_likely_reads.end()) {
        if (nit->second.afsId == afsId && nit->second.offset == offset) {
            DWORD fileId = nit->second.fileId;
            DEEPDEBUGLOG(&k_kload,
                "Probable fileId: %d (afsId:%d, offset:0x%08x, hFile:0x%08x)",
                fileId, afsId, offset, hFile);
            return fileId;
        }
    }
    return GetFileIdByOffset(afsId, offset);
}

KEXPORT void SetNextProbableReadForHandle(DWORD afsId, 
        DWORD offset, DWORD fileId, HANDLE hFile)
{
    struct NEXT_LIKELY_READ nlr;
    nlr.afsId = afsId;
    nlr.offset = offset;
    nlr.fileId = fileId;
    _next_likely_reads[hFile] = nlr;
}

KEXPORT void ClearProbableReadForHandle(HANDLE hFile, DWORD wasOffset)
{
    map<HANDLE,struct NEXT_LIKELY_READ>::iterator nit;
    nit = _next_likely_reads.find(hFile);
    if (nit != _next_likely_reads.end()) {
        if (nit->second.offset == wasOffset) {
            _next_likely_reads.erase(nit);
        }
    }
}

KEXPORT DWORD GetOffsetByFileId(DWORD afsId, DWORD fileId)
{
    DWORD* pageLenTable = (DWORD*)dta[AFS_PAGELEN_TABLE];
    AFS_INFO* afsInfo = (AFS_INFO*)pageLenTable[afsId];
    DWORD offset = 0;
    for (DWORD i=0; i<=fileId; i++) {
        offset += 0x800 * afsInfo->pages[i];
    }
    return offset;
}

void newReadNumPagesCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov eax, dword ptr ds:[edi+ebx*4+0x11c]
        push eax // org numpages
        push ebx // fileId
        push edi // base 
        call newReadNumPages
        add esp, 0x0c  // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop ebp
        popfd
        retn
    }
}

DWORD newReadNumPages(DWORD base, DWORD fileId, DWORD orgNumPages)
{
    //Log(&k_kload, "newReadNumPages called");
    //for (int j=0; j<DATALEN; j++) {
    //    LogWithTwoNumbers(&k_kload, "dta[%d]=%08x", j, dta[j]);
    //}
    DWORD afsId = GetAfsIdByBase(base);
    //LogWithNumber(&k_kload, "afsId=%d", afsId);

    GETNUMPAGES_PROC NextCall=NULL;
    for (int i=0;i<(l_GetNumPages.num);i++)
        if (l_GetNumPages.addr[i]!=0) {
            NextCall=(GETNUMPAGES_PROC)l_GetNumPages.addr[i];
            DWORD numPages = orgNumPages;
            bool handled = NextCall(afsId, fileId, orgNumPages, &numPages);
            if (handled) {
                return numPages;
            }
        }

    return orgNumPages;
}

void afsreplace_init(int v)
{
    //LogWithNumber(&k_kload, "afsreplace_init:: v=%d", v);
    memcpy(dta, dtaArray[v], sizeof(dta));
}
