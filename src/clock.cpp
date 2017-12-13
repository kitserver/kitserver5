// clock.cpp
#include <windows.h>
#include <stdio.h>
#include "clock.h"
#include "kload_exp.h"

KMOD k_clock={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitClock();
KEXPORT void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops, bool addRetn);
void clockAdjustCallPoint();
KEXPORT void clockAdjust(DWORD min, DWORD sec, DWORD half);

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	int i,j;
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_clock,"Attaching dll...");
		
		hInst=hInstance;
		
		int v=GetPESInfo()->GameVersion;
		switch (v) {
			case gvWE9LEPC:
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_clock,"clock module currently only works with WE9:LE");
		return false;
		
		//Everything is OK!
		GameVersIsOK:

		RegisterKModule(&k_clock);
		
		HookFunction(hk_D3D_Create,(DWORD)InitClock);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_clock,"Detaching dll...");
		Log(&k_clock,"Detaching done.");
	}
	
	return true;
}

KEXPORT void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops, bool addRetn)
{
    DWORD target = (DWORD)func + codeShift;
	if (addr && target)
	{
	    BYTE* bptr = (BYTE*)addr;
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 16, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(addr + 1);
	        ptr[0] = target - (DWORD)(addr + 5);
            // padding with NOPs
            for (int i=0; i<numNops; i++) bptr[5+i] = 0x90;
            if (addRetn)
                bptr[5+numNops]=0xc3;
	        TRACE2X(&k_clock, "Function (%08x) HOOKED at address (%08x)", target, addr);
	    }
	}
}

void InitClock()
{
    HookCallPoint(0x9c15d9, clockAdjustCallPoint, 6, 2, false);
    Log(&k_clock, "Clock display adjuster enabled.");
}

void clockAdjustCallPoint()
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
        push eax // half
        push edx // sec
        push ecx // min
        call clockAdjust
        add esp, 0x0c  // pop parameters
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

KEXPORT void clockAdjust(DWORD min, DWORD sec, DWORD half)
{
    WORD* clockData = (WORD*)0x38c341c;
    switch (half) {
        case 1:
            min += 45;
            break;
        case 3:
            min += 15;
            break;
    }
    clockData[0] = min;
    clockData[2] = sec;
    clockData[4] = half;
}

