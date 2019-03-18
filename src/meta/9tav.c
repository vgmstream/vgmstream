#include "meta.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "9tav_streamfile.h"

/* 9TAV - from Metal Gear Solid 2/3 HD (Vita) */
VGMSTREAM * init_vgmstream_9tav(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag, channel_count, sample_rate, track_count;
    int32_t num_samples, loop_start, loop_end;
    size_t track_size;
    uint32_t config_data;
    int i, is_padded;
    layered_layout_data * data = NULL;
    STREAMFILE* temp_streamFile = NULL;


    /* checks */
    /* .9tav: header id */
    if (!check_extensions(streamFile, "9tav"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x39544156) /* "9TAV" */
        goto fail;

    /* 0x04: always 0x09 */
    channel_count   = read_16bitLE(0x08,streamFile);
    track_count     = read_16bitLE(0x0a,streamFile); /* MGS3 uses multitracks */
    sample_rate     = read_32bitLE(0x0c,streamFile);
    track_size      = read_32bitLE(0x10,streamFile); /* without padding */
  //data_size       = read_32bitLE(0x14,streamFile); /* without padding */
    num_samples     = read_32bitLE(0x18,streamFile);
    config_data     = read_32bitBE(0x1c,streamFile);


    if (read_32bitBE(0x20,streamFile) == 0x4D544146) { /* "MTAF" */
        /* MGS3 has a MTAF header (data size and stuff don't match, probably for track info) */
        loop_start  = read_32bitLE(0x78, streamFile);
        loop_end    = read_32bitLE(0x7c, streamFile);
        loop_flag   = read_32bitLE(0x90, streamFile) & 1;

        is_padded = 1; /* data also has padding and other oddities */
        start_offset = 0x00;
    }
    else {
        /* MGS2 doesn't */
        loop_start = 0;
        loop_end = 0;
        loop_flag = 0;

        is_padded = 0;
        start_offset = 0x20;
    }


    /* init layout */
    data = init_layout_layered(track_count);
    if (!data) goto fail;

    /* open each layer subfile */
    for (i = 0; i < data->layer_count; i++) {
        data->layers[i] = allocate_vgmstream(channel_count, loop_flag);
        if (!data->layers[i]) goto fail;

        data->layers[i]->meta_type = meta_9TAV;
        data->layers[i]->sample_rate = sample_rate;
        data->layers[i]->num_samples = num_samples;
        data->layers[i]->loop_start_sample = loop_start;
        data->layers[i]->loop_end_sample = loop_end;

#ifdef VGM_USE_ATRAC9
        {
            atrac9_config cfg = {0};
            cfg.channels = channel_count;
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
            temp_streamFile = setup_9tav_streamfile(streamFile, 0xFE4, track_size, i, track_count);
            if (!temp_streamFile) goto fail;
        }

        if (!vgmstream_open_stream(data->layers[i],temp_streamFile == NULL ? streamFile : temp_streamFile,start_offset))
            goto fail;

        close_streamfile(temp_streamFile);
        temp_streamFile = NULL;
    }

    /* setup layered VGMSTREAMs */
    if (!setup_layout_layered(data))
        goto fail;

    /* build the layered VGMSTREAM */
    vgmstream = allocate_layered_vgmstream(data);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    if (!vgmstream)
        free_layout_layered(data);
    return NULL;
}
