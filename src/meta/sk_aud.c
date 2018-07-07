#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

static int get_sk_num_samples(STREAMFILE *streamFile, off_t start_offset);

/* AUD/SK - Silicon Knights obfuscated Ogg (cutscene/voices) [Eternal Darkness (GC)] */
VGMSTREAM * init_vgmstream_sk_aud(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count, sample_rate;

    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"aud"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x11534B10) /* \11"SK"\10 */
        goto fail;

    /* the format is just mutant Ogg so actually peeking into the Vorbis id packet here */
    channel_count   = read_8bit   (0x23,streamFile);
    sample_rate     = read_32bitLE(0x24,streamFile);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = get_sk_num_samples(streamFile, 0);
    vgmstream->meta_type = meta_SK_AUD;

#ifdef VGM_USE_VORBIS
    {
        vorbis_custom_config cfg = {0};

        vgmstream->layout_type = layout_none;
        vgmstream->coding_type = coding_VORBIS_custom;
        vgmstream->codec_data = init_vorbis_custom(streamFile, 0x00, VORBIS_SK, &cfg);
        if (!vgmstream->codec_data) goto fail;

        start_offset = cfg.data_start_offset;
    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* SK/Ogg doesn't have num_samples info, manually read total samples */
static int get_sk_num_samples(STREAMFILE *streamFile, off_t start_offset) {
    uint32_t expected_id = 0x11534B10; /* \11"SK"\10 (would read "OggS" by changing the ID) */
    off_t off = get_streamfile_size(streamFile) - 4-1-1-8-4-4-4;

    /* simplest way is to find last OggS/SK page from stream end */
    while (off >= start_offset) {
        uint32_t current_id = read_32bitBE(off, streamFile);
        if (current_id == expected_id) { /* last packet starts with 0x0004, if more checks are needed */
            return read_32bitLE(off+4+1+1, streamFile); /* get last granule = total samples (64b but whatevs) */
        }

        off--;
    }

    return 0;
}
