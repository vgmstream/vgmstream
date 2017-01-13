#include <string.h>
#include "header.h"
#include "vgmstream.h"
#include "streamfile.h"
#include "streamtypes.h"
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
    int buffer_size = STREAMFILE_DEFAULT_BUFFER_SIZE;

#ifdef VGM_USE_FFMPEG
    if (vgmstream->coding_type == coding_FFmpeg) /* not needed */
        return 1;
#endif

    /* minor optimizations */
    if (vgmstream->layout_type == layout_interleave
            &&vgmstream->interleave_block_size > 0
            && vgmstream->interleave_block_size > buffer_size) {
        buffer_size = vgmstream->interleave_block_size;
    }

    if (buffer_size > 0x8000) {
        buffer_size = 0x8000;
        /* todo if interleave is big enough open one streamfile per channel so each uses it's own buffer */
    }


    streamFile->get_name(streamFile,filename,sizeof(filename));
    /* open the file for reading by each channel */
    {
        file = streamFile->open(streamFile,filename,buffer_size);
        if (!file) return 0;

        for (ch=0; ch < vgmstream->channels; ch++) {

            vgmstream->ch[ch].streamfile = file;

            if (vgmstream->layout_type == layout_none
#ifdef VGM_USE_MPEG
                    || vgmstream->layout_type == layout_mpeg //todo simplify using flag "start offset"
#endif
                    ) { /* no appreciable difference for mpeg */
                /* for some codecs like IMA where channels work with the same bytes */
                vgmstream->ch[ch].channel_start_offset =
                        vgmstream->ch[ch].offset = start_offset;
            }
            else {
                vgmstream->ch[ch].channel_start_offset =
                        vgmstream->ch[ch].offset = start_offset
                        + vgmstream->interleave_block_size*ch;
            }
        }
    }


    return 1;
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
