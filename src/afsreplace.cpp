// afsreplace.cpp

#include "windows.h"
#include "kload_exp.h"
#include "afsreplace.h"

extern KMOD k_kload;


#define DATALEN 1
enum {
    AFS_PAGELEN_TABLE
};

static DWORD dataArray[][DATALEN] = {
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

static DWORD data[DATALEN];

CALLLINE l_GetNumPages={0,NULL};

DWORD newReadNumPages(DWORD base, DWORD fileId, DWORD orgNumPages);


KEXPORT DWORD GetAfsIdByBase(DWORD base)
{
    //LogWithNumber(&k_kload, "GetAfsIdByBase:: AFS_PAGELEN_TABLE = %d",
    //        AFS_PAGELEN_TABLE);
    DWORD* pageLenTable = (DWORD*)data[AFS_PAGELEN_TABLE];
    //LogWithNumber(&k_kload, "GetAfsIdByBase:: pageLenTable = %08x",
    //        (DWORD)pageLenTable);
    for (DWORD i=0; i<8; i++)
        if (pageLenTable[i]==base)
            return i;
    return 0xffffffff;
}

KEXPORT DWORD GetFileIdByOffset(DWORD afsId, DWORD offset)
{
    DWORD* pageLenTable = (DWORD*)data[AFS_PAGELEN_TABLE];
    AFS_INFO* afsInfo = (AFS_INFO*)pageLenTable[afsId];
    DWORD fileId = 0;
    DWORD offsetSoFar = 0;
    for (; fileId<afsInfo->numItems; fileId++) {
        offsetSoFar += 0x800 * afsInfo->pages[fileId];
        if (offsetSoFar >= offset)
            return fileId;
    }
    if (fileId == afsInfo->numItems)
        return 0xffffffff;
    return fileId;
}

KEXPORT DWORD GetOffsetByFileId(DWORD afsId, DWORD fileId)
{
    DWORD* pageLenTable = (DWORD*)data[AFS_PAGELEN_TABLE];
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
    //    LogWithTwoNumbers(&k_kload, "data[%d]=%08x", j, data[j]);
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
    memcpy(data, dataArray[v], sizeof(data));
}