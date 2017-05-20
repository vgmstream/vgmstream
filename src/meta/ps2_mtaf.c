#include "meta.h"
#include "../util.h"


/* MTAF - found in Metal Gear Solid 3: Snake Eater (Subsistence and HD too) */
VGMSTREAM * init_vgmstream_ps2_mtaf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count;
    int32_t loop_start, loop_end;


    /* check extension */
    if ( !check_extensions(streamFile,"mtaf"))
        goto fail;

    /* base header */
    if (read_32bitBE(0x00, streamFile) != 0x4d544146) /* "MTAF" */
        goto fail;
    /* 0x04(4): pseudo file size (close but smaller) */
    /* 0x08(4): version? (0),  0x0c(20): null,  0x30(32): some kind of id or config? */


    /* HEAD chunk */
    if (read_32bitBE(0x40, streamFile) != 0x48454144) /* "HEAD" */
        goto fail;
    if (read_32bitLE(0x44, streamFile) != 0xB0) /* HEAD size */
        goto fail;


    /* 0x48(4): null,  0x4c: usually channel count (sometimes 0x10 with 2ch),  0x50(4): 0x7F (vol?),  0x54(2): 0x40 (pan?)  */
    channel_count = 2 * read_8bit(0x61, streamFile); /* 0x60(4): full block size (0x110 * channels), but this works */
    /* 0x70(4): ? (00/05/07),  0x80 .. 0xf8: null */

    loop_start = read_32bitLE(0x58, streamFile);
    loop_end   = read_32bitLE(0x5c, streamFile);
    loop_flag = (loop_start != loop_end);

    /* check loop points vs frame counts */
    if (loop_start/0x100 != read_32bitLE(0x64, streamFile) ||
        loop_end  /0x100 != read_32bitLE(0x68, streamFile) ) {
        VGM_LOG("MTAF: wrong loop points\n");
        goto fail;
    }

    /* TRKP chunks (x16) */
    /* just seem to contain pan/vol stuff (0x7f/0x40), one TRKP with data per channel and the rest fixed values */

    /* DATA chunk */
    if (read_32bitBE(0x7f8, streamFile) != 0x44415441) /* "DATA" */
        goto fail;
    /* 0x7fc: data size (without blocks in case of blocked layout) */

    /* without blocks it should start with 0x00000100 ("frame 1 from track 0") */
    //is_blocked = read_32bitLE(0x800,streamFile) != 0x00000100 && read_32bitLE(0x810,streamFile) == 0x00000100;


    start_offset = 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = 48000; /* always */
    vgmstream->num_samples = loop_end;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_MTAF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = 0x110/2; /* kinda hacky for MTAF track layout */
    vgmstream->meta_type = meta_PS2_MTAF;


    /* open the file for reading, in a specific way */
    {
        int i;
        char filename[PATH_LIMIT];

        streamFile->get_name(streamFile,filename,sizeof(filename));
        for (i = 0; i < channel_count; i++) {
            STREAMFILE * file = streamFile->open(streamFile,filename,vgmstream->interleave_block_size);
            if (!file) goto fail;
            vgmstream->ch[i].streamfile = file;
            vgmstream->ch[i].channel_start_offset = vgmstream->ch[i].offset = start_offset + vgmstream->interleave_block_size*2*(i/2);
        }
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
