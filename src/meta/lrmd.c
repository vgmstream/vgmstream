#include "meta.h"
#include "../coding/coding.h"
#include "lrmd_streamfile.h"

/* LRMD - Sony/SCEI's format (Loco Roco Music Data?) [LocoRoco 2 (PSP), LocoRoco: Midnight Carnival (PSP)] */
VGMSTREAM* init_vgmstream_lrmd(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* sb = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t stream_offset, section1_offset, section2_offset, basename_offset, subname_offset;
    size_t stream_size, max_chunk, block_size = 0, chunk_start, chunk_size;
    int loop_flag, channel_count, sample_rate, layers;
    int32_t num_samples, loop_start, loop_end;
    int total_subsongs, target_subsong = sf->stream_index;


    /* checks */
    if (!is_id32be(0x00, sf, "LRMD"))
        return NULL;
    /* 0x00: version 1? */
    /* 0x08: header size */
    /* 0x0c: body size */

    if (!is_id32be(0x10, sf, "REQD"))
        return NULL;
    /* 0x14: chunk size */
    /* 0x18: null? */
    /* 0x1c: 1? */

    /* .lrmh+lrmb: actual extensions */
    if (!check_extensions(sf, "lrmh"))
        goto fail;

    sb = open_streamfile_by_ext(sf, "lrmb");
    if (!sb) goto fail;

    /* 0x20: null */
    basename_offset = read_u32le(0x24, sf);
    if (read_u16le(0x28, sf) != 0x4000) { /* pitch? */
        VGM_LOG("LRMD: unknown value\n");
        goto fail;
    }
    max_chunk = read_u16le(0x2a, sf);
    num_samples = read_u32le(0x2c, sf);
    /* 0x30: null? */
    /* 0x34: data size for all layers */
    layers = read_u32le(0x38, sf);
    section1_offset = read_u32le(0x3c, sf);
    /* 0x40: lip table entries */
    /* 0x44: lip table offset */
    /* 0x48: section2 flag */
    section2_offset = read_u32le(0x4c, sf);
    /* 0x40: section3 flag */
    /* 0x44: section3 (unknown) */

    total_subsongs = layers;
    if (target_subsong == 0) target_subsong = 1;
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;

    /* data is divided into N interleaved layers sharing config (channels may vary), and
     * since they have names it's worth showing as subsongs */

    /* section1: layer config */
    {
        int i;
        int frame_size = max_chunk / layers / 2; /* even for songs with mono layers */

        chunk_size = 0;
        for (i = 0; i < layers; i++) {
            off_t header_offset = section1_offset + i * 0x18;
            int layer_channels;

            /* not too sure but needed for LR2's muihouse last 3 layers */
            layer_channels = read_u8(header_offset + 0x0d, sf) != 0 ? 1 : 2;

            if (i + 1 == target_subsong) {
                /* 0x00: null */
                subname_offset = read_u32le(header_offset + 0x04, sf);
                /* 0x08: unk */
                /* 0x0c: flags? */
                /* 0x10: null? */
                /* 0x14: null? */

                chunk_start = chunk_size;
                block_size = frame_size * layer_channels;

                channel_count = layer_channels;
                sample_rate = 44100;
            }

            chunk_size += frame_size * layer_channels;
        }
        if (block_size == 0)
            goto fail;
    }

    /* section2: loops */
    /* 0x00: offset to "loop" name */
    if (section2_offset > 0) {
        loop_end   = read_u32le(section2_offset + 0x04, sf);
        loop_start = read_u32le(section2_offset + 0x08, sf);
        loop_flag  = read_u32le(section2_offset + 0x0c, sf);
    }
    else {
        loop_end = 0;
        loop_start = 0;
        loop_flag = 0;
    }


    /* data de-interleave */
    temp_sf = setup_lrmd_streamfile(sb, block_size, chunk_start, chunk_size);
    if (!temp_sf) goto fail;

    stream_offset = 0x00;
    stream_size = get_streamfile_size(temp_sf);


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_LRMD;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;
    vgmstream->stream_size = stream_size;
    vgmstream->num_streams = total_subsongs;

#ifdef VGM_USE_FFMPEG
    {
        int encoder_delay = 1024; /* assumed */

        vgmstream->num_samples -= encoder_delay;

        vgmstream->codec_data = init_ffmpeg_atrac3_raw(temp_sf, stream_offset, stream_size, vgmstream->num_samples, vgmstream->channels, vgmstream->sample_rate, block_size, encoder_delay);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;
    }
#else
    goto fail;
#endif

    /* name custom main + layer name */
    {
        int name_len = read_string(vgmstream->stream_name, STREAM_NAME_SIZE - 1, basename_offset, sf);

        strcat(vgmstream->stream_name, "/");
        name_len++;

        read_string(vgmstream->stream_name + name_len, STREAM_NAME_SIZE - name_len, subname_offset, sf);
    }

    if (!vgmstream_open_stream(vgmstream, temp_sf, stream_offset))
        goto fail;

    close_streamfile(sb);
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(sb);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}
