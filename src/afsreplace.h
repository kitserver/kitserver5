#ifndef AFSREPLACE_H
#define AFSREPLACE_H

void afsreplace_init(int v);
void newReadNumPagesCallPoint();

typedef bool (*GETNUMPAGES_PROC)(DWORD afsId, DWORD fileId, 
        DWORD orgNumPages, DWORD* numPages);
KEXPORT DWORD GetAfsIdByBase(DWORD base);
KEXPORT DWORD GetFileIdByOffset(DWORD afsId, DWORD offset);
KEXPORT DWORD GetOffsetByFileId(DWORD afsId, DWORD fileId);

typedef struct _AFS_INFO
{
    DWORD unknown1;
    DWORD unknown2;
    WORD numItems;
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
