#include "meta.h"
#include "../coding/coding.h"


/* HIS - Her Interactive games [Nancy Drew series (PC)] */
VGMSTREAM * init_vgmstream_his(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int channel_count, loop_flag = 0, bps, sample_rate, num_samples, version;
    off_t start_offset;


    /* checks */
    if (!check_extensions(streamFile, "his"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) == 0x48657220) { /* "Her Interactive Sound\x1a" */
        /* Nancy Drew: Secrets Can Kill (PC) */
        version = 0;
        channel_count = read_16bitLE(0x16,streamFile);
        sample_rate = read_32bitLE(0x18,streamFile);
        /* 0x1c: bitrate */
        /* 0x20: block size */
        bps = read_16bitLE(0x22,streamFile);

        if (read_32bitBE(0x24,streamFile) != 0x64617461) /* "data" */
            goto fail;
        num_samples = pcm_bytes_to_samples(read_32bitLE(0x28,streamFile), channel_count, bps);

        start_offset = 0x2c;
    }
    else if (read_32bitBE(0x00,streamFile) == 0x48495300) { /* HIS\0 */
        /* most(?) others */
        version = read_32bitLE(0x04,streamFile);
        /* 0x08: codec */
        channel_count = read_16bitLE(0x0a,streamFile);
        sample_rate = read_32bitLE(0x0c,streamFile);
        /* 0x10: bitrate */
        /* 0x14: block size */
        bps = read_16bitLE(0x16,streamFile);

        num_samples = pcm_bytes_to_samples(read_32bitLE(0x18,streamFile), channel_count, bps); /* true even for Ogg */

        /* later games use "OggS" */
        if (version == 1)
            start_offset = 0x1c; /* Nancy Drew: The Final Scene (PC) */
        else if (version == 2 && read_32bitBE(0x1e,streamFile) == 0x4F676753)
            start_offset = 0x1e; /* Nancy Drew: The Haunted Carousel (PC) */
        else if (version == 2 && read_32bitBE(0x20,streamFile) == 0x4F676753)
            start_offset = 0x20; /* Nancy Drew: The Silent Spy (PC) */
        else
            goto fail;
    }
    else {
        goto fail;
    }


    if (version == 2) {
#ifdef VGM_USE_VORBIS
        ogg_vorbis_meta_info_t ovmi = {0};

        ovmi.meta_type = meta_HIS;
        return init_vgmstream_ogg_vorbis_callbacks(streamFile, NULL, start_offset, &ovmi);
#else
        goto fail;
#endif
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
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

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
