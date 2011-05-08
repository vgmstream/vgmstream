//#include <stdlib.h>
#include "meta.h"
#include "../util.h"

/* MTAF (Metal Gear Solid 3: Snake Eater) */
VGMSTREAM * init_vgmstream_ps2_mtaf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset;

    int stream_count;
    int loop_flag = 1;
    int channel_count;
    int32_t loop_start;
    int32_t loop_end;

    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("mtaf",filename_extension(filename))) goto fail;

    /* check header */

    // master MTAF header (mostly useless)

    if (read_32bitBE(0, streamFile) != 0x4d544146) // "MTAF"
    {
        //fprintf(stderr, "no MTAF header at 0x%08lx\n", cur_off);
        goto fail;
    }

    //const uint32_t pseudo_size = readint32(&mtaf_header_buf[4]);

    // check the rest is clear
    for (i = 0x8; i < 0x20; i++)
    {
        if (read_8bit(i, streamFile) != 0)
        {
            //fprintf(stderr, "unexpected nonzero in MTAF header at 0x%08lx\n", cur_off+i);
            goto fail;
        }
    }

    // ignore the rest for now

    // HEAD chunk header

    if (read_32bitBE(0x40, streamFile) != 0x48454144) // "HEAD"
    {
                //fprintf(stderr, "no HEAD chunk at 0x%08lx\n", cur_off);
                goto fail;
    }

    {
        uint32_t mtaf_head_chunk_size = read_32bitLE(0x44, streamFile);
        if (mtaf_head_chunk_size != 0xB0)
        {
            //fprintf(stderr, "unexpected size for MTAF header at 0x%08lx\n", cur_off);
            goto fail;
        }
    }

    stream_count = read_8bit(0x61, streamFile);

    // check some standard stuff
    if (   0 != read_32bitLE(0x48, streamFile) ||
           0x7F != read_32bitLE(0x50, streamFile) ||
           0x40 != read_32bitLE(0x54, streamFile) ||
           0 != read_16bitLE(0x62, streamFile) ||
           0 != read_32bitLE(0x6c, streamFile)) // ||
        //5 != readint32(&mtaf_header_buf[0x68])) ||
        //(dc.streams==3 ? 12:0) != readint32(&mtaf_header_buf[0x7c]))
    {
        //fprintf(stderr, "unexpected header values at 0x%08lx\n", cur_off);
        goto fail;
    }

    // 0 streams should be impossible
    if (stream_count == 0)
    {
        //fprintf(stderr, "0 streams at 0x%08lx\n", cur_off);
        goto fail;
    }

    // check the other stream count indicator
    if (stream_count*0x10 != read_8bit(0x60, streamFile))
    {
        //fprintf(stderr, "secondary stream count mismatch at 0x%08lx\n", cur_off);
        goto fail;
    }

#if 0
    // maybe this is how to compute channels per stream?
    // check total channel count
    if (2*stream_count != read_32bitLE(0x4c, streamFile))
    {
        //fprintf(stderr, "total channel count does not match stream count at 0x%08lx\n", cur_off);
        goto fail;
    }
#endif

    // check loop points as frame counts
    if (read_32bitLE(0x64, streamFile) != read_32bitLE(0x58, streamFile)/0x100 ||
        read_32bitLE(0x68, streamFile) != read_32bitLE(0x5c, streamFile)/0x100)
    {
        //fprintf(stderr, "loop frame count mismatch at 0x%lx\n", cur_off);
        goto fail;
    }

    // check that rest is clear
    for (i = 0x78; i < 0xf8; i++)
    {
        if (read_8bit(i, streamFile) != 0)
        {
            //fprintf(stderr, "unexpected nonzero in HEAD chunk at 0x%lx\n", cur_off+i);
            goto fail;
        }
    }

    // check TRKP chunks
    for (i = 0; i < 16; i++)
    {
        if (read_32bitBE(0xf8+0x70*i, streamFile) != 0x54524b50 || // "TRKP"
            read_32bitLE(0xf8+0x70*i+4, streamFile) != 0x68)
        {
            //fprintf(stderr, "missing or unusual TRKP chunk #%d at 0x%lx\n", i, cur_off);
            goto fail;
        }
    }

    // check for grand finale, DATA
    if (read_32bitBE(0x7f8, streamFile) != 0x44415441) // "DATA"
    {
        //fprintf(stderr, "missing DATA header at 0x%lx\n", cur_off);
        goto fail;
    }

    start_offset = 0x800;

    // seems to always be the case
    channel_count = 2 * stream_count;

    loop_start = read_32bitLE(0x58, streamFile);
    loop_end = read_32bitLE(0x5c, streamFile);
    if (loop_start == loop_end) loop_flag = 0;

    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    // a guess
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = 48000;
    vgmstream->coding_type = coding_MTAF;
    vgmstream->num_samples = read_32bitLE(0x5c, streamFile);

    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->interleave_block_size = 0x110/2;

    vgmstream->layout_type = layout_interleave;
    vgmstream->meta_type = meta_PS2_MTAF;

    //const uint32_t pseudo_data_size = readint32(&mtaf_header_buf[4]);

    // TODO: first block

    /* open the file for reading */
    for (i = 0; i < channel_count; i++) {
        STREAMFILE * file = streamFile->open(streamFile,filename,vgmstream->interleave_block_size);
        if (!file) goto fail;
        vgmstream->ch[i].streamfile = file;
        vgmstream->ch[i].channel_start_offset = vgmstream->ch[i].offset = start_offset + vgmstream->interleave_block_size*2*(i/2);
    }

    return vgmstream;

fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}
