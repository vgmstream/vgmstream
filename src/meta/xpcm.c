#include "meta.h"
#include "../coding/coding.h"

/* XPCM - from Circus games [Eternal Fantasy (PC), D.C. White Season (PC)] */
VGMSTREAM* init_vgmstream_xpcm(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    size_t pcm_size;
    int loop_flag, channels, codec, flags, sample_rate;


    /* checks */
    if (!is_id32be(0x00,sf, "XPCM"))
        return NULL;
    if (!check_extensions(sf, "pcm"))
        return NULL;

    pcm_size    = read_u32le(0x04,sf);
    codec       = read_u8   (0x08, sf);
    flags       = read_u8   (0x09, sf);
    // 0x0a: always null
    // 0x0c: always 0x01 (PCM codec?)
    channels    = read_u16le(0x0e,sf);
    sample_rate = read_s32le(0x10,sf);
    // 0x14: average bitrate
    // 0x18: block size
    // 0x1a: output bps (16)
    start_offset = 0x1c; // compressed size in codec 0x01/03

    loop_flag  = 0;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_XPCM;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = pcm_size / sizeof(int16_t) / channels;

    switch(codec) {
        case 0x00:
            if (flags != 0) goto fail;
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;

        case 0x02:
            if (flags != 0) goto fail;
            vgmstream->coding_type = coding_CIRCUS_ADPCM;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x01;
            break;

        case 0x01: /* VQ + LZ (usually music) */
        case 0x03: /* VQ + deflate (usually sfx/voices) */
            vgmstream->codec_data = init_circus_vq(sf, 0x20, codec, flags);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_CIRCUS_VQ;
            vgmstream->layout_type = layout_none;

            /* not too sure about samples, since decompressed size isn't exact sometimes vs
             * total decompression, though it's what the code uses to alloc bufs (plus + 0x2000 leeway) */
            //vgmstream->num_samples = pcm_size / 0x2000 * 4064 / channel_count;
            //if (decompressed_size % 0x2000 != 0)
            //    vgmstream->num_samples += (pcm_size % 0x2000) / channel_count;
            break;

        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,sf,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
