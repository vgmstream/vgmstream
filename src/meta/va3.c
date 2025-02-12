#include "meta.h"
#include "../coding/coding.h"


/* VA3 - from Konami games [Dance Dance Revolution Supernova 2 (Arcade)] */
VGMSTREAM* init_vgmstream_va3(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels;
    uint32_t data_size;

    /* checks */
    if (!is_id32be(0x00, sf, "!3AV"))
        return NULL;
    // .va3: actual extension
    if (!check_extensions(sf, "va3"))
        return NULL;

    start_offset = 0x800;
    data_size = read_u32le(0x04, sf);
    loop_flag = 0;
    channels = 2;

    //0c: null (loops?)
    //10: null (loops?)
    //18: flag? (always 1)
    //1c: always 0x0200 (channels?)
    //20: always 100 (volume?)


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_VA3;
    vgmstream->num_samples = read_s32le(0x08, sf);
    vgmstream->sample_rate = read_s32le(0x14, sf);

#ifdef VGM_USE_FFMPEG
    {
        int block_align = 0xC0 * vgmstream->channels;
        int encoder_delay = 0; //TODO

        vgmstream->codec_data = init_ffmpeg_atrac3_raw(sf, start_offset,data_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
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
