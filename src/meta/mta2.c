#include "meta.h"
#include "mta2_streamfile.h"


/* MTA2 - found in Metal Gear Solid 4 (PS3) */
VGMSTREAM * init_vgmstream_mta2(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate;
    int32_t loop_start, loop_end;


    /* checks */
    if ( !check_extensions(streamFile,"mta2"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x4d544132) /* "MTA2" */
        goto fail;
    /* allow truncated files for now? */
    //if (read_32bitBE(0x04, streamFile) + 0x08 != get_streamfile_size(streamFile))
    //    goto fail;

    /* base header (everything is very similar to MGS3's MTAF but BE) */
    /* 0x08(4): version? (1),  0x0c(52): null */

    /* HEAD chunk */
    if (read_32bitBE(0x40, streamFile) != 0x48454144) /* "HEAD" */
        goto fail;
    if (read_32bitBE(0x44, streamFile) != 0xB0) /* HEAD size */
        goto fail;

    /* 0x48(4): null,  0x4c: ? (0x10),   0x50(4): 0x7F (vol?),  0x54(2): 0x40 (pan?) */
    channel_count = read_16bitBE(0x56, streamFile); /* counting all tracks */
    /* 0x60(4): full block size (0x110 * channels), indirectly channels_per_track = channels / (block_size / 0x110) */
    /* 0x80 .. 0xf8: null */

    loop_start = read_32bitBE(0x58, streamFile);
    loop_end   = read_32bitBE(0x5c, streamFile);
    loop_flag = (loop_start != loop_end); /* also flag possibly @ 0x73 */
#if 0
    /* those values look like some kind of loop offsets */
    if (loop_start/0x100 != read_32bitBE(0x68, streamFile) ||
        loop_end  /0x100 != read_32bitBE(0x6C, streamFile) ) {
        VGM_LOG("MTA2: wrong loop points\n");
        goto fail;
    }
#endif

    sample_rate = (int)read_f32be(0x7c, streamFile); /* sample rate in 32b float (WHY?) typically 48000.0 */
    if (sample_rate == 0)
        sample_rate = 48000; /* default when not specified (most of the time) */


    /* TRKP chunks (x16) */
    /* just seem to contain pan/vol stuff (0x7f/0x40), TRKP per track (sometimes +1 main track?) */
    /* there is channel layout bitmask at 0x0f (ex. 1ch = 0x04, 3ch = 0x07, 4ch = 0x33, 6ch = 0x3f), surely:
     * FL 0x01, FR 0x02, FC = 0x04, BL = 0x08, BR = 0x10, BC = 0x20 */

    start_offset = 0x800;

    /* DATA chunk */
    if (read_32bitBE(0x7f8, streamFile) != 0x44415441) // "DATA"
        goto fail;
    //if (read_32bitBE(0x7fc, streamFile) + start_offset != get_streamfile_size(streamFile))
    //    goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = loop_end;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->coding_type = coding_MTA2;
    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_MTA2;

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* ****************************************************************************** */

/* MTA2 in containers */
VGMSTREAM * init_vgmstream_mta2_container(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t subfile_offset;


    /* checks */
    /* .dbm: iPod metadata + mta2 with KCEJ blocks, .bgm: mta2 with KCEJ blocks (fake?) */
    if ( !check_extensions(streamFile,"dbm,bgm,mta2"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) == 0x444C424D) { /* "DLBM" */
        subfile_offset = 0x800;
    }
    else if (read_32bitBE(0x00,streamFile) == 0x00000010) {
        subfile_offset = 0x00;
    }
    else {
        goto fail;
    }
    /* subfile size is implicit in KCEJ blocks */

    temp_streamFile = setup_mta2_streamfile(streamFile, subfile_offset, 1, "mta2");
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_mta2(temp_streamFile);
    close_streamfile(temp_streamFile);

    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}
