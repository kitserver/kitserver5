//filedec.cpp

#include <windows.h>
#include <stdio.h>
#include <direct.h>
#include <cstdlib>
#include <io.h>

#define DECRYPT_START 0x573
#define DECRYPT_FACTOR 0x6c078965
#define DECRYPT_FACTOR2 0x6c371625

DWORD CS_fileSizes[]={0x131400,0x47000,0x3000,0x65400,0x65800,0xb7800,
					0xb4400,0x9000,0x9000,0x9000,0x4800,0x11400,0x65400};

DWORD sectStarts[]={0,0x1400,0x1800};

//_H is only the header used for crypting/decrypting
#define OPTSECT_STADIUMNAMES_H		 4
#define OPTSECT_STADIUMNAMES		 5 //61 bytes for each name
#define OPTSECT_LEAGUENAMES			 6 //84 bytes each, shown name begins at byte 22
#define OPTSECT_CREATEDPLAYERS_H     7
#define OPTSECT_CREATEDPLAYERS       8 //124 bytes for each (up to 184) player
#define OPTSECT_SHOES				 9 //92 bytes for each of the 9 shoes
#define OPTSECT_STDPLAYERS_H		10
#define OPTSECT_STDPLAYERS			11 //124 bytes for each of the 5000 players
#define OPTSECT_PLAYERNUMBERS_H		12
#define OPTSECT_NATPLAYERNUMBERS	13 //23 bytes for each team (number=value+1)
									   //first national (64), then 0-byte-teams (2), then
									   //created players teams (8)
#define OPTSECT_CLUBPLAYERNUMBERS	14 //23 bytes for each team (number=value+1)
#define OPTSECT_NATTEAMPLAYERS		15 //46 bytes for each team (23 players * 2 bytes)
#define OPTSECT_CLUBTEAMPLAYERS		16 //64 bytes for each club team (145)
#define OPTSECT_TEAMFORMATIONS		17 //628 bytes each (national+club)
#define OPTSECT_TEAMINFOS_H			18
#define OPTSECT_TEAMINFOS			19 //140 bytes (club)
#define OPTSECT_TEAMKITINFOS_H		20
#define OPTSECT_TEAMNATKITINFOS		21 //544 bytes for each team (national)
#define OPTSECT_TEAMCLUBKITINFOS	22 //544 bytes for each team (club)


DWORD optSections[]={0,OPTSECT_STADIUMNAMES,OPTSECT_CREATEDPLAYERS_H,OPTSECT_STDPLAYERS_H,
					OPTSECT_PLAYERNUMBERS_H,OPTSECT_TEAMINFOS_H,OPTSECT_TEAMKITINFOS_H,
					24,26};

BYTE array2[]={0x41,0x6C,0x6C,0x20,0x59,0x6F,0x75,0x72,0x20,0x53,0x6F,0x63,0x63,0x65,0x72,0x20,
0x47,0x61,0x6D,0x65,0x20,0x41,0x72,0x65,0x20,0x42,0x65,0x6C,0x6F,0x6E,0x67,0x20,0x54,0x6F,0x20,
0x57,0x45,0x2E,0x81,0x40,0x8D,0xC5,0x8F,0x89,0x82,0xCC,0x95,0xB6,0x82,0xCD,0x82,0xA8,0x96,0xF1,
0x91,0xA9,0x82,0xC8,0x9F,0xAD,0x97,0x8E,0x82,0xC5,0x81,0x42,0x82,0xC7,0x82,0xA4,0x82,0xE2,0x82,
0xE7,0x83,0x45,0x83,0x43,0x83,0x43,0x83,0x8C,0x82,0xCC,0x83,0x66,0x81,0x5B,0x83,0x5E,0x82,0xF0,
0x89,0xFC,0x91,0xA2,0x82,0xB5,0x82,0xC4,0x83,0x49,0x81,0x5B,0x83,0x4E,0x83,0x56,0x83,0x87,0x83,
0x93,0x82,0xC5,0x8F,0xA4,0x94,0x84,0x82,0xB5,0x82,0xC4,0x82,0xA2,0x82,0xE9,0x94,0x79,0x82,0xAA,
0x8B,0x8F,0x82,0xE9,0x82,0xE7,0x82,0xB5,0x82,0xA2,0x81,0x41,0x8B,0xE0,0x91,0x4B,0x82,0xAA,0x97,
0x8D,0x82,0xDC,0x82,0xC8,0x82,0xA2,0x8C,0xC2,0x90,0x6C,0x82,0xE2,0x81,0x41,0x83,0x86,0x81,0x5B,
0x83,0x55,0x81,0x5B,0x8A,0xD4,0x82,0xCC,0x83,0x7B,0x83,0x89,0x83,0x93,0x83,0x65,0x83,0x42,0x83,
0x41,0x82,0xC5,0x83,0x66,0x81,0x5B,0x83,0x5E,0x82,0xCC,0x82,0xE2,0x82,0xE8,0x8E,0xE6,0x82,0xE8,
0x82,0xF0,0x82,0xB7,0x82,0xE9,0x82,0xCC,0x82,0xCD,0x83,0x51,0x81,0x5B,0x83,0x80,0x82,0xC9,0x91,
0xCE,0x82,0xB7,0x82,0xE9,0x8F,0xEE,0x94,0x4D,0x82,0xF0,0x8A,0xB4,0x82,0xB6,0x82,0xE9,0x82,0xB5,
0x81,0x41,0x82,0xB1,0x82,0xBF,0x82,0xE7,0x82,0xC6,0x82,0xB5,0x82,0xC4,0x82,0xE0,0x8A,0xF0,0x82,
0xB5,0x82,0xA2,0x8C,0xC0,0x82,0xE8,0x82,0xBE,0x82,0xAF,0x82,0xC7,0x81,0x41,0x8F,0xA4,0x94,0x84,
0x8E,0x6E,0x82,0xDF,0x82,0xE7,0x82,0xEA,0x82,0xE9,0x82,0xC6,0x82,0xB1,0x82,0xBF,0x82,0xE7,0x82,
0xC6,0x82,0xB5,0x82,0xC4,0x82,0xE0,0x91,0x7A,0x92,0xE8,0x82,0xB5,0x82,0xC4,0x82,0xC8,0x82,0xA2,
0x8E,0x96,0x82,0xC9,0x82,0xC8,0x82,0xE9,0x82,0xCC,0x82,0xC5,0x83,0x67,0x83,0x89,0x83,0x75,0x83,
0x8B,0x96,0x68,0x8E,0x7E,0x82,0xCC,0x82,0xBD,0x82,0xDF,0x82,0xC9,0x88,0xC3,0x8D,0x86,0x89,0xBB,
0x82,0xB5,0x82,0xDC,0x82,0xB7,0x81,0x42,0x00,0x00};


void DeEncCode(BYTE* buf, DWORD fs);
bool CorrectChecksums(BYTE* buf);
void DecryptOptionfile(BYTE* buf, bool decrypt=true);
int main(int argc, char* argv[]);


int main(int argc, char* argv[])
{
	char newFilename[4096];
	BYTE* buf=NULL;
	DWORD fs=0;
	DWORD choice=0;
	bool successful=false;
	
	if (argc<2) {
		printf("Too few parameters!\n");
		system("PAUSE");
		return 0;
	};

	
	FILE* f = fopen(argv[1], "rb");
    if (!f) {
    	printf("Could't open file!\n");
		system("PAUSE");
		return 0;
	};
	
	fs=_filelength(_fileno(f));
	if (fs==0) {
		fclose(f);
		printf("0 Byte file!\n");
		system("PAUSE");
		return 0;
	};
	
	buf=new BYTE[fs];
    fread(buf, fs, 1, f);
    fclose(f);
    
    printf("What do you want to do?\n");
    printf("1 - Decode/Encode\n");
    printf("2 - Correct checksums (in decoded file)\n");
    printf("3 - Correct checksums -> Encode\n");
    printf("4 - Decode -> Correct checksums -> Encode\n");
    printf("5 - Decrypt Option file\n");
    printf("6 - Encrypt Option file\n");
    printf("7 - Decode -> Decrypt Option file\n");
    printf("8 - Encrypt Option file -> Correct checksums -> Encode\n");
    scanf("%d",&choice);
    
    if (choice<1 || choice>8) {
    	delete buf;
		buf=NULL;
    	return 0;
    };
    
    if (choice>=1 || choice<=8) {
		sprintf(newFilename,"%s_dec.bin",argv[1]);
		f = fopen(newFilename, "wb");
		if (!f) {
			delete buf;
			buf=NULL;
	    	printf("Could't create decoded file!\n");
			system("PAUSE");
			return 0;
		};
	};
    
    switch (choice) {
    case 1:
		DeEncCode(buf,fs);
		
		successful=true;
		break;
	
	case 2:
		if (!CorrectChecksums(buf))
			break;
			
		successful=true;
		break;
	
	case 3:
		if (!CorrectChecksums(buf))
			break;
		
		DeEncCode(buf,fs);
		
		successful=true;
		break;
	
	case 4:
		DeEncCode(buf,fs);
		
		if (!CorrectChecksums(buf))
			break;
		
		DeEncCode(buf,fs);
		
		successful=true;
		break;
	
	case 5:
		DecryptOptionfile(buf, true);
		
		successful=true;
		break;
		
	case 6:
		DecryptOptionfile(buf, false);
		
		successful=true;
		break;
			
	case 7:
		DeEncCode(buf,fs);
		
		DecryptOptionfile(buf, true);
		
		successful=true;
		break;
		
			
	case 8:
		DecryptOptionfile(buf, false);
		
		if (!CorrectChecksums(buf))
			break;
			
		DeEncCode(buf,fs);
		
		successful=true;
		break;
	};
	
    if (choice>=1 || choice<=8) {
    	if (successful)
    		fwrite(buf,fs,1,f);
		fclose(f);
	};
		
	delete buf;
	buf=NULL;
		
	system("PAUSE");
	return 0;
}

void DeEncCode(BYTE* buf, DWORD fs)
{
	DWORD param=DECRYPT_START;
    DWORD start=0;
    
	for (int i=0;i<start;i++) {
		param*=DECRYPT_FACTOR;
		param++;
	};
		
	for (i=0;i<fs;i++) {
		buf[i]^=(param & 0xff);
		param*=DECRYPT_FACTOR;
		param++;
	};
	
	printf("Decoding/Encoding was successful!\n");
	
	return;
};

bool CorrectChecksums(BYTE* buf)
{
	DWORD checksum=0;
	BYTE sectType=0;
    DWORD max=0, tmp=0;
    BYTE* sectStart=NULL;
    BYTE numSects=2;
    
    BYTE fileType=buf[12];
    // 0=option file
    // 1=replay
    // 2=formations
    // 3=cup
    // 4=league
    // 5=master league
    // 6=
    // 7=challenge training
    // 8=dribbling
    // 9=
    // 10=master league (best team)
    // 11=
    // 12=
    
	if (fileType>12) {
		printf("Unknown file type (%d)!\n",fileType);
		return false;
	};
    
    if (fileType==4 || fileType==5)
    	numSects=3;
    
    for (int i=0;i<numSects;i++) {
		sectStart=buf+sectStarts[i];
	
		sectType=sectStart[10];

		if (sectType>12) {
			printf("Unknown section type (%d)!\n",sectType);
			return false;
		};
		
		max=CS_fileSizes[sectType];
		max+=0x3ff;
		tmp=(max & 0x80000000)?0xffffffff:0;
		tmp&=0x3ff;
		max+=tmp;
		max=max>>10;
		max+=0x3c;
		max=max<<10;
		tmp=(max & 0x80000000)?0xffffffff:0;
		tmp&=0x3ff;
		max+=tmp;
		max=max>>10;
		max+=4;
		max=max<<10;
		max-=0xc;
		
		if (max>*(DWORD*)sectStart)
			max=*(DWORD*)sectStart;
			
		max=max>>2;
		
		checksum=0;
		for (int j=0;j<max;j++)
			checksum+=(*(DWORD*)(sectStart+j*4+0xc));
		
		*(DWORD*)(sectStart+4)=checksum;
		printf("New checksum for section %d is %x!\n",i+1,checksum);
	};
	
	printf("Checksums were corrected successfully!\n");
		
	return true;
};

void DecryptOptionfile(BYTE* buf, bool decrypt)
{
	DWORD* dwBuf=NULL;
	DWORD sectStart=0;
	DWORD val=0;
	BYTE val2=0;
	
	for (int i=0;i<9;i++) {
		dwBuf=(DWORD*)buf;
		sectStart=dwBuf[optSections[i]+0x4a];
		dwBuf=(DWORD*)(buf+sectStart);
		
		if (decrypt) {
			for (int j=0;j<(dwBuf[0]>>2);j++) {
				val2=array2[j%0x16f];
				val=dwBuf[j+3] - val2 - ((val2 & 0x80)?0xffffff00:0);
				dwBuf[j+3]=val ^ DECRYPT_FACTOR2;
			};
		} else {
			for (int j=0;j<(dwBuf[0]>>2);j++) {
				val=dwBuf[j+3] ^ DECRYPT_FACTOR2;
				val2=array2[j%0x16f];
				val+=val2 + ((val2 & 0x80)?0xffffff00:0);
				dwBuf[j+3]=val;
			};
		};
	};
	
	if (decrypt)
		printf("Decrypting Option file was successful!\n");
	else
		printf("Encrypting Option file was successful!\n");
			
	return;
};
