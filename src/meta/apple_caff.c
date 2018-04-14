#include "meta.h"
#include "../coding/coding.h"

/* Apple Core Audio Format File - from iOS games [Vectros (iOS), Ridge Racer Accelerated (iOS)] */
VGMSTREAM * init_vgmstream_apple_caff(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset = 0, chunk_offset;
    size_t file_size, data_size = 0;
    int loop_flag, channel_count = 0, sample_rate = 0;

    int found_desc = 0 /*, found_pakt = 0*/, found_data = 0;
    uint32_t codec = 0 /*, codec_flags = 0*/;
    uint32_t bytes_per_packet = 0, samples_per_packet = 0, channels_per_packet = 0, bits_per_sample = 0;
    int valid_samples = 0 /*, priming_samples = 0, unused_samples = 0*/;


    /* checks */
    if (!check_extensions(streamFile, "caf"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x63616666) /* "caff" */
        goto fail;
    if (read_32bitBE(0x04,streamFile) != 0x00010000) /* version/flags */
        goto fail;

    file_size = get_streamfile_size(streamFile);
    chunk_offset = 0x08;

    while (chunk_offset < file_size) {
        uint32_t chunk_type = read_32bitBE(chunk_offset+0x00,streamFile);
        uint32_t chunk_size = (uint32_t)read_64bitBE(chunk_offset+0x04,streamFile);
        chunk_offset += 0x0c;

        switch (chunk_type) {

            case 0x64657363: /* "desc" */
                found_desc = 1;

                {
                    uint64_t sample_long = (uint64_t)read_64bitBE(chunk_offset+0x00, streamFile);
                    double* sample_double; /* double sample rate, double the fun */

                    sample_double = (double*)&sample_long;
                    sample_rate = (int)(*sample_double);
                }

                codec = read_32bitBE(chunk_offset+0x08, streamFile);
                //codec_flags         = read_32bitBE(chunk_offset+0x0c, streamFile);
                bytes_per_packet    = read_32bitBE(chunk_offset+0x10, streamFile);
                samples_per_packet  = read_32bitBE(chunk_offset+0x14, streamFile);
                channels_per_packet = read_32bitBE(chunk_offset+0x18, streamFile);
                bits_per_sample     = read_32bitBE(chunk_offset+0x1C, streamFile);
                break;

            case 0x70616b74:    /* "pakt" */
                //found_pakt = 1;

                //packets_table_size = (uint32_t)read_64bitBE(chunk_offset+0x00,streamFile); /* 0 for constant bitrate */
                valid_samples = (uint32_t)read_64bitBE(chunk_offset+0x08,streamFile);
                //priming_samples = read_32bitBE(chunk_offset+0x10,streamFile); /* encoder delay samples */
                //unused_samples = read_32bitBE(chunk_offset+0x14,streamFile); /* footer samples */
                break;

            case 0x64617461: /* "data" */
                found_data = 1;

                /* 0x00: version? 0x00/0x01 */
                start_offset = chunk_offset + 0x04;
                data_size = chunk_size - 0x4;
                break;

            default: /*  "free" "kuki" "info" "chan" etc: ignore */
                break;
        }

        /* done with chunk */
        chunk_offset += chunk_size;
    }

    if (!found_desc || !found_data)
        goto fail;
    if (start_offset == 0 || data_size == 0)
        goto fail;


    loop_flag = 0;
    channel_count = channels_per_packet;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_CAFF;

    switch(codec) {
        case 0x6C70636D: /* "lpcm" */
            vgmstream->num_samples = valid_samples;
            if (!vgmstream->num_samples)
                vgmstream->num_samples = pcm_bytes_to_samples(data_size, channel_count, bits_per_sample);

            //todo check codec_flags for BE/LE, signed/etc
            if (bits_per_sample == 8) {
                vgmstream->coding_type = coding_PCM8;
            }
            else {
                goto fail;
            }
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = bytes_per_packet / channels_per_packet;

            break;

        case 0x696D6134: /* "ima4" [Vectros (iOS), Dragon Quest (iOS)] */
            vgmstream->num_samples = valid_samples;
            if (!vgmstream->num_samples) /* rare [Endless Fables 2 (iOS) */
                vgmstream->num_samples = apple_ima4_bytes_to_samples(data_size, channel_count);

            vgmstream->coding_type = coding_APPLE_IMA4;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = bytes_per_packet / channels_per_packet;

            /* ima4 defaults */
            //if (channels_per_packet != 1 && channels_per_packet != 2)
            //    goto fail;
            if (samples_per_packet != 64)
                goto fail;
            if ((samples_per_packet / 2 + 2) * channels_per_packet != bytes_per_packet)
                goto fail;
            if (bits_per_sample != 0 && bits_per_sample != 4) /* 4 is rare too [Endless Fables 2 (iOS) */
                goto fail;

            /* check for full packets and that all packets are accounted for */
            //if (found_pakt) {
            //    if (data_size % (vgmstream->interleave_block_size*channel_count) != 0)
            //        goto fail;
            //    if ((valid_samples+unused_samples)%((vgmstream->interleave_block_size-2)*2) != 0)
            //        goto fail;
            //    if (data_size/vgmstream->interleave_block_size/channel_count !=
            //            (valid_samples+unused_samples)/((vgmstream->interleave_block_size-2)*2))
            //        goto fail;
            //}

            break;

        case 0x61616320: /* "aac " [Ridge Racer Accelerated (iOS)] */
        case 0x616C6163: /* "alac" [Chrono Trigger v1 (iOS)] */
        default: /* should be parsed by FFMpeg in its meta (involves parsing complex chunks) */
            goto fail;
    }

    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}
