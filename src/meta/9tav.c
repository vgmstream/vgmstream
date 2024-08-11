#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "../util/layout_utils.h"
#include "9tav_streamfile.h"

/* 9TAV - from Metal Gear Solid 2/3 HD (Vita) */
VGMSTREAM* init_vgmstream_9tav(STREAMFILE* sf) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channels, sample_rate, track_count;
    int32_t num_samples, loop_start, loop_end;
    size_t track_size;
    uint32_t config_data;
    bool is_padded;
    layered_layout_data* data = NULL;
    STREAMFILE* temp_sf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "9TAV"))
        return NULL;
    /* .9tav: header id */
    if (!check_extensions(sf, "9tav"))
        return NULL;

    /* 0x04: always 0x09 (codec?) */
    channels        = read_u16le(0x08,sf);
    track_count     = read_u16le(0x0a,sf); /* MGS3 uses multitracks */
    sample_rate     = read_s32le(0x0c,sf);
    track_size      = read_u32le(0x10,sf); /* without padding */
  //data_size       = read_u32le(0x14,sf); /* without padding */
    num_samples     = read_s32le(0x18,sf);
    config_data     = read_u32be(0x1c,sf);


    if (is_id32be(0x20,sf, "MTAF")) {
        /* MGS3 has a MTAF header (data size and stuff don't match, probably for track info) */
        loop_start  = read_s32le(0x78, sf);
        loop_end    = read_s32le(0x7c, sf);
        loop_flag   = read_u32le(0x90, sf) & 1;

        is_padded = true; /* data also has padding and other oddities */
        start_offset = 0x00;
    }
    else {
        /* MGS2 doesn't */
        loop_start = 0;
        loop_end = 0;
        loop_flag = false;

        is_padded = false;
        start_offset = 0x20;
    }


    /* init layout */
    data = init_layout_layered(track_count);
    if (!data) goto fail;

    /* open each layer subfile */
    for (int i = 0; i < data->layer_count; i++) {
        data->layers[i] = allocate_vgmstream(channels, loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->meta_type = meta_9TAV;
        data->layers[i]->sample_rate = sample_rate;
        data->layers[i]->num_samples = num_samples;
        data->layers[i]->loop_start_sample = loop_start;
        data->layers[i]->loop_end_sample = loop_end;

#ifdef VGM_USE_ATRAC9
        {
            atrac9_config cfg = {0};
            cfg.channels = channels;
            cfg.config_data = config_data;
            cfg.encoder_delay = atrac9_bytes_to_samples_cfg(track_size, cfg.config_data) - num_samples;  /* seems ok */
            if (cfg.encoder_delay > 4096) /* doesn't seem too normal */
                cfg.encoder_delay = 0;

            data->layers[i]->codec_data = init_atrac9(&cfg);
            if (!data->layers[i]->codec_data) goto fail;
            data->layers[i]->coding_type = coding_ATRAC9;
            data->layers[i]->layout_type = layout_none;
        }
#else
        goto fail;
#endif

        if (is_padded) {
            temp_sf = setup_9tav_streamfile(sf, 0xFE4, track_size, i, track_count);
            if (!temp_sf) goto fail;
        }

        if (!vgmstream_open_stream(data->layers[i],temp_sf == NULL ? sf : temp_sf,start_offset))
            goto fail;

        close_streamfile(temp_sf);
        temp_sf = NULL;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;

    /* build the layered VGMSTREAM */
    vgmstream = allocate_layered_vgmstream(data);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_layered(data);
    return NULL;
}
