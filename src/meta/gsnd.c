#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/chunks.h"

/* GSP+GSB - from Tecmo's Super Swing Golf 1 & 2 (Wii), Quantum Theory (PS3/X360) */
VGMSTREAM* init_vgmstream_gsnd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    int loop_flag, channel_count, sample_rate, num_samples, loop_start, loop_end;
    off_t start_offset, chunk_offset, first_offset;
    size_t data_size;
    int codec;

    /* checks */
    if (!is_id32be(0x00,sf, "GSND"))
        return NULL;

    if (!check_extensions(sf,"gsp"))
        return NULL;

    sb = open_streamfile_by_ext(sf, "gsb");
    if (!sb) goto fail;

    /* 0x04: version? */
    /* 0x08: 1? */
    /* 0x0c: 0? */
    first_offset = read_32bitBE(0x10,sf); /* usually 0x14 */

    if (!find_chunk_be(sf, get_id32be("HEAD"),first_offset,1, &chunk_offset,NULL))
        goto fail;
    /* 0x00: header size */
    /* 0x04: num_chunks */

    if (!find_chunk_be(sf, get_id32be("DATA"),first_offset,1, &chunk_offset,NULL))
        goto fail;
    data_size       = read_32bitBE(chunk_offset + 0x00,sf);
    codec           = read_32bitBE(chunk_offset + 0x04,sf);
    sample_rate     = read_32bitBE(chunk_offset + 0x08,sf);
    /* 0x0c: always 16? */
    channel_count   = read_16bitBE(chunk_offset + 0x0e,sf);
    /* 0x10: always 0? */
    num_samples     = read_32bitBE(chunk_offset + 0x14,sf);
    /* 0x18: always 0? */
    /* 0x1c: unk (varies with codec_id) */

    if (!find_chunk_be(sf, get_id32be("BSIC"),first_offset,1, &chunk_offset,NULL))
        goto fail;
    /* 0x00/0x04: probably volume/pan/etc floats (1.0) */
    /* 0x08: null? */
    loop_flag   = read_8bit(chunk_offset+0x0c,sf);
    loop_start  = read_32bitBE(chunk_offset+0x10,sf);
    loop_end    = read_32bitBE(chunk_offset+0x14,sf);

    //if (!find_chunk_be(streamHeader, get_id32be("NAME"),first_offset,1, &chunk_offset,NULL))
    //    goto fail;
    /* 0x00: name_size */
    /* 0x04+: name (same as filename) */


    start_offset = 0x00;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_GSND;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    switch (codec) {
        case 0x04: { /* DSP [Super Swing Golf (Wii)] */
            size_t block_header_size;
            size_t num_blocks;

            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_blocked_gsnd;

            if (!find_chunk_be(sf, get_id32be("GCEX"),first_offset,1, &chunk_offset,NULL))
                goto fail;

            //vgmstream->current_block_size = read_32bitBE(chunk_offset+0x00,streamHeader);
            block_header_size = read_32bitBE(chunk_offset+0x04,sf);
            num_blocks = read_32bitBE(chunk_offset+0x08,sf);
            vgmstream->num_samples = (data_size - block_header_size * num_blocks) / 8 / vgmstream->channels * 14;
            /* 0x0c+: unk */

            dsp_read_coefs_be(vgmstream, sf, chunk_offset+0x18, 0x30);
            break;
        }
#ifdef VGM_USE_FFMPEG
        case 0x08: { /* ATRAC3 [Quantum Theory (PS3)] */
            int block_align, encoder_delay;

            block_align   = 0x98 * vgmstream->channels;
            encoder_delay = 1024 + 69*2; /* observed default, matches XMA (needed as many files start with garbage) */
            vgmstream->num_samples = atrac3_bytes_to_samples(data_size, block_align) - encoder_delay;
            /* fix num_samples as header samples seem to be modified to match altered (49999/48001) sample rates somehow */

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sb, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* set offset samples (offset 0 jumps to sample 0 > pre-applied delay, and offset end loops after sample end > adjusted delay) */
            vgmstream->loop_start_sample = atrac3_bytes_to_samples(loop_start, block_align); //- encoder_delay
            vgmstream->loop_end_sample = atrac3_bytes_to_samples(loop_end, block_align) - encoder_delay;
            break;
        }

        case 0x09: { /* XMA2 [Quantum Theory (X360)] */
            if (!find_chunk_be(sf, get_id32be("XMEX"),first_offset,1, &chunk_offset,NULL)) /* "XMEX" */
                goto fail;
            /* 0x00: fmt0x166 header (BE) */
            /* 0x34: seek table */

            vgmstream->codec_data = init_ffmpeg_xma_chunk_split(sf, sb, start_offset, data_size, chunk_offset, 0x34);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            xma_fix_raw_samples(vgmstream, sb, start_offset,data_size, 0, 0,0); /* samples are ok */
            break;
        }
#endif
        default:
            goto fail;
    }


    if (!vgmstream_open_stream(vgmstream, sb, start_offset))
        goto fail;
    close_streamfile(sb);
    return vgmstream;

fail:
    close_streamfile(sb);
    close_vgmstream(vgmstream);
    return NULL;
}
