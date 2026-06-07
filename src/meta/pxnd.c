#include "meta.h"
#include "../coding/coding.h"


/* PXND - from Pixelbite games [Space Marshals 3 (Android/iOS), Cypher 007 (macOS)] */
VGMSTREAM* init_vgmstream_pxnd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;

    /* checks */
    if (!is_id32be(0x00,sf, "PXND"))
        return NULL;
    /* .snd: actual extension in exes/bigfiles */
    if (!check_extensions(sf, "snd"))
        return NULL;

    int version         = read_u32be(0x04, sf);
    int channels        = read_s32be(0x08, sf);
    int sample_rate     = read_s32be(0x0c, sf);
    int32_t num_samples = read_s32be(0x10, sf); // without encoder delay
    int encoder_delay   = read_s32be(0x14, sf); // always 2112

    bool loop_flag;
    int32_t loop_start = 0, loop_end = 0;
    uint32_t stream_size, stream_offset;
    switch (version) {
        case 3:
            stream_size = read_u32be(0x18, sf);
            stream_offset = 0x1c;

            loop_start = 0;
            loop_end = num_samples;
            loop_flag = false; // v3 files do full loops, unsure about sfx
            break;

        case 6:
            loop_start  = read_s32be(0x18, sf);
            loop_end    = read_s32be(0x1c, sf);
            stream_size = read_u32be(0x20, sf);
            stream_offset = 0x24;

            loop_flag = loop_start > 0; // some files with loop_start 0 do full loops but others don't, no proper way no know
            break;

        default:
            return NULL;
    }

    if (stream_offset  + stream_size != get_streamfile_size(sf))
        return NULL;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

    // has raw AAC data (no frame sizes), so it's (probably?) equivalent to ADIF AAC.
    vgmstream->codec_data = init_aac_raw(sample_rate, channels, encoder_delay);
    if (!vgmstream->codec_data) goto fail;
    vgmstream->coding_type = coding_AAC_raw;
    vgmstream->layout_type = layout_none;

    vgmstream->meta_type = meta_PXND;

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;
    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}
