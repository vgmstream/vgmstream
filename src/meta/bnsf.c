#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* BNSF - Namco Bandai's Bandai Namco Sound Format/File [Tales of Graces (Wii), Tales of Berseria (PS4)] */
VGMSTREAM * init_vgmstream_bnsf(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0, first_offset = 0x0C;
    int loop_flag = 0, channel_count = 0, sample_rate;
    int num_samples, loop_start = 0, loop_end = 0, loop_adjust, block_samples;
    uint32_t codec, subcodec = 0;
    size_t bnsf_size, sdat_size, block_size;
    off_t loop_chunk = 0, sfmt_chunk, sdat_chunk;


    /* checks */
    if (!check_extensions(streamFile,"bnsf"))
        goto fail;
    if (read_32bitBE(0,streamFile) != 0x424E5346) /* "BNSF" */
        goto fail;

    codec = read_32bitBE(0x08,streamFile);

    /* check file size (siren22 uses full size) */
    bnsf_size = read_32bitBE(0x04,streamFile);
    if (bnsf_size + (codec == 0x49533232 ? 0x00 : 0x08) != get_streamfile_size(streamFile))
        goto fail;

    if (!find_chunk_be(streamFile, 0x73666d74,first_offset,0, &sfmt_chunk,NULL)) /* "sfmt" */
        goto fail;
    if (!find_chunk_be(streamFile, 0x73646174,first_offset,0, &sdat_chunk,&sdat_size)) /* "sdat" */
        goto fail;
    if ( find_chunk_be(streamFile, 0x6C6F6F70,first_offset,0, &loop_chunk,NULL)) { /* "loop" */
        loop_flag = 1;
        loop_start = read_32bitBE(loop_chunk+0x00, streamFile); /* block-aligned */
        loop_end   = read_32bitBE(loop_chunk+0x04,streamFile) + 1;
    }

    subcodec      = read_16bitBE(sfmt_chunk+0x00,streamFile);
    channel_count = read_16bitBE(sfmt_chunk+0x02,streamFile);
    sample_rate   = read_32bitBE(sfmt_chunk+0x04,streamFile);
    num_samples   = read_32bitBE(sfmt_chunk+0x08,streamFile);
    loop_adjust   = read_32bitBE(sfmt_chunk+0x0c,streamFile); /* 0 if no looping */
    block_size    = read_16bitBE(sfmt_chunk+0x10,streamFile);
    block_samples = read_16bitBE(sfmt_chunk+0x12,streamFile);
    //max_samples = sdat_size / block_size * block_samples /* num_samples is smaller */

    start_offset = sdat_chunk;

    /* without adjust some files have a small pop when looping */
    if (loop_adjust >= block_samples)
        goto fail; /* shouldn't happen, plus decoder can't handle it */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start + loop_adjust;
    vgmstream->loop_end_sample = loop_end;

    vgmstream->meta_type = meta_BNSF;
    vgmstream->layout_type = layout_interleave;
    vgmstream->interleave_block_size = block_size/channel_count;

    /* Late IS14 voice/ambient files use subcodec 0x02 [Tales of Zestiria (PS3/PC), The Idolm@ster 2 (PS3)].
     * Bitstream looks modified (most noticeable in silent frames), probably not encrypted, still 1ch frames */
    if (subcodec != 0)
        goto fail;

    switch (codec) {
#ifdef VGM_USE_G7221
        case 0x49533134: /* "IS14" */
            vgmstream->coding_type = coding_G7221C;
            vgmstream->codec_data = init_g7221(vgmstream->channels, vgmstream->interleave_block_size);
            if (!vgmstream->codec_data) goto fail;

            break;
#endif
#ifdef VGM_USE_G719
        case 0x49533232: /* "IS22" */
            vgmstream->coding_type = coding_G719;
            vgmstream->codec_data = init_g719(vgmstream->channels, vgmstream->interleave_block_size);
            if (!vgmstream->codec_data) goto fail;

            break;
#endif
        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
