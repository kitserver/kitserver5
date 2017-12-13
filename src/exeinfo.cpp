#include <stdio.h>
#include "imageutil.h"

void main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("Usage: exeinfo <exefilename>\n");
		return;
	}

	FILE* f = fopen(argv[1], "rb");
	if (f != NULL)
	{
		if (SeekEntryPoint(f))
		{
			DWORD ep;
			fread(&ep, 4, 1, f);
			printf("Entry point: %08x\n", ep);
		}
		if (SeekImageBase(f))
		{
			DWORD ib;
			fread(&ib, 4, 1, f);
			printf("Image base: %08x\n", ib);
		}
		if (SeekSectionVA(f, ".rdata"))
		{
			DWORD va;
			fread(&va, 4, 1, f);
			printf(".rdata virtual address: %08x\n", va);
		}
		if (SeekSectionVA(f, ".data"))
		{
			DWORD va;
			fread(&va, 4, 1, f);
			printf(".data virtual address: %08x\n", va);
		}
		fclose(f);
	}
	else
	{
		printf("Error: unable to open %s for reading\n", argv[1]);
		return;
	}
}
