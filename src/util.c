#include <string.h>
#include "util.h"
#include "streamtypes.h"

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

void put_16bitLE(uint8_t * buf, int16_t i) {
    buf[0] = i;
    buf[1] = i >> 8;
}

void put_32bitLE(uint8_t * buf, int32_t i) {
    buf[0] = i;
    buf[1] = i >> 8;
    buf[2] = i >> 16;
    buf[3] = i >> 24;
}

/* make a header for PCM .wav */
/* buffer must be 0x2c bytes */
void make_wav_header(uint8_t * buf, int32_t sample_count, int32_t sample_rate, int channels) {
    size_t bytecount;

    bytecount = sample_count*channels*sizeof(sample);

    /* RIFF header */
    memcpy(buf+0, "RIFF", 4);
    /* size of RIFF */
    put_32bitLE(buf+4, bytecount+0x2c-8);

    /* WAVE header */
    memcpy(buf+8, "WAVE", 4);

    /* WAVE fmt chunk */
    memcpy(buf+0xc, "fmt ", 4);
    /* size of WAVE fmt chunk */
    put_32bitLE(buf+0x10, 0x10);

    /* compression code 1=PCM */
    put_16bitLE(buf+0x14, 1);

    /* channel count */
    put_16bitLE(buf+0x16, channels);

    /* sample rate */
    put_32bitLE(buf+0x18, sample_rate);

    /* bytes per second */
    put_32bitLE(buf+0x1c, sample_rate*channels*sizeof(sample));

    /* block align */
    put_16bitLE(buf+0x20, channels*sizeof(sample));

    /* significant bits per sample */
    put_16bitLE(buf+0x22, sizeof(sample)*8);

    /* PCM has no extra format bytes, so we don't even need to specify a count */

    /* WAVE data chunk */
    memcpy(buf+0x24, "data", 4);
    /* size of WAVE data chunk */
    put_32bitLE(buf+0x28, bytecount);
}
