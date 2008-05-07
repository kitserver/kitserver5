#include <windows.h>
#include <stdio.h>
#include "d3dfont.h"
#include "bserv.h"
#include "kload_exp.h"

#include <map>
#include <pngdib.h>

KMOD k_bserv={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

AFSENTRY AFSArray[16];
BSERV_CFG bserv_cfg;
int selectedBall=-1;
DWORD numBalls=0;
BALLS *balls;
bool isSelectMode=false;
char display[BUFLEN];
char model[BUFLEN];
char texture[BUFLEN];
BITMAPINFO* ballbmInfo;

std::map<DWORD,LPVOID> g_Buffers;
std::map<DWORD,LPVOID>::iterator g_BuffersIterator;
	
void bservReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped);
void bservUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2, DWORD result);
bool bservAllocMem(DWORD infoBlock, DWORD param2, DWORD* size);
bool bservFreeMemory(DWORD addr);
void FreeAllBuffers();
void SaveAFSAddr(HANDLE file,DWORD FileNumber,AFSENTRY* afs,DWORD tmp);
void ApplyBallTexture(BYTE* orgBall, BYTE* ballTex,DWORD bitsOff, DWORD palOff);
void ShuffleBallTexture(BYTE* dest, BYTE* src, DWORD bitsOff);
void ReadBalls();
void AddBall(LPTSTR sdisplay,LPTSTR smodel,LPTSTR stexture);
void FreeBalls();
void SetBall(DWORD id);
void bservKeyboardProc(int code1, WPARAM wParam, LPARAM lParam);
void BeginDrawBallLabel();
void EndDrawBallLabel();
void DrawBallLabel(IDirect3DDevice8* self);
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename);
static int read_file_to_mem(char *fn,unsigned char **ppfiledata, int *pfilesize);
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size);
void FreePNGTexture(BITMAPINFO* bitmap);


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	char ballCfg[BUFLEN];
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{	
		Log(&k_bserv,"Attaching dll...");
		
		switch (GetPESInfo()->GameVersion) {
			case gvPES5PC: //support for PES5 PC...
			case gvWE9PC: //... and WE9 PC
            case gvWE9LEPC: //... and WE9:LE PC
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_bserv,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:
		
		RegisterKModule(&k_bserv);
		
		char tmp[BUFLEN];
		
		strcpy(tmp,GetPESInfo()->pesdir);
		strcat(tmp,"dat\\0_text.afs");
		
		HANDLE TempHandle=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
		DWORD HeapAddress=(DWORD)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,8);
		
		for (int i=0;i<16;i++)
			SaveAFSAddr(TempHandle,i,&(AFSArray[i]),HeapAddress);
		
		HeapFree(GetProcessHeap(),HEAP_ZERO_MEMORY,(LPVOID)HeapAddress);
		CloseHandle(TempHandle);
		
		ReadBalls();
		
		//load settings
	    ZeroMemory(ballCfg, BUFLEN);
	    sprintf(ballCfg, "%s\\bserv.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(ballCfg, "rb");
	    if (f) {
	        fread(&bserv_cfg, sizeof(BSERV_CFG), 1, f);
	        fclose(f);
	    } else {
	    	bserv_cfg.selectedBall=-1;
	    };
	    
		SetBall(bserv_cfg.selectedBall);
		
		HookFunction(hk_ReadFile,(DWORD)bservReadFile);
		HookFunction(hk_Unpack,(DWORD)bservUnpack);
		HookFunction(hk_AllocMem,(DWORD)bservAllocMem);
		HookFunction(hk_BeforeFreeMemory,(DWORD)bservFreeMemory);
	    HookFunction(hk_Input,(DWORD)bservKeyboardProc);
	    		
	    HookFunction(hk_DrawKitSelectInfo,(DWORD)DrawBallLabel);
	    HookFunction(hk_OnShowMenu,(DWORD)BeginDrawBallLabel);
	    HookFunction(hk_OnHideMenu,(DWORD)EndDrawBallLabel);
    
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_bserv,"Detaching dll...");
		
		//save settings
		bserv_cfg.selectedBall=selectedBall;
	    ZeroMemory(ballCfg, BUFLEN);
	    sprintf(ballCfg, "%s\\bserv.dat", GetPESInfo()->mydir);
	    FILE* f = fopen(ballCfg, "wb");
	    if (f) {
	        fwrite(&bserv_cfg, sizeof(BSERV_CFG), 1, f);
	        fclose(f);
	    }
		
		UnhookFunction(hk_ReadFile,(DWORD)bservReadFile);
		UnhookFunction(hk_Unpack,(DWORD)bservUnpack);
		UnhookFunction(hk_AllocMem,(DWORD)bservAllocMem);
		UnhookFunction(hk_BeforeFreeMemory,(DWORD)bservFreeMemory);
		UnhookFunction(hk_Input,(DWORD)bservKeyboardProc);
		UnhookFunction(hk_DrawKitSelectInfo,(DWORD)DrawBallLabel);
		
		FreePNGTexture(ballbmInfo);
		FreeBalls();
		FreeAllBuffers();
		
		Log(&k_bserv,"Detaching done.");
	};

	return true;
};

void bservReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
  LPDWORD lpNumberOfBytesRead,  LPOVERLAPPED lpOverlapped)
{
	int found=-1;
	DWORD dwOffset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
	
	for (int i=0;i<16;i++) {
		if (AFSArray[i].Buffer==(DWORD)lpBuffer) AFSArray[i].Buffer=0;
		if (AFSArray[i].AFSAddr==dwOffset) {found=i;break;};
	};
	if (found!=-1) {
		AFSArray[found].Buffer=(DWORD)lpBuffer;
	};
	return;
};

void bservUnpack(DWORD addr1, DWORD addr2, DWORD size1, DWORD zero, DWORD* size2, DWORD result)
{
	char tmp[BUFLEN];
	DWORD NBW;
	int found=-1;
	ENCBUFFERHEADER *e;

	if (selectedBall<0) return;
	
	for (int i=0;i<16;i++) {
		if (AFSArray[i].Buffer==(addr1-0x20)) {found=i;break;};
	};
	
	if (found!=-1) {
		strcpy(tmp,GetPESInfo()->gdbDir);
		strcat(tmp,"GDB\\balls\\");
		if (found%2==0) {
			Log(&k_bserv,"Unpacking ball model...");
			
			strcat(tmp,"mdl\\");
			strcat(tmp,model);
			HANDLE file=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
			if (file!=INVALID_HANDLE_VALUE) {
				DWORD FileSize=GetFileSize(file,NULL);
				DWORD buf=(DWORD)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,FileSize);
				SetFilePointer(file,0,NULL,FILE_BEGIN);
				ReadFile(file,(LPVOID)buf,FileSize,&NBW,0);
				CloseHandle(file);
				
				e=(ENCBUFFERHEADER*)buf;
				DWORD buf2=(DWORD)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,e->dwDecSize);
				
				LogWithNumber(&k_bserv,"MemUnpack=%x",MemUnpack(buf+0x20,buf2,e->dwEncSize,&(e->dwDecSize)));
				
				memcpy((LPVOID)addr2,(LPVOID)buf2,e->dwDecSize);

				HeapFree(GetProcessHeap(),HEAP_ZERO_MEMORY,(LPVOID)buf);
				HeapFree(GetProcessHeap(),HEAP_ZERO_MEMORY,(LPVOID)buf2);
			};
		} else {
			Log(&k_bserv,"Unpacking ball texture...");
			strcat(tmp,texture);
			HANDLE file=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
			if (file!=INVALID_HANDLE_VALUE) {
				DWORD buf=0;
				DWORD bitsOff=0;
				DWORD palOff=0;
				BITMAPINFOHEADER *bmpinfo=NULL;
				
				if (lstrcmpi(tmp + lstrlen(tmp)-4, ".png")==0) {
					if (!ballbmInfo)
						LoadPNGTexture(&ballbmInfo,tmp);
					buf=(DWORD)ballbmInfo;
					bmpinfo=(BITMAPINFOHEADER*)buf;
					palOff= bmpinfo->biSize;
					bitsOff= palOff+0x400;
				} else {
					DWORD FileSize=GetFileSize(file,NULL);
					buf=(DWORD)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,FileSize);
					SetFilePointer(file,0,NULL,FILE_BEGIN);
					ReadFile(file,(LPVOID)buf,FileSize,&NBW,0);
					CloseHandle(file);
					
					BITMAPFILEHEADER* bfh = (BITMAPFILEHEADER*)buf;
					bitsOff= bfh->bfOffBits;
					bmpinfo=(BITMAPINFOHEADER*)(buf+sizeof(BITMAPFILEHEADER));
					palOff= bmpinfo->biSize + sizeof(BITMAPFILEHEADER);
				};

				DECBUFFERHEADER *hdr=(DECBUFFERHEADER*)addr2;
				if ((bmpinfo->biWidth==256) && (bmpinfo->biHeight==256)) {
					Log(&k_bserv,"Bitmap size is 256x256!");
					//hdr->dwDecSize=0x10480;
					hdr->texWidth=256;
					hdr->texHeight=256;
					hdr->blockWidth=128;
					hdr->blockHeight=128;
				}
				else if ((bmpinfo->biWidth==512) && (bmpinfo->biHeight==512)) {
					Log(&k_bserv,"Bitmap size is 512x512!");
					//hdr->dwDecSize=0x40480;
					hdr->texWidth=512;
					hdr->texHeight=512;
					hdr->blockWidth=256;
					hdr->blockHeight=256;
				}
				else if ((bmpinfo->biWidth==1024) && (bmpinfo->biHeight==1024)) {
					Log(&k_bserv,"Bitmap size is 1024x1024!");
					hdr->texWidth=1024;
					hdr->texHeight=1024;
					hdr->blockWidth=512;
					hdr->blockHeight=512;
				}
				else
					Log(&k_bserv,"Unknow bitmap size!");
				
				hdr->dwDecSize=((ENCBUFFERHEADER*)addr1)->dwDecSize;
				
				//memcpy((LPVOID)(addr2+0x80),(LPVOID)(buf+54),hdr->dwDecSize-0x80);
				ApplyBallTexture((BYTE*)addr2,(BYTE*)buf,bitsOff,palOff);
				Log(&k_bserv,"Ball texture has been applied!");
				FreePNGTexture(ballbmInfo);
			} else {
				Log(&k_bserv,"Ball texture file couldn't be opened!");
			};
		};
	};
	return;
};

bool bservAllocMem(DWORD infoBlock, DWORD param2, DWORD* size)
{
	char tmp[BUFLEN];
	DWORD NBW=0, newsize=0;
	DWORD src=*(DWORD*)(infoBlock+0x60);
	DWORD* space=(DWORD*)(infoBlock+0x64);
	int found=-1;

	if (selectedBall<0) return true;
	
	for (int i=0;i<16;i++) {
		if (AFSArray[i].Buffer==src) {found=i;break;};
	};
	
	if ((src!=0) && (found!=-1)) {
		strcpy(tmp,GetPESInfo()->gdbDir);
		strcat(tmp,"GDB\\balls\\");
		if (found%2==0) {
			Log(&k_bserv,"Allocation space for ball model...");
			strcat(tmp,"mdl\\");
			strcat(tmp,model);
			LogWithString(&k_bserv,"Trying to get size of %s",tmp);
			HANDLE TempHandle=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
			if (TempHandle!=INVALID_HANDLE_VALUE) {
				SetFilePointer(TempHandle,8,0,0);
				ReadFile(TempHandle,(LPVOID)&newsize,4,&NBW,0);
				LogWithNumber(&k_bserv,"New size is %d bytes",newsize);
				CloseHandle(TempHandle);
			};
		} else {
			Log(&k_bserv,"Allocation space for ball texture...");
			strcat(tmp,texture);
			LogWithString(&k_bserv,"Trying to get size of %s",tmp);
			HANDLE TempHandle=CreateFile(tmp,GENERIC_READ,3,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
			if (TempHandle!=INVALID_HANDLE_VALUE) {
				if (lstrcmpi(tmp + lstrlen(tmp)-4, ".png")==0) {
					if (ballbmInfo!=NULL)
						FreePNGTexture(ballbmInfo);
					newsize=LoadPNGTexture(&ballbmInfo, tmp)-sizeof(BITMAPINFOHEADER)+0x80;
					LogWithNumber(&k_bserv,"New size is %d bytes",newsize);
				} else {
					newsize=GetFileSize(TempHandle,NULL)-54+0x80;
					LogWithNumber(&k_bserv,"New size is %d bytes",newsize);
				};
				CloseHandle(TempHandle);
			};
		};
		if (newsize>0) {
			//As PES seems to have problems with buffers of over 0x100000 bytes but this
			//is necessary to use 1024x1024 pixel balls, we allocate the memory ourself
			*space=(DWORD)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,newsize);
			g_Buffers[*space]=(LPVOID)(*space);
			return false;
		};
	};
	return true;
};

bool bservFreeMemory(DWORD addr)
{
	bool result=true;

	if (g_Buffers[addr] != 0) {
		HeapFree(GetProcessHeap(),0,g_Buffers[addr]);
		g_Buffers.erase(addr);
		result=false;
	};

	return result;
};

void FreeAllBuffers()
{
	for (g_BuffersIterator=g_Buffers.begin();g_BuffersIterator!=g_Buffers.end();g_BuffersIterator++)
		HeapFree(GetProcessHeap(),0,g_BuffersIterator->second);
		
	g_Buffers.clear();			
	return;
};

void SaveAFSAddr(HANDLE file,DWORD FileNumber,AFSENTRY* afs,DWORD tmp)
{
	DWORD NBW=0;
	afs->FileNumber=FileNumber;
	SetFilePointer(file,8*(FileNumber+1),0,0);
	ReadFile(file,(LPVOID)tmp,8,&NBW,0);
	afs->AFSAddr=*(DWORD*)tmp;
	afs->FileSize=*(DWORD*)(tmp+4);
	afs->Buffer=0;
	return;
};

// Substiture ball texture with data from BMP.
void ApplyBallTexture(BYTE* orgBall, BYTE* ballTex,DWORD bitsOff, DWORD palOff)
{
	if (ballTex == 0) 
		return;

	// copy palette from bitmap

	TRACE2(&k_bserv,"bitsOff = %08x", bitsOff);
	TRACE2(&k_bserv,"palOff  = %08x", palOff);

	for (int bank=0; bank<8; bank++)
	{
		memcpy(orgBall + 0x80 + bank*32*4,        ballTex + palOff + bank*32*4,        8*4);
		memcpy(orgBall + 0x80 + bank*32*4 + 16*4, ballTex + palOff + bank*32*4 + 8*4,  8*4);
		memcpy(orgBall + 0x80 + bank*32*4 + 8*4,  ballTex + palOff + bank*32*4 + 16*4, 8*4);
		memcpy(orgBall + 0x80 + bank*32*4 + 24*4, ballTex + palOff + bank*32*4 + 24*4, 8*4);
	}
	// swap R and B
	for (int i=0; i<0x100; i++) 
	{
		BYTE blue = orgBall[0x80 + i*4];
		BYTE red = orgBall[0x80 + i*4 + 2];
		BYTE transp = orgBall[0x80 + i*4 + 3];
		orgBall[0x80 + i*4] = red;
		orgBall[0x80 + i*4 + 2] = blue;
		if (transp==0)
			orgBall[0x80 + i*4 + 3]=0x80;
	}
	TRACE(&k_bserv,"Palette copied.");

	// copy the texture and shuffle it
	ShuffleBallTexture(orgBall, ballTex, bitsOff);
}

/**
 * Shuffle the ball texture data.
 */
void ShuffleBallTexture(BYTE* dest, BYTE* src, DWORD bitsOff)
{
	DECBUFFERHEADER* destHeader = (DECBUFFERHEADER*)dest;

	BYTE* srcData = (BYTE*)src + bitsOff;
	BYTE* destData = (BYTE*)destHeader + destHeader->bitsOffset;

	int unitX = destHeader->texWidth / destHeader->blockWidth;
	int unitY = destHeader->texHeight / destHeader->blockHeight;
	int pitch = destHeader->texWidth;

	int w = destHeader->texWidth;
	int h = destHeader->texHeight;
    int y = 0;

	// copy all pixels.
	// NOTE: we need a loop here to reverse the order of lines
	for (y=0; y<h; y++)
	{
		memcpy(destData + (h-y-1)*w, srcData + y*w, w);
	}

	// block swap
	for (y=0; y<h; y++)
	{
		if (y < 2 || y >= h-2 || ((y-2)/4)%2 == 1)
			continue;

		for (int k=0; k<w/8; k++)
		{
			BYTE copy[4];
			memcpy(copy, destData + y*w + k*8, 4);
			memcpy(destData + y*w + k*8, destData + y*w + k*8 + 4, 4);
			memcpy(destData + y*w + k*8 + 4, copy, 4);
		}
	}

	// block interlace
	for (y=0; y<h; y++)
	{
		for (int k=0; k<w/16; k++)
		{
			BYTE left[8], right[8];
			memcpy(left, destData + y*w + k*16, 8);
			memcpy(right, destData + y*w + k*16 + 8, 8);

			for (int x=0; x<8; x++)
			{
				destData[y*w + k*16 + x*2] = left[x];
				destData[y*w + k*16 + x*2 + 1] = right[x];
			}
		}
	}

	// make a extra buffer
	BYTE* copy = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, w*h);
	if (copy == NULL)
	{
		Log(&k_bserv,"ShuffleBallTexture: Out of memory ERROR.");
		return;
	}

	// swap lines
	for (y=0; y<h/4; y++)
	{
		memcpy(copy + y*4*w, destData + y*4*w, w);
		memcpy(copy + (y*4 + 1)*w, destData + (y*4 + 2)*w, w);
		memcpy(copy + (y*4 + 2)*w, destData + (y*4 + 1)*w, w);
		memcpy(copy + (y*4 + 3)*w, destData + (y*4 + 3)*w, w);
	}

	// reverse fourliner
	// NOTE: as done in previous operation, "copy" contains the result
	// of all previous steps.
	for (y=0; y<h/2; y++)
	{
		for (int x=0; x<w/2; x++)
		{
			destData[y*2*w + x*2] = copy[y*2*w + x];
			destData[y*2*w + x*2 + 1] = copy[(y*2+1)*w + x];
			destData[(y*2+1)*w + x*2] = copy[y*2*w + x + w/2];
			destData[(y*2+1)*w + x*2 + 1] = copy[(y*2+1)*w + x + w/2];
		}
	}

	// free buffer
	HeapFree(GetProcessHeap(), 0, copy);
}

void ReadBalls()
{
	char tmp[BUFLEN];
	char str[BUFLEN];
	char *comment=NULL;
	char sdisplay[BUFLEN], smodel[BUFLEN], stexture[BUFLEN];
		
	strcpy(tmp,GetPESInfo()->gdbDir);
	strcat(tmp,"GDB\\balls\\map.txt");
	
	FILE* cfg=fopen(tmp, "rt");
	if (cfg==NULL) return;
	while (true) {
		ZeroMemory(str, BUFLEN);
		fgets(str, BUFLEN-1, cfg);
		if (feof(cfg)) break;
		
		// skip comments
		comment=NULL;
		comment = strstr(str, "#");
		if (comment != NULL) comment[0] = '\0';
		
		// parse line
		ZeroMemory(sdisplay,BUFLEN);
		ZeroMemory(smodel,BUFLEN);
		ZeroMemory(stexture,BUFLEN);
		if (sscanf(str,"\"%[^\"]\",\"%[^\"]\",\"%[^\"]\"",sdisplay,smodel,stexture)==3)
			AddBall(sdisplay,smodel,stexture);
	};
	fclose(cfg);
	
	return;
};

void AddBall(LPTSTR sdisplay,LPTSTR smodel,LPTSTR stexture)
{
	BALLS *tmp=new BALLS[numBalls+1];
	memcpy(tmp,balls,numBalls*sizeof(BALLS));
	delete balls;
	balls=tmp;
	
	balls[numBalls].display=new char [strlen(sdisplay)+1];
	strcpy(balls[numBalls].display,sdisplay);
	
	balls[numBalls].model=new char [strlen(smodel)+1];
	strcpy(balls[numBalls].model,smodel);
	
	balls[numBalls].texture=new char [strlen(stexture)+1];
	strcpy(balls[numBalls].texture,stexture);

	numBalls++;
	return;
};

void FreeBalls()
{
	for (int i=0;i<numBalls;i++) {
		delete balls[i].display;
		delete balls[i].model;
		delete balls[i].texture;
	};
	delete balls;
	numBalls=0;
	selectedBall=-1;
	return;
};

void SetBall(DWORD id)
{
	char tmp[BUFLEN];
	
	if (id<numBalls)
		selectedBall=id;
	else if (id<0)
		selectedBall=-1;
	else
		selectedBall=-1;
		
	if (selectedBall<0) {
		strcpy(tmp,"game choice");
		strcpy(model,"\0");
		strcpy(texture,"\0");
	} else {
		strcpy(tmp,balls[selectedBall].display);
		strcpy(model,balls[selectedBall].model);
		strcpy(texture,balls[selectedBall].texture);
	};
	
	strcpy(display,"Ball: ");
	strcat(display,tmp);
	
	return;
};

void bservKeyboardProc(int code1, WPARAM wParam, LPARAM lParam)
{
	if ((!isSelectMode) || (code1 < 0))
		return; 

	if ((code1==HC_ACTION) && (lParam & 0x80000000)) {
		if (wParam == KeyNextBall) {
			SetBall(selectedBall+1);
		} else if (wParam == KeyPrevBall) {
			if (selectedBall<0)
				SetBall(numBalls-1);
			else
				SetBall(selectedBall-1);
		} else if (wParam == KeyResetBall) {
			SetBall(-1);
		} else if (wParam == KeyRandomBall) {
			LARGE_INTEGER num;
			QueryPerformanceCounter(&num);
			int iterations = num.LowPart % MAX_ITERATIONS;
			for (int j=0;j<iterations;j++)
				SetBall(selectedBall+1);
		};
	};
	
	return;
};

void BeginDrawBallLabel()
{
	isSelectMode=true;
	dksiSetMenuTitle("Ball selection");
	return;
};

void EndDrawBallLabel()
{
	isSelectMode=false;
	return;
};

void DrawBallLabel(IDirect3DDevice8* self)
{
	SIZE size;
	DWORD color = 0xffffffff; // white
	
	if (selectedBall<0)
		color = 0xffc0c0c0; // gray if ball is game choice
	
	UINT g_bbWidth=GetPESInfo()->bbWidth;
	UINT g_bbHeight=GetPESInfo()->bbHeight;
	double stretchX=GetPESInfo()->stretchX;
	double stretchY=GetPESInfo()->stretchY;
	
	KGetTextExtent(display,16,&size);
	//draw shadow
	if (selectedBall>=0)
		KDrawText((g_bbWidth-size.cx)/2+3*stretchX,g_bbHeight*0.75+3*stretchY,0xff000000,16,display,true);
	//print ball label
	KDrawText((g_bbWidth-size.cx)/2,g_bbHeight*0.75,color,16,display,true);

	return;
};

// Load texture from PNG file. Returns the size of loaded texture
DWORD LoadPNGTexture(BITMAPINFO** tex, char* filename)
{
	TRACE4(&k_bserv,"LoadPNGTexture: loading %s", filename);
    DWORD size = 0;

    PNGDIB *pngdib;
    LPBITMAPINFOHEADER* ppDIB = (LPBITMAPINFOHEADER*)tex;

    pngdib = pngdib_p2d_init();
	//TRACE(&k_bserv,"LoadPNGTexture: structure initialized");

    BYTE* memblk;
    int memblksize;
    if(read_file_to_mem(filename,&memblk, &memblksize)) {
        TRACE(&k_bserv,"LoadPNGTexture: unable to read PNG file");
        return 0;
    }
    //TRACE(&k_bserv,"LoadPNGTexture: file read into memory");

    pngdib_p2d_set_png_memblk(pngdib,memblk,memblksize);
	pngdib_p2d_set_use_file_bg(pngdib,1);
	pngdib_p2d_run(pngdib);

	//TRACE(&k_bserv,"LoadPNGTexture: run done");
    pngdib_p2d_get_dib(pngdib, ppDIB, (int*)&size);
	//TRACE(&k_bserv,"LoadPNGTexture: get_dib done");

    pngdib_done(pngdib);
	TRACE(&k_bserv,"LoadPNGTexture: done done");

	TRACE2(&k_bserv,"LoadPNGTexture: *ppDIB = %08x", (DWORD)*ppDIB);
    if (*ppDIB == NULL) {
		TRACE(&k_bserv,"LoadPNGTexture: ERROR - unable to load PNG image.");
        return 0;
    }

    // read transparency values from tRNS chunk
    // and put them into DIB's RGBQUAD.rgbReserved fields
    ApplyAlphaChunk((RGBQUAD*)&((BITMAPINFO*)*ppDIB)->bmiColors, memblk, memblksize);

    HeapFree(GetProcessHeap(), 0, memblk);

	TRACE(&k_bserv,"LoadPNGTexture: done");
	return size;
};

// Read a file into a memory block.
static int read_file_to_mem(char *fn,unsigned char **ppfiledata, int *pfilesize)
{
	HANDLE hfile;
	DWORD fsize;
	//unsigned char *fbuf;
	BYTE* fbuf;
	DWORD bytesread;

	hfile=CreateFile(fn,GENERIC_READ,FILE_SHARE_READ,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hfile==INVALID_HANDLE_VALUE) return 1;

	fsize=GetFileSize(hfile,NULL);
	if(fsize>0) {
		//fbuf=(unsigned char*)GlobalAlloc(GPTR,fsize);
		//fbuf=(unsigned char*)calloc(fsize,1);
        fbuf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fsize);
		if(fbuf) {
			if(ReadFile(hfile,(void*)fbuf,fsize,&bytesread,NULL)) {
				if(bytesread==fsize) { 
					(*ppfiledata)  = fbuf;
					(*pfilesize) = (int)fsize;
					CloseHandle(hfile);
					return 0;   // success
				}
			}
			free((void*)fbuf);
		}
	}
	CloseHandle(hfile);
	return 1;  // error
};

/**
 * Extracts alpha values from tRNS chunk and applies stores
 * them in the RGBQUADs of the DIB
 */
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size)
{
    // find the tRNS chunk
    DWORD offset = 8;
    while (offset < size) {
        PNG_CHUNK_HEADER* chunk = (PNG_CHUNK_HEADER*)(memblk + offset);
        if (chunk->dwName == MAKEFOURCC('t','R','N','S')) {
            int numColors = SWAPBYTES(chunk->dwSize);
            BYTE* alphaValues = memblk + offset + sizeof(chunk->dwSize) + sizeof(chunk->dwName);
            for (int i=0; i<numColors; i++) {
                palette[i].rgbReserved = (alphaValues[i]==0xff) ? 0x80 : alphaValues[i]/2;
            }
        }
        // move on to next chunk
        offset += sizeof(chunk->dwSize) + sizeof(chunk->dwName) + 
            SWAPBYTES(chunk->dwSize) + sizeof(DWORD); // last one is CRC
    }
};

void FreePNGTexture(BITMAPINFO* bitmap)
{
	if (bitmap != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)bitmap);
	}
};
