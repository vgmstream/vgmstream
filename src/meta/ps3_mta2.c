#include "meta.h"
#include "../util.h"


/* MTA2 - found in Metal Gear Solid 4 */
VGMSTREAM * init_vgmstream_ps3_mta2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t header_offset, start_offset;
    int loop_flag, channel_count, sample_rate; //block_offset;
    int32_t loop_start, loop_end;


    /* check extension */
    /* .mta2: normal file, .bgm: mta2 with block layout, .dbm: iPod metadata + block layout mta2 */
    if ( !check_extensions(streamFile,"mta2,bgm,dbm"))
        goto fail;

    /* base header (everything is very similar to MGS3's MTAF but BE) */
    if (read_32bitBE(0x00,streamFile) == 0x4d544132) {  /* "MTA2" (.mta) */
        //block_offset = 0;
        header_offset = 0x00;
    } else if (read_32bitBE(0x20,streamFile) == 0x4d544132) { /* "MTA2" @ 0x20 (.bgm) */
        //block_offset = 0x10;
        header_offset = 0x20;
    } else if (read_32bitBE(0x00, streamFile) == 0x444C424D
            && read_32bitBE(0x820,streamFile) == 0x4d544132) { /* "DLBM" + "MTA2" @ 0x820 (.dbm) */
        //block_offset = 0x810;
        header_offset = 0x820;
    } else {
        goto fail;
    }
    /* 0x04(4): file size -4-4 (not including block headers in case of block layout) */
    /* 0x08(4): version? (1),  0x0c(52): null */


    /* HEAD chunk */
    if (read_32bitBE(header_offset+0x40, streamFile) != 0x48454144) /* "HEAD" */
        goto fail;
    if (read_32bitBE(header_offset+0x44, streamFile) != 0xB0) /* HEAD size */
        goto fail;



    /* 0x48(4): null,  0x4c: ? (0x10),   0x50(4): 0x7F (vol?),  0x54(2): 0x40 (pan?) */
    channel_count = read_16bitBE(header_offset+0x56, streamFile); /* counting all tracks */
    /* 0x60(4): full block size (0x110 * channels), indirectly channels_per_track = channels / (block_size / 0x110) */
    /* 0x80 .. 0xf8: null */

    loop_start = read_32bitBE(header_offset+0x58, streamFile);
    loop_end   = read_32bitBE(header_offset+0x5c, streamFile);
    loop_flag = (loop_start != loop_end); /* also flag possibly @ 0x73 */
#if 0
    /* those values look like some kind of loop offsets */
    if (loop_start/0x100 != read_32bitBE(header_offset+0x68, streamFile) ||
        loop_end  /0x100 != read_32bitBE(header_offset+0x6C, streamFile) ) {
        VGM_LOG("MTA2: wrong loop points\n");
        goto fail;
    }
#endif

    sample_rate = read_32bitBE(header_offset+0x7c, streamFile);
    if (sample_rate) { /* sample rate in 32b float (WHY?) typically 48000.0 */
        float sample_float;
        memcpy(&sample_float, &sample_rate, 4);
        sample_rate = (int)sample_float;
    } else { /* default when not specified (most of the time) */
        sample_rate = 48000;
    }


    /* TRKP chunks (x16) */
    /* just seem to contain pan/vol stuff (0x7f/0x40), TRKP per track (sometimes +1 main track?) */
    /* there is channel layout bitmask @ 0x0f (ex. 1ch = 0x04, 3ch = 0x07, 4ch = 0x33, 6ch = 0x3f), surely:
     * FRONT_L = 0x01,  FRONT_R = 0x02,  FRONT_M = 0x04, BACK_L  = 0x08,  BACK_R  = 0x10,  BACK_M  = 0x20 */

    /* DATA chunk */
    if (read_32bitBE(header_offset+0x7f8, streamFile) != 0x44415441) // "DATA"
        goto fail;
    /* 0x7fc: data size (without blocks in case of blocked layout) */

    start_offset = header_offset + 0x800;

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = loop_end;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_MTA2;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_PS3_MTA2;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
