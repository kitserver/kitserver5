#ifndef AFSREPLACE_H
#define AFSREPLACE_H

void afsreplace_init(int v);
void newReadNumPagesCallPoint();

typedef bool (*GETNUMPAGES_PROC)(DWORD afsId, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages);
KEXPORT DWORD GetAfsIdByBase(DWORD base);
KEXPORT DWORD GetFileIdByOffset(DWORD afsId, DWORD offset);
KEXPORT DWORD GetOffsetByFileId(DWORD afsId, DWORD fileId);
KEXPORT DWORD GetProbableFileIdForHandle(DWORD afsId, 
        DWORD offset, HANDLE hFile);
KEXPORT void SetNextProbableReadForHandle(DWORD afsId, 
        DWORD offset, DWORD fileId, HANDLE hFile);

typedef struct _AFS_INFO
{
    DWORD unknown1;
    DWORD unknown2;
    DWORD numItems;
    WORD numItems2;
    BYTE unknown3;
    BYTE entryIsDword;
    char filename[0x108];
    union 
    {
        WORD wordPages[1];
        DWORD pages[1];
    };
} AFS_INFO;

#endif
