#include "meta.h"
#include "../coding/coding.h"

/* VXN - from Gameloft mobile games */
VGMSTREAM* init_vgmstream_vxn(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    int loop_flag = 0, channel_count, codec, sample_rate, block_align, bits, num_samples;
    off_t start_offset, stream_offset, chunk_offset, first_offset = 0x00;
    size_t stream_size;
    int total_subsongs, target_subsong = sf->stream_index;

    /* checks */
    if (!check_extensions(sf,"vxn"))
        goto fail;

    if (read_u32be(0x00,sf) != 0x566F784E) /* "VoxN" */
        goto fail;
    if (read_u32le(0x10,sf) != get_streamfile_size(sf))
        goto fail;

    /* header is RIFF-like with many custom chunks */
    if (!find_chunk_le(sf, 0x41666D74,first_offset,0, &chunk_offset,NULL)) /* "Afmt" */
        goto fail;
    codec = read_u16le(chunk_offset+0x00, sf);
    channel_count = read_u16le(chunk_offset+0x02, sf);
    sample_rate = read_u32le(chunk_offset+0x04, sf);
    block_align = read_u16le(chunk_offset+0x08, sf);
    bits = read_16bitLE(chunk_offset+0x0a, sf);

    /* files are divided into segment subsongs, often a leadout and loop in that order
     * (the "Plst" and "Rule" chunks may have order info) */
    if (!find_chunk_le(sf, 0x5365676D,first_offset,0, &chunk_offset,NULL))  /* "Segm" */
        goto fail;
    total_subsongs = read_u32le(chunk_offset+0x00, sf);
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    stream_offset = read_u32le(chunk_offset+0x04 + (target_subsong-1)*0x18 + 0x00, sf);
    stream_size   = read_u32le(chunk_offset+0x04 + (target_subsong-1)*0x18 + 0x04, sf);
    num_samples   = read_u32le(chunk_offset+0x04 + (target_subsong-1)*0x18 + 0x08, sf);

    if (!find_chunk_le(sf, 0x44617461,first_offset,0, &chunk_offset,NULL)) /* "Data" */
        goto fail;
    start_offset = chunk_offset + stream_offset;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_VXN;

    switch (codec) {
        case 0x0001:    /* PCM */
            if (bits != 16) goto fail;

            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->interleave_block_size = block_align;
            vgmstream->layout_type = layout_interleave;
            break;

        case 0x0002:    /* MSADPCM (ex. Asphalt 7) */
            if (bits != 4) goto fail;

            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->frame_size = block_align;
            vgmstream->layout_type = layout_none;

            if (find_chunk_le(sf, 0x4D736165,first_offset,0, &chunk_offset,NULL)) { /* "Msae" */
                if (!msadpcm_check_coefs(sf, chunk_offset + 0x02))
                    goto fail;
            }
            break;

        case 0x0011:    /* MS-IMA (ex. Asphalt 6) */
            if (bits != 4 && bits != 16) goto fail; /* 16=common, 4=Asphalt Injection (Vita) */

            vgmstream->coding_type = coding_MS_IMA;
            vgmstream->interleave_block_size = block_align;
            vgmstream->layout_type = layout_none;
            break;

#ifdef VGM_USE_FFMPEG
        case 0x0800:    /* Musepack (ex. Asphalt Xtreme) */
            if (bits != -1) goto fail;

            vgmstream->codec_data = init_ffmpeg_offset(sf, start_offset,stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* unlike standard .mpc, .vxn has no seek table so no need to fix */
            //ffmpeg_set_force_seek(vgmstream->codec_data);
            break;
#endif

        default:
            VGM_LOG("VXN: unknown codec 0x%02x\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
