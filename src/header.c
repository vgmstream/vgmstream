#include <string.h>
#include "vgmstream.h"
#include "streamfile.h"
#include "util.h"

/**
 * checks if the stream filename is one of the extensions (comma-separated, ex. "adx" or "adx,aix")
 *
 * returns 0 on failure
 */
int header_check_extensions(STREAMFILE *streamFile, const char * cmpexts) {
    char filename[PATH_LIMIT];
    const char * ext = NULL;
    const char * cmpext = NULL;
    size_t ext_len;

    streamFile->get_name(streamFile,filename,sizeof(filename));
    ext = filename_extension(filename);
    ext_len = strlen(ext);

    cmpext = cmpexts;
    do {
        if (strncasecmp(ext,cmpext, ext_len)==0 )
            return 1;
        cmpext = strstr(cmpext, ",");
        if (cmpext != NULL)
            cmpext = cmpext + 1; /* skip comma */
    } while (cmpext != NULL);

    return 0;
}


/**
 * opens a stream at offset
 *
 * returns 0 on failure
 */
int header_open_stream(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t start_offset) {
    STREAMFILE * file;
    char filename[PATH_LIMIT];
    int ch;

    streamFile->get_name(streamFile,filename,sizeof(filename));


#if 0
    /* there is no appreciable difference using this */
    if (vgmstream->layout_type == layout_mpeg) {
        for (ch=0; ch < vgmstream->channels; ch++) {
            vgmstream->ch[ch].streamfile = streamFile->open(streamFile,filename,MPEG_BUFFER_SIZE);
                vgmstream->ch[ch].channel_start_offset= vgmstream->ch[ch].offset=start_offset;
        }
    }
    else
#endif
    {
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) return 0;

        for (ch=0; ch < vgmstream->channels; ch++) {
            vgmstream->ch[ch].streamfile = file;

            if (vgmstream->coding_type == coding_MS_IMA
                    || vgmstream->coding_type == coding_XBOX) { /*todo not needed?*/
                /* both IMA channels work with same bytes */
                vgmstream->ch[ch].channel_start_offset =
                        vgmstream->ch[ch].offset = start_offset;
            }
            else {
                vgmstream->ch[ch].channel_start_offset =
                        vgmstream->ch[ch].offset = start_offset
                        + vgmstream->interleave_block_size*ch;
            }
            /*todo if interleave and ch > 0 and layout none don't change offset? */
        }
    }


    return 1;
}


/**
 * Copies a XMA2 riff to buf
 *
 * returns number of bytes in buf or -1 when buf is not big enough
 */
int header_make_riff_xma2(uint8_t * buf, size_t buf_size, size_t sample_count, size_t data_size, int channels, int sample_rate, int block_count, int block_size) {
    uint16_t codec_XMA2 = 0x0166;
    size_t riff_size = 4+4+ 4 + 0x3c + 4+4;
    size_t bytecount;
    uint32_t streams = 0;
    uint32_t speakers = 0; /* see audiodefs.h */

    if (buf_size < riff_size)
        return -1;

    bytecount = sample_count * channels * sizeof(sample);

    /* untested (no support for > 2ch xma for now) */
    switch (channels) {
        case 1:
            streams = 1;
            speakers = 0x00000004; /* FC */
            break;
        case 2:
            streams = 1;
            speakers = 0x00000001 | 0x00000002; /* FL FR */
            break;
        case 3:
            streams = 3;
            speakers = 0x00000001 | 0x00000002 | 0x00000004; /* FL FC FR */
            break;
        case 4:
            streams = 2;
            speakers = 0x00000001 | 0x00000002 | 0x00000010 | 0x00000020; /* FL FR BL BR */
            break;
        case 5:
            streams = 3;
            speakers = 0x00000001 | 0x00000002 | 0x00000010 | 0x00000020 | 0x00000004; /* FL C FR BL BR*/
            break;
        case 6:
            streams = 3;
            speakers = 0x00000001 | 0x00000002 | 0x00000010 | 0x00000020 | 0x00000200 | 0x00000400; /* FL FR BL BR SL SR */
            break;
         default:
            streams = 1;
            speakers = 0x80000000;
            break;
    }

    /*memset(buf,0, sizeof(uint8_t) * fmt_size);*/

    memcpy(buf+0x00, "RIFF", 4);
    put_32bitLE(buf+0x04, (int32_t)(riff_size-4-4 + data_size)); /* riff size */
    memcpy(buf+0x08, "WAVE", 4);

    memcpy(buf+0x0c, "fmt ", 4);
    put_32bitLE(buf+0x10, 0x34);/*size*/
    put_16bitLE(buf+0x14, codec_XMA2);
    put_16bitLE(buf+0x16, channels);
    put_32bitLE(buf+0x18, sample_rate);
    put_32bitLE(buf+0x1c, sample_rate*channels*sizeof(sample)); /* average bytes per second (wrong) */
    put_16bitLE(buf+0x20, (int16_t)(channels*sizeof(sample))); /* block align */
    put_16bitLE(buf+0x22, sizeof(sample)*8); /* bits per sample */

    put_16bitLE(buf+0x24, 0x22); /* extra data size */
    put_16bitLE(buf+0x26, streams); /* number of streams */
    put_32bitLE(buf+0x28, speakers); /* speaker position  */
    put_32bitLE(buf+0x2c, bytecount); /* PCM samples */
    put_32bitLE(buf+0x30, block_size); /* XMA block size */
    /* (looping values not set, expected to be handled externally) */
    put_32bitLE(buf+0x34, 0); /* play begin */
    put_32bitLE(buf+0x38, 0); /* play length */
    put_32bitLE(buf+0x3c, 0); /* loop begin */
    put_32bitLE(buf+0x40, 0); /* loop length */
    put_8bit(buf+0x44, 0); /* loop count */
    put_8bit(buf+0x45, 4); /* encoder version */
    put_16bitLE(buf+0x46, block_count); /* blocks count = entried in seek table */

    memcpy(buf+0x48, "data", 4);
    put_32bitLE(buf+0x4c, data_size); /* data size */

    return riff_size;
}


/**
 * reads DSP coefs built in the streamfile
 */
void header_dsp_read_coefs_be(VGMSTREAM * vgmstream, STREAMFILE *streamFile, off_t offset, off_t spacing) {
    int ch, i;
    /* get ADPCM coefs */
    for (ch=0; ch < vgmstream->channels; ch++) {
        for (i=0; i < 16; i++) {
            vgmstream->ch[ch].adpcm_coef[i] =
                    read_16bitBE(offset + ch*spacing + i*2, streamFile);
        }
    }
}


#if 0
/**
 * Converts a data offset (without headers) to sample, so full datasize would be num_samples
 *
 * return -1 on failure
 */
int data_offset_to_samples(layout_t layout, int channels, size_t interleave, size_t data_offset) {
    // todo get samples per block
    // VAG: datasize * 28 / 16 / channels;
    // IMA: (datasize / 0x24 / channels) * ((0x24-4)*2);//0x24 = interleave?
    // DSP:  datasize / 8 / channel_count * 14;
}

#endif
