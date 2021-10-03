// savegames.cpp
#include <windows.h>
#include <stdio.h>
#include "SGF.h"
#include "extrafiles.h"
#include "savegames.h"
#include "kload_exp.h"

#include <map>

KMOD k_savegames={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

#define BUFALLOC(s) (BYTE*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,s)
#define BUFFREE(a) HeapFree(GetProcessHeap(),NULL,(LPVOID)a)

HINSTANCE hInst;
SAVEGAMEFUNCS sgf;
SAVEGAMEEXPFUNCS* sgef=NULL;
DWORD numExpModules=0;

bool isAsyncReadStarted=false;
BYTE* readBuffer=0;
DWORD readCaller=0;
DWORD writeCaller=0;
char readFilename[4096];
BYTE* replayExtFile=NULL;
BYTE* MLExtFile[2]={NULL,NULL};;

//data of replays
bool replHasData1=false;
bool replHasData2=false;
WORD replPlayerIDs[64];

WORD lastPlayerIDs[64];


//data of ML
WORD MLplayerIDs[32];
WORD loadMLplayerIDs[64];


COPYPLAYERDATA orgCopyPlayerData=NULL;


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
void InitSavegames();
void WriteData(char* filename);
void ReadData(char* filename);
void sgCopyPlayerData();
DWORD sgAsyncWriteFile(DWORD p1, DWORD p2, DWORD p3, DWORD p4, DWORD p5, DWORD p6);
DWORD sgAsyncReadFile(DWORD p1, DWORD p2, DWORD p3, DWORD p4, DWORD p5, DWORD p6, DWORD p7, DWORD p8);
void sgFinishAsyncRead();
void sgAsyncOpFinished();

bool IsValidPDsectId(BYTE* addr);
bool IsValidPDsectFace(BYTE* addr);
bool IsValidPDsectHair(BYTE* addr);

void sgFreeBuffer(BYTE* addr);
BYTE* RequestReplayPlayerData(BYTE type, DWORD player);

DWORD sgMLSetPlayerID(DWORD p1, DWORD counter);
BYTE* RequestMLPlayerData(BYTE type, DWORD player);

DWORD GetMLPlayerDataSize(DWORD player);
DWORD CopyMLPlayerData(DWORD player, BYTE* playersData, DWORD totalSize);


BOOL FileExists(char* filename)
{
    TRACE4(&k_savegames,"FileExists: Checking file: %s", filename);
    HANDLE hFile;
    hFile = CreateFile(TEXT(filename),        // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL, // normal file
                       NULL);                 // no attr. template
     
    if (hFile == INVALID_HANDLE_VALUE) 
    { 
        return FALSE;
    }
    CloseHandle(hFile);
    return TRUE;
}


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	int i,j;
	
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		Log(&k_savegames,"Attaching dll...");
		
		hInst=hInstance;
		
		int v=GetPESInfo()->GameVersion;
		switch (v) {
			case gvPES5PC:
			//case gvWE9PC:
			//case gvWE9LEPC:
				goto GameVersIsOK;
				break;
		};
		//Will land here if game version is not supported
		Log(&k_savegames,"Your game version is currently not supported!");
		return false;
		
		//Everything is OK!
		GameVersIsOK:

		RegisterKModule(&k_savegames);
		
		HookFunction(hk_D3D_Create,(DWORD)InitSavegames);
		
		memcpy(code,codeArray[v-1],sizeof(code));
		memcpy(dta,dtaArray[v-1],sizeof(dta));
		
		sgf.RequestReplayPlayerData=(REQUESTREPLAYPLAYERDATA)RequestReplayPlayerData;
		sgf.RequestMLPlayerData=(REQUESTMLPLAYERDATA)RequestMLPlayerData;
		sgf.FreeBuffer=(SGFREEBUFFER)sgFreeBuffer;
		SetSavegameFuncs(&sgf);
		
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		Log(&k_savegames,"Detaching dll...");
			
		MasterUnhookFunction(code[C_ASYNC_WRITEFILE_CS1],sgAsyncWriteFile);
		MasterUnhookFunction(code[C_ASYNC_WRITEFILE_CS2],sgAsyncWriteFile);
		MasterUnhookFunction(code[C_ASYNC_READFILE_CS],sgAsyncReadFile);
		
		Log(&k_savegames,"Detaching done.");
	};
	
	return true;
};

void InitSavegames()
{
	sgef=GetSavegameExpFuncs(&numExpModules);
	
	orgCopyPlayerData=(COPYPLAYERDATA)code[C_COPYPLAYERDATA];
			
	DWORD protection=0, newProtection=PAGE_EXECUTE_READWRITE;
	BYTE* bptr;
	DWORD* ptr;

	if (code[C_COPYPLAYERDATA_JMP] != 0) {
	    bptr = (BYTE*)code[C_COPYPLAYERDATA_JMP];
	    ptr = (DWORD*)(code[C_COPYPLAYERDATA_JMP] + 1);
	    if (VirtualProtect(bptr, 5, newProtection, &protection)) {
	        bptr[0] = 0xe9;
	        ptr[0] = (DWORD)sgCopyPlayerData - (DWORD)(code[C_COPYPLAYERDATA_JMP] + 5);
	        Log(&k_savegames,"CopyPlayerData HOOKED at code[C_COPYPLAYERDATA_JMP]");
		};
    };
    
    if (code[RESERVEDFILEMEMORY] != 0)
	{
		ptr = (DWORD*)code[RESERVEDFILEMEMORY];
	    if (VirtualProtect(ptr, 4, newProtection, &protection)) {
	        *ptr=NEWRESERVEDFILEMEMORY;
	    };
	};
    
    if (code[RESERVEDMEMORY1] != 0  &&  code[RESERVEDMEMORY2] != 0)
	{
		ptr = (DWORD*)code[RESERVEDMEMORY1];
	    if (VirtualProtect(ptr, 4, newProtection, &protection)) {
	        *ptr=NEWRESERVEDMEMORY;
	    };
	    
	    ptr = (DWORD*)code[RESERVEDMEMORY2];
		if (VirtualProtect(ptr, 4, newProtection, &protection)) {
	        *ptr=NEWRESERVEDMEMORY;
	    };
	};

	MasterHookFunction(code[C_ASYNC_WRITEFILE_CS1],6,sgAsyncWriteFile);
	MasterHookFunction(code[C_ASYNC_WRITEFILE_CS2],6,sgAsyncWriteFile);
	MasterHookFunction(code[C_ASYNC_READFILE_CS],8,sgAsyncReadFile);
	
	MasterHookFunction(code[C_MLSETPLAYERID_CS],2,sgMLSetPlayerID);

    
    if (code[C_ASYNC_OP_FINISHED_JMP] != 0) {
	    bptr = (BYTE*)code[C_ASYNC_OP_FINISHED_JMP];
	    ptr = (DWORD*)(code[C_ASYNC_OP_FINISHED_JMP] + 1);
	    if (VirtualProtect(bptr, 5, newProtection, &protection)) {
	        bptr[0] = 0xe9;
	        ptr[0] = (DWORD)sgAsyncOpFinished - (DWORD)(code[C_ASYNC_OP_FINISHED_JMP] + 5);
	        Log(&k_savegames,"AsyncOpFinished HOOKED at code[C_ASYNC_OP_FINISHED_JMP]");
		};
    };
	
	UnhookFunction(hk_D3D_Create,(DWORD)InitSavegames);
	
	return;
};


void sgCopyPlayerData()
{
	BYTE* playerData=(BYTE*)(dta[PLAYERDATA_BASE]);
	WORD* currPlayerIDs=(WORD*)(dta[CPD_PLAYERIDS]);
	WORD orgPlayerIDs[64];
	int i,j;
	
	memcpy(orgPlayerIDs,currPlayerIDs,64*sizeof(WORD));
	
	//modify player ids to be loaded here (if playing matches
	//with loaded ml teams having an extrafile)
	
	for (i=0;i<64;i++) {
		if (orgPlayerIDs[i]>=0x7600 && orgPlayerIDs[i]<0x7640) {
			//player belongs to an ml file with extra info,
			//restore original id
			//orgPlayerIDs[i]=???
		};
	};
	
	orgCopyPlayerData();
	
	//these are the ids of the players really used in game
	//and when saving replays
	memcpy(lastPlayerIDs,currPlayerIDs,64*sizeof(WORD));

	for (i=0;i<64;i++) {
		//give the ids that should be saved internally to the modules
		for (j=0;j<numExpModules;j++)
			if (sgef[j].giveIdToModule!=NULL) {
				sgef[j].giveIdToModule((DWORD)playerData+i*0x344,orgPlayerIDs[i]);
			};
	};
	
	return;
};

DWORD sgAsyncWriteFile(DWORD p1, DWORD p2, DWORD p3, DWORD p4, DWORD p5, DWORD p6)
{
	char filename[4096];
	char buf[128];
	int i, j;
	
	DWORD result=MasterCallNext(p1,p2,p3,p4,p5,p6);
	
	if (result!=1)
		return result;
	
	DWORD moreInfo=p1+0x1c;
	for (i=0;i<3;i++)
		if (*(DWORD*)(p1+0x18+i*0x20) != 1)
			if (i<2)
				moreInfo+=0x20;
			else
				moreInfo=0;
		else
			break;
	
	if (moreInfo==0)
		return result;
	
	BYTE* writeBuffer=(BYTE*)(*(DWORD*)(*(DWORD*)moreInfo+0x18));
	writeCaller=p6;
	
	BYTE folder=((p2 & 0xff)*4) + ((p2>>16) & 0xff) + 1;
	ZeroMemory(buf,128);
	memcpy(buf,(BYTE*)p1,20);
	sprintf(filename,"%s\\folder%d\\%s",(char*)(*(DWORD*)(dta[SAVEDDATAFOLDER])),folder,buf);

	//MessageBox(0,buf,"WRITE",0);
	
	if (IsBadReadPtr(writeBuffer,1))
		return result;
	
	char extFilename[4096];
	char bakFilename[4096];
	
	SAVEGAMEDATA saveData;
	BYTE* globalData=NULL;
	BYTE* team1Data=NULL;
	BYTE* team2Data=NULL;
	BYTE* playersData=NULL;
	DWORD dataSize=sizeof(SAVEGAMEDATA);
	DWORD globalSize=0;
	DWORD team1Size=0;	
	DWORD team2Size=0;
	DWORD playersSize=0;
	DWORD totalSize=0;
	DWORD start1=0, actSect=0;
	DWORD temp1=0;

	bool gotAnyData=false;
	DWORD numSects[64];
	
	DWORD fileType=writeBuffer[12];
	
	ZeroMemory((BYTE*)&saveData,sizeof(SAVEGAMEDATA));
	strcpy(saveData.sig,"KSI");
	
	switch (fileType) {
	case PESTYPE_REPLAY:
		saveData.type=SGTYPE_REPLAY;
		globalSize=0;
		team1Size=0;	
		team2Size=0;
		playersSize=sizeof(SG_REPLAYPLAYERSDATA);	//header, always there
		
		gotAnyData=false;
		for (i=0;i<64;i++) {
			if (lastPlayerIDs[i]>=0x7060 && lastPlayerIDs[i]<0x70A0) {
				//ML
				temp1=GetMLPlayerDataSize(lastPlayerIDs[i]-0x7060);
				if (temp1>0) {
					playersSize+=temp1;
					gotAnyData=true;
				};
				
			} else {
				numSects[i]=0;
				
				//by now we always save just the ids
				//->1 id section
				numSects[i]++;
				playersSize+=sizeof(SG_PLAYERSECT_ID);
				gotAnyData=true;
				
				playersSize+=(4+numSects[i]*sizeof(DWORD));
			};
		};
				
		if (gotAnyData) {
			playersData=BUFALLOC(playersSize);
			SG_REPLAYPLAYERSDATA* sg_replayPlayersData=(SG_REPLAYPLAYERSDATA*)playersData;
			SG_PLAYERDATA* sg_playerData;
			SG_PLAYERSECT_ID* sg_playerSect_ID;
			
			totalSize=sizeof(SG_REPLAYPLAYERSDATA);
			for (i=0;i<64;i++) {
				if (lastPlayerIDs[i]>=0x7060 && lastPlayerIDs[i]<0x70A0) {
					//ML
					if (temp1==0) {
						(sg_replayPlayersData->playersStarts)[i]=0;
						continue;
					};
					
					(sg_replayPlayersData->playersStarts)[i]=totalSize;
					totalSize=CopyMLPlayerData(lastPlayerIDs[i]-0x7060,playersData,totalSize);
					
				} else {
					if (numSects[i]==0) {
						(sg_replayPlayersData->playersStarts)[i]=0;
						continue;
					};
					
					(sg_replayPlayersData->playersStarts)[i]=totalSize;
					sg_playerData=(SG_PLAYERDATA*)(playersData+totalSize);
					
					sg_playerData->numSects=numSects[i];
					actSect=0;
					start1=totalSize+4+numSects[i]*sizeof(DWORD);
					
					
					
					sg_playerSect_ID=(SG_PLAYERSECT_ID*)(playersData+start1);
					sg_playerSect_ID->type=PDSECT_ID;
					sg_playerSect_ID->id=lastPlayerIDs[i];
					
					(sg_playerData->sectStarts)[actSect]=start1;
					actSect++;
					start1+=sizeof(SG_PLAYERSECT_ID);
					
					
					
					totalSize=start1;
				};
			};
		} else {
			playersSize=0;
		};

		break;
		
	case PESTYPE_ML_BESTTEAM:
		saveData.type=SGTYPE_MLBESTTEAM;
		globalSize=0;
		team1Size=0;	
		team2Size=0;
		playersSize=sizeof(SG_ML_BT_PLAYERSDATA);	//header, always there
		
		gotAnyData=false;
		for (i=0;i<32;i++) {
			if (MLplayerIDs[i]==0)
				continue;
			
			playersSize+=sizeof(SG_PLAYERSECT_ID);
			gotAnyData=true;
			
			playersSize+=(4+sizeof(DWORD));
		};
				
		if (gotAnyData) {
			playersData=BUFALLOC(playersSize);
			SG_ML_BT_PLAYERSDATA* sg_ML_BT_PlayersData=(SG_ML_BT_PLAYERSDATA*)playersData;
			SG_PLAYERDATA* sg_playerData;
			SG_PLAYERSECT_ID* sg_playerSect_ID;
			
			totalSize=sizeof(SG_ML_BT_PLAYERSDATA);
			for (i=0;i<32;i++) {
				if (MLplayerIDs[i]==0) {
					(sg_ML_BT_PlayersData->playersStarts)[i]=0;
					continue;
				};
				
				(sg_ML_BT_PlayersData->playersStarts)[i]=totalSize;
				sg_playerData=(SG_PLAYERDATA*)(playersData+totalSize);
				
				sg_playerData->numSects=1;
				start1=totalSize+4+sizeof(DWORD);
				
				sg_playerSect_ID=(SG_PLAYERSECT_ID*)(playersData+start1);
				sg_playerSect_ID->type=PDSECT_ID;
				sg_playerSect_ID->id=MLplayerIDs[i];
				
				(sg_playerData->sectStarts)[0]=start1;
			
				totalSize=start1+sizeof(SG_PLAYERSECT_ID);
			};
		} else {
			playersSize=0;
		};
		
		break;
		
	default:
		return result;
		break;
	};
	
	
	
	if (globalData==NULL)
		globalSize=0;
		
	if (team1Data==NULL)
		team1Size=0;
		
	if (team2Data==NULL)
		team2Size=0;
		
	if (playersData==NULL)
		playersSize=0;
	
	
	totalSize=sizeof(SAVEGAMEDATA);
	
	saveData.globalDataStart=(globalSize>0)?totalSize:0;
	totalSize+=globalSize;
	
	saveData.team1DataStart=(team1Size>0)?totalSize:0;
	totalSize+=team1Size;
	
	saveData.team2DataStart=(team2Size>0)?totalSize:0;
	totalSize+=team2Size;
	
	saveData.playersDataStart=(playersSize>0)?totalSize:0;
	totalSize+=playersSize;
	if (totalSize==sizeof(SAVEGAMEDATA)) {
		//we wouldn't save anything, so exit now
		return result;
	};
	
	strcpy(extFilename,filename);
	strcat(extFilename,".ksi");
	
	if (FileExists(extFilename)) {
		//if file already exist, rename it
		strcpy(bakFilename,extFilename);
		strcat(bakFilename,".bak");
		MoveFileEx(extFilename,bakFilename,MOVEFILE_REPLACE_EXISTING);
	};
	
	FILE* f=fopen(extFilename,"wb");
	if (f != NULL) {
		fwrite((BYTE*)&saveData,sizeof(SAVEGAMEDATA),1,f);
		
		if (globalSize>0)
			fwrite(globalData,globalSize,1,f);
			
		if (team1Size>0)
			fwrite(team1Data,team1Size,1,f);
		
		if (team2Size>0)
			fwrite(team2Data,team2Size,1,f);
			
		if (playersSize>0)
			fwrite(playersData,playersSize,1,f);
		
		fclose(f);
	};
	
	if (globalData!=NULL)
		BUFFREE(globalData);
		
	if (team1Data!=NULL)
		BUFFREE(team1Data);
		
	if (team2Data!=NULL)
		BUFFREE(team2Data);
		
	if (playersData!=NULL)
		BUFFREE(playersData);
	
	return result;
};

DWORD sgAsyncReadFile(DWORD p1, DWORD p2, DWORD p3, DWORD p4, DWORD p5, DWORD p6, DWORD p7, DWORD p8)
{
	char buf[128];
	DWORD result=MasterCallNext(p1,p2,p3,p4,p5,p6,p7,p8);
	
	if (result!=1)
		return result;
		
	readBuffer=(BYTE*)(*(DWORD*)(*(DWORD*)(p1+0x1c)+0x18));
	readCaller=p8;
	isAsyncReadStarted=true;
	
	BYTE folder=((p2 & 0xff)*4) + ((p2>>16) & 0xff) + 1;
	ZeroMemory(buf,128);
	memcpy(buf,(BYTE*)p1,20);
	sprintf(readFilename,"%s\\folder%d\\%s",(char*)(*(DWORD*)(data[SAVEDDATAFOLDER])),folder,buf);
	
	//MessageBox(0,buf,"READ",0);

	return result;
};

void sgFinishAsyncRead()
{
	char extFilename[4096];
	DWORD tmp=0;
	
	if (readBuffer==0 || IsBadReadPtr(readBuffer,1))
		return;
		
	if (readCaller!=code[C_READSELECTEDFILE]+5) {
		//we only care about files that are "really" loaded
		//(when the user has chosen a file)
		return;
	};
	
	DWORD fileType=readBuffer[12];
	if (fileType>12)
		return;
		
	//read extra file into memory
	strcpy(extFilename,readFilename);
	strcat(extFilename,".ksi");
	
	HANDLE hfile=CreateFile(extFilename,GENERIC_READ,FILE_SHARE_READ,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (hfile==INVALID_HANDLE_VALUE) return;
	DWORD fsize=GetFileSize(hfile,NULL);
	BYTE* extFile=BUFALLOC(fsize);
	if (!ReadFile(hfile,extFile,fsize,&tmp,NULL)) {
		CloseHandle(hfile);
		BUFFREE(extFile);
		return;
	};
	CloseHandle(hfile);

	SAVEGAMEDATA* saveData=(SAVEGAMEDATA*)extFile;
	
	if (strcmp(saveData->sig,"KSI") != 0) {
		BUFFREE(extFile);
		return;
	};
	
	BYTE currMLfile=*(BYTE*)(dta[CURR_MLFILE]);
	
	switch (fileType) {
	case PESTYPE_REPLAY:
		if (saveData->type != SGTYPE_REPLAY) {
			BUFFREE(extFile);
			return;
		};
		
		if (replayExtFile != NULL)
			BUFFREE(replayExtFile);
		
		replayExtFile=extFile;
		break;
		
	case PESTYPE_ML_BESTTEAM:
		if (saveData->type != SGTYPE_MLBESTTEAM) {
			BUFFREE(extFile);
			return;
		};
		
		if (MLExtFile[currMLfile] != NULL)
			BUFFREE(MLExtFile[currMLfile]);
		
		MLExtFile[currMLfile]=extFile;
		break;
		
	default:
		BUFFREE(extFile);
		return;
		break;
	};
	
	//MessageBox(0,"read finished!","",0);
	
	return;
};
	

void sgAsyncOpFinished()
{
	if (isAsyncReadStarted && *(DWORD*)(dta[CURRENT_ASYNCOP])==0) {
		sgFinishAsyncRead();
		isAsyncReadStarted=false;
		readBuffer=0;
	};
		
	return;
};

void sgFreeBuffer(BYTE* addr)
{
	if (addr != NULL)
		BUFFREE(addr);
	
	return;
};


BYTE* RequestReplayPlayerData(BYTE type, DWORD player)
{
	//possible types:
	//0=face
	//1=hair
	//2=boots
	BYTE* result=NULL;
		
	if (type>0 || player>63 || replayExtFile==NULL)
		return NULL;
		
	DWORD playersDataStart=((SAVEGAMEDATA*)replayExtFile)->playersDataStart;
	if (playersDataStart==0) return NULL;
	DWORD thisPlayerStart=((SG_REPLAYPLAYERSDATA*)(replayExtFile+playersDataStart))->playersStarts[player];
	if (thisPlayerStart==0) return NULL;
	thisPlayerStart+=(DWORD)(replayExtFile+playersDataStart);
	SG_PLAYERDATA* pd=(SG_PLAYERDATA*)thisPlayerStart;
	
	if (pd->numSects==0) return NULL;
	
	//find the section which has highest priority
	BYTE* sectStart;
	DWORD sectType;
	DWORD bestSection;
	DWORD bestSectionType=PDSECT_NONE;
	
	for (int i=0;i<pd->numSects;i++) {
		sectStart=replayExtFile+playersDataStart+(pd->sectStarts)[i];
		sectType=*(DWORD*)sectStart;

		//check if section suits to the requested type
		switch (type) {
		case 0:	//face
			switch (sectType) {
			//player id (PDSECT_ID) is only used when no other data is found
			case PDSECT_ID:
				if (bestSectionType==PDSECT_NONE) {
					if (IsValidPDsectId(sectStart)) {
						bestSection=i;
						bestSectionType=sectType;
					};
				};
				break;
				
			case PDSECT_FACE:
				if (bestSectionType==PDSECT_NONE || bestSectionType==PDSECT_ID) {
					if (IsValidPDsectFace(sectStart)) {
						bestSection=i;
						bestSectionType=sectType;
					};
				};
				break;
				
			};
			
			break;

		
		case 1: //hair
			switch (sectType) {
			case PDSECT_ID:
				if (bestSectionType==PDSECT_NONE) {
					if (IsValidPDsectId(sectStart)) {
						bestSection=i;
						bestSectionType=sectType;
					};
				};
				break;
				
			case PDSECT_FACE:
				if (bestSectionType==PDSECT_NONE || bestSectionType==PDSECT_ID) {
					if (IsValidPDsectHair(sectStart)) {
						bestSection=i;
						bestSectionType=sectType;
					};
				};
				break;
				
			};
			
			break;

		
		//case 2: boots
		};
		
	};
		
	
	if (bestSectionType>=PDSECT_NONE) return NULL;
	
	BYTE* bestSectionStart=replayExtFile+playersDataStart+(pd->sectStarts)[bestSection];
	SG_PLAYERSECT_ID* sgPlayerSectId;
	SG_PLAYERSECT_DATA* sgPlayerSectData;
	DATAOFID* dataOfId;
	DATAOFMEMORY* dataOfMemory;
		
		
	switch (bestSectionType) {
	case PDSECT_ID:
		sgPlayerSectId=(SG_PLAYERSECT_ID*)bestSectionStart;
		result=BUFALLOC(sizeof(DATAOFID));
		dataOfId=(DATAOFID*)result;
		
		dataOfId->type=0;
		dataOfId->id=sgPlayerSectId->id;
		
		break;
		
	case PDSECT_FACE:
	case PDSECT_HAIR:
		sgPlayerSectData=(SG_PLAYERSECT_DATA*)bestSectionStart;
		result=BUFALLOC(sizeof(DATAOFMEMORY) + sgPlayerSectData->size);
		dataOfMemory=(DATAOFMEMORY*)result;
		
		dataOfMemory->type=1;
		dataOfMemory->size=sgPlayerSectData->size;
		memcpy(dataOfMemory->dta,sgPlayerSectData->dta,sgPlayerSectData->size);
		
		break;
		
	default:
		return NULL;
	};
	
	
	return result;
};

bool IsValidPDsectId(BYTE* addr)
{
	SG_PLAYERSECT_ID* sect=(SG_PLAYERSECT_ID*)addr;
	if (sect->type==PDSECT_ID && sect->id != 0)
		return true;
		
	return false;
};

bool IsValidPDsectFace(BYTE* addr)
{
	SG_PLAYERSECT_DATA* sect=(SG_PLAYERSECT_DATA*)addr;
	if (sect->type==PDSECT_FACE && sect->size != 0)
		return true;
		
	return false;
};

bool IsValidPDsectHair(BYTE* addr)
{
	SG_PLAYERSECT_DATA* sect=(SG_PLAYERSECT_DATA*)addr;
	if (sect->type==PDSECT_HAIR && sect->size != 0)
		return true;
		
	return false;
};

DWORD sgMLSetPlayerID(DWORD p1, DWORD counter)
{
	DWORD playerID;
	__asm mov playerID, eax
	
	playerID&=0xffff;
	
	if ((counter & 0xffff)==0)
		ZeroMemory(MLplayerIDs,32*sizeof(WORD));
	
	MLplayerIDs[(counter & 0xffff)]=playerID;
	
	DWORD result=MasterCallNext(p1,counter);
	return result;
};


BYTE* RequestMLPlayerData(BYTE type, DWORD player)
{
	//possible types:
	//0=face
	//1=hair
	//2=boots
	BYTE* result=NULL;
		
	if (type>0 || player>63)
		return NULL;
		
	BYTE* extFile=(player<32)?MLExtFile[0]:MLExtFile[1];
	
	if (extFile==NULL)
		return NULL;
		
	DWORD playersDataStart=((SAVEGAMEDATA*)extFile)->playersDataStart;
	if (playersDataStart==0) return NULL;
	DWORD thisPlayerStart=((SG_REPLAYPLAYERSDATA*)(extFile+playersDataStart))->playersStarts[player%32];
	if (thisPlayerStart==0) return NULL;
	thisPlayerStart+=(DWORD)(extFile+playersDataStart);
	SG_PLAYERDATA* pd=(SG_PLAYERDATA*)thisPlayerStart;
	
	if (pd->numSects==0) return NULL;
	
	//find the section which has highest priority
	BYTE* sectStart;
	DWORD sectType;
	DWORD bestSection;
	DWORD bestSectionType=PDSECT_NONE;
	
	for (int i=0;i<pd->numSects;i++) {
		sectStart=extFile+playersDataStart+(pd->sectStarts)[i];
		sectType=*(DWORD*)sectStart;

		//check if section suits to the requested type
		switch (type) {
		case 0:	//face
			switch (sectType) {
			//player id (PDSECT_ID) is only used when no other data is found
			case PDSECT_ID:
				if (bestSectionType==PDSECT_NONE) {
					if (IsValidPDsectId(sectStart)) {
						bestSection=i;
						bestSectionType=sectType;
					};
				};
				break;
				
			case PDSECT_FACE:
				if (bestSectionType==PDSECT_NONE || bestSectionType==PDSECT_ID) {
					if (IsValidPDsectFace(sectStart)) {
						bestSection=i;
						bestSectionType=sectType;
					};
				};
				break;
				
			};
			
			break;

		
		case 1: //hair
			switch (sectType) {
			case PDSECT_ID:
				if (bestSectionType==PDSECT_NONE) {
					if (IsValidPDsectId(sectStart)) {
						bestSection=i;
						bestSectionType=sectType;
					};
				};
				break;
				
			case PDSECT_FACE:
				if (bestSectionType==PDSECT_NONE || bestSectionType==PDSECT_ID) {
					if (IsValidPDsectHair(sectStart)) {
						bestSection=i;
						bestSectionType=sectType;
					};
				};
				break;
				
			};
			
			break;

		
		//case 2: boots
		};
		
	};
		
	
	if (bestSectionType>=PDSECT_NONE) return NULL;
	
	BYTE* bestSectionStart=extFile+playersDataStart+(pd->sectStarts)[bestSection];
	SG_PLAYERSECT_ID* sgPlayerSectId;
	SG_PLAYERSECT_DATA* sgPlayerSectData;
	DATAOFID* dataOfId;
	DATAOFMEMORY* dataOfMemory;
		
		
	switch (bestSectionType) {
	case PDSECT_ID:
		sgPlayerSectId=(SG_PLAYERSECT_ID*)bestSectionStart;
		result=BUFALLOC(sizeof(DATAOFID));
		dataOfId=(DATAOFID*)result;
		
		dataOfId->type=0;
		dataOfId->id=sgPlayerSectId->id;
		
		break;
		
	case PDSECT_FACE:
	case PDSECT_HAIR:
		sgPlayerSectData=(SG_PLAYERSECT_DATA*)bestSectionStart;
		result=BUFALLOC(sizeof(DATAOFMEMORY) + sgPlayerSectData->size);
		dataOfMemory=(DATAOFMEMORY*)result;
		
		dataOfMemory->type=1;
		dataOfMemory->size=sgPlayerSectData->size;
		memcpy(dataOfMemory->dta,sgPlayerSectData->dta,sgPlayerSectData->size);
		
		break;
		
	default:
		return NULL;
	};
	
	
	return result;
};

DWORD GetMLPlayerDataSize(DWORD player)
{
	if (player>63)
		return 0;
	
	BYTE* currMLextFile=(player<32)?MLExtFile[0]:MLExtFile[1];
	if (currMLextFile==NULL) return 0;
	
	DWORD playersDataStart=((SAVEGAMEDATA*)currMLextFile)->playersDataStart;
	if (playersDataStart==0) return 0;
	DWORD thisPlayerStart=((SG_REPLAYPLAYERSDATA*)(currMLextFile+playersDataStart))->playersStarts[player%32];
	if (thisPlayerStart==0) return 0;
	thisPlayerStart+=(DWORD)(currMLextFile+playersDataStart);
	SG_PLAYERDATA* pd=(SG_PLAYERDATA*)thisPlayerStart;
	
	if (pd->numSects==0) return 0;
		
	//return the size of all sections
	DWORD result=4;
	SG_PLAYERSECT_ID* sgPlayerSectId;
	SG_PLAYERSECT_DATA* sgPlayerSectData;
	
	for (int i=0;i<pd->numSects;i++) {
		BYTE* sectStart=currMLextFile+playersDataStart+(pd->sectStarts)[i];
		DWORD sectType=*(DWORD*)sectStart;
		
		switch (sectType) {
		case PDSECT_ID:
			result+=sizeof(SG_PLAYERSECT_ID);
			break;
			
		case PDSECT_FACE:
		case PDSECT_HAIR:
			sgPlayerSectData=(SG_PLAYERSECT_DATA*)sectStart;
			result+=sizeof(SG_PLAYERSECT_DATA) + sgPlayerSectData->size;
			break;
			
		default:
			return 0;
			break;
		};
		
		result+=sizeof(DWORD);
	};
		
	
	return result;
};

DWORD CopyMLPlayerData(DWORD player, BYTE* playersData, DWORD totalSize)
{
	SG_PLAYERSECT_ID* sgPlayerSectId;
	SG_PLAYERSECT_DATA* sgPlayerSectData;
	
	if (player>63 || playersData==NULL)
		return totalSize;
	
	BYTE* currMLextFile=(player<32)?MLExtFile[0]:MLExtFile[1];
	if (currMLextFile==NULL) return totalSize;
		
	SG_PLAYERDATA* sg_playerData=(SG_PLAYERDATA*)(playersData+totalSize);
		
	DWORD playersDataStart=((SAVEGAMEDATA*)currMLextFile)->playersDataStart;
	if (playersDataStart==0) return totalSize;
	DWORD thisPlayerStart=((SG_REPLAYPLAYERSDATA*)(currMLextFile+playersDataStart))->playersStarts[player%32];
	if (thisPlayerStart==0) return totalSize;
	thisPlayerStart+=(DWORD)(currMLextFile+playersDataStart);
	SG_PLAYERDATA* pd=(SG_PLAYERDATA*)thisPlayerStart;
	
	sg_playerData->numSects=pd->numSects;
	DWORD start1=totalSize+4+(pd->numSects)*sizeof(DWORD);	
	
	for (int i=0;i<(pd->numSects);i++) {
		BYTE* sectStart=currMLextFile+playersDataStart+(pd->sectStarts)[i];
		DWORD sectType=*(DWORD*)(playersData+start1);
		
		(sg_playerData->sectStarts)[i]=start1;
		
		switch (sectType) {
		case PDSECT_ID:
			memcpy(playersData+start1,sectStart,sizeof(SG_PLAYERSECT_ID));
			start1+=sizeof(SG_PLAYERSECT_ID);
			break;
		
		case PDSECT_FACE:
		case PDSECT_HAIR:
			sgPlayerSectData=(SG_PLAYERSECT_DATA*)sectStart;
			memcpy(playersData+start1,sectStart,sizeof(SG_PLAYERSECT_DATA) + sgPlayerSectData->size);
			start1+=sizeof(SG_PLAYERSECT_ID) + sgPlayerSectData->size;
			break;
		};
	};
	
	return start1;
};