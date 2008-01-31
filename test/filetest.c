#include <stdio.h>
#include "streamfile.h"

char buf[0x1002];

int main(void) {
    STREAMFILE * infile;
    FILE * outfile;
    size_t filesize,i;

    infile = open_streamfile("bob.bin");
    if (!infile) {
       printf("failed to open\n");
       return 1;
    }

    outfile = fopen("fred.bin","wb");

    filesize = get_streamfile_size(infile);

    for (i=0;i<filesize;i+=0x1002) {
        size_t bytes_read = read_streamfile(buf,i,0x1002,infile);

        fwrite(buf,1,bytes_read,outfile);

        if (bytes_read != 0x1002) {
            if (bytes_read+i==filesize) break;
            printf("error, short read\n");
            break;
        }
    }

    fclose(outfile);

    close_streamfile(infile);

    return 0;
}
