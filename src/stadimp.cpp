//A small tool for importing stadiums into GDB

#include <windows.h>
#include <stdio.h>
#include <direct.h>
#include <cstdlib>

#define TITLE "STADIUM IMPORTER 1.0"

static char* FILE_NAMES[] = {
    "1_day_fine\\crowd_a0.str",
    "1_day_fine\\crowd_a1.str",
    "1_day_fine\\crowd_a2.str",
    "1_day_fine\\crowd_a3.str",
    "1_day_fine\\crowd_h0.str",
    "1_day_fine\\crowd_h1.str",
    "1_day_fine\\crowd_h2.str",
    "1_day_fine\\crowd_h3.str",
    "1_day_fine\\stad1_main.bin",
    "1_day_fine\\stad2_entrance.bin",
    "1_day_fine\\stad3_adboards.bin",
    "2_day_rain\\crowd_a0.str",
    "2_day_rain\\crowd_a1.str",
    "2_day_rain\\crowd_a2.str",
    "2_day_rain\\crowd_a3.str",
    "2_day_rain\\crowd_h0.str",
    "2_day_rain\\crowd_h1.str",
    "2_day_rain\\crowd_h2.str",
    "2_day_rain\\crowd_h3.str",
    "2_day_rain\\stad1_main.bin",
    "2_day_rain\\stad2_entrance.bin",
    "2_day_rain\\stad3_adboards.bin",
    "3_day_snow\\crowd_a0.str",
    "3_day_snow\\crowd_a1.str",
    "3_day_snow\\crowd_a2.str",
    "3_day_snow\\crowd_a3.str",
    "3_day_snow\\crowd_h0.str",
    "3_day_snow\\crowd_h1.str",
    "3_day_snow\\crowd_h2.str",
    "3_day_snow\\crowd_h3.str",
    "3_day_snow\\stad1_main.bin",
    "3_day_snow\\stad2_entrance.bin",
    "3_day_snow\\stad3_adboards.bin",
    "4_night_fine\\crowd_a0.str",
    "4_night_fine\\crowd_a1.str",
    "4_night_fine\\crowd_a2.str",
    "4_night_fine\\crowd_a3.str",
    "4_night_fine\\crowd_h0.str",
    "4_night_fine\\crowd_h1.str",
    "4_night_fine\\crowd_h2.str",
    "4_night_fine\\crowd_h3.str",
    "4_night_fine\\stad1_main.bin",
    "4_night_fine\\stad2_entrance.bin",
    "4_night_fine\\stad3_adboards.bin",
    "5_night_rain\\crowd_a0.str",
    "5_night_rain\\crowd_a1.str",
    "5_night_rain\\crowd_a2.str",
    "5_night_rain\\crowd_a3.str",
    "5_night_rain\\crowd_h0.str",
    "5_night_rain\\crowd_h1.str",
    "5_night_rain\\crowd_h2.str",
    "5_night_rain\\crowd_h3.str",
    "5_night_rain\\stad1_main.bin",
    "5_night_rain\\stad2_entrance.bin",
    "5_night_rain\\stad3_adboards.bin",
    "6_night_snow\\crowd_a0.str",
    "6_night_snow\\crowd_a1.str",
    "6_night_snow\\crowd_a2.str",
    "6_night_snow\\crowd_a3.str",
    "6_night_snow\\crowd_h0.str",
    "6_night_snow\\crowd_h1.str",
    "6_night_snow\\crowd_h2.str",
    "6_night_snow\\crowd_h3.str",
    "6_night_snow\\stad1_main.bin",
    "6_night_snow\\stad2_entrance.bin",
    "6_night_snow\\stad3_adboards.bin",
    "adboards_tex\\default.bin",
};

static char* FOLDER_NAMES[] = {
    "1_day_fine",
    "2_day_rain",
    "3_day_snow",
    "4_night_fine",
    "5_night_rain",
    "6_night_snow",
    "adboards_tex",
};


#define ADBOARDS 66

int main(int argc, char* argv[])
{
	char stadname[255];
	char buf[4096];
	char buf2[4096];
	char pattern[4096];
	char gdbDir[4096];
	char gdbPath[4096];
	char ext[10];
	char* tmp;
	char* filelist;	
	WIN32_FIND_DATA fData;
	DWORD numFiles=0, NBW;
	DWORD built=2000,capacity=50000;
	char city[255];
	
	strcpy(city,"\0");
	
	ZeroMemory(buf,4096);
	ZeroMemory(buf2,4096);
	ZeroMemory(stadname,255);
	ZeroMemory(pattern,4096);
	ZeroMemory(gdbDir,4096);
	ZeroMemory(gdbPath,4096);
	ZeroMemory(ext,10);
		
	printf("### %s ###\n",TITLE);
	printf("###      by Robbie       ###\n\n");
	printf("This is a small tool which helps you to\nimport downloaded stadiums into your GDB.\n");
	printf("It loads all files from a folder called\n\"stadimp\" inside the kitserver folder.\n");
	printf("The adboard file has to be named \"adboards.bin\".\n");
	printf("Please place all 66 files of the stadium\nin a subfolder called \"stadium\".\n");
	printf("When you did this, type the stadium name\nto continue or hit return to exit:\n\n");
	//scanf("%[^\n]",stadname);
	gets(stadname);
	
	if (strlen(stadname)==0)
		return 0;
		
	strcpy(gdbDir,".\\");
		
	//create the folders
	sprintf(gdbPath, "%sGDB\\stadiums\\%s",gdbDir,stadname);
	_mkdir(gdbPath);
	
	for (int i=0;i<7;i++) {
		sprintf(buf,"%s\\%s",gdbPath,FOLDER_NAMES[i]);
		_mkdir(buf);
	};
	
	//collect the input data
	printf("\nCopying stadium files...\n");
	sprintf(pattern,"%s\\stadimp\\stadium\\*",gdbDir);
	HANDLE hff = FindFirstFile(pattern, &fData);
	if (hff != INVALID_HANDLE_VALUE) {
        while(true)
        {
        	if (!(fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                if (fData.cFileName[0]!='.') {
		        	ZeroMemory(ext,10);
		        	strcpy(ext,fData.cFileName+strlen(fData.cFileName)-4);
		        	
		        	if (strcmp(ext,".bin")==0 || strcmp(ext,".str")==0) {
		        		tmp=new char[(numFiles+1)*1024];
		        			ZeroMemory(tmp,(numFiles+1)*1024);
		        		if (numFiles>0) {
		        			memcpy(tmp,filelist,1024*numFiles);
		        			delete filelist;
		        		};
		        		filelist=tmp;

		        		strcpy(filelist+numFiles*1024,fData.cFileName);
		        		numFiles++;
					};
				};
            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);
    }
    
    if (numFiles<ADBOARDS) {
    	printf("Not enough files found!\n");
    	system("PAUSE");
    	return 0;
    };
	
	//sort the list
	for (i=1;i<numFiles;i++) {
		strcpy(buf,filelist+i*1024);
		int j=i;
		while (strcmp(filelist+(j-1)*1024,buf)>0) {
			strcpy(filelist+j*1024,filelist+(j-1)*1024);
			j--;
			if (j==0) break;
		};
		strcpy(filelist+j*1024,buf);
	};
	
	//copy the first 66 files
	for (i=0;i<ADBOARDS;i++) {
		sprintf(buf,"%s\\stadimp\\stadium\\%s",gdbDir,filelist+i*1024);
		sprintf(buf2,"%s\\%s",gdbPath,FILE_NAMES[i]);
		CopyFile(buf,buf2,FALSE);
		printf("%s --> %s\n",filelist+i*1024,FILE_NAMES[i]);
	};
	
	//adboards
	sprintf(buf,"%s\\stadimp\\adboards.bin",gdbDir);
	
	HANDLE adb=CreateFile(buf, GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);	
        
	if (adb != INVALID_HANDLE_VALUE) {
		CloseHandle(adb);
		printf("\nCopying adboards...\n");
		sprintf(buf2,"%s\\%s",gdbPath,FILE_NAMES[ADBOARDS]);
		CopyFile(buf,buf2,FALSE);
		printf("%s --> %s\n","adboards.bin",FILE_NAMES[ADBOARDS]);
	};
	
	//information
	printf("\nPlease enter information about the stadium now:\n");
	printf("Built: ");
	scanf("%d",&built);
	printf("Capacity: ");
	scanf("%d",&capacity);
	printf("City: ");
	putc('\0',stdin);
	gets(city);
	
	sprintf(buf,"%s\\info.txt",gdbPath);
	
	DeleteFile(buf);
	
	HANDLE infos=CreateFile(buf, GENERIC_WRITE,FILE_SHARE_READ,NULL,
        OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);

	sprintf(buf2,"## %s ##\r\n## Created by %s ##\r\n\r\nbuilt = %d\r\ncapacity = %d\r\ncity = %s\r\n",
				stadname,TITLE,built,capacity,city);

	WriteFile(infos,buf2,strlen(buf2),&NBW,NULL);

	CloseHandle(infos);	
	
	printf("\n\nEverything is done now!\n\n");
	printf("Type \"yes\" to delete the files now or hit\nreturn to exit: ");
	
	putc('\0',stdin);
	gets(buf);
	if (strcmp(buf,"yes")!=0)
		return 0;
		
	sprintf(pattern,"%s\\stadimp\\stadium\\*",gdbDir);
	hff = FindFirstFile(pattern, &fData);
	if (hff != INVALID_HANDLE_VALUE) {
        while(true)
        {
        	if (!(fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                if (fData.cFileName[0]!='.') {
		        	ZeroMemory(ext,10);
		        	strcpy(ext,fData.cFileName+strlen(fData.cFileName)-4);
		        	
		        	if (strcmp(ext,".bin")==0 || strcmp(ext,".str")==0) {
		        		sprintf(buf,"%s\\stadimp\\stadium\\%s",gdbDir,fData.cFileName);
		        		DeleteFile(buf);
					};
				};
            // proceed to next file
            if (!FindNextFile(hff, &fData)) break;
        }
        FindClose(hff);
    }
    
	sprintf(buf,"%s\\stadimp\\adboards.bin",gdbDir);
	DeleteFile(buf);

	printf("\nFiles have been deleted!\n\n");
	
	system("PAUSE");
	return 0;
}

