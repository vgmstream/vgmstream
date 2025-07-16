#include "meta.h"
#include "../coding/coding.h"

#if defined(VGM_USE_MP4V2) && defined(VGM_USE_FDKAAC)

// VGM_USE_FDKAAC
static VGMSTREAM* init_vgmstream_mp4_aac_offset(STREAMFILE* sf, uint64_t start, uint64_t size);

VGMSTREAM* init_vgmstream_mp4_aac(STREAMFILE* sf) {
    return init_vgmstream_mp4_aac_offset( sf, 0x00, get_streamfile_size(sf));
}

static VGMSTREAM* init_vgmstream_mp4_aac_offset(STREAMFILE* sf, uint64_t start, uint64_t size) {
    VGMSTREAM* vgmstream = NULL;
    mp4_aac_codec_data* data = NULL;

    data = init_mp4_aac(sf);
    if (!data) goto fail;

    int channels = mp4_aac_get_channels(data);

    vgmstream = allocate_vgmstream(channels, 0);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = mp4_aac_get_sample_rate(data);
    vgmstream->num_samples = mp4_aac_get_samples(data);

    vgmstream->codec_data = data;
    vgmstream->coding_type = coding_MP4_AAC;

    vgmstream->layout_type = layout_none;
    vgmstream->meta_type = meta_MP4;

    return vgmstream;
fail:
    free_mp4_aac(data);
    if (vgmstream) {
        vgmstream->codec_data = NULL;
        close_vgmstream(vgmstream);
    }
    return NULL;
}
#endif
