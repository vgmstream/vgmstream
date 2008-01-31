#include <string.h>
#include "util.h"

int check_sample_rate(int32_t sr) {
    return !(sr<1000 || sr>96000);
}

const char * filename_extension(const char * filename) {
    const char * ext;

    /* You know what would be nice? strrchrnul().
     * Instead I have to do it myself. */
    ext = strrchr(filename,'.');
    if (ext==NULL) ext=filename+strlen(filename); /* point to null, i.e. an empty string for the extension */
    else ext=ext+1; /* skip the dot */

    return ext;
}

void interleave_channel(sample * outbuffer, sample * inbuffer, int32_t sample_count, int channel_count, int channel_number) {
    int32_t insample,outsample;

    if (channel_count==1) {
        memcpy(outbuffer,inbuffer,sizeof(sample)*sample_count);
        return;
    }

    for (insample=0,outsample=channel_number;insample<sample_count;insample++,outsample+=channel_count) {
        outbuffer[outsample]=inbuffer[insample];
    }
}

/* failed attempt at interleave in place */
/*
void interleave_stereo(sample * buffer, int32_t sample_count) {
    int32_t tomove, belongs;
    sample moving,temp;

    tomove = sample_count;
    moving = buffer[tomove];

    do {
        if (tomove<sample_count)
            belongs = tomove*2;
        else
            belongs = (tomove-sample_count)*2+1;

        printf("move %d to %d\n",tomove,belongs);

        temp = buffer[belongs];
        buffer[belongs] = moving;
        moving = temp;

        tomove = belongs;
    } while (tomove != sample_count);
}
*/
