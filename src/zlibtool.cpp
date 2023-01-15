#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "soft\zlib123-dll\include\zlib.h"

#define BYTE unsigned char
#define UCHAR unsigned char

int main(int argc, char *argv[])
{
    if (argc<2) {
        printf("Usage: zlibtool <file-to-compress> [-d]\n");
        printf("\t-d: decompress (default is compress)\n");
        printf("\tresult of compression will have a .zlib extension\n");
        printf("\tresult of uncompression will have a .unzlib extension\n");
        return 0;
    }

    FILE* inf = fopen(argv[1],"rb");
    if (!inf) {
        fprintf(stderr,"Error: unable to open input file. Code=%d\n", errno);
        return 1;
    }
    struct stat st;
    fstat(_fileno(inf),&st);
    BYTE* src = (BYTE*)malloc(st.st_size);
    fread(src,st.st_size,1,inf);
    fclose(inf);

    uLongf destLen;
    BYTE* dest;
    char outname[256] = {0};
    if (argc<3) {
        destLen = st.st_size*5;
        dest = (BYTE*)malloc(destLen); // big buffer just in case;
        int retval = compress(dest,&destLen,src,st.st_size);
        if (retval != Z_OK) {
            fprintf(stderr,"compression failed. retval=%d\n", retval);
            free(src);
            free(dest);
            return 1;
        }
        sprintf(outname,"%s.zlib",argv[1]);

    } else if (strcmp(argv[2],"-d")==0) {
        destLen = *(uLongf*)(src + 0x08);
        dest = (BYTE*)malloc(destLen); // big enough buffer

        int retval = uncompress(dest,&destLen,src+0x20,st.st_size-0x20);
        if (retval != Z_OK) {
            fprintf(stderr,"decompression failed. retval=%d\n", retval);
            free(src);
            free(dest);
            return 1;
        }
        sprintf(outname,"%s.unzlib",argv[1]);

    } else if (strcmp(argv[2],"-draw")==0) {
        destLen = st.st_size*5;
        dest = (BYTE*)malloc(destLen); // big enough buffer

        int retval = uncompress(dest,&destLen,src,st.st_size);
        if (retval != Z_OK) {
            fprintf(stderr,"decompression failed. retval=%d\n", retval);
            free(src);
            free(dest);
            return 1;
        }
        sprintf(outname,"%s.unzlib",argv[1]);
    }

    FILE* outf = fopen(outname,"wb");
    if (!outf) {
        fprintf(stderr,"Error: unable to open output file. Code=%d\n", errno);
        free(src);
        free(dest);
        return 1;
    }
    if (argc<3) {
        fwrite("\x00\x01\x01WESYS",8,1,outf);
        fwrite(&destLen,sizeof(uLongf),1,outf);
        fwrite(&st.st_size,sizeof(uLongf),1,outf);
    }
    fwrite(dest,destLen,1,outf);
    fclose(outf);
    free(src);
    free(dest);
    return 0;
}

