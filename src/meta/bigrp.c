#include "meta.h"
#include "../coding/coding.h"
#include "../coding/ice_decoder_icelib.h"


/* .BIGRP - from Inti Creates "ICE"/"Imperial" engine [Blaster Master Zero 2 (SW), Gunvolt 3 (SW)] */
VGMSTREAM* init_vgmstream_bigrp(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    uint32_t header_size, entry_size, stream_size;
    int total_subsongs, target_subsong = sf->stream_index;
    int codec, channels, loop_flag, sample_rate;
    int32_t loop_start, num_samples;


    /* checks */

    /* simple checks to avoid opening the lib if possible
     * early games use smaller sizes [Bloodstained COTM (Vita), Mighty Gunvolt Burst (PC)] */
    header_size = read_u32le(0x00,sf);
    if (read_u32le(0x00,sf) != 0x0c && read_u32le(0x00,sf) != 0x10)
        goto fail;
    entry_size = read_u32le(0x04,sf);
    if (entry_size != 0x34 && entry_size != 0x40)
        goto fail;

    if (!check_extensions(sf, "bigrp"))
        goto fail;

    if (target_subsong == 0) target_subsong = 1;
    total_subsongs = read_s32le(0x08,sf);
    if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;


    /* could read all this from the lib, meh */
    {
        uint32_t offset = header_size + entry_size * (target_subsong - 1);

        /* 00: hash */
        /* 04: group? */
        codec   = read_u32le(offset + 0x08, sf);

        switch(codec) {
            case 0x00: /* range [BMZ2 (SW), Bloodstained COTM (Vita), BMZ1 (PS4)] */
            case 0x03:
                sample_rate = read_s32le(offset + 0x0c, sf);
                channels    = read_u8   (offset + 0x10, sf);
                /* 0x11: spf */
                /* 0x12: unknown (volume?) */
                loop_flag   = read_u32le(offset + 0x14, sf);
                /* 0x18: frame codes */
                loop_start  = read_s32le(offset + 0x1c, sf);
                stream_size = read_u32le(offset + 0x20, sf); /* intro block */
                /* 0x24: intro offset */
                num_samples = read_s32le(offset + 0x28, sf) + loop_start;
                stream_size = read_u32le(offset + 0x2c, sf) + stream_size; /* body block */
                /* 0x30: body offset */
                break;

            default:
                /* dummy to avoid breaking some files that mix codecs with midi */
                channels =  1;
                sample_rate = 48000;
                loop_flag = 0;
                loop_start = 0;
                num_samples = sample_rate;
                stream_size = 0;
                break;
        }
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_BIGRP;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = num_samples;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;

    switch(codec) {
        case 0x00:
        case 0x03:
            vgmstream->codec_data = init_ice(sf, target_subsong);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = codec == 0x00 ? coding_ICE_RANGE : coding_ICE_DCT;
            vgmstream->layout_type = layout_none;
            break;

        case 0x01:
        case 0x02:
            vgmstream->coding_type = coding_SILENCE;
            snprintf(vgmstream->stream_name, STREAM_NAME_SIZE, "[%s]", (codec == 0x01 ? "data" : "midi"));
            break;

        default:
            goto fail;
    }

    //if (!vgmstream_open_stream(vgmstream, sf, 0x00))
    //    goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
