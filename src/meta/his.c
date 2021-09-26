#include "meta.h"
#include "../coding/coding.h"


/* HIS - Her Interactive games [Nancy Drew series (PC)] */
VGMSTREAM * init_vgmstream_his(STREAMFILE *sf) {
    VGMSTREAM * vgmstream = NULL;
    int channels, loop_flag = 0, bps, sample_rate, num_samples, version;
    off_t start_offset;


    /* checks */
    if (!is_id32be(0x00,sf, "Her ") &&
        !is_id32be(0x00,sf, "HIS\0"))
        goto fail;
    if (!check_extensions(sf, "his"))
        goto fail;

    if (is_id32be(0x00,sf, "Her ")) { /* "Her Interactive Sound\x1a" */
        /* Nancy Drew: Secrets Can Kill (PC) */
        version = 0;
        channels = read_u16le(0x16,sf);
        sample_rate = read_u32le(0x18,sf);
        /* 0x1c: bitrate */
        /* 0x20: block size */
        bps = read_u16le(0x22,sf);

        if (!is_id32be(0x24,sf, "data"))
            goto fail;
        num_samples = pcm_bytes_to_samples(read_u32le(0x28,sf), channels, bps);

        start_offset = 0x2c;
    }
    else if (is_id32be(0x00,sf, "HIS\0")) {
        /* most(?) others */
        version = read_u32le(0x04,sf);
        /* 0x08: codec */
        channels = read_u16le(0x0a,sf);
        sample_rate = read_u32le(0x0c,sf);
        /* 0x10: bitrate */
        /* 0x14: block size */
        bps = read_u16le(0x16,sf);

        num_samples = pcm_bytes_to_samples(read_u32le(0x18,sf), channels, bps); /* true even for Ogg */

        /* later games use "OggS" */
        if (version == 1)
            start_offset = 0x1c; /* Nancy Drew: The Final Scene (PC) */
        else if (version == 2 && is_id32be(0x1e,sf, "OggS"))
            start_offset = 0x1e; /* Nancy Drew: The Haunted Carousel (PC) */
        else if (version == 2 && is_id32be(0x20,sf, "OggS"))
            start_offset = 0x20; /* Nancy Drew: The Silent Spy (PC) */
        else
            goto fail;
    }
    else {
        goto fail;
    }


    if (version == 2) {
        ogg_vorbis_meta_info_t ovmi = {0};

        ovmi.meta_type = meta_HIS;
        return init_vgmstream_ogg_vorbis_config(sf, start_offset, &ovmi);
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_HIS;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    switch (bps) {
        case 8:
            vgmstream->coding_type = coding_PCM8_U;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x01;
            break;
        case 16:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;
            break;
        default:
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
