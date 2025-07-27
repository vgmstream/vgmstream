#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"

static int32_t get_sk_num_samples(STREAMFILE* sf, off_t start_offset);

/* SK (.AUD) - Silicon Knights obfuscated Ogg (cutscene/voices) [Eternal Darkness (GC)] */
VGMSTREAM* init_vgmstream_sk_aud(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channels, sample_rate;

    /* checks */
    if (read_u32be(0x00,sf) != 0x11534B10) /* \11"SK"\10 */
        return NULL;

    if (!check_extensions(sf,"aud"))
        return NULL;

    /* the format is just mutant Ogg so actually peeking into the Vorbis id packet here */
    channels    = read_u8   (0x23,sf);
    sample_rate = read_s32le(0x24,sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = get_sk_num_samples(sf, 0);
    vgmstream->meta_type = meta_SK_AUD;

#ifdef VGM_USE_VORBIS
    {
        vorbis_custom_config cfg = {0};

        vgmstream->layout_type = layout_none;
        vgmstream->coding_type = coding_VORBIS_custom;
        vgmstream->codec_data = init_vorbis_custom(sf, 0x00, VORBIS_SK, &cfg);
        if (!vgmstream->codec_data) goto fail;

        start_offset = cfg.data_start_offset;
    }
#else
    goto fail;
#endif

    if (!vgmstream_open_stream(vgmstream, sf, start_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

// TODO: improve reads in blocks
/* SK/Ogg doesn't have num_samples info, manually read total samples */
static int32_t get_sk_num_samples(STREAMFILE* sf, off_t start_offset) {
    uint32_t expected_id = 0x11534B10; /* \11"SK"\10 (would read "OggS" by changing the ID) */
    off_t off = get_streamfile_size(sf) - 4-1-1-8-4-4-4;

    /* simplest way is to find last OggS/SK page from stream end */
    while (off >= start_offset) {
        uint32_t current_id = read_u32be(off, sf);
        if (current_id == expected_id) { /* last packet starts with 0x0004, if more checks are needed */
            return read_s32le(off+4+1+1, sf); /* get last granule = total samples (64b but whatevs) */
        }

        off--;
    }

    return 0;
}
