#include "meta.h"
#include "../coding/coding.h"

/* .AUDIO_DATA - Traveller's Tales "NTT" engine audio format [Lego Star Wars: The Skywalker Saga (PC/Switch)] */
VGMSTREAM* init_vgmstream_tt_ad(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t offset, stream_offset, stream_size;
    int loop_flag, channels, sample_rate, codec, frame_size = 0;
    int32_t num_samples;


    /* checks */
    if (!is_id32be(0x00,sf, "FMT "))
        goto fail;

    /* actual extension */
    if (!check_extensions(sf, "audio_data"))
        goto fail;

    offset = 0x08;
    /* 0x00: null */
    codec = read_u16le(offset + 0x02,sf);
    sample_rate = read_s32le(offset + 0x04,sf);
    num_samples = read_s32le(offset + 0x08,sf);
    channels = read_u8(offset + 0x0c,sf);
    /* 0x0d: bps (16=IMA, 32=Ogg) */
    /* 0x10: 
        Ogg = some size?
        IMA = frame size + flag? */
    if (codec == 0x0a)
        frame_size = read_u16le(offset + 0x10,sf);


    loop_flag = 0; /* music just repeats? */

    offset += read_u32le(0x04, sf);

    /* Ogg seek table*/
    if (is_id32be(offset, sf, "SEEK")) {
        offset += 0x08 + read_u32le(offset + 0x04, sf);
    }

    /* found with some IMA */
    if (is_id32be(offset, sf, "RMS ")) {
        offset += 0x08 + read_u32le(offset + 0x04, sf);
    }

    if (!is_id32be(offset, sf, "DATA"))
        goto fail;

    stream_offset = offset + 0x08;
    stream_size = read_u32le(offset + 0x04, sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_TT_AD;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;

    switch(codec) {
#ifdef VGM_USE_VORBIS
        case 0x01: {
            vgmstream->codec_data = init_ogg_vorbis(sf, stream_offset, stream_size, NULL);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_OGG_VORBIS;
            vgmstream->layout_type = layout_none;

            break;
        }
#endif

        case 0x0a:
            vgmstream->coding_type = coding_MS_IMA_mono;
            vgmstream->layout_type = layout_blocked_tt_ad;
            vgmstream->frame_size = frame_size;
            vgmstream->interleave_block_size = frame_size;
            break;

        default:
            vgm_logi("FMT: unsupported codec 0x%x\n", codec);
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream, sf, stream_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
